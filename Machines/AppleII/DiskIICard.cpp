//
//  DiskII.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "DiskIICard.hpp"

using namespace AppleII;

DiskIICard::DiskIICard(const ROMMachine::ROMFetcher &rom_fetcher, bool is_16_sector) {
	auto roms = rom_fetcher(
		"DiskII",
		{
			"boot.rom",
			"state-machine.rom"
		});
	boot_ = std::move(*roms[0]);
	state_machine_ = std::move(*roms[1]);
}

void DiskIICard::perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
	if(isReadOperation(operation) && address < 0x100) {
		*value &= boot_[address];
	} else {
		using Control = Apple::DiskII::Control;
		using Mode = Apple::DiskII::Mode;
		switch(address & 0xf) {
			case 0x0:	diskii_.set_control(Control::P0, false);	break;
			case 0x1:	diskii_.set_control(Control::P0, true);		break;
			case 0x2:	diskii_.set_control(Control::P1, false);	break;
			case 0x3:	diskii_.set_control(Control::P1, true);		break;
			case 0x4:	diskii_.set_control(Control::P2, false);	break;
			case 0x5:	diskii_.set_control(Control::P2, true);		break;
			case 0x6:	diskii_.set_control(Control::P3, false);	break;
			case 0x7:	diskii_.set_control(Control::P3, true);		break;

			case 0x8:	diskii_.set_control(Control::Motor, false);	break;
			case 0x9:	diskii_.set_control(Control::Motor, true);	break;

			case 0xa:	diskii_.select_drive(0);					break;
			case 0xb:	diskii_.select_drive(1);					break;

			case 0xc:
				/* shift register? */
				if(isReadOperation(operation))
					*value = diskii_.get_shift_register();
			break;
			case 0xd:
				/* data register? */
				diskii_.set_shift_register(*value);
			break;

			case 0xe:	diskii_.set_mode(Mode::Read);				break;
			case 0xf:	diskii_.set_mode(Mode::Write);				break;
		}
	}
}

void DiskIICard::run_for(Cycles cycles, int stretches) {
	diskii_.run_for(cycles);
}
