//
//  DMAController.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "DMAController.hpp"

#include <cstdio>

using namespace Atari::ST;

DMAController::DMAController() {
}

uint16_t DMAController::read(int address) {
	switch(address & 7) {
		// Reserved.
		default: break;

		// Disk controller or sector count.
		case 2:
			if(control_ & 0x10) {
				return sector_count_;
			} else {
				return fdc_.get_register(control_ >> 1);
			}
		break;

		// DMA status.
		case 3:
		return status_;

		// DMA addressing.
		case 4:
		case 5:
		case 6:
		break;
	}
	return 0xffff;
}

void DMAController::write(int address, uint16_t value) {
	switch(address & 7) {
		// Reserved.
		default: break;

		// Disk controller or sector count.
		case 2:
			if(control_ & 0x10) {
				sector_count_ = value;
			} else {
				fdc_.set_register(control_ >> 1, uint8_t(value));
			}
		break;

		// DMA control; meaning is:
		//
		//	b1, b2 = address lines for FDC access.
		//	b3 = 1 => HDC access; 0 => FDC access.
		//	b4 = 1 => sector count access; 1 => FDC access.
		//	b6 = 1 => DMA off; 0 => DMA on.
		//	b7 = 1 => FDC access; 0 => HDC access.
		//	b8 = 1 => write to [F/H]DC registers; 0 => read.
		//
		//	All other bits: undefined.
		//	TODO: determine how b3 and b7 differ.
		case 3:
			control_ = value;
		break;

		// DMA addressing.
		case 4:
		case 5:
		case 6:
		break;
	}
}

void DMAController::run_for(HalfCycles duration) {
	running_time_ += duration;
	fdc_.run_for(duration.flush<Cycles>());
}
