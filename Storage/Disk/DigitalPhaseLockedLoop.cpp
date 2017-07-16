//
//  DigitalPhaseLockedLoop.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "DigitalPhaseLockedLoop.hpp"
#include <algorithm>
#include <cstdlib>

using namespace Storage;

DigitalPhaseLockedLoop::DigitalPhaseLockedLoop(int clocks_per_bit, int tolerance, size_t length_of_history) :
		clocks_per_bit_(clocks_per_bit),
		tolerance_(tolerance),
		phase_(0),
		window_length_(clocks_per_bit),
		phase_error_pointer_(0),
		phase_error_history_(length_of_history, 0),
		offset_history_(length_of_history, 0),
		offset_(0) {}

void DigitalPhaseLockedLoop::run_for_cycles(int number_of_cycles) {
	offset_ += number_of_cycles;
	phase_ += number_of_cycles;
	if(phase_ >= window_length_) {
		int windows_crossed = phase_ / window_length_;

		// check whether this triggers any 0s, if anybody cares
		if(delegate_) {
			if(window_was_filled_) windows_crossed--;
			for(int c = 0; c < windows_crossed; c++)
				delegate_->digital_phase_locked_loop_output_bit(0);
		}

		window_was_filled_ = false;
		phase_ %= window_length_;
	}
}

void DigitalPhaseLockedLoop::add_pulse() {
	if(!window_was_filled_) {
		if(delegate_) delegate_->digital_phase_locked_loop_output_bit(1);
		window_was_filled_ = true;
		post_phase_offset(phase_, offset_);
		offset_ = 0;
	}
}

void DigitalPhaseLockedLoop::post_phase_offset(int phase, int offset) {
	offset_history_[phase_error_pointer_] = offset;
	phase_error_pointer_ = (phase_error_pointer_ + 1) % offset_history_.size();

	int total_spacing = 0;
	int total_divisor = 0;
	for(int offset : offset_history_) {
		int multiple = (offset + (clocks_per_bit_ >> 1)) / clocks_per_bit_;
		if(!multiple) continue;
		total_divisor += multiple;
		total_spacing += offset;
	}
	if(total_divisor) {
		window_length_ = total_spacing / total_divisor;
		window_length_ = std::max(std::min(window_length_, clocks_per_bit_ + tolerance_), clocks_per_bit_ - tolerance_);
	}

	int error = phase - (window_length_ >> 1);

	// use a simple spring mechanism as a lowpass filter for phase
	phase_ -= (error + 1) >> 1;

	// use the average of the last few errors to affect frequency
/*	size_t phase_error_history_size = phase_error_history_.size();

	phase_error_history_[phase_error_pointer_] = error;
	phase_error_pointer_ = (phase_error_pointer_ + 1)%phase_error_history_size;

	int total_error = 0;
	for(size_t c = 0; c < phase_error_history_size; c++) {
		total_error += phase_error_history_[c];
	}
	int denominator = (int)(phase_error_history_size * 4);
	window_length_ += (total_error + (denominator >> 1)) / denominator;
	window_length_ = std::max(std::min(window_length_, clocks_per_bit_ + tolerance_), clocks_per_bit_ - tolerance_);*/
}
