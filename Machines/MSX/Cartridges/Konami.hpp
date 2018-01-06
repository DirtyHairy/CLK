//
//  Konami.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Konami_hpp
#define Konami_hpp

#include "../ROMSlotHandler.hpp"

namespace MSX {
namespace Cartridge {

class KonamiROMSlotHandler: public ROMSlotHandler {
	public:
		KonamiROMSlotHandler(MSX::MemoryMap &map, int slot) :
			map_(map), slot_(slot) {}

		void write(uint16_t address, uint8_t value) {
			switch(address >> 13) {
				default: break;
				case 3:
					map_.map(slot_, value * 8192, 0x6000, 0x2000);
				break;
				case 4:
					map_.map(slot_, value * 8192, 0x8000, 0x2000);
				break;
				case 5:
					map_.map(slot_, value * 8192, 0xa000, 0x2000);
				break;
			}
		}

	private:
		MSX::MemoryMap &map_;
		int slot_;
};

}
}

#endif /* Konami_hpp */
