//
//  IntelligentKeyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/11/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "IntelligentKeyboard.hpp"

using namespace Atari::ST;

IntelligentKeyboard::IntelligentKeyboard(Serial::Line &input, Serial::Line &output) : output_line_(output) {
	input.set_read_delegate(this, Storage::Time(2, 15625));
	output_line_.set_writer_clock_rate(15625);

	mouse_button_state_ = 0;
	mouse_movement_[0] = 0;
	mouse_movement_[1] = 0;
}

bool IntelligentKeyboard::serial_line_did_produce_bit(Serial::Line *, int bit) {
	// Shift.
	command_ = (command_ >> 1) | (bit << 9);

	// If that's 10 bits, decode a byte and stop.
	bit_count_ = (bit_count_ + 1) % 10;
	if(!bit_count_) {
		dispatch_command(uint8_t(command_ >> 1));
		command_ = 0;
		return false;
	}

	// Continue.
	return true;
}

ClockingHint::Preference IntelligentKeyboard::preferred_clocking() {
	return output_line_.transmission_data_time_remaining().as_integral() ? ClockingHint::Preference::RealTime : ClockingHint::Preference::None;
}

void IntelligentKeyboard::run_for(HalfCycles duration) {
	// Take this opportunity to check for mouse and keyboard events,
	// which will have been received asynchronously.
	if(mouse_mode_ == MouseMode::Relative) {
		const int captured_movement[2] = { mouse_movement_[0].load(), mouse_movement_[1].load() };
		const int captured_button_state = mouse_button_state_;
		if(
			(posted_button_state_ != captured_button_state) ||
			(abs(captured_movement[0]) >= mouse_threshold_[0]) ||
			(abs(captured_movement[1]) >= mouse_threshold_[1]) ) {
			mouse_movement_[0] -= captured_movement[0];
			mouse_movement_[1] -= captured_movement[1];

			post_relative_mouse_event(captured_movement[0], captured_movement[1]);
		}
	} else {

	}

	// Forward key changes; implicit assumption here: mutexs are cheap while there's
	// negligible contention.
	{
		std::lock_guard<decltype(key_queue_mutex_)> guard(key_queue_mutex_);
		for(uint8_t key: key_queue_) {
			output_bytes({key});
		}
		key_queue_.clear();
	}

	output_line_.advance_writer(duration);
}

void IntelligentKeyboard::output_bytes(std::initializer_list<uint8_t> values) {
	// Wrap the value in a start and stop bit, and send it on its way.
	for(auto value : values) {
		output_line_.write(2, 10, 0x200 | (value << 1));
	}
	update_clocking_observer();
}

void IntelligentKeyboard::dispatch_command(uint8_t command) {
	// Enqueue for parsing.
	command_sequence_.push_back(command);

	// For each possible command, check that the proper number of bytes are present.
	// If not, exit. If so, perform and drop out of the switch.
	switch(command_sequence_.front()) {
		default:
			printf("Unrecognised IKBD command %02x\n", command);
		break;

		case 0x80:
			/*
				Reset: 0x80 0x01.
				"Any byte following an 0x80 command byte other than 0x01 is ignored (and causes the 0x80 to be ignored)."
			*/
			if(command_sequence_.size() != 2) return;
			if(command_sequence_[1] == 0x01) {
				reset();
			}
		break;

		case 0x07:
			if(command_sequence_.size() != 2) return;
			set_mouse_button_actions(command_sequence_[1]);
		break;

		case 0x08:
			set_relative_mouse_position_reporting();
		break;

		case 0x09:
			if(command_sequence_.size() != 5) return;
			set_absolute_mouse_position_reporting(
				uint16_t((command_sequence_[1] << 8) | command_sequence_[2]),
				uint16_t((command_sequence_[3] << 8) | command_sequence_[4])
			);
		break;

		case 0x0a:
			if(command_sequence_.size() != 3) return;
			set_mouse_keycode_reporting(command_sequence_[1], command_sequence_[2]);
		break;

		case 0x0b:
			if(command_sequence_.size() != 3) return;
			set_mouse_threshold(command_sequence_[1], command_sequence_[2]);
		break;

		case 0x0c:
			if(command_sequence_.size() != 3) return;
			set_mouse_scale(command_sequence_[1], command_sequence_[2]);
		break;

		case 0x0d:
			interrogate_mouse_position();
		break;

		case 0x0e:
			if(command_sequence_.size() != 6) return;
			/* command_sequence_[1] has no defined meaning. */
			set_mouse_position(
				uint16_t((command_sequence_[2] << 8) | command_sequence_[3]),
				uint16_t((command_sequence_[4] << 8) | command_sequence_[5])
			);
		break;

		case 0x0f:	set_mouse_y_upward();		break;
		case 0x10:	set_mouse_y_downward();		break;
		case 0x11:	resume();					break;
		case 0x12:	disable_mouse();			break;
		case 0x13:	pause();					break;

		case 0x14:	set_joystick_event_mode();			break;
		case 0x15:	set_joystick_interrogation_mode();	break;
		case 0x16:	interrogate_joysticks();			break;
		case 0x1a:	disable_joysticks();				break;
	}

	// There was no premature exit, so a complete command sequence must have been satisfied.
	command_sequence_.clear();
}

