//
//  68000Storage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "../68000.hpp"

#include <algorithm>

namespace CPU {
namespace MC68000 {

struct ProcessorStorageConstructor {
	ProcessorStorageConstructor(ProcessorStorage &storage) : storage_(storage) {}

	using BusStep = ProcessorStorage::BusStep;

	/*!
	*/
	int calc_action_for_mode(int mode) const {
		using Action = ProcessorBase::MicroOp::Action;
		switch(mode & 0xff) {
			default: return 0;
			case 0x12:	return int(Action::CalcD16PC);		// (d16, PC)
			case 0x13:	return int(Action::CalcD8PCXn);		// (d8, PC, Xn)
			case 0x05:	return int(Action::CalcD16An);		// (d16, An)
			case 0x06:	return int(Action::CalcD8AnXn);		// (d8, An, Xn)
		}
	}

	int combined_mode(int mode, int reg) {
		return (mode == 7) ? (0x10 | reg) : mode;
	}

	/*!
		Installs BusSteps that implement the described program into the relevant
		instance storage, returning the offset within @c all_bus_steps_ at which
		the generated steps begin.

		@param access_pattern A string describing the bus activity that occurs
			during this program. This should follow the same general pattern as
			those in yacht.txt; full description below.

		@discussion
		The access pattern is defined, as in yacht.txt, to be a string consisting
		of the following discrete bus actions. Spaces are ignored.

		* n: no operation; data bus is not used;
		* -: idle state; data bus is not used but is also not available;
		* p: program fetch; reads from the PC and adds two to it;
		* W: write MSW of something onto the bus;
		* w: write LSW of something onto the bus;
		* R: read MSW of something from the bus;
		* r: read LSW of soemthing from the bus;
		* S: push the MSW of something onto the stack;
		* s: push the LSW of something onto the stack;
		* U: pop the MSW of something from the stack;
		* u: pop the LSW of something from the stack;
		* V: fetch a vector's MSW;
		* v: fetch a vector's LSW;
		* i: acquire interrupt vector in an IACK cycle;
		* F: fetch the SSPs MSW;
		* f: fetch the SSP's LSW.

		Quite a lot of that is duplicative, implying both something about internal
		state and something about what's observable on the bus, but it's helpful to
		stick to that document's coding exactly for easier debugging.

		p fetches will fill the prefetch queue, attaching an action to both the
		step that precedes them and to themselves. The SSP fetches will go straight
		to the SSP.

		Other actions will by default act via effective_address_ and bus_data_.
		The user should fill in the steps necessary to get data into or extract
		data from those.
	*/
	size_t assemble_program(const char *access_pattern, const std::vector<uint32_t *> &addresses = {}, bool read_full_words = true) {
		auto address_iterator = addresses.begin();
		RegisterPair32 *scratch_data_read = storage_.bus_data_;
		RegisterPair32 *scratch_data_write = storage_.bus_data_;
		using Action = BusStep::Action;

		std::vector<BusStep> steps;

		// Parse the access pattern to build microcycles.
		while(*access_pattern) {
			ProcessorBase::BusStep step;

			switch(*access_pattern) {
				case '\t': case ' ': // White space acts as a no-op; it's for clarity only.
					++access_pattern;
				break;

				case 'n':	// This might be a plain NOP cycle, in which some internal calculation occurs,
							// or it might pair off with something afterwards.
					switch(access_pattern[1]) {
						default:	// This is probably a pure NOP; if what comes after this 'n' isn't actually
									// valid, it should be caught in the outer switch the next time around the loop.
							steps.push_back(step);
							++access_pattern;
						break;

						case '-':	// This is two NOPs in a row.
							steps.push_back(step);
							steps.push_back(step);
							access_pattern += 2;
						break;

						case 'F':	// Fetch SSP MSW.
						case 'f':	// Fetch SSP LSW.
							step.microcycle.length = HalfCycles(5);
							step.microcycle.operation = Microcycle::NewAddress | Microcycle::Read | Microcycle::IsProgram;	// IsProgram is a guess.
							step.microcycle.address = &storage_.effective_address_[0];
							step.microcycle.value = isupper(access_pattern[1]) ? &storage_.stack_pointers_[1].halves.high : &storage_.stack_pointers_[1].halves.low;
							steps.push_back(step);

							step.microcycle.length = HalfCycles(3);
							step.microcycle.operation = Microcycle::SelectWord | Microcycle::Read | Microcycle::IsProgram;
							step.action = Action::IncrementEffectiveAddress0;
							steps.push_back(step);

							access_pattern += 2;
						break;

						case 'V':	// Fetch exception vector low.
						case 'v':	// Fetch exception vector high.
							step.microcycle.length = HalfCycles(5);
							step.microcycle.operation = Microcycle::NewAddress | Microcycle::Read | Microcycle::IsProgram;	// IsProgram is a guess.
							step.microcycle.address = &storage_.effective_address_[0];
							step.microcycle.value = isupper(access_pattern[1]) ? &storage_.program_counter_.halves.high : &storage_.program_counter_.halves.low;
							steps.push_back(step);

							step.microcycle.length = HalfCycles(3);
							step.microcycle.operation |= Microcycle::SelectWord | Microcycle::Read | Microcycle::IsProgram;
							step.action = Action::IncrementEffectiveAddress0;
							steps.push_back(step);

							access_pattern += 2;
						break;

						case 'p':	// Fetch from the program counter into the prefetch queue.
							step.microcycle.length = HalfCycles(5);
							step.microcycle.operation = Microcycle::NewAddress | Microcycle::Read | Microcycle::IsProgram;
							step.microcycle.address = &storage_.program_counter_.full;
							step.microcycle.value = &storage_.prefetch_queue_.halves.low;
							step.action = Action::AdvancePrefetch;
							steps.push_back(step);

							step.microcycle.length = HalfCycles(3);
							step.microcycle.operation |= Microcycle::SelectWord | Microcycle::Read | Microcycle::IsProgram;
							step.action = Action::IncrementProgramCounter;
							steps.push_back(step);

							access_pattern += 2;
						break;

						case 'r':	// Fetch LSW (or only) word (/byte)
						case 'R':	// Fetch MSW word
						case 'w':	// Store LSW (or only) word (/byte)
						case 'W': {	// Store MSW word
							const bool is_read = tolower(access_pattern[1]) == 'r';
							RegisterPair32 **scratch_data = is_read ? &scratch_data_read : &scratch_data_write;

							step.microcycle.length = HalfCycles(5);
							step.microcycle.operation = Microcycle::NewAddress | (is_read ? Microcycle::Read : 0);
							step.microcycle.address = *address_iterator;
							step.microcycle.value = isupper(access_pattern[1]) ? &(*scratch_data)->halves.high : &(*scratch_data)->halves.low;
							steps.push_back(step);

							step.microcycle.length = HalfCycles(3);
							step.microcycle.operation |= (read_full_words ? Microcycle::SelectWord : Microcycle::SelectByte) | (is_read ? Microcycle::Read : 0);
							if(access_pattern[1] == 'R') {
								step.action = Action::IncrementEffectiveAddress0;
							}
							if(access_pattern[1] == 'W') {
								step.action = Action::IncrementEffectiveAddress1;
							}
							steps.push_back(step);

							if(!isupper(access_pattern[1])) {
								++(*scratch_data);
 								++address_iterator;
							}
							access_pattern += 2;
						} break;
					}
				break;

				default:
					std::cerr << "MC68000 program builder; Unknown access type " << *access_pattern << std::endl;
					assert(false);
			}
		}

		// Add a final 'ScheduleNextProgram' sentinel.
		BusStep end_program;
		end_program.action = Action::ScheduleNextProgram;
		steps.push_back(end_program);

		// If the new steps already exist, just return the existing index to them;
		// otherwise insert them.
		const auto position = std::search(storage_.all_bus_steps_.begin(), storage_.all_bus_steps_.end(), steps.begin(), steps.end());
		if(position != storage_.all_bus_steps_.end()) {
			return size_t(position - storage_.all_bus_steps_.begin());
		}

		const auto start = storage_.all_bus_steps_.size();
		std::copy(steps.begin(), steps.end(), std::back_inserter(storage_.all_bus_steps_));
		return start;
	}

