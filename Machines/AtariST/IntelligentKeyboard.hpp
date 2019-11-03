//
//  IntelligentKeyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/11/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef IntelligentKeyboard_hpp
#define IntelligentKeyboard_hpp

#include "../../ClockReceiver/ClockingHintSource.hpp"
#include "../../Components/SerialPort/SerialPort.hpp"
#include "../KeyboardMachine.hpp"

#include "../../Inputs/Mouse.hpp"

#include <atomic>
#include <mutex>

namespace Atari {
namespace ST {

enum class Key: uint16_t {
	Escape = 1,
	k1, k2, k3, k4, k5, k6, k7, k8, k9, k0, Hyphen, Equals, Backspace,
	Tab, Q, W, E, R, T, Y, U, I, O, P, OpenSquareBracket, CloseSquareBracket, Return,
	Control, A, S, D, F, G, H, J, K, L, Semicolon, Quote, BackTick,
	LeftShift, Backslash, Z, X, C, V, B, N, M, Comma, FullStop, ForwardSlash, RightShift,
	/* 0x37 is unused. */
	Alt = 0x38, Space, CapsLock, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10,
	/* Various gaps follow. */
	Home = 0x47, Up,
	KeypadMinus = 0x4a, Left,
	Right = 0x4d, KeypadPlus,
	Down = 0x50,
	Insert = 0x52, Delete,
	ISO = 0x60, Undo, Help, KeypadOpenBracket, KeypadCloseBracket, KeypadDivide, KeypadMultiply,
	Keypad7, Keypad8, Keypad9, Keypad4, KeyPad5, Keypad6, Keypad1, Keypad2, Keypad3, Keypad0, KeypadDecimalPoint,
	KeypadEnter
};
static_assert(uint16_t(Key::RightShift) == 0x36, "RightShift should have key code 0x36; check intermediate entries");
static_assert(uint16_t(Key::F10) == 0x44, "F10 should have key code 0x44; check intermediate entries");
static_assert(uint16_t(Key::KeypadEnter) == 0x72, "KeypadEnter should have key code 0x72; check intermediate entries");

/*!
	A receiver for the Atari ST's "intelligent keyboard" commands, which actually cover
	keyboard input and output and mouse handling.
*/
class IntelligentKeyboard:
	public Serial::Line::ReadDelegate,
	public ClockingHint::Source,
	public Inputs::Mouse {
	public:
		IntelligentKeyboard(Serial::Line &input, Serial::Line &output);
		ClockingHint::Preference preferred_clocking() final;
		void run_for(HalfCycles duration);

		void set_key_state(Key key, bool is_pressed);
		class KeyboardMapper: public KeyboardMachine::MappedMachine::KeyboardMapper {
			uint16_t mapped_key_for_key(Inputs::Keyboard::Key key) final;
		};

	private:
		// MARK: - Key queue.
		std::mutex key_queue_mutex_;
		std::vector<uint8_t> key_queue_;

		// MARK: - Serial line state.
		int bit_count_ = 0;
		int command_ = 0;
		Serial::Line &output_line_;

		void output_bytes(std::initializer_list<uint8_t> value);
		bool serial_line_did_produce_bit(Serial::Line *, int bit) final;

		// MARK: - Command dispatch.
		std::vector<uint8_t> command_sequence_;
		void dispatch_command(uint8_t command);

		// MARK: - Flow control.
		void reset();
		void resume();
		void pause();

		// MARK: - Mouse.
		void disable_mouse();
		void set_relative_mouse_position_reporting();
		void set_absolute_mouse_position_reporting(uint16_t max_x, uint16_t max_y);
		void set_mouse_position(uint16_t x, uint16_t y);
		void set_mouse_keycode_reporting(uint8_t delta_x, uint8_t delta_y);
		void set_mouse_threshold(uint8_t x, uint8_t y);
		void set_mouse_scale(uint8_t x, uint8_t y);
		void set_mouse_y_downward();
		void set_mouse_y_upward();
		void set_mouse_button_actions(uint8_t actions);
		void interrogate_mouse_position();

		// Inputs::Mouse.
		void move(int x, int y) final;
		int get_number_of_buttons() final;
		void set_button_pressed(int index, bool is_pressed) final;
		void reset_all_buttons() final;

		enum class MouseMode {
			Relative, Absolute
		} mouse_mode_ = MouseMode::Relative;

		// Absolute positioning state.
		int mouse_range_[2] = {0, 0};
		int mouse_scale_[2] = {0, 0};

		// Relative positioning state.
		int posted_button_state_ = 0;
		int mouse_threshold_[2] = {1, 1};
		void post_relative_mouse_event(int x, int y);

		// Received mouse state.
		std::atomic<int> mouse_movement_[2];
		std::atomic<int> mouse_button_state_;

		// MARK: - Joystick.
		void disable_joysticks();
};

}
}

#endif /* IntelligentKeyboard_hpp */
