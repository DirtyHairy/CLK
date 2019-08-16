//
//  ncr5380.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "ncr5380.hpp"

#include "../../Outputs/Log.hpp"

using namespace NCR::NCR5380;

NCR5380::NCR5380(int clock_rate) : clock_rate_(clock_rate) {
	device_id_ = bus_.add_device();
}

void NCR5380::write(int address, uint8_t value) {
	using Line = SCSI::Line;
	switch(address & 7) {
		case 0:
			LOG("[SCSI 0] Set current SCSI bus state to " << PADHEX(2) << int(value));
			data_bus_ = value;
		break;

		case 1: {
			LOG("[SCSI 1] Initiator command register set: " << PADHEX(2) << int(value));
			initiator_command_ = value;

			SCSI::BusState mask = SCSI::DefaultBusState;
			if(value & 0x80) mask |= Line::Reset;
			test_mode_ = value & 0x40;
			/* bit 5 = differential enable if this were a 5381 */
			if(value & 0x10) mask |= Line::Acknowledge;
			if(value & 0x08) mask |= Line::Busy;
			if(value & 0x04) mask |= Line::SelectTarget;
			if(value & 0x02) mask |= Line::Attention;
			assert_data_bus_ = value & 0x01;
			bus_output_ = (bus_output_ & ~(Line::Reset | Line::Acknowledge | Line::Busy | Line::SelectTarget | Line::Attention)) | mask;
		} break;

		case 2:
			LOG("[SCSI 2] Set mode: " << PADHEX(2) << int(value));
			mode_ = value;

			// bit 7: 1 = use block mode DMA mode (if DMA mode is also enabled)
			// bit 6: 1 = be a SCSI target; 0 = be an initiator
			// bit 5: 1 = check parity
			// bit 4: 1 = generate an interrupt if parity checking is enabled and an error is found
			// bit 3: 1 = generate an interrupt when an EOP is received from the DMA controller
			// bit 2: 1 = generate an interrupt and reset low 6 bits of register 1 if an unexpected loss of Line::Busy occurs
			// bit 1: 1 = use DMA mode
			// bit 0: 1 = begin arbitration mode (device ID should be in register 0)

			/*
				Arbitration is accomplished using a bus-free filter to continuously monitor BSY.
				If BSY remains inactive for at least 400 nsec then the SCSI bus is considered free
				and arbitration may begin. Arbitration will begin if the bus is free, SEL is inactive
				and the ARBITRATION bit (port 2, bit 0) is active. Once arbitration has begun
				(BSY asserted), an arbitration delay of 2.2 /Lsec must elapse before the data bus
				can be examined to deter- mine if arbitration has been won. This delay must be
				implemented in the controlling software driver.
			*/

			if(mode_ & 1) {
				if(state_ == ExecutionState::None) {
					set_execution_state(ExecutionState::WatchingBusy);
					arbitration_in_progress_ = false;
					lost_arbitration_ = false;
				}
			} else {
				set_execution_state(ExecutionState::None);
			}
		break;

		case 3:
			LOG("[SCSI 3] Set target command: " << PADHEX(2) << int(value));
		break;

		case 4:
			LOG("[SCSI 4] Set select enabled: " << PADHEX(2) << int(value));
		break;

		case 5:
			LOG("[SCSI 5] Start DMA send: " << PADHEX(2) << int(value));
		break;

		case 6:
			LOG("[SCSI 6] Start DMA target receive: " << PADHEX(2) << int(value));
		break;

		case 7:
			LOG("[SCSI 7] Start DMA initiator receive: " << PADHEX(2) << int(value));
		break;
	}

	// Data is output only if the data bus is asserted.
	if(assert_data_bus_) {
		bus_output_ &= data_bus_;
	} else {
		bus_output_ |= SCSI::Line::Data;
	}

	// In test mode, still nothing is output. Otherwise throw out
	// the current value of bus_output_.
	if(test_mode_) {
		bus_.set_device_output(device_id_, SCSI::DefaultBusState);
	} else {
		bus_.set_device_output(device_id_, bus_output_);
	}
}

