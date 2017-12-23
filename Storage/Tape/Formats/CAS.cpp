//
//  CAS.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CAS.hpp"

#include <cassert>
#include <cstring>

using namespace Storage::Tape;

namespace  {
	const uint8_t header_signature[8] = {0x1f, 0xa6, 0xde, 0xba, 0xcc, 0x13, 0x7d, 0x74};
}

CAS::CAS(const char *file_name) {
	Storage::FileHolder file(file_name);
	uint8_t lookahead[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	// Get the first header.
	get_next(file, lookahead, 8);
	if(std::memcmp(lookahead, header_signature, sizeof(header_signature))) throw ErrorNotCAS;

	File *active_file = nullptr;
	while(!file.eof()) {
		// Just found a header, so flush the lookahead.
		get_next(file, lookahead, 8);

		// If no file is active, create one, as this must be an identification block.
		if(!active_file) {
			// Determine the new file type.
			Block type;
			switch(lookahead[0]) {
				case 0xd3:	type = Block::CSAVE;	break;
				case 0xd0:	type = Block::BSAVE;	break;
				case 0xea:	type = Block::ASCII;	break;

				// This implies something has gone wrong with parsing.
				default: throw ErrorNotCAS;
			}

			// Set the type and feed in the initial data.
			files_.emplace_back();
			active_file = &files_.back();
			active_file->type = type;
		}

		// Add a new chunk for the new incoming data.
		active_file->chunks.emplace_back();

		// Keep going until another header arrives.
		while(std::memcmp(lookahead, header_signature, sizeof(header_signature)) && !file.eof()) {
			active_file->chunks.back().push_back(lookahead[0]);
			get_next(file, lookahead, 1);
		}

		switch(active_file->type) {
			case Block::ASCII:
				// ASCII files have as many chunks as necessary, the final one being back filled
				// with 0x1a.
				if(active_file->chunks.size() >= 2) {
					std::vector<uint8_t> &last_chunk = active_file->chunks.back();
					if(last_chunk.back() == 0x1a)
						active_file = nullptr;
				}
			break;

			default:
				// CSAVE and BSAVE files have exactly two chunks, the second being the data.
				if(active_file->chunks.size() == 2)
					active_file = nullptr;
			break;
		}
	}
}

void CAS::get_next(Storage::FileHolder &file, uint8_t (&buffer)[8], std::size_t quantity) {
	assert(quantity <= 8);

	if(quantity < 8)
		std::memmove(buffer, &buffer[quantity], 8 - quantity);

	while(quantity--) {
		buffer[7 - quantity] = file.get8();
	}
}

bool CAS::is_at_end() {
	return phase_ == Phase::EndOfFile;
}

void CAS::virtual_reset() {
	phase_ = Phase::Header;
	file_pointer_ = 0;
	chunk_pointer_ = 0;
	distance_into_phase_ = 0;
	distance_into_bit_ = 0;
}

Tape::Pulse CAS::virtual_get_next_pulse() {
	Pulse pulse;
	pulse.length.clock_rate = 4800;

	// If this is a gap, then that terminates a file. If this is already the end
	// of the file then perpetual gaps await.
	if(phase_ == Phase::Gap || phase_ == Phase::EndOfFile) {
		pulse.length.length = pulse.length.clock_rate;
		pulse.type = Pulse::Type::Zero;

		if(phase_ == Phase::Gap) {
			phase_ = Phase::Header;
			file_pointer_ ++;
			chunk_pointer_ = 0;
			distance_into_phase_ = 0;
		}

		return pulse;
	}

	// Determine which bit is now forthcoming.
	int bit = 1;

	switch(phase_) {
		default: break;

		case Phase::Header: {
			distance_into_bit_++;
			if(distance_into_bit_ == 2) {
				distance_into_phase_++;
				if(distance_into_phase_ == (chunk_pointer_ ? 15360 : 3840)) {
					phase_ = Phase::Bytes;
					distance_into_phase_ = 0;
					distance_into_bit_ = 0;
				}
			}
		} break;

		case Phase::Bytes: {
			int byte_value = files_[file_pointer_].chunks[chunk_pointer_][distance_into_phase_ / 11];
			int bit_offset = distance_into_phase_ % 11;
			switch(bit_offset) {
				case 0:		bit = 0;									break;
				default:	bit = (byte_value >> (bit_offset - 1)) & 1;	break;
				case 9:
				case 10:	bit = 1;									break;
			}

			distance_into_bit_++;
			if(distance_into_bit_ == (bit ? 4 : 2)) {
				distance_into_bit_ = 0;
				distance_into_phase_++;
				if(distance_into_phase_ == files_[file_pointer_].chunks[chunk_pointer_].size() * 11) {
					distance_into_phase_ = 0;
					chunk_pointer_++;
					if(chunk_pointer_ == files_[file_pointer_].chunks.size()) {
						chunk_pointer_ = 0;
						file_pointer_++;
						phase_ = (chunk_pointer_ == files_.size()) ? Phase::EndOfFile : Phase::Gap;
					}
				}
			}
		} break;
	}

	pulse.length.length = static_cast<unsigned int>(2 - bit);
	pulse.type = (distance_into_bit_ & 1) ? Pulse::Type::High : Pulse::Type::Low;

	return pulse;
}
