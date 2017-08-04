//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "AmstradCPC.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Components/8255/i8255.hpp"
#include "../../Components/AY38910/AY38910.hpp"
#include "../../Components/6845/CRTC6845.hpp"

#include "../../Storage/Tape/Tape.hpp"

namespace AmstradCPC {

/*!
	Models the CPC's interrupt timer. Inputs are vsync, hsync, interrupt acknowledge and reset, and its output
	is simply yes or no on whether an interupt is currently requested. Internally it uses a counter with a period
	of 52 and occasionally adjusts or makes decisions based on bit 4.

	Hsync and vsync signals are expected to come directly from the CRTC; they are not decoded from a composite stream.
*/
class InterruptTimer {
	public:
		InterruptTimer() : timer_(0), interrupt_request_(false) {}

		/*!
			Indicates that a new hsync pulse has been recognised. This should be
			supplied on the falling edge of the CRTC HSYNC signal, which is the
			trailing edge because it is active high.
		*/
		inline void signal_hsync() {
			// Increment the timer and if it has hit 52 then reset it and
			// set the interrupt request line to true.
			timer_++;
			if(timer_ == 52) {
				timer_ = 0;
				interrupt_request_ = true;
			}

			// If a vertical sync has previously been indicated then after two
			// further horizontal syncs the timer should either (i) set the interrupt
			// line, if bit 4 is clear; or (ii) reset the timer.
			if(reset_counter_) {
				reset_counter_--;
				if(!reset_counter_) {
					if(timer_ & 32) {
						interrupt_request_ = true;
					}
					timer_ = 0;
				}
			}
		}

		/// Indicates the leading edge of a new vertical sync.
		inline void signal_vsync() {
			reset_counter_ = 2;
		}

		/// Indicates that an interrupt acknowledge has been received from the Z80.
		inline void signal_interrupt_acknowledge() {
			interrupt_request_ = false;
			timer_ &= ~32;
		}

		/// @returns @c true if an interrupt is currently requested; @c false otherwise.
		inline bool get_request() {
			return interrupt_request_;
		}

		/// Resets the timer.
		inline void reset_count() {
			timer_ = 0;
			interrupt_request_ = false;
		}

	private:
		int reset_counter_;
		bool interrupt_request_;
		int timer_;
};

/*!
	Provides a holder for an AY-3-8910 and its current cycles-since-updated count.
	Therefore acts both to store an AY and to bookkeep this emulator's idiomatic
	deferred clocking for this component.
*/
class AYDeferrer {
	public:
		/// Constructs a new AY instance and sets its clock rate.
		inline void setup_output() {
			ay_.reset(new GI::AY38910);
			ay_->set_input_rate(1000000);
		}

		/// Destructs the AY.
		inline void close_output() {
			ay_.reset();
		}

		/// Adds @c half_cycles half cycles to the amount of time that has passed.
		inline void run_for(HalfCycles half_cycles) {
			cycles_since_update_ += half_cycles;
		}

		/// Issues a request to the AY to perform all processing up to the current time.
		inline void flush() {
			ay_->run_for(cycles_since_update_.divide_cycles(Cycles(4)));
			ay_->flush();
		}

		/// @returns the speaker the AY is using for output.
		std::shared_ptr<Outputs::Speaker> get_speaker() {
			return ay_;
		}

		/// @returns the AY itself.
		GI::AY38910 *ay() {
			return ay_.get();
		}

	private:
		std::shared_ptr<GI::AY38910> ay_;
		HalfCycles cycles_since_update_;
};

/*!
	Provides the mechanism of receipt for the CRTC outputs. In practice has the gate array's
	video fetching and serialisation logic built in. So this is responsible for all video
	generation and therefore owns details such as the current palette.
*/
class CRTCBusHandler {
	public:
		CRTCBusHandler(uint8_t *ram, InterruptTimer &interrupt_timer) :
			cycles_(0),
			was_enabled_(false),
			was_sync_(false),
			pixel_data_(nullptr),
			pixel_pointer_(nullptr),
			was_hsync_(false),
			ram_(ram),
			interrupt_timer_(interrupt_timer),
			pixel_divider_(1),
			mode_(2),
			next_mode_(2) {
				build_mode_tables();
			}

