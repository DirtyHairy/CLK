//
//  6850.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "6850.hpp"

using namespace Motorola::ACIA;

const HalfCycles ACIA::SameAsTransmit;

ACIA::ACIA(HalfCycles transmit_clock_rate, HalfCycles receive_clock_rate) :
	transmit_clock_rate_(transmit_clock_rate),
	receive_clock_rate_((receive_clock_rate != SameAsTransmit) ? receive_clock_rate : transmit_clock_rate) {
	transmit.set_writer_clock_rate(transmit_clock_rate);
	request_to_send.set_writer_clock_rate(transmit_clock_rate);
}

uint8_t ACIA::read(int address) {
	if(address&1) {
		clear_interrupt_cause(ReceiveNeedsRead);
		received_data_ |= NoValueMask;
	} else {
		clear_interrupt_cause(StatusNeedsRead);
		return
			((received_data_ & NoValueMask) ? 0x00 : 0x01) |
			((next_transmission_ == NoValueMask) ? 0x02 : 0x00) |
			(data_carrier_detect.read() ? 0x04 : 0x00) |
			(clear_to_send.read() ? 0x08 : 0x00) |
			(interrupt_causes_ ? 0x80 : 0x00)
		;

		// b0: receive data full.
		// b1: transmit data empty.
		// b2: DCD.
		// b3: CTS.
		// b4: framing error (i.e. no first stop bit where expected).
		// b5: receiver overran.
		// b6: parity error.
		// b7: IRQ state.
	}
	return 0x00;
}

void ACIA::write(int address, uint8_t value) {
	if(address&1) {
		next_transmission_ = value;
		consider_transmission();
		clear_interrupt_cause(TransmitNeedsWrite);
	} else {
		if((value&3) == 3) {
			transmit.reset_writing();
			transmit.write(true);
			request_to_send.reset_writing();
		} else {
			switch(value & 3) {
				default:
				case 0: divider_ = 1;	break;
				case 1: divider_ = 16;	break;
				case 2: divider_ = 64;	break;
			}
			switch((value >> 2) & 7) {
				default:
				case 0:	data_bits_ = 7; stop_bits_ = 2; parity_ = Parity::Even;	break;
				case 1:	data_bits_ = 7; stop_bits_ = 2; parity_ = Parity::Odd;	break;
				case 2:	data_bits_ = 7; stop_bits_ = 1; parity_ = Parity::Even;	break;
				case 3:	data_bits_ = 7; stop_bits_ = 1; parity_ = Parity::Odd;	break;
				case 4:	data_bits_ = 8; stop_bits_ = 2; parity_ = Parity::None;	break;
				case 5:	data_bits_ = 8; stop_bits_ = 1; parity_ = Parity::None;	break;
				case 6:	data_bits_ = 8; stop_bits_ = 1; parity_ = Parity::Even;	break;
				case 7:	data_bits_ = 8; stop_bits_ = 1; parity_ = Parity::Odd;	break;
			}
			switch((value >> 5) & 3) {
				case 0:	request_to_send.write(false); transmit_interrupt_enabled_ = false;	break;
				case 1:	request_to_send.write(false); transmit_interrupt_enabled_ = true;	break;
				case 2:	request_to_send.write(true); transmit_interrupt_enabled_ = false;	break;
				case 3:
					request_to_send.write(false);
					transmit_interrupt_enabled_ = false;
					transmit.reset_writing();
					transmit.write(false);
				break;
			}
			receive.set_read_delegate(this, Storage::Time(divider_ * 2, int(receive_clock_rate_.as_integral())));
			receive_interrupt_enabled_ = value & 0x80;
		}
	}
	update_clocking_observer();
}