void IntelligentKeyboard::reset() {
	// Reset should perform a self test, lasting at most 200ms, then post 0xf0.
	// Following that it should look for any keys that currently seem to be pressed.
	// Those are considered stuck and a break code is generated for them.
	output_bytes({0xf0});
}

void IntelligentKeyboard::resume() {
}

void IntelligentKeyboard::pause() {
}

void IntelligentKeyboard::disable_mouse() {
}

void IntelligentKeyboard::set_relative_mouse_position_reporting() {
}

void IntelligentKeyboard::set_absolute_mouse_position_reporting(uint16_t max_x, uint16_t max_y) {
}

void IntelligentKeyboard::set_mouse_position(uint16_t x, uint16_t y) {
}

void IntelligentKeyboard::set_mouse_keycode_reporting(uint8_t delta_x, uint8_t delta_y) {
}

void IntelligentKeyboard::set_mouse_threshold(uint8_t x, uint8_t y) {
}

void IntelligentKeyboard::set_mouse_scale(uint8_t x, uint8_t y) {
}

void IntelligentKeyboard::set_mouse_y_downward() {
}

void IntelligentKeyboard::set_mouse_y_upward() {
}

void IntelligentKeyboard::set_mouse_button_actions(uint8_t actions) {
}

void IntelligentKeyboard::interrogate_mouse_position() {
	output_bytes({
		0xf7,	// Beginning of mouse response.
		0x00,	// 0000dcba; a = right button down since last interrogation, b = right button up since, c/d = left button.
		0x00,	// x motion: MSB, LSB
		0x00,
		0x00,	// y motion: MSB, LSB
		0x00
	});
}

void IntelligentKeyboard::post_relative_mouse_event(int x, int y) {
	posted_button_state_ = mouse_button_state_;

	// Break up the motion to impart, if it's too large.
	do {
		int stepped_motion[2] = {
			(x >= -128 && x < 127) ? x : (x > 0 ? 127 : -128),
			(y >= -128 && y < 127) ? y : (y > 0 ? 127 : -128),
		};

		output_bytes({
			uint8_t(0xf8 | posted_button_state_),	// Command code is a function of button state.
			uint8_t(stepped_motion[0]),
			uint8_t(stepped_motion[1]),
		});

		x -= stepped_motion[0];
		y -= stepped_motion[1];
	} while(x || y);
}

// MARK: - Keyboard Input
void IntelligentKeyboard::set_key_state(Key key, bool is_pressed) {
	std::lock_guard<decltype(key_queue_mutex_)> guard(key_queue_mutex_);
	if(is_pressed) {
		key_queue_.push_back(uint8_t(key));
	} else {
		key_queue_.push_back(0x80 | uint8_t(key));
	}
}