		/*!
			The CRTC entry function; takes the current bus state and determines what output 
			to produce based on the current palette and mode.
		*/
		inline void perform_bus_cycle(const Motorola::CRTC::BusState &state) {
			// Sync is taken to override pixels, and is combined as a simple OR.
			bool is_sync = state.hsync || state.vsync;

			// If a transition between sync/border/pixels just occurred, flush whatever was
			// in progress to the CRT and reset counting.
			if(state.display_enable != was_enabled_ || is_sync != was_sync_) {
				if(was_sync_) {
					crt_->output_sync(cycles_ * 16);
				} else {
					if(was_enabled_) {
						if(cycles_) {
							crt_->output_data(cycles_ * 16, pixel_divider_);
							pixel_pointer_ = pixel_data_ = nullptr;
						}
					} else {
						output_border(cycles_);
					}
				}

				cycles_ = 0;
				was_sync_ = is_sync;
				was_enabled_ = state.display_enable;
			}

			// increment cycles since state changed
			cycles_++;

			// collect some more pixels if output is ongoing
			if(!is_sync && state.display_enable) {
				if(!pixel_data_) {
					pixel_pointer_ = pixel_data_ = crt_->allocate_write_area(320);
				}
				if(pixel_pointer_) {
					// the CPC shuffles output lines as:
					//	MA13 MA12	RA2 RA1 RA0		MA9 MA8 MA7 MA6 MA5 MA4 MA3 MA2 MA1 MA0		CCLK
					// ... so form the real access address.
					uint16_t address =
						(uint16_t)(
							((state.refresh_address & 0x3ff) << 1) |
							((state.row_address & 0x7) << 11) |
							((state.refresh_address & 0x3000) << 2)
						);

					// fetch two bytes and translate into pixels
					switch(mode_) {
						case 0:
							((uint16_t *)pixel_pointer_)[0] = mode0_output_[ram_[address]];
							((uint16_t *)pixel_pointer_)[1] = mode0_output_[ram_[address+1]];
							pixel_pointer_ += 4;
						break;

						case 1:
							((uint32_t *)pixel_pointer_)[0] = mode1_output_[ram_[address]];
							((uint32_t *)pixel_pointer_)[1] = mode1_output_[ram_[address+1]];
							pixel_pointer_ += 8;
						break;

						case 2:
							((uint64_t *)pixel_pointer_)[0] = mode2_output_[ram_[address]];
							((uint64_t *)pixel_pointer_)[1] = mode2_output_[ram_[address+1]];
							pixel_pointer_ += 16;
						break;

						case 3:
							((uint16_t *)pixel_pointer_)[0] = mode3_output_[ram_[address]];
							((uint16_t *)pixel_pointer_)[1] = mode3_output_[ram_[address+1]];
							pixel_pointer_ += 4;
						break;

					}

					// flush the current buffer pixel if full; the CRTC allows many different display
					// widths so it's not necessarily possible to predict the correct number in advance
					// and using the upper bound could lead to inefficient behaviour
					if(pixel_pointer_ == pixel_data_ + 320) {
						crt_->output_data(cycles_ * 16, pixel_divider_);
						pixel_pointer_ = pixel_data_ = nullptr;
						cycles_ = 0;
					}
				}
			}

			// check for a trailing hsync; if one occurred then that's the trigger potentially to change
			// modes, and should also be sent on to the interrupt timer
			if(was_hsync_ && !state.hsync) {
				if(mode_ != next_mode_) {
					mode_ = next_mode_;
					switch(mode_) {
						default:
						case 0:		pixel_divider_ = 4;	break;
						case 1:		pixel_divider_ = 2;	break;
						case 2:		pixel_divider_ = 1;	break;
					}
				}

				interrupt_timer_.signal_hsync();
			}

			// check for a leading vsync; that also needs to be communicated to the interrupt timer
			if(!was_vsync_ && state.vsync) {
				interrupt_timer_.signal_vsync();
			}

			// update current state for edge detection next time around
			was_vsync_ = state.vsync;
			was_hsync_ = state.hsync;
		}

		/// Constructs an appropriate CRT for video output.
		void setup_output(float aspect_ratio) {
			crt_.reset(new Outputs::CRT::CRT(1024, 16, Outputs::CRT::DisplayType::PAL50, 1));
			crt_->set_rgb_sampling_function(
				"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
				"{"
					"uint sample = texture(texID, coordinate).r;"
					"return vec3(float((sample >> 4) & 3u), float((sample >> 2) & 3u), float(sample & 3u)) / 2.0;"
				"}");
			crt_->set_visible_area(Outputs::CRT::Rect(0.05f, 0.05f, 0.9f, 0.9f));
		}

