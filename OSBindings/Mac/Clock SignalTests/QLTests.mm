//
//  EmuTOSTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 10/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <array>
#include <cassert>

#include "68000.hpp"
#include "CSROMFetcher.hpp"

class QL: public CPU::MC68000::BusHandler {
	public:
		QL(const std::vector<uint8_t> &rom) : m68000_(*this) {
			assert(!(rom.size() & 1));
			rom_.resize(rom.size() / 2);

			for(size_t c = 0; c < rom_.size(); ++c) {
				rom_[c] = (rom[c << 1] << 8) | rom[(c << 1) + 1];
			}
		}

		void run_for(HalfCycles cycles) {
			m68000_.run_for(cycles);
		}

		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int is_supervisor) {
			const uint32_t address = cycle.word_address();
			uint32_t word_address = address;

			// As much about the Atari ST's memory map as is relevant here: the ROM begins
			// at 0xfc0000, and the first eight bytes are mirrored to the first four memory
			// addresses in order for /RESET to work properly. RAM otherwise fills the first
			// 512kb of the address space. Trying to write to ROM raises a bus error.

			const bool is_peripheral = word_address >= (rom_.size() + ram_.size());
			const bool is_rom = word_address < rom_.size();
			uint16_t *const base = is_rom ? rom_.data() : ram_.data();
			if(!is_rom) {
				word_address %= ram_.size();
			}

			using Microcycle = CPU::MC68000::Microcycle;
			if(cycle.data_select_active()) {
				uint16_t peripheral_result = 0xffff;
/*				if(is_peripheral) {
					switch(address & 0x7ff) {
						// A hard-coded value for TIMER B.
						case (0xa21 >> 1):
							peripheral_result = 0x00000001;
						break;
					}
					printf("Peripheral: %c %08x", (cycle.operation & Microcycle::Read) ? 'r' : 'w', *cycle.address);
					if(!(cycle.operation & Microcycle::Read)) {
						if(cycle.operation & Microcycle::SelectByte)
							printf(" %02x", cycle.value->halves.low);
						else
							printf(" %04x", cycle.value->full);
					}
					printf("\n");
				}*/

				switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
					default: break;

					case Microcycle::SelectWord | Microcycle::Read:
//						printf("[word r %08x] ", *cycle.address);
						cycle.value->full = is_peripheral ? peripheral_result : base[word_address];
					break;
					case Microcycle::SelectByte | Microcycle::Read:
//						printf("[byte r %08x] ", *cycle.address);
						cycle.value->halves.low = (is_peripheral ? peripheral_result : base[word_address]) >> cycle.byte_shift();
					break;
					case Microcycle::SelectWord:
						assert(!(is_rom && !is_peripheral));
//						printf("[word w %08x <- %04x] ", *cycle.address, cycle.value->full);
						if(!is_peripheral) base[word_address] = cycle.value->full;
					break;
					case Microcycle::SelectByte:
						assert(!(is_rom && !is_peripheral));
//						printf("[byte w %08x <- %02x] ", *cycle.address, (cycle.value->full >> cycle.byte_shift()) & 0xff);
						if(!is_peripheral) base[word_address] = (cycle.value->full & cycle.byte_mask()) | (base[word_address] & (0xffff ^ cycle.byte_mask()));
					break;
				}
			}

			return HalfCycles(0);
		}

	private:
		CPU::MC68000::Processor<QL, true> m68000_;

		std::vector<uint16_t> rom_;
		std::array<uint16_t, 64*1024> ram_;
};

@interface QLTests : XCTestCase
@end

@implementation QLTests {
	std::unique_ptr<QL> _machine;
}

- (void)setUp {
    const auto roms = CSROMFetcher()("SinclairQL", {"js.rom"});
    _machine.reset(new QL(*roms[0]));
}

- (void)testStartup {
    // This is an example of a functional test case.
    // Use XCTAssert and related functions to verify your tests produce the correct results.
    _machine->run_for(HalfCycles(40000000));
}

@end
