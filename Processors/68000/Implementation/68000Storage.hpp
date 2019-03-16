//
//  68000Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef MC68000Storage_h
#define MC68000Storage_h

class ProcessorStorage {
	public:
		ProcessorStorage();

	protected:
		RegisterPair32 data_[8];
		RegisterPair32 address_[8];
		RegisterPair32 program_counter_;

		RegisterPair32 stack_pointers_[2];	// [0] = user stack pointer; [1] = supervisor; the values from here
											// are copied into/out of address_[7] upon mode switches.

		RegisterPair16 prefetch_queue_[2];
		bool dtack_ = true;

		// Various status bits.
		int is_supervisor_;
		uint_fast32_t zero_flag_;		// The zero flag is set if this value is zero.
		uint_fast32_t carry_flag_;		// The carry flag is set if this value is non-zero.
		uint_fast32_t extend_flag_;		// The extend flag is set if this value is non-zero.
		uint_fast32_t overflow_flag_;	// The overflow flag is set if this value is non-zero.
		uint_fast32_t negative_flag_;	// The negative flag is set if this value is non-zero.

		// Generic sources and targets for memory operations.
		uint32_t effective_address_;
		RegisterPair32 bus_data_[2];

		enum class Operation {
			ABCD,	SBCD,
			ADD,	AND,	EOR,	OR,		SUB,

			MOVEb,	MOVEw,	MOVEl
		};

		/*!
			Bus steps are sequences of things to communicate to the bus.
		*/
		struct BusStep {
			Microcycle microcycle;
			enum class Action {
				None,

				/// Performs effective_address_ += 2.
				IncrementEffectiveAddress,

				/// Performs program_counter_ += 2.
				IncrementProgramCounter,

				/// Copies prefetch_queue_[1] to prefetch_queue_[0].
				AdvancePrefetch,

				/*!
					Terminates an atomic program; if nothing else is pending, schedules the next instruction.
					This action is special in that it usurps any included microcycle. So any Step with this
					as its action acts as an end-of-list sentinel.
				*/
				ScheduleNextProgram

			} action = Action::None;
		};

		/*!
			A micro-op is: (i) an action to take; and (ii) a sequence of bus operations
			to perform after taking the action.

			A nullptr bus_program terminates a sequence of micro operations.
		*/
		struct MicroOp {
			enum class Action {
				None,
				PerformOperation,

				PredecrementSourceAndDestination1,
				PredecrementSourceAndDestination2,
				PredecrementSourceAndDestination4,
			} action = Action::None;
			BusStep *bus_program = nullptr;

			MicroOp() {}
			MicroOp(Action action) : action(action) {}
			MicroOp(Action action, BusStep *bus_program) : action(action), bus_program(bus_program) {}
		};

		/*!
			A program represents the implementation of a particular opcode, as a sequence
			of micro-ops and, separately, the operation to perform plus whatever other
			fields the operation requires.
		*/
		struct Program {
			MicroOp *micro_operations = nullptr;
			RegisterPair32 *source = nullptr;
			RegisterPair32 *destination = nullptr;
			Operation operation;
		};

		// Storage for all the sequences of bus steps and micro-ops used throughout
		// the 68000.
		std::vector<BusStep> all_bus_steps_;
		std::vector<MicroOp> all_micro_ops_;

		// A lookup table from instructions to implementations.
		Program instructions[65536];

		// Special programs, for exception handlers.
		BusStep *reset_program_;

		// Current bus step pointer, and outer program pointer.
		Program *active_program_ = nullptr;
		MicroOp *active_micro_op_ = nullptr;
		BusStep *active_step_ = nullptr;

	private:
		friend class ProcessorStorageConstructor;
};

#endif /* MC68000Storage_h */
