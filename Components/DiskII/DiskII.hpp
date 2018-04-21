//
//  DiskII.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef DiskII_hpp
#define DiskII_hpp

#include "../../ClockReceiver/ClockReceiver.hpp"
#include <cstdint>

namespace Apple {

/*!
	Provides an emulation of the Apple Disk II.
*/
class DiskII {
	public:
		enum class Control {
			P0, P1, P2, P3,
			Motor,
		};
		enum class Mode {
			Read, Write
		};
		void set_control(Control control, bool on);
		void set_mode(Mode mode);
		void select_drive(int drive);
		void set_shift_register(uint8_t value);
		uint8_t get_shift_register();

		void run_for(const Cycles cycles);
};

}

#endif /* DiskII_hpp */