	/*!
		Disassembles the instruction @c instruction and inserts it into the
		appropriate lookup tables.

		install_instruction acts, in effect, in the manner of a disassembler. So this class is
		formulated to run through all potential 65536 instuction encodings and attempt to
		disassemble each, rather than going in the opposite direction.

		This has two benefits:

			(i) which addressing modes go with which instructions falls out automatically;
			(ii) it is a lot easier during the manual verification stage of development to work
				from known instructions to their disassembly rather than vice versa; especially
			(iii) given that there are plentiful disassemblers against which to test work in progress.
	*/
	void install_instructions() {
		enum class Decoder {
			Decimal,
			RegOpModeReg,
			SizeModeRegisterImmediate,
			DataSizeModeQuick,
			RegisterModeModeRegister,	// twelve lowest bits are register, mode, mode, register, for destination and source respectively.
			ModeRegister,				// six lowest bits are mode, then register.

			MOVEtoSR,					// six lowest bits are [mode, register], decoding to MOVE SR
			CMPI,						// eight lowest bits are [size, mode, register], decoding to CMPI
			BRA,						// eight lowest bits are ignored, and an 'n np np' is scheduled
			Bcc,						// twelve lowest bits are ignored, only a PerformAction is scheduled
		};

		using Operation = ProcessorStorage::Operation;
		using Action = ProcessorStorage::MicroOp::Action;
		using MicroOp = ProcessorBase::MicroOp;
		struct PatternMapping {
			uint16_t mask, value;
			Operation operation;
			Decoder decoder;
		};

		/*
			Inspired partly by 'wrm' (https://github.com/wrm-za I assume); the following
			table draws from the M68000 Programmer's Reference Manual, currently available at
			https://www.nxp.com/files-static/archives/doc/ref_manual/M68000PRM.pdf

			After each line is the internal page number on which documentation of that
			instruction mapping can be found, followed by the page number within the PDF
			linked above.

			NB: a vector is used to allow easy iteration.
		*/
		const std::vector<PatternMapping> mappings = {
			{0xf1f0, 0x8100, Operation::SBCD, Decoder::Decimal},		// 4-171 (p275)
			{0xf1f0, 0xc100, Operation::ABCD, Decoder::Decimal},		// 4-3 (p107)

			{0xf000, 0x8000, Operation::OR, Decoder::RegOpModeReg},		// 4-150 (p226)
			{0xf000, 0x9000, Operation::SUB, Decoder::RegOpModeReg},	// 4-174 (p278)
			{0xf000, 0xb000, Operation::EOR, Decoder::RegOpModeReg},	// 4-100 (p204)
			{0xf000, 0xc000, Operation::AND, Decoder::RegOpModeReg},	// 4-15 (p119)
			{0xf000, 0xd000, Operation::ADD, Decoder::RegOpModeReg},	// 4-4 (p108)

			{0xff00, 0x0600, Operation::ADD, Decoder::SizeModeRegisterImmediate},	// 4-9 (p113)

			{0xff00, 0x0600, Operation::ADD, Decoder::DataSizeModeQuick},	// 4-11 (p115)

			{0xf000, 0x1000, Operation::MOVEb, Decoder::RegisterModeModeRegister},	// 4-116 (p220)
			{0xf000, 0x2000, Operation::MOVEl, Decoder::RegisterModeModeRegister},	// 4-116 (p220)
			{0xf000, 0x3000, Operation::MOVEw, Decoder::RegisterModeModeRegister},	// 4-116 (p220)

			{0xffc0, 0x46c0, Operation::MOVEtoSR, Decoder::MOVEtoSR},		// 6-19 (p473)

			{0xffc0, 0x0c00, Operation::CMPb, Decoder::CMPI},	// 4-79 (p183)
			{0xffc0, 0x0c40, Operation::CMPw, Decoder::CMPI},	// 4-79 (p183)
			{0xffc0, 0x0c80, Operation::CMPl, Decoder::CMPI},	// 4-79 (p183)

			{0xff00, 0x6000, Operation::BRA, Decoder::BRA},		// 4-55 (p159)
			{0xf000, 0x6000, Operation::Bcc, Decoder::Bcc},		// 4-25 (p129)
		};

		std::vector<size_t> micro_op_pointers(65536, std::numeric_limits<size_t>::max());

		// The arbitrary_base is used so that the offsets returned by assemble_program into
		// storage_.all_bus_steps_ can be retained and mapped into the final version of
		// storage_.all_bus_steps_ at the end.
		BusStep arbitrary_base;

#define op(...) 	storage_.all_micro_ops_.emplace_back(__VA_ARGS__)
#define seq(...)	&arbitrary_base + assemble_program(__VA_ARGS__)

		// Perform a linear search of the mappings above for this instruction.
		for(size_t instruction = 0; instruction < 65536; ++instruction)	{
			for(const auto &mapping: mappings) {
				if((instruction & mapping.mask) == mapping.value) {
					auto operation = mapping.operation;
					const auto micro_op_start = storage_.all_micro_ops_.size();

					// The following fields are used commonly enough to be worht pulling out here.
					const int source_register = instruction & 7;
					const int source_mode = (instruction >> 3) & 7;

					switch(mapping.decoder) {
						// This decoder actually decodes nothing; it just schedules a PerformOperation followed by an empty step.
						case Decoder::Bcc: {
							op(Action::PerformOperation);
							op();
						} break;

						// A little artificial, there's nothing really to decode for BRA.
						case Decoder::BRA: {
							op(Action::PerformOperation, seq("n np np"));
							op();
						} break;

						// Decodes the format used by ABCD and SBCD.
						case Decoder::Decimal: {
							const int destination = (instruction >> 9) & 7;
							const int source = instruction & 7;

							if(instruction & 8) {
								storage_.instructions[instruction].source = &storage_.bus_data_[0];
								storage_.instructions[instruction].destination = &storage_.bus_data_[1];

								op(	int(Action::Decrement1) | MicroOp::SourceMask | MicroOp::DestinationMask,
									seq("n nr nr np nw", { &storage_.address_[source].full, &storage_.address_[destination].full, &storage_.address_[destination].full }, false));
								op(Action::PerformOperation);
							} else {
								storage_.instructions[instruction].source = &storage_.data_[source];
								storage_.instructions[instruction].destination = &storage_.data_[destination];

								op(Action::PerformOperation, seq("np n"));
								op();
							}
						} break;

						case Decoder::CMPI: {
							if(source_mode == 1) continue;

							const auto destination_mode = source_mode;
							const auto destination_register = source_register;

							storage_.instructions[instruction].source = &storage_.prefetch_queue_;
							storage_.instructions[instruction].set_destination(storage_, destination_mode, destination_register);

							const bool is_byte_access = mapping.operation == Operation::CMPb;
							const bool is_long_word_access = mapping.operation == Operation::CMPl;
							const int mode = (is_long_word_access ? 0x100 : 0) | combined_mode(destination_mode, destination_register);
							switch(mode) {
								case 0x000:	// CMPI.bw #, Dn
									op(Action::PerformOperation, seq("np np"));
									op();
								break;

								case 0x100:	// CMPI.l #, Dn
									op(Action::None, seq("np"));
									op(Action::PerformOperation, seq("np np n"));
									op();
								break;

								case 0x002:	// CMPI.bw #, (An)
								case 0x003:	// CMPI.bw #, (An)+
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np nr np", { &storage_.address_[destination_register].full }, !is_byte_access));
									if(mode == 0x3) {
										op(int(is_byte_access ? Action::Increment1 : Action::Increment2) | MicroOp::DestinationMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x102:	// CMPI.l #, (An)
								case 0x103:	// CMPI.l #, (An)+
									op(Action::CopyDestinationToEffectiveAddress, seq("np"));
									op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::SourceMask, seq("np nR nr np", { &storage_.effective_address_[1] }));
									if(mode == 0x103) {
										op(int(Action::Increment4) | MicroOp::DestinationMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x004:	// CMPI.bw #, -(An)
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np n"));
									op(int(is_byte_access ? Action::Decrement1 : Action::Decrement1) | MicroOp::DestinationMask, seq("nr np", { &storage_.address_[destination_register].full }));
									op(Action::PerformOperation);
								break;

								case 0x104:	// CMPI.l #, -(An)
									op(int(Action::Decrement4) | MicroOp::DestinationMask, seq("np"));
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np n"));
									op(Action::CopyDestinationToEffectiveAddress, seq("nR nr np", { &storage_.effective_address_[1] }));
									op(Action::PerformOperation);
								break;

#define pseq(x) ((mode == 0x06) || (mode == 0x13) ? "n" x : x)

								case 0x012:	// CMPI.bw #, (d16, PC)
								case 0x013:	// CMPI.bw #, (d8, PC, Xn)
								case 0x005:	// CMPI.bw #, (d16, An)
								case 0x006:	// CMPI.bw #, (d8, An, Xn)
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np"));
									op(	calc_action_for_mode(destination_mode) | MicroOp::DestinationMask,
										seq(pseq("nr np"), { &storage_.effective_address_[1] }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x112:	// CMPI.l #, (d16, PC)
								case 0x113:	// CMPI.l #, (d8, PC, Xn)
								case 0x105:	// CMPI.l #, (d16, An)
								case 0x106:	// CMPI.l #, (d8, An, Xn)
									op(Action::CopyDestinationToEffectiveAddress, seq("np"));
									op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::SourceMask, seq("np"));
									op(	calc_action_for_mode(destination_mode) | MicroOp::DestinationMask,
										seq(pseq("np nR nr np"), { &storage_.effective_address_[1] }));
									op(Action::PerformOperation);
								break;

#undef pseq

								case 0x010:	// CMPI.bw #, (xxx).w
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np"));
									op(int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("np nr np",  { &storage_.effective_address_[1] }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x110:	// CMPI.l #, (xxx).w
									op(Action::None, seq("np"));
									op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::SourceMask, seq("np np"));
									op(int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("nR nr np",  { &storage_.effective_address_[1] }));
									op(Action::PerformOperation);
								break;

								case 0x011:	// CMPI.bw #, (xxx).l
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("np nr np",  { &storage_.effective_address_[1] }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x111:	// CMPI.l #, (xxx).l
									op(Action::None, seq("np"));
									op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::SourceMask, seq("np np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("np nR nr np",  { &storage_.effective_address_[1] }));
									op(Action::PerformOperation);
								break;

								default: continue;
							}
						} break;

						case Decoder::MOVEtoSR: {
							if(source_mode == 1) continue;
							storage_.instructions[instruction].set_source(storage_, source_mode, source_register);

							/* DEVIATION FROM YACHT.TXT: it has all of these reading an extra word from the PC;
							this looks like a mistake so I've padded with nil cycles in the middle. */
							const int mode = combined_mode(source_mode, source_register);
							switch(mode) {
								case 0x00:	// MOVE Dn, SR
									op(Action::PerformOperation, seq("nn np"));
									op();
								break;

								case 0x02:	// MOVE (An), SR
								case 0x03:	// MOVE (An)+, SR
									op(Action::None, seq("nr nn nn np", { &storage_.address_[source_register].full }));
									if(source_mode == 0x3) {
										op(int(Action::Increment2) | MicroOp::SourceMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x04:	// MOVE -(An), SR
									op(Action::Decrement2, seq("n nr nn nn np", { &storage_.address_[source_register].full }));
									op(Action::PerformOperation);
								break;

#define pseq(x) ((mode == 0x06) || (mode == 0x13) ? "n" x : x)

								case 0x12:	// MOVE (d16, PC), SR
								case 0x13:	// MOVE (d8, PC, Xn), SR
								case 0x05:	// MOVE (d16, An), SR
								case 0x06:	// MOVE (d8, An, Xn), SR
									op(calc_action_for_mode(source_mode) | MicroOp::SourceMask, seq(pseq("np nr nn nn np"), { &storage_.effective_address_[0] }));
									op(Action::PerformOperation);
								break;

#undef pseq

								case 0x10:	// MOVE (xxx).W, SR
									op(
										int(MicroOp::Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nr nn nn np", { &storage_.effective_address_[0] }));
									op(Action::PerformOperation);
								break;

								case 0x11:	// MOVE (xxx).L, SR
									op(Action::None, seq("np"));
									op(int(MicroOp::Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nr", { &storage_.effective_address_[0] }));
									op(Action::PerformOperation, seq("nn nn np"));
									op();
								break;

								case 0x14:	// MOVE #, SR
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op(int(Action::PerformOperation), seq("np nn nn np"));
									op();
								break;

								default: continue;
							}
						} break;

						// Decodes the format used by most MOVEs and all MOVEAs.
						case Decoder::RegisterModeModeRegister: {
							const int destination_mode = (instruction >> 6) & 7;
							const int destination_register = (instruction >> 9) & 7;

							switch(source_mode) {
								case 0:	// Dn
									storage_.instructions[instruction].source = &storage_.data_[source_register];
								break;

								case 1:	// An
									storage_.instructions[instruction].source = &storage_.address_[source_register];
								break;

								default: // (An), (An)+, -(An), (d16, An), (d8, An Xn), (xxx).W, (xxx).L
									storage_.instructions[instruction].source = &storage_.bus_data_[0];
								break;
							}

							switch(destination_mode) {
								case 0:	// Dn
									storage_.instructions[instruction].destination = &storage_.data_[destination_register];
								break;

								case 1:	// An
									storage_.instructions[instruction].destination = &storage_.address_[destination_register];
								break;

								default: // (An), (An)+, -(An), (d16, An), (d8, An Xn), (xxx).W, (xxx).L
									storage_.instructions[instruction].destination = &storage_.bus_data_[1];
								break;
							}

							const bool is_byte_access = mapping.operation == Operation::MOVEb;
							const bool is_long_word_access = mapping.operation == Operation::MOVEl;

							// There are no byte moves to address registers.
							if(is_byte_access && destination_mode == 1) {
								continue;
							}

							// Construct a single word to describe the addressing mode:
							//
							//	0xssdd, where ss or dd =
							//		0n with n a regular addresing mode between 0 and 6; or
							//		1n with n being the nominal 'register' where addressing mode is 7.
							//
							//	i.e.	(see 4-118 / p.222)
							//
							//		00 = Dn
							//		01 = An
							//		02 = (An)
							//		03 = (An)+
							//		04 = -(An)
							//		05 = (d16, An)
							//		06 = (d8, An, Xn)
							//		10 = (xxx).W
							//		11 = (xxx).L
							//		12 = (d16, PC)
							//		13 = (d8, PC, Xn)
							//		14 = #
							//
							// ... for no reason other than to make the switch below easy to read.
							const int both_modes =
								(combined_mode(source_mode, source_register) << 8) |
								combined_mode(destination_mode, destination_register) |
								(is_long_word_access ? 0x10000 : 0);

							switch(both_modes) {

							//
							// Source = Dn or An
							//

								case 0x10001:	// MOVEA.l Dn, An
								case 0x10101:	// MOVEA.l An, An
								case 0x00001:	// MOVEA.w Dn, An
								case 0x00101:	// MOVEA.w An, An
									operation = is_long_word_access ? Operation::MOVEAl : Operation::MOVEAw;	// Substitute MOVEA for MOVE.
								case 0x10000:	// MOVE.l Dn, Dn
								case 0x10100:	// MOVE.l An, Dn
								case 0x00000:	// MOVE.bw Dn, Dn
								case 0x00100:	// MOVE.bw An, Dn
									op(Action::PerformOperation, seq("np"));
									op();
								break;

								case 0x0002:	// MOVE Dn, (An)
								case 0x0102:	// MOVE An, (An)
								case 0x0003:	// MOVE Dn, (An)+
								case 0x0103:	// MOVE An, (An)+
									op(is_byte_access ? Action::SetMoveFlagsb : Action::SetMoveFlagsw, seq("nw np", { &storage_.address_[destination_register].full }, !is_byte_access));
									if(destination_mode == 0x3) {
										op(int(is_byte_access ? Action::Increment1 : Action::Increment2) | MicroOp::DestinationMask);
									} else {
										op();
									}
								break;

								case 0x0004:	// MOVE Dn, -(An)
								case 0x0104:	// MOVE An, -(An)
									op(	int(is_byte_access ? Action::Decrement1 : Action::Decrement2) | MicroOp::DestinationMask,
										seq("np nw", { &storage_.address_[destination_register].full }, !is_byte_access));
									op(is_byte_access ? Action::SetMoveFlagsb : Action::SetMoveFlagsw);
								break;

								case 0x0005:	// MOVE Dn, (d16, An)
								case 0x0105:	// MOVE An, (d16, An)
									// np nw np
								continue;

								case 0x0006:	// MOVE Dn, (d8, An, Xn)
								case 0x0106:	// MOVE An, (d8, An, Xn)
									// n np nw np
								continue;

								case 0x0010:	// MOVE Dn, (xxx).W
								case 0x0110:	// MOVE An, (xxx).W
									// np nw np
								continue;

								case 0x0011:	// MOVE Dn, (xxx).L
								case 0x0111:	// MOVE An, (xxx).L
									// np np nw np
								continue;

							//
							// Source = (An) or (An)+
							//

								case 0x00201:	// MOVEA.w (An), An
								case 0x00301:	// MOVEA.w (An)+, An
									operation = Operation::MOVEAw;
								case 0x00200:	// MOVE.bw (An), Dn
								case 0x00300:	// MOVE.bw (An)+, Dn
									op(Action::None, seq("nr np", { &storage_.address_[source_register].full }, !is_byte_access));
									if(source_mode == 0x3) {
										op(int(is_byte_access ? Action::Increment1 : Action::Increment2) | MicroOp::SourceMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x10201:	// MOVEA.l (An), An
								case 0x10301:	// MOVEA.l (An)+, An
									operation = Operation::MOVEAl;
								case 0x10200:	// MOVE.l (An), Dn
								case 0x10300:	// MOVE.l (An)+, Dn
									op(Action::CopySourceToEffectiveAddress, seq("nR nr np", { &storage_.effective_address_[0] }));
									if(source_mode == 0x3) {
										op(int(Action::Increment4) | MicroOp::SourceMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x00202:	// MOVE.bw (An), (An)
								case 0x00302:	// MOVE.bw (An)+, (An)
								case 0x00203:	// MOVE.bw (An), (An)+
								case 0x00303:	// MOVE.bw (An)+, (An)+
									op(Action::None, seq("nr", { &storage_.address_[source_register].full }));
									op(Action::PerformOperation, seq("nw np", { &storage_.address_[destination_register].full }));
									if(source_mode == 0x3 || destination_mode == 0x3) {
										op(
											int(is_byte_access ? Action::Increment1 : Action::Increment2) |
											(source_mode == 0x3 ? MicroOp::SourceMask : 0) |
											(source_mode == 0x3 ? MicroOp::DestinationMask : 0));
									} else {
										op();
									}
								continue;

								case 0x10202:	// MOVE.l (An), (An)
								case 0x10302:	// MOVE.l (An)+, (An)
								case 0x10203:	// MOVE.l (An), (An)+
								case 0x10303:	// MOVE.l (An)+, (An)+
									op(Action::CopyDestinationToEffectiveAddress);
									op(Action::CopySourceToEffectiveAddress, seq("nR nr", { &storage_.effective_address_[0] }));
									op(Action::PerformOperation, seq("nW nw np", { &storage_.effective_address_[1] }));
									if(source_mode == 0x3 || destination_mode == 0x3) {
										op(
											int(Action::Increment4) |
											(source_mode == 0x3 ? MicroOp::SourceMask : 0) |
											(source_mode == 0x3 ? MicroOp::DestinationMask : 0));
									} else {
										op();
									}
								continue;

								case 0x0204:	// MOVE (An), -(An)
								case 0x0304:	// MOVE (An)+, -(An)
									// nr np nw
								continue;

								case 0x0205:	// MOVE (An), (d16, An)
								case 0x0305:	// MOVE (An)+, (d16, An)
									// nr np nw np
								continue;

								case 0x0206:	// MOVE (An), (d8, An, Xn)
								case 0x0306:	// MOVE (An)+, (d8, An, Xn)
									// nr n np nw np
								continue;

								case 0x0210:	// MOVE (An), (xxx).W
								case 0x0310:	// MOVE (An)+, (xxx).W
									// nr np nw np
								continue;

								case 0x0211:	// MOVE (An), (xxx).L
								case 0x0311:	// MOVE (An)+, (xxx).L
									// nr np nw np np
								continue;

							//
							// Source = -(An)
							//

								case 0x0401:	// MOVEA -(An), An
									operation = Operation::MOVEAw;				// Substitute MOVEA for MOVE.
								case 0x0400:	// MOVE -(An), Dn
									op(	int(is_byte_access ? Action::Decrement1 : Action::Decrement2) | MicroOp::SourceMask,
										seq("n nr np", { &storage_.address_[source_register].full }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x0402:	// MOVE -(An), (An)
								case 0x0403:	// MOVE -(An), (An)+
									// n nr nw np
								continue;

								case 0x0404:	// MOVE -(An), -(An)
									// n nr np nw
								continue;

								case 0x0405:	// MOVE -(An), (d16, An)
									// n nr np nw
								continue;

								case 0x0406:	// MOVE -(An), (d8, An, Xn)
									// n nr n np nw np
								continue;

								case 0x0410:	// MOVE -(An), (xxx).W
									// n nr np nw np
								continue;

								case 0x0411:	// MOVE -(An), (xxx).L
									// n nr np nw np np
								continue;

							//
							// Source = (d16, An) or (d8, An, Xn)
							//

#define action_calc() int(source_mode == 0x05 ? Action::CalcD16An : Action::CalcD8AnXn)
#define pseq(x) (source_mode == 0x06 ? "n" x : x)

								case 0x0501:	// MOVE (d16, An), An
								case 0x0601:	// MOVE (d8, An, Xn), An
									operation = Operation::MOVEAw;
								case 0x0500:	// MOVE (d16, An), Dn
								case 0x0600:	// MOVE (d8, An, Xn), Dn
									op(action_calc() | MicroOp::SourceMask, seq(pseq("np nr np"), { &storage_.effective_address_[0] }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x0502:	// MOVE (d16, An), (An)
								case 0x0503:	// MOVE (d16, An), (An)+
								case 0x0602:	// MOVE (d8, An, Xn), (An)
								case 0x0603:	// MOVE (d8, An, Xn), (An)+
									// np nr nw np
									// n np nr nw np
								continue;

								case 0x0504:	// MOVE (d16, An), -(An)
								case 0x0604:	// MOVE (d8, An, Xn), -(An)
									// np nr np nw
									// n np nr np nw
								continue;

								case 0x0505:	// MOVE (d16, An), (d16, An)
								case 0x0605:	// MOVE (d8, An, Xn), (d16, An)
									// np nr np nw np
									// n np nr np nw np
								continue;

								case 0x0506:	// MOVE (d16, An), (d8, An, Xn)
								case 0x0606:	// MOVE (d8, An, Xn), (d8, An, Xn)
									// np nr n np nw np
									// n np nr n np nw np
								continue;

								case 0x0510:	// MOVE (d16, An), (xxx).W
								case 0x0610:	// MOVE (d8, An, Xn), (xxx).W
									// np nr np nw np
									// n np nr np nw np
								continue;

								case 0x0511:	// MOVE (d16, An), (xxx).L
								case 0x0611:	// MOVE (d8, An, Xn), (xxx).L
									// np nr np nw np np
									// n np nr np nw np np
								continue;

#undef action_calc
#undef prefix

							//
							// Source = (xxx).W
							//

								case 0x1001:	// MOVEA (xxx).W, An
									operation = Operation::MOVEAw;
								case 0x1000:	// MOVE (xxx).W, Dn
									op(
										int(MicroOp::Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nr np", { &storage_.effective_address_[0] }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x1002:	// MOVE (xxx).W, (An)
								case 0x1003:	// MOVE (xxx).W, (An)+
									// np nr nw np
								continue;

								case 0x1004:	// MOVE (xxx).W, -(An)
									// np nr np nw
								continue;

								case 0x1005:	// MOVE (xxx).W, (d16, An)
									// np nr np nw np
								continue;

								case 0x1006:	// MOVE (xxx).W, (d8, An, Xn)
									// np nr n np nw np
								continue;

								case 0x1010:	// MOVE (xxx).W, (xxx).W
									// np nr np nw np
								continue;

								case 0x1011:	// MOVE (xxx).W, (xxx).L
									// np nr np nw np np
								continue;

							//
							// Source = (xxx).L
							//

								case 0x1101:	// MOVEA (xxx).L, Dn
									operation = Operation::MOVEAw;
								case 0x1100:	// MOVE (xxx).L, Dn
									op(Action::None, seq("np"));
									op(int(MicroOp::Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nr", { &storage_.effective_address_[0] }, !is_byte_access));
									op(Action::PerformOperation, seq("np"));
									op();
								break;

							//
							// Source = (d16, PC)
							//

								case 0x1200:	// MOVE (d16, PC), Dn
									op(int(Action::CalcD16PC) | MicroOp::SourceMask, seq("n np nr np", { &storage_.effective_address_[0] }, !is_byte_access));
									op(Action::PerformOperation);
								break;

							//
							// Source = (d8, An, Xn)
							//

								case 0x1300:	// MOVE (d8, An, Xn), Dn
									op(int(Action::CalcD8PCXn) | MicroOp::SourceMask, seq("n np nr np", { &storage_.effective_address_[0] }, !is_byte_access));
									op(Action::PerformOperation);
								break;

							//
							// Source = #
							//

								case 0x1401:	// MOVE #, Dn
									operation = Operation::MOVEAw;
								case 0x1400:	// MOVE #, Dn
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op(int(Action::PerformOperation), seq("np np"));
									op();
								break;

							//
							// Default
							//

								default:
									std::cerr << "Unimplemented MOVE " << std::hex << both_modes << " " << instruction << std::endl;
									// TODO: all other types of mode.
								continue;
							}
						} break;

						default:
							std::cerr << "Unhandled decoder " << int(mapping.decoder) << std::endl;
						continue;
					}

					// Install the operation and make a note of where micro-ops begin.
					storage_.instructions[instruction].operation = operation;
					micro_op_pointers[instruction] = micro_op_start;

					// Don't search further through the list of possibilities.
					break;
				}
			}
		}

#undef seq
#undef op

		// Finalise micro-op and program pointers.
		for(size_t instruction = 0; instruction < 65536; ++instruction) {
			if(micro_op_pointers[instruction] != std::numeric_limits<size_t>::max()) {
				storage_.instructions[instruction].micro_operations = &storage_.all_micro_ops_[micro_op_pointers[instruction]];

				auto operation = storage_.instructions[instruction].micro_operations;
				while(!operation->is_terminal()) {
					const auto offset = size_t(operation->bus_program - &arbitrary_base);
					assert(offset >= 0 &&  offset < storage_.all_bus_steps_.size());
					operation->bus_program = &storage_.all_bus_steps_[offset];
					++operation;
				}
			}
		}
	}

	private:
		ProcessorStorage &storage_;
};

}
}

CPU::MC68000::ProcessorStorage::ProcessorStorage() {
	ProcessorStorageConstructor constructor(*this);

	// Create the special programs.
	const size_t reset_offset = constructor.assemble_program("n n n n n nn nF nf nV nv np np");
	const size_t branch_taken_offset = constructor.assemble_program("n np np");
	const size_t branch_byte_not_taken_offset = constructor.assemble_program("nn np");
	const size_t branch_word_not_taken_offset = constructor.assemble_program("nn np np");

	// Install operations.
	constructor.install_instructions();

	// Realise the special programs as direct pointers.
	reset_bus_steps_ = &all_bus_steps_[reset_offset];
	branch_taken_bus_steps_ = &all_bus_steps_[branch_taken_offset];
	branch_byte_not_taken_bus_steps_ = &all_bus_steps_[branch_byte_not_taken_offset];
	branch_word_not_taken_bus_steps_ = &all_bus_steps_[branch_word_not_taken_offset];

	// Set initial state. Largely TODO.
	active_step_ = reset_bus_steps_;
	effective_address_[0] = 0;
	is_supervisor_ = 1;
}

void CPU::MC68000::ProcessorStorage::write_back_stack_pointer() {
	stack_pointers_[is_supervisor_] = address_[7];
}

void CPU::MC68000::ProcessorStorage::set_is_supervisor(bool is_supervisor) {
	const int new_is_supervisor = is_supervisor ? 1 : 0;
	if(new_is_supervisor != is_supervisor_) {
		stack_pointers_[is_supervisor_] = address_[7];
		is_supervisor_ = new_is_supervisor;
		address_[7] = stack_pointers_[is_supervisor_];
	}
}