uint8_t NCR5380::read(int address) {
	switch(address & 7) {
		case 0:
			LOG("[SCSI 0] Get current SCSI bus state");
		return uint8_t(bus_.get_state());

		case 1:
			LOG("[SCSI 1] Initiator command register get");
		return
			// Bits repeated as they were set.
			(initiator_command_ & ~0x60) |

			// Arbitration in progress.
			(arbitration_in_progress_ ? 0x40 : 0x00) |

			// Lost arbitration.
			(lost_arbitration_ ? 0x20 : 0x00);

		case 2:
			LOG("[SCSI 2] Get mode");
		return mode_;

		case 3:
			LOG("[SCSI 3] Get target command");
		return 0xff;

		case 4: {
			LOG("[SCSI 4] Get current bus state");
			const auto bus_state = bus_.get_state();
			return
				((bus_state & SCSI::Line::Reset)			? 0x80 : 0x00) |
				((bus_state & SCSI::Line::Busy)				? 0x40 : 0x00) |
				((bus_state & SCSI::Line::Request)			? 0x20 : 0x00) |
				((bus_state & SCSI::Line::Message)			? 0x10 : 0x00) |
				((bus_state & SCSI::Line::Control)			? 0x08 : 0x00) |
				((bus_state & SCSI::Line::Input)			? 0x04 : 0x00) |
				((bus_state & SCSI::Line::SelectTarget)		? 0x02 : 0x00) |
				((bus_state & SCSI::Line::Parity)			? 0x01 : 0x00);
		}

		case 5: {
			LOG("[SCSI 5] Get bus and status");
			const auto bus_state = bus_.get_state();
			return
				((bus_state & SCSI::Line::Attention) ? 0x02 : 0x00) |
				((bus_state & SCSI::Line::Acknowledge) ? 0x01 : 0x00);
		}

		case 6:
			LOG("[SCSI 6] Get input data");
		return 0xff;

		case 7:
			LOG("[SCSI 7] Reset parity/interrupt");
		return 0xff;
	}
	return 0;
}

void NCR5380::run_for(Cycles cycles) {
	if(state_ == ExecutionState::None) return;

	++time_in_state_;
	switch(state_) {
		default: break;

		case ExecutionState::WatchingBusy:
			/*
				Arbitration is accomplished using a bus-free filter to continuously monitor BSY.
				If BSY remains inactive for at least 400 nsec then the SCSI bus is considered free
				and arbitration may begin. Arbitration will begin if the bus is free, SEL is inactive
				and the ARBITRATION bit (port 2, bit 0) is active. Once arbitration has begun
				(BSY asserted)...
			*/
			if(bus_.get_state() & SCSI::Line::Busy) {
				lost_arbitration_ = true;
				set_execution_state(ExecutionState::None);
			}

			// Check for having hit the 400ns state.
			if(time_in_state_ == 400 * clock_rate_ / 1000000000) {
				arbitration_in_progress_ = false;
				if(bus_.get_state() & SCSI::Line::SelectTarget) {
					lost_arbitration_ = true;
					set_execution_state(ExecutionState::None);
				} else {
					bus_output_ |= SCSI::Line::Busy;
					set_execution_state(ExecutionState::None);
				}
			}
		break;
	}
}

void NCR5380::set_execution_state(ExecutionState state) {
	time_in_state_ = 0;
	update_clocking_observer();
}

ClockingHint::Preference NCR5380::preferred_clocking() {
	// Request real-time clocking if any sort of timed bus watching is ongoing,
	// given that there's no knowledge in here as to what clocking other devices
	// on the SCSI bus might be enjoying.
	return (state_ == ExecutionState::None) ? ClockingHint::Preference::RealTime : ClockingHint::Preference::None;
}