void ACIA::run_for(HalfCycles length) {
	if(transmit.transmission_data_time_remaining() > HalfCycles(0)) {
		const auto write_data_time_remaining = transmit.write_data_time_remaining();

		// There's at most one further byte available to enqueue, so a single 'if'
		// rather than a 'while' is correct here. It's the responsibilit of the caller
		// to ensure run_for lengths are appropriate for longer sequences.
		if(length >= write_data_time_remaining) {
			if(next_transmission_ != NoValueMask) {
				transmit.advance_writer(write_data_time_remaining);
				consider_transmission();
				transmit.advance_writer(length - write_data_time_remaining);
			} else {
				transmit.advance_writer(length);
				update_clocking_observer();
				if(transmit_interrupt_enabled_) add_interrupt_cause(TransmitNeedsWrite);
			}
		} else {
			transmit.advance_writer(length);
		}
	}
}

void ACIA::consider_transmission() {
	if(next_transmission_ != NoValueMask && !transmit.write_data_time_remaining()) {
		// Establish start bit and [7 or 8] data bits.
		if(data_bits_ == 7) next_transmission_ &= 0x7f;
		int transmission = next_transmission_ << 1;

		// Add a parity bit, if any.
		int mask = 0x2 << data_bits_;
		if(parity_ != Parity::None) {
			transmission |= parity(uint8_t(next_transmission_)) ? mask : 0;
			mask <<= 1;
		}

		// Add stop bits.
		for(int c = 0; c < stop_bits_; ++c) {
			transmission |= mask;
			mask <<= 1;
		}

		// Output all that.
		const int total_bits = expected_bits();
		transmit.write(divider_ * 2, total_bits, transmission);

		// Mark the transmit register as empty again.
		next_transmission_ = NoValueMask;
	}
}

ClockingHint::Preference ACIA::preferred_clocking() {
	// Real-time clocking is required if a transmission is ongoing; this is a courtesy for whomever
	// is on the receiving end.
	if(transmit.transmission_data_time_remaining() > 0) return ClockingHint::Preference::RealTime;

	// If a bit reception is ongoing that might lead to an interrupt, ask for real-time clocking
	// because it's unclear when the interrupt might come.
	if(bits_incoming_ && receive_interrupt_enabled_) return ClockingHint::Preference::RealTime;

	// No clocking required then.
	return ClockingHint::Preference::None;
}

bool ACIA::get_interrupt_line() const {
	return interrupt_causes_;
}

int ACIA::expected_bits() {
	return 1 + data_bits_ + stop_bits_ + (parity_ != Parity::None);
}

uint8_t ACIA::parity(uint8_t value) {
	value ^= value >> 4;
	value ^= value >> 2;
	value ^= value >> 1;
	return value ^ (parity_ == Parity::Even);
}

bool ACIA::serial_line_did_produce_bit(Serial::Line *line, int bit) {
	// Shift this bit into the 11-bit input register; this is big enough to hold
	// the largest transmission symbol.
	++bits_received_;
	bits_incoming_ = (bits_incoming_ >> 1) | (bit << 10);

	// If that's the now-expected number of bits, update.
	const int bit_target = expected_bits();
	if(bits_received_ >= bit_target) {
		bits_received_ = 0;
		received_data_ = uint8_t(bits_incoming_ >> (12 - bit_target));
		if(receive_interrupt_enabled_) add_interrupt_cause(ReceiveNeedsRead);
		update_clocking_observer();
		return false;
	}

	// TODO: overrun, and parity.

	// Keep receiving, and consider a potential clocking change.
	if(bits_received_ == 1) update_clocking_observer();
	return true;
}

void ACIA::set_interrupt_delegate(InterruptDelegate *delegate) {
	interrupt_delegate_ = delegate;
}

void ACIA::add_interrupt_cause(int cause) {
	const bool is_changing_state = !interrupt_causes_;
	interrupt_causes_ |= cause | StatusNeedsRead;
	if(interrupt_delegate_ && is_changing_state)
		interrupt_delegate_->acia6850_did_change_interrupt_status(this);
}

void ACIA::clear_interrupt_cause(int cause) {
	const bool was_set = interrupt_causes_;
	interrupt_causes_ &= ~cause;
	if(interrupt_delegate_ && was_set && !interrupt_causes_)
		interrupt_delegate_->acia6850_did_change_interrupt_status(this);
}