		/// Destructs the CRT.
		void close_output() {
			crt_.reset();
		}

		/// @returns the CRT.
		std::shared_ptr<Outputs::CRT::CRT> get_crt() {
			return crt_;
		}

		/*!
			Sets the next video mode. Per the documentation, mode changes take effect only at the end of line,
			not immediately. So next means "as of the end of this line".
		*/
		void set_next_mode(int mode) {
			next_mode_ = mode;
		}

		/// @returns the current value of the CRTC's vertical sync output.
		bool get_vsync() const {
			return was_vsync_;
		}

		/// Palette management: selects a pen to modify.
		void select_pen(int pen) {
			pen_ = pen;
		}

		/// Palette management: sets the colour of the selected pen.
		void set_colour(uint8_t colour) {
			if(pen_ & 16) {
				// If border is[/was] currently being output, flush what should have been
				// drawn in the old colour.
				if(!was_sync_ && !was_enabled_) {
					output_border(cycles_);
					cycles_ = 0;
				}
				border_ = mapped_palette_value(colour);
			} else {
				palette_[pen_] = mapped_palette_value(colour);
				// TODO: no need for a full regeneration, of every mode, every time
				build_mode_tables();
			}
		}

	private:
		void output_border(unsigned int length) {
			uint8_t *colour_pointer = (uint8_t *)crt_->allocate_write_area(1);
			if(colour_pointer) *colour_pointer = border_;
			crt_->output_level(length * 16);
		}

		void build_mode_tables() {
			for(int c = 0; c < 256; c++) {
				// prepare mode 0
				uint8_t *mode0_pixels = (uint8_t *)&mode0_output_[c];
				mode0_pixels[0] = palette_[((c & 0x80) >> 7) | ((c & 0x20) >> 3) | ((c & 0x08) >> 2) | ((c & 0x02) << 2)];
				mode0_pixels[1] = palette_[((c & 0x40) >> 6) | ((c & 0x10) >> 2) | ((c & 0x04) >> 1) | ((c & 0x01) << 3)];

				// prepare mode 1
				uint8_t *mode1_pixels = (uint8_t *)&mode1_output_[c];
				mode1_pixels[0] = palette_[((c & 0x80) >> 7) | ((c & 0x08) >> 2)];
				mode1_pixels[1] = palette_[((c & 0x40) >> 6) | ((c & 0x04) >> 1)];
				mode1_pixels[2] = palette_[((c & 0x20) >> 5) | ((c & 0x02) >> 0)];
				mode1_pixels[3] = palette_[((c & 0x10) >> 4) | ((c & 0x01) << 1)];

				// prepare mode 2
				uint8_t *mode2_pixels = (uint8_t *)&mode2_output_[c];
				mode2_pixels[0] = palette_[((c & 0x80) >> 7)];
				mode2_pixels[1] = palette_[((c & 0x40) >> 6)];
				mode2_pixels[2] = palette_[((c & 0x20) >> 5)];
				mode2_pixels[3] = palette_[((c & 0x10) >> 4)];
				mode2_pixels[4] = palette_[((c & 0x08) >> 3)];
				mode2_pixels[5] = palette_[((c & 0x04) >> 2)];
				mode2_pixels[6] = palette_[((c & 0x03) >> 1)];
				mode2_pixels[7] = palette_[((c & 0x01) >> 0)];

				// prepare mode 3
				uint8_t *mode3_pixels = (uint8_t *)&mode3_output_[c];
				mode3_pixels[0] = palette_[((c & 0x80) >> 7) | ((c & 0x08) >> 2)];
				mode3_pixels[1] = palette_[((c & 0x40) >> 6) | ((c & 0x04) >> 1)];
			}
		}

