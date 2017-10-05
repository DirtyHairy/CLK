//
//  BestEffortUpdater.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "BestEffortUpdater.hpp"

#include <cmath>

using namespace Concurrency;

void BestEffortUpdater::update() {
	// Perform an update only if one is not currently ongoing.
	if(!update_is_ongoing_.test_and_set()) {
		async_task_queue_.enqueue([this]() {
			// Get time now using the highest-resolution clock provided by the implementation, and determine
			// the duration since the last time this section was entered.
			const std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
			const auto elapsed = now - previous_time_point_;
			previous_time_point_ = now;

			if(has_previous_time_point_) {
				// If the duration is valid, convert it to integer cycles, maintaining a rolling error and call the delegate
				// if there is one. Proceed only if the number of cycles is positive, and cap it to the per-second maximum —
				// it's possible this is an adjustable clock so be ready to swallow unexpected adjustments.
				const int64_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
				if(duration > 0) {
					double cycles = ((static_cast<double>(duration) * clock_rate_) / 1e9) + error_;
					error_ = fmod(cycles, 1.0);

					if(delegate_) {
						delegate_->update(this, static_cast<int>(std::min(cycles, clock_rate_)), has_skipped_);
					}
					has_skipped_ = false;
				}
			} else {
				has_previous_time_point_ = true;
			}

			// Allow furthers updates to occur.
			update_is_ongoing_.clear();
		});
	} else {
		async_task_queue_.enqueue([this]() {
			has_skipped_ = true;
		});
	}
}

void BestEffortUpdater::flush() {
	async_task_queue_.flush();
}

void BestEffortUpdater::set_delegate(Delegate *const delegate) {
	async_task_queue_.enqueue([this, delegate]() {
		delegate_ = delegate;
	});
}

void BestEffortUpdater::set_clock_rate(const double clock_rate) {
	async_task_queue_.enqueue([this, clock_rate]() {
		this->clock_rate_ = clock_rate;
	});
}
