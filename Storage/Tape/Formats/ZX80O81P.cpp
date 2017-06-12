//
//  ZX80O.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "ZX80O81P.hpp"
#include "../../Data/ZX8081.hpp"

using namespace Storage::Tape;

ZX80O81P::ZX80O81P(const char *file_name) :
	Storage::FileHolder(file_name) {

	// Grab the actual file contents
	data_.resize((size_t)file_stats_.st_size);
	fread(data_.data(), 1, (size_t)file_stats_.st_size, file_);

	// If it's a ZX81 file, prepend a file name.
	char type = (char)tolower(file_name[strlen(file_name) - 1]);
	if(type == 'p' || type == '1') {
		// TODO, maybe: prefix a proper file name; this is leaving the file nameless.
		data_.insert(data_.begin(), 0x80);
	}

	std::shared_ptr<::Storage::Data::ZX8081::File> file = Storage::Data::ZX8081::FileFromData(data_);
	if(!file) throw ErrorNotZX80O81P;

	// then rewind and start again
	virtual_reset();
}

void ZX80O81P::virtual_reset() {
	data_pointer_ = 0;
	is_past_silence_ = false;
	has_ended_final_byte_ = false;
	is_high_ = true;
	bit_pointer_ = wave_pointer_ = 0;
}

bool ZX80O81P::has_finished_data() {
	return (data_pointer_ == data_.size()) && !wave_pointer_ && !bit_pointer_;
}

bool ZX80O81P::is_at_end() {
	return has_finished_data() && has_ended_final_byte_;
}

Tape::Pulse ZX80O81P::virtual_get_next_pulse() {
	Tape::Pulse pulse;

	// Start with 1 second of silence.
	if(!is_past_silence_ || has_finished_data()) {
		pulse.type = Pulse::Type::Low;
		pulse.length.length = 5;
		pulse.length.clock_rate = 1;
		is_past_silence_ = true;
		has_ended_final_byte_ = has_finished_data();
		return pulse;
	}

	// For each byte, output 8 bits and then silence.
	if(!bit_pointer_ && !wave_pointer_) {
		byte_ = data_[data_pointer_];
		data_pointer_++;
		bit_pointer_ = 0;
		wave_pointer_ = 0;
	}

	if(!wave_pointer_) {
		// post-waves silence (here actually a pre-waves silence) is 1300µs
		pulse.length.length = 13;
		pulse.length.clock_rate = 10000;
		pulse.type = Pulse::Type::Low;

		wave_pointer_ ++;
	} else {
		// pulses are of length 150µs
		pulse.length.length = 3;
		pulse.length.clock_rate = 20000;

		if(is_high_) {
			pulse.type = Pulse::Type::High;
			is_high_ = false;
		} else {
			pulse.type = Pulse::Type::Low;
			is_high_ = true;

			// Bytes are stored MSB first.
			int wave_count = (byte_ & (0x80 >> bit_pointer_)) ? 9 : 4;
			wave_pointer_++;
			if(wave_pointer_ == wave_count + 1) {
				bit_pointer_ = (bit_pointer_ + 1)&7;
				wave_pointer_ = 0;
			}
		}
	}

	return pulse;
}