		uint8_t mapped_palette_value(uint8_t colour) {
#define COL(r, g, b) (r << 4) | (g << 2) | b
			static const uint8_t mapping[32] = {
				COL(1, 1, 1),	COL(1, 1, 1),	COL(0, 2, 1),	COL(2, 2, 1),
				COL(0, 0, 1),	COL(2, 0, 1),	COL(0, 1, 1),	COL(2, 1, 1),
				COL(2, 0, 1),	COL(2, 2, 1),	COL(2, 2, 0),	COL(2, 2, 2),
				COL(2, 0, 0),	COL(2, 0, 2),	COL(2, 1, 0),	COL(2, 1, 2),
				COL(0, 0, 1),	COL(0, 2, 1),	COL(0, 2, 0),	COL(0, 2, 2),
				COL(0, 0, 0),	COL(0, 0, 2),	COL(0, 1, 0),	COL(0, 1, 2),
				COL(1, 0, 1),	COL(1, 2, 1),	COL(1, 2, 0),	COL(1, 2, 2),
				COL(1, 0, 0),	COL(1, 0, 2),	COL(1, 1, 0),	COL(1, 1, 2),
			};
#undef COL
			return mapping[colour];
		}

		unsigned int cycles_;
		bool was_enabled_, was_sync_, was_hsync_, was_vsync_;
		std::shared_ptr<Outputs::CRT::CRT> crt_;
		uint8_t *pixel_data_, *pixel_pointer_;

		uint8_t *ram_;

		int next_mode_, mode_;

		unsigned int pixel_divider_;
		uint16_t mode0_output_[256];
		uint32_t mode1_output_[256];
		uint64_t mode2_output_[256];
		uint16_t mode3_output_[256];

		int pen_;
		uint8_t palette_[16];
		uint8_t border_;

		InterruptTimer &interrupt_timer_;
};

/*!
	Passively holds the current keyboard state. Keyboard state is modified in response
	to external messages, so is handled by the machine, but is read by the i8255 port
	handler, so is factored out.
*/
struct KeyboardState {
	KeyboardState() : rows{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} {}
	uint8_t rows[10];
};

/*!
	Provides the mechanism of receipt for input and output of the 8255's various ports.
*/
class i8255PortHandler : public Intel::i8255::PortHandler {
	public:
		i8255PortHandler(
			const KeyboardState &key_state,
			const CRTCBusHandler &crtc_bus_handler,
			AYDeferrer &ay,
			Storage::Tape::BinaryTapePlayer &tape_player) :
				key_state_(key_state),
				crtc_bus_handler_(crtc_bus_handler),
				ay_(ay),
				tape_player_(tape_player) {}

		/// The i8255 will call this to set a new output value of @c value for @c port.
		void set_value(int port, uint8_t value) {
			switch(port) {
				case 0:
					// Port A is connected to the AY's data bus.
					ay_.ay()->set_data_input(value);
				break;
				case 1:
					// Port B is an input only. So output goes nowehere.
				break;
				case 2: {
					// The low four bits of the value sent to Port C select a keyboard line.
					// At least for now, do a static push of the keyboard state here. So this
					// is a capture. TODO: it should be a live connection.
					int key_row = value & 15;
					if(key_row < 10) {
						ay_.ay()->set_port_input(false, key_state_.rows[key_row]);
					} else {
						ay_.ay()->set_port_input(false, 0xff);
					}

					// Bit 4 sets the tape motor on or off.
					tape_player_.set_motor_control((value & 0x10) ? true : false);
					// Bit 5 sets the current tape output level
					tape_player_.set_tape_output((value & 0x20) ? true : false);

					// Bits 6 and 7 set BDIR and BC1 for the AY.
					ay_.ay()->set_control_lines(
						(GI::AY38910::ControlLines)(
							((value & 0x80) ? GI::AY38910::BDIR : 0) |
							((value & 0x40) ? GI::AY38910::BC1 : 0) |
							GI::AY38910::BC2
						));
				} break;
			}
		}

		/// The i8255 will call this to obtain a new input for @c port.
		uint8_t get_value(int port) {
			switch(port) {
				case 0: return ay_.ay()->get_data_output();	// Port A is wired to the AY
				case 1:	return
					(crtc_bus_handler_.get_vsync() ? 0x01 : 0x00) |	// Bit 0 returns CRTC vsync.
					(tape_player_.get_input() ? 0x80 : 0x00) |		// Bit 7 returns cassette input.
					0x7e;	// Bits unimplemented:
							//
							//	Bit 6: printer ready (1 = not)
							//	Bit 5: the expansion port /EXP pin, so depends on connected hardware
							//	Bit 4: 50/60Hz switch (1 = 50Hz)
							//	Bits 1–3: distributor ID (111 = Amstrad)
				default: return 0xff;
			}
		}

