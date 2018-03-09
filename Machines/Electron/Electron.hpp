//
//  Electron.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Electron_hpp
#define Electron_hpp

#include "../../Configurable/Configurable.hpp"

#include <cstdint>
#include <vector>

namespace Electron {

enum ROMSlot: uint8_t {
	ROMSlot0 = 0,
	ROMSlot1,	ROMSlot2,	ROMSlot3,
	ROMSlot4,	ROMSlot5,	ROMSlot6,	ROMSlot7,

	ROMSlotKeyboard = 8,	ROMSlot9,
	ROMSlotBASIC = 10,		ROMSlot11,

	ROMSlot12,	ROMSlot13,	ROMSlot14,	ROMSlot15,

	ROMSlotOS,		ROMSlotDFS,
	ROMSlotADFS1,	ROMSlotADFS2
};

/// @returns The options available for an Electron.
std::vector<std::unique_ptr<Configurable::Option>> get_options();

/*!
	@abstract Represents an Acorn Electron.

	@discussion An instance of Electron::Machine represents the current state of an
	Acorn Electron.
*/
class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an Electron.
		static Machine *Electron();

		/*!
			Sets the contents of @c slot to @c data. If @c is_writeable is @c true then writing to the slot
			is enabled — it acts as if it were sideways RAM. Otherwise the slot is modelled as containing ROM.
		*/
		virtual void set_rom(ROMSlot slot, const std::vector<uint8_t> &data, bool is_writeable) = 0;
};

}

#endif /* Electron_hpp */
