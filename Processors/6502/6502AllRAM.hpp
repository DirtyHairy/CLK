//
//  6502AllRAM.hpp
//  CLK
//
//  Created by Thomas Harte on 13/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef MOS6502AllRAM_cpp
#define MOS6502AllRAM_cpp

#include "6502.hpp"

namespace CPU {
namespace MOS6502 {

class AllRAMProcessor: public Processor<AllRAMProcessor> {
	public:
		AllRAMProcessor();

		int perform_bus_operation(MOS6502::BusOperation operation, uint16_t address, uint8_t *value);

		void set_data_at_address(uint16_t startAddress, size_t length, const uint8_t *data);
		uint32_t get_timestamp();

	private:
		uint8_t memory_[65536];
		uint32_t timestamp_;
};

}
}

#endif /* MOS6502AllRAM_cpp */
