//
//  TimedEventLoop.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TimedEventLoop.hpp"
#include "../NumberTheory/Factors.hpp"

#include <algorithm>
#include <cassert>

using namespace Storage;

TimedEventLoop::TimedEventLoop(unsigned int input_clock_rate) :
	input_clock_rate_(input_clock_rate) {}

void TimedEventLoop::run_for(const Cycles cycles) {
	int remaining_cycles = cycles.as_int();
#ifndef NDEBUG
	int cycles_advanced = 0;
#endif

	while(cycles_until_event_ <= remaining_cycles) {
#ifndef NDEBUG
		cycles_advanced += cycles_until_event_;
#endif
		advance(cycles_until_event_);
		remaining_cycles -= cycles_until_event_;
		cycles_until_event_ = 0;
		process_next_event();
	}

	if(remaining_cycles) {
		cycles_until_event_ -= remaining_cycles;
#ifndef NDEBUG
		cycles_advanced += remaining_cycles;
#endif
		advance(remaining_cycles);
	}

	assert(cycles_advanced == cycles.as_int());
	assert(cycles_until_event_ > 0);
}

unsigned int TimedEventLoop::get_cycles_until_next_event() {
	return static_cast<unsigned int>(std::max(cycles_until_event_, 0));
}

unsigned int TimedEventLoop::get_input_clock_rate() {
	return input_clock_rate_;
}

void TimedEventLoop::reset_timer() {
	subcycles_until_event_.set_zero();
	cycles_until_event_ = 0;
}

void TimedEventLoop::jump_to_next_event() {
	reset_timer();
	process_next_event();
}

void TimedEventLoop::set_next_event_time_interval(Time interval) {
	// Calculate [interval]*[input clock rate] + [subcycles until this event]
	// = interval.numerator * input clock / interval.denominator + subcycles.numerator / subcycles.denominator
	// = (interval.numerator * input clock * subcycles.denominator + subcycles.numerator * interval.denominator) / (interval.denominator * subcycles.denominator)
	int64_t denominator = static_cast<int64_t>(interval.clock_rate) * static_cast<int64_t>(subcycles_until_event_.clock_rate);
	int64_t numerator =
		static_cast<int64_t>(subcycles_until_event_.clock_rate) * static_cast<int64_t>(input_clock_rate_) * static_cast<int64_t>(interval.length) +
		static_cast<int64_t>(interval.clock_rate) * static_cast<int64_t>(subcycles_until_event_.length);

	// Simplify if necessary: try just simplifying the interval and recalculating; if that doesn't
	// work then try simplifying the whole thing.
	if(numerator < 0 || denominator < 0 || denominator > std::numeric_limits<int>::max()) {
		interval.simplify();
		denominator = static_cast<int64_t>(interval.clock_rate) * static_cast<int64_t>(subcycles_until_event_.clock_rate);
		numerator =
			static_cast<int64_t>(subcycles_until_event_.clock_rate) * static_cast<int64_t>(input_clock_rate_) * static_cast<int64_t>(interval.length) +
			static_cast<int64_t>(interval.clock_rate) * static_cast<int64_t>(subcycles_until_event_.length);
	}

	if(numerator < 0 || denominator < 0 || denominator > std::numeric_limits<int>::max()) {
		int64_t common_divisor = NumberTheory::greatest_common_divisor(numerator % denominator, denominator);
		denominator /= common_divisor;
		numerator /= common_divisor;
	}

	// If even that doesn't work then reduce precision.
	if(numerator < 0 || denominator < 0 || denominator > std::numeric_limits<int>::max()) {
//		printf(".");
		const double double_interval = interval.get<double>();
		const double double_subcycles_remaining = subcycles_until_event_.get<double>();
		const double output = double_interval * static_cast<double>(input_clock_rate_) + double_subcycles_remaining;

		if(output < 1.0) {
			denominator = std::numeric_limits<int>::max();
			numerator = static_cast<int>(denominator * output);
		} else {
			numerator = std::numeric_limits<int>::max();
			denominator = static_cast<int>(numerator / output);
		}
	}

	// So this event will fire in the integral number of cycles from now, putting us at the remainder
	// number of subcycles
	const int addition = static_cast<int>(numerator / denominator);
	assert(cycles_until_event_ == 0);
	assert(addition >= 0);
	if(addition < 0) {
		assert(false);
	}
	cycles_until_event_ += addition;
	subcycles_until_event_.length = static_cast<unsigned int>(numerator % denominator);
	subcycles_until_event_.clock_rate = static_cast<unsigned int>(denominator);
	subcycles_until_event_.simplify();
}

Time TimedEventLoop::get_time_into_next_event() {
	// TODO: calculate, presumably as [length of interval] - ([cycles left] + [subcycles left])
	Time zero;
	return zero;
}