	private:
		AYDeferrer &ay_;
		const KeyboardState &key_state_;
		const CRTCBusHandler &crtc_bus_handler_;
		Storage::Tape::BinaryTapePlayer &tape_player_;
};

/*!
	The actual Amstrad CPC implementation; tying the 8255, 6845 and AY to the Z80.
*/
class ConcreteMachine:
	public CPU::Z80::BusHandler,
	public Machine {
	public:
		ConcreteMachine() :
			z80_(*this),
			crtc_counter_(HalfCycles(4)),	// This starts the CRTC exactly out of phase with the memory accesses
			crtc_(crtc_bus_handler_),
			crtc_bus_handler_(ram_, interrupt_timer_),
			i8255_(i8255_port_handler_),
			i8255_port_handler_(key_state_, crtc_bus_handler_, ay_, tape_player_),
			tape_player_(8000000) {
			// primary clock is 4Mhz
			set_clock_rate(4000000);
		}

		/// The entry point for performing a partial Z80 machine cycle.
		inline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			// Amstrad CPC timing scheme: assert WAIT for three out of four cycles
			clock_offset_ = (clock_offset_ + cycle.length) & HalfCycles(7);
			z80_.set_wait_line(clock_offset_ >= HalfCycles(2));

			// Update the CRTC once every eight half cycles; aiming for half-cycle 4 as
			// per the initial seed to the crtc_counter_, but any time in the final four
			// will do as it's safe to conclude that nobody else has touched video RAM
			// during that whole window
			crtc_counter_ += cycle.length;
			Cycles crtc_cycles = crtc_counter_.divide_cycles(Cycles(4));
			if(crtc_cycles > Cycles(0)) crtc_.run_for(crtc_cycles);
			z80_.set_interrupt_line(interrupt_timer_.get_request());

			// TODO (in the player, not here): adapt it to accept an input clock rate and
			// run_for as HalfCycles
			tape_player_.run_for(cycle.length.as_int());

			// Pump the AY.
			ay_.run_for(cycle.length);

			// Stop now if no action is strictly required.
			if(!cycle.is_terminal()) return HalfCycles(0);

			uint16_t address = cycle.address ? *cycle.address : 0x0000;
			switch(cycle.operation) {
				case CPU::Z80::PartialMachineCycle::ReadOpcode:
				case CPU::Z80::PartialMachineCycle::Read:
					*cycle.value = read_pointers_[address >> 14][address & 16383];
				break;

				case CPU::Z80::PartialMachineCycle::Write:
					write_pointers_[address >> 14][address & 16383] = *cycle.value;
				break;

				case CPU::Z80::PartialMachineCycle::Output:
					// Check for a gate array access.
					if((address & 0xc000) == 0x4000) {
						switch(*cycle.value >> 6) {
							case 0: crtc_bus_handler_.select_pen(*cycle.value & 0x1f);		break;
							case 1: crtc_bus_handler_.set_colour(*cycle.value & 0x1f);		break;
							case 2:
								// Perform ROM paging.
								read_pointers_[0] = (*cycle.value & 4) ? &ram_[0] : os_.data();
								read_pointers_[3] = (*cycle.value & 8) ? &ram_[49152] : basic_.data();

								// Reset the interrupt timer if requested.
								if(*cycle.value & 0x10) interrupt_timer_.reset_count();

								// Post the next mode.
								crtc_bus_handler_.set_next_mode(*cycle.value & 3);
							break;
							case 3: printf("RAM paging?\n"); break;
						}
					}

					// Check for a CRTC access
					if(!(address & 0x4000)) {
						switch((address >> 8) & 3) {
							case 0:	crtc_.select_register(*cycle.value);	break;
							case 1:	crtc_.set_register(*cycle.value);		break;
							case 2: case 3:	printf("Illegal CRTC write?\n");	break;
						}
					}

					// Check for an 8255 PIO access
					if(!(address & 0x800)) {
						i8255_.set_register((address >> 8) & 3, *cycle.value);
					}
				break;
				case CPU::Z80::PartialMachineCycle::Input:
					// Default to nothing answering
					*cycle.value = 0xff;

					// Check for a CRTC access
					if(!(address & 0x4000)) {
						switch((address >> 8) & 3) {
							case 0:	case 1: printf("Illegal CRTC read?\n");	break;
							case 2: *cycle.value = crtc_.get_status();		break;
							case 3:	*cycle.value = crtc_.get_register();	break;
						}
					}

					// Check for a PIO access
					if(!(address & 0x800)) {
						*cycle.value = i8255_.get_register((address >> 8) & 3);
					}
				break;

				case CPU::Z80::PartialMachineCycle::Interrupt:
					// Nothing is loaded onto the bus during an interrupt acknowledge, but
					// the fact of the acknowledge needs to be posted on to the interrupt timer.
					*cycle.value = 0xff;
					interrupt_timer_.signal_interrupt_acknowledge();
				break;

				default: break;
			}

			// This implementation doesn't use time-stuffing; once in-phase waits won't be longer
			// than a single cycle so there's no real performance benefit to trying to find the
			// next non-wait when a wait cycle comes in, and there'd be no benefit to reproducing
			// the Z80's knowledge of where wait cycles occur here.
			return HalfCycles(0);
		}

