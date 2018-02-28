//
//  SN76489.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "SN76489.hpp"

#include <cmath>

using namespace TI;

SN76489::SN76489(Concurrency::DeferringAsyncTaskQueue &task_queue) : task_queue_(task_queue) {
	// Build a volume table.
	double multiplier = pow(10.0, -0.1);
	double volume = 8191.0f;
	for(int c = 0; c < 16; ++c) {
		volumes_[c] = (int)round(volume);
		volume *= multiplier;
	}
	volumes_[15] = 0;
	evaluate_output_volume();
}

void SN76489::set_register(uint8_t value) {
	task_queue_.defer([value, this] () {
		if(value & 0x80) {
			active_register_ = value;
		}

		const int channel = (active_register_ >> 5)&3;
		if(active_register_ & 0x10) {
			// latch for volume
			channels_[channel].volume = value & 0xf;
			evaluate_output_volume();
		} else {
			// latch for tone/data
			if(channel < 3) {
				if(value & 0x80) {
					channels_[channel].divider = (channels_[channel].divider & ~0xf) | (value & 0xf);
				} else {
					channels_[channel].divider = static_cast<uint16_t>((channels_[channel].divider & 0xf) | ((value & 0x3f) << 4));
				}
			} else {
				// writes to the noise register always reset the shifter
				noise_shifter_ = shifter_is_16bit_ ? 0x8000 : 0x4000;

				if(value & 4) {
					noise_mode_ = shifter_is_16bit_ ? Noise16 : Noise15;
				} else {
					noise_mode_ = shifter_is_16bit_ ? Periodic16 : Periodic15;
				}

				channels_[3].divider = static_cast<uint16_t>(0x10 << (value & 3));
				// Special case: if these bits are both set, the noise channel should track channel 2,
				// which is marked with a divider of 0xffff.
				if(channels_[3].divider == 0x80) channels_[3].divider = 0xffff;
			}
		}
	});
}

void SN76489::evaluate_output_volume() {
	output_volume_ = static_cast<int16_t>(
		channels_[0].level * volumes_[channels_[0].volume] +
		channels_[1].level * volumes_[channels_[1].volume] +
		channels_[2].level * volumes_[channels_[2].volume] +
		channels_[3].level * volumes_[channels_[3].volume]
	);
}

void SN76489::get_samples(std::size_t number_of_samples, std::int16_t *target) {
	// For now: assume a divide by eight.

	std::size_t c = 0;
	while((master_divider_&7) && c < number_of_samples) {
		target[c] = output_volume_;
		master_divider_++;
		c++;
	}

	while(c < number_of_samples) {
		bool did_flip = false;

#define step_channel(x, s) \
		if(channels_[x].counter) channels_[x].counter--;\
		else {\
			channels_[x].level ^= 1;\
			channels_[x].counter = channels_[x].divider;\
			s;\
		}

		step_channel(0, /**/);
		step_channel(1, /**/);
		step_channel(2, did_flip = true);

#undef step_channel

		if(channels_[3].divider != 0xffff) {
			if(channels_[3].counter) channels_[3].counter--;
			else {
				did_flip = true;
				channels_[3].counter = channels_[3].divider;
			}
		}

		if(did_flip) {
			channels_[3].level = noise_shifter_ & 1;
			int new_bit = channels_[3].level;
			switch(noise_mode_) {
				default: break;
				case Noise15:
					new_bit ^= (noise_shifter_ >> 1);
				break;
				case Noise16:
					new_bit ^= (noise_shifter_ >> 3);
				break;
			}
			noise_shifter_ >>= 1;
			noise_shifter_ |= (new_bit & 1) << (shifter_is_16bit_ ? 15 : 14);
		}

		evaluate_output_volume();

		for(int ic = 0; ic < 8 && c < number_of_samples; ic++) {
			target[c] = output_volume_;
			c++;
			master_divider_++;
		}
	}

	master_divider_ &= 7;
}