uint16_t IntelligentKeyboard::KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) {
	using Key = Inputs::Keyboard::Key;
	using STKey = Atari::ST::Key;
	switch(key) {
		default: return KeyboardMachine::MappedMachine::KeyNotMapped;

#define Bind(x, y) case Key::x: return uint16_t(STKey::y)
#define QBind(x) case Key::x: return uint16_t(STKey::x)

		QBind(k1);	QBind(k2);	QBind(k3);	QBind(k4);	QBind(k5);	QBind(k6);	QBind(k7);	QBind(k8);	QBind(k9);	QBind(k0);
		QBind(Q);	QBind(W);	QBind(E);	QBind(R);	QBind(T);	QBind(Y);	QBind(U);	QBind(I);	QBind(O);	QBind(P);
		QBind(A);	QBind(S);	QBind(D);	QBind(F);	QBind(G);	QBind(H);	QBind(J);	QBind(K);	QBind(L);
		QBind(Z);	QBind(X);	QBind(C);	QBind(V);	QBind(B);	QBind(N);	QBind(M);

		QBind(Left);	QBind(Right);	QBind(Up);	QBind(Down);

		QBind(BackTick);	QBind(Tab);
		QBind(Hyphen);	QBind(Equals);	QBind(Backspace);
		QBind(OpenSquareBracket);
		QBind(CloseSquareBracket);
		QBind(CapsLock);
		QBind(Semicolon);
		QBind(Quote);
		Bind(Enter, Return);
		QBind(LeftShift);
		QBind(RightShift);

		Bind(Comma, Comma);
		Bind(FullStop, FullStop);
		Bind(ForwardSlash, ForwardSlash);

		Bind(LeftOption, Alt);
		Bind(RightOption, Alt);
		QBind(Space);
		QBind(Backslash);

/*		Bind(KeyPadDelete, KeyPadDelete);
		Bind(KeyPadEquals, KeyPadEquals);
		Bind(KeyPadSlash, KeyPadSlash);
		Bind(KeyPadAsterisk, KeyPadAsterisk);
		Bind(KeyPadMinus, KeyPadMinus);
		Bind(KeyPadPlus, KeyPadPlus);
		Bind(KeyPadEnter, KeyPadEnter);
		Bind(KeyPadDecimalPoint, KeyPadDecimalPoint);

		Bind(KeyPad9, KeyPad9);
		Bind(KeyPad8, KeyPad8);
		Bind(KeyPad7, KeyPad7);
		Bind(KeyPad6, KeyPad6);
		Bind(KeyPad5, KeyPad5);
		Bind(KeyPad4, KeyPad4);
		Bind(KeyPad3, KeyPad3);
		Bind(KeyPad2, KeyPad2);
		Bind(KeyPad1, KeyPad1);
		Bind(KeyPad0, KeyPad0);*/

#undef QBind
#undef Bind
	}
}

// MARK: - Mouse Input

void IntelligentKeyboard::move(int x, int y) {
	mouse_movement_[0] += x;
	mouse_movement_[1] += y;
}

int IntelligentKeyboard::get_number_of_buttons() {
	return 2;
}

void IntelligentKeyboard::set_button_pressed(int index, bool is_pressed) {
	const auto mask = 1 << (index ^ 1);	// The primary button is b1; the secondary is b0.
	if(is_pressed) {
		mouse_button_state_ |= mask;
	} else {
		mouse_button_state_ &= ~mask;
	}
}

void IntelligentKeyboard::reset_all_buttons() {
	mouse_button_state_ = 0;
}

// MARK: - Joystick Output
void IntelligentKeyboard::disable_joysticks() {
	joystick_mode_ = JoystickMode::Disabled;
}

void IntelligentKeyboard::set_joystick_event_mode() {
	joystick_mode_ = JoystickMode::Event;
}

void IntelligentKeyboard::set_joystick_interrogation_mode() {
	joystick_mode_ = JoystickMode::Interrogation;
}

void IntelligentKeyboard::interrogate_joysticks() {
	output_bytes({
		0xfd,
		0x00,
		0x00
	});
}