		/// Another Z80 entry point; indicates that a partcular run request has concluded.
		void flush() {
			// Just flush the AY.
			ay_.flush();
		}

		/// A CRTMachine function; indicates that outputs should be created now.
		void setup_output(float aspect_ratio) {
			crtc_bus_handler_.setup_output(aspect_ratio);
			ay_.setup_output();
		}

		/// A CRTMachine function; indicates that outputs should be destroyed now.
		void close_output() {
			crtc_bus_handler_.close_output();
			ay_.close_output();
		}

		/// @returns the CRT in use.
		std::shared_ptr<Outputs::CRT::CRT> get_crt() {
			return crtc_bus_handler_.get_crt();
		}

		/// @returns the speaker in use.
		std::shared_ptr<Outputs::Speaker> get_speaker() {
			return ay_.get_speaker();
		}

		/// Wires virtual-dispatched CRTMachine run_for requests to the static Z80 method.
		void run_for(const Cycles cycles) {
			z80_.run_for(cycles);
		}

		/// The ConfigurationTarget entry point; should configure this meachine as described by @c target.
		void configure_as_target(const StaticAnalyser::Target &target) {
			// Establish reset memory map as per machine model (or, for now, as a hard-wired 464)
			read_pointers_[0] = os_.data();
			read_pointers_[1] = &ram_[16384];
			read_pointers_[2] = &ram_[32768];
			read_pointers_[3] = basic_.data();

			write_pointers_[0] = &ram_[0];
			write_pointers_[1] = &ram_[16384];
			write_pointers_[2] = &ram_[32768];
			write_pointers_[3] = &ram_[49152];

			// If there are any tapes supplied, use the first of them.
			if(!target.tapes.empty()) {
				tape_player_.set_tape(target.tapes.front());
			}
		}

		// See header; provides the system ROMs.
		void set_rom(ROMType type, std::vector<uint8_t> data) {
			// Keep only the two ROMs that are currently of interest.
			switch(type) {
				case ROMType::OS464:		os_ = data;		break;
				case ROMType::BASIC464:		basic_ = data;	break;
				default: break;
			}
		}

		// See header; sets a key as either pressed or released.
		void set_key_state(uint16_t key, bool isPressed) {
			int line = key >> 4;
			uint8_t mask = (uint8_t)(1 << (key & 7));
			if(isPressed) key_state_.rows[line] &= ~mask; else key_state_.rows[line] |= mask;
		}

		// See header; sets all keys to released.
		void clear_all_keys() {
			memset(key_state_.rows, 0xff, 10);
		}

	private:
		CPU::Z80::Processor<ConcreteMachine> z80_;

		CRTCBusHandler crtc_bus_handler_;
		Motorola::CRTC::CRTC6845<CRTCBusHandler> crtc_;

		AYDeferrer ay_;
		i8255PortHandler i8255_port_handler_;
		Intel::i8255::i8255<i8255PortHandler> i8255_;

		InterruptTimer interrupt_timer_;
		Storage::Tape::BinaryTapePlayer tape_player_;

		HalfCycles clock_offset_;
		HalfCycles crtc_counter_;
		HalfCycles half_cycles_since_ay_update_;

		uint8_t ram_[65536];
		std::vector<uint8_t> os_, basic_;

		uint8_t *read_pointers_[4];
		uint8_t *write_pointers_[4];

		KeyboardState key_state_;
};

}

using namespace AmstradCPC;

// See header; constructs and returns an instance of the Amstrad CPC.
Machine *Machine::AmstradCPC() {
	return new AmstradCPC::ConcreteMachine;
}
