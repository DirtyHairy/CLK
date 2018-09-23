//
//  MasterSystem.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/09/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "MasterSystem.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/9918/9918.hpp"
#include "../../Components/SN76489/SN76489.hpp"

#include "../CRTMachine.hpp"
#include "../JoystickMachine.hpp"

#include "../../ClockReceiver/ForceInline.hpp"

#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#include "../../Analyser/Static/Sega/Target.hpp"

namespace {
const int sn76489_divider = 2;
}

namespace Sega {
namespace MasterSystem {

class ConcreteMachine:
	public Machine,
	public CPU::Z80::BusHandler,
	public CRTMachine::Machine {

	public:
		ConcreteMachine(const Analyser::Static::Sega::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			z80_(*this),
			sn76489_(TI::SN76489::Personality::SMS, audio_queue_, sn76489_divider),
			speaker_(sn76489_) {
			speaker_.set_input_rate(3579545.0f / static_cast<float>(sn76489_divider));
			set_clock_rate(3579545);

			for(auto &pointer: read_pointers_) {
				pointer = nullptr;
			}
			for(auto &pointer: write_pointers_) {
				pointer = nullptr;
			}

			if(target.model == Analyser::Static::Sega::Target::Model::MasterSystem) {
				const auto roms = rom_fetcher("MasterSystem", {"bios.sms"});
				if(!roms[0]) {
					throw ROMMachine::Error::MissingROMs;
				}

				roms[0]->resize(8*1024);
				memcpy(&bios_, roms[0]->data(), roms[0]->size());
				map(read_pointers_, bios_, 8*1024, 0);

				map(read_pointers_, ram_, 8*1024, 0xc000, 0x10000);
				map(write_pointers_, ram_, 8*1024, 0xc000, 0x10000);
			} else {
				map(read_pointers_, ram_, 1024, 0xc000, 0x10000);
				map(write_pointers_, ram_, 1024, 0xc000, 0x10000);
			}
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		void setup_output(float aspect_ratio) override {
			vdp_.reset(new TI::TMS::TMS9918(TI::TMS::SMSVDP));
			get_crt()->set_video_signal(Outputs::CRT::VideoSignal::Composite);
		}

		void close_output() override {
			vdp_.reset();
		}

		Outputs::CRT::CRT *get_crt() override {
			return vdp_->get_crt();
		}

		Outputs::Speaker::Speaker *get_speaker() override {
			return &speaker_;
		}

		void run_for(const Cycles cycles) override {
			z80_.run_for(cycles);
		}

		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			time_since_vdp_update_ += cycle.length;
			time_since_sn76489_update_ += cycle.length;

			if(cycle.is_terminal()) {
				uint16_t address = cycle.address ? *cycle.address : 0x0000;
				switch(cycle.operation) {
					case CPU::Z80::PartialMachineCycle::ReadOpcode:
					case CPU::Z80::PartialMachineCycle::Read:
						*cycle.value = read_pointers_[address >> 10] ? read_pointers_[address >> 10][address & 1023] : 0xff;
					break;

					case CPU::Z80::PartialMachineCycle::Write:
						if(write_pointers_[address >> 10]) write_pointers_[address >> 10][address & 1023] = *cycle.value;
					break;

					case CPU::Z80::PartialMachineCycle::Input:
						switch(address & 0xc1) {
							case 0x00:
								printf("TODO: memory control\n");
								*cycle.value = 0xff;
							break;
							case 0x01:
								printf("TODO: I/O port control\n");
								*cycle.value = 0xff;
							break;
							case 0x40: case 0x41:
								printf("TODO: get current line\n");
								*cycle.value = 0xff;
							break;
							case 0x80: case 0x81:
								update_video();
								*cycle.value = vdp_->get_register(address);
								z80_.set_interrupt_line(vdp_->get_interrupt_line());
								time_until_interrupt_ = vdp_->get_time_until_interrupt();
							break;
							case 0xc0:
								printf("TODO: I/O port A/N\n");
								*cycle.value = 0;
							break;
							case 0xc1:
								printf("TODO: I/O port B/misc\n");
								*cycle.value = 0;
							break;

							default:
								printf("Clearly some sort of typo\n");
							break;
						}
					break;

					case CPU::Z80::PartialMachineCycle::Output:
						switch(address & 0xc1) {
							case 0x00:
								printf("TODO: memory control\n");
							break;
							case 0x01:
								printf("TODO: I/O port control\n");
							break;
							case 0x40: case 0x41:
								update_audio();
								sn76489_.set_register(*cycle.value);
							break;
							case 0x80: case 0x81:
								update_video();
								vdp_->set_register(address, *cycle.value);
								z80_.set_interrupt_line(vdp_->get_interrupt_line());
								time_until_interrupt_ = vdp_->get_time_until_interrupt();
							break;
							case 0xc0:
								printf("TODO: I/O port A/N\n");
							break;
							case 0xc1:
								printf("TODO: I/O port B/misc\n");
							break;

							default:
								printf("Clearly some sort of typo\n");
							break;
						}
					break;

					case CPU::Z80::PartialMachineCycle::Interrupt:
						*cycle.value = 0xff;
					break;

					default: break;
				}
			}

			if(time_until_interrupt_ > 0) {
				time_until_interrupt_ -= cycle.length;
				if(time_until_interrupt_ <= HalfCycles(0)) {
					z80_.set_interrupt_line(true, time_until_interrupt_);
				}
			}

			return HalfCycles(0);
		}

		void flush() {
			update_video();
			update_audio();
			audio_queue_.perform();
		}

	private:
		inline void update_audio() {
			speaker_.run_for(audio_queue_, time_since_sn76489_update_.divide_cycles(Cycles(sn76489_divider)));
		}
		inline void update_video() {
			vdp_->run_for(time_since_vdp_update_.flush());
		}

		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;
		std::unique_ptr<TI::TMS::TMS9918> vdp_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		TI::SN76489 sn76489_;
		Outputs::Speaker::LowpassSpeaker<TI::SN76489> speaker_;

		HalfCycles time_since_vdp_update_;
		HalfCycles time_since_sn76489_update_;
		HalfCycles time_until_interrupt_;

		uint8_t ram_[8*1024];
		uint8_t bios_[8*1024];
		std::vector<uint8_t> cartridge_;

		// The memory map has a 1kb granularity; this is determined by the SG1000's 1kb of RAM.
		const uint8_t *read_pointers_[64];
		uint8_t *write_pointers_[64];
		template <typename T> void map(T **target, uint8_t *source, int size, int start_address, int end_address = -1) {
			if(end_address == -1) end_address = start_address + size;
			for(int address = start_address; address < end_address; address += 1024) {
				target[address >> 10] = &source[(address - start_address) & (size - 1)];
			}
		}
};

}
}

using namespace Sega::MasterSystem;

Machine *Machine::MasterSystem(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Sega::Target;
	const Target *const sega_target = dynamic_cast<const Target *>(target);
	return new ConcreteMachine(*sega_target, rom_fetcher);
}

Machine::~Machine() {}
