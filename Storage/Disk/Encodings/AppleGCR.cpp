//
//  AppleGCR.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "AppleGCR.hpp"

using namespace Storage::Encodings;

unsigned int AppleGCR::five_and_three_encoding_for_value(int value) {
	static const unsigned int values[] = {
		0xab, 0xad, 0xae, 0xaf, 0xb5, 0xb6, 0xb7, 0xba,
		0xbb, 0xbd, 0xbe, 0xbf, 0xd6, 0xd7, 0xda, 0xdb,
		0xdd, 0xde, 0xdf, 0xea, 0xeb, 0xed, 0xee, 0xef,
		0xf5, 0xf6, 0xf7, 0xfa, 0xfb, 0xfd, 0xfe, 0xff
	};
	return values[value & 0x1f];
}

void AppleGCR::encode_five_and_three_block(uint8_t *destination, uint8_t *source) {
	destination[0] = static_cast<uint8_t>(five_and_three_encoding_for_value( source[0] >> 3 ));
	destination[1] = static_cast<uint8_t>(five_and_three_encoding_for_value( (source[0] << 2) | (source[1] >> 6) ));
	destination[2] = static_cast<uint8_t>(five_and_three_encoding_for_value( source[1] >> 1 ));
	destination[3] = static_cast<uint8_t>(five_and_three_encoding_for_value( (source[1] << 4) | (source[2] >> 4) ));
	destination[4] = static_cast<uint8_t>(five_and_three_encoding_for_value( (source[2] << 1) | (source[3] >> 7) ));
	destination[5] = static_cast<uint8_t>(five_and_three_encoding_for_value( source[3] >> 2 ));
	destination[6] = static_cast<uint8_t>(five_and_three_encoding_for_value( (source[3] << 3) | (source[4] >> 5) ));
	destination[7] = static_cast<uint8_t>(five_and_three_encoding_for_value( source[4] ));
}

unsigned int AppleGCR::six_and_two_encoding_for_value(int value) {
	static const unsigned int values[] = {
		0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
		0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
		0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
		0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
		0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
		0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
		0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
		0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
	};
	return values[value & 0x3f];
}

void AppleGCR::encode_six_and_two_block(uint8_t *destination, uint8_t *source) {
	destination[0] = static_cast<uint8_t>(six_and_two_encoding_for_value( source[0] >> 2 ));
	destination[1] = static_cast<uint8_t>(six_and_two_encoding_for_value( (source[0] << 4) | (source[1] >> 4) ));
	destination[2] = static_cast<uint8_t>(six_and_two_encoding_for_value( (source[1] << 2) | (source[2] >> 6) ));
	destination[3] = static_cast<uint8_t>(six_and_two_encoding_for_value( source[2] ));
}

/*!
	Produces a PCM segment containing @c length sync bytes, each aligned to the beginning of
	a @c bit_size -sized window.
*/
static Storage::Disk::PCMSegment sync(int length, int bit_size) {
	Storage::Disk::PCMSegment segment;

	// Allocate sufficient storage.
	segment.data.resize(static_cast<size_t>(((length * bit_size) + 7) >> 3), 0);

	while(length--) {
		segment.data[segment.number_of_bits >> 3] |= 0xff >> (segment.number_of_bits & 7);
		if(segment.number_of_bits & 7) {
			segment.data[1 + (segment.number_of_bits >> 3)] |= 0xff << (8 - (segment.number_of_bits & 7));
		}
		segment.number_of_bits += static_cast<unsigned int>(bit_size);
	}

	return segment;
}

Storage::Disk::PCMSegment AppleGCR::six_and_two_sync(int length) {
	return sync(length, 9);
}

Storage::Disk::PCMSegment AppleGCR::five_and_three_sync(int length) {
	return sync(length, 10);
}

Storage::Disk::PCMSegment AppleGCR::header(uint8_t volume, uint8_t track, uint8_t sector) {
	const uint8_t checksum = volume ^ track ^ sector;

	// Apple headers are encoded using an FM-esque scheme rather than 6 and 2, or 5 and 3.
	Storage::Disk::PCMSegment segment;
	segment.data.resize(14);
	segment.number_of_bits = 14*8;

	segment.data[0] = header_prologue[0];
	segment.data[1] = header_prologue[1];
	segment.data[2] = header_prologue[2];

#define WriteFM(index, value)	\
	segment.data[index+0] = static_cast<uint8_t>((value >> 1) | 0xaa);	\
	segment.data[index+1] = static_cast<uint8_t>(value | 0xaa);	\

	WriteFM(3, volume);
	WriteFM(5, track);
	WriteFM(7, sector);
	WriteFM(9, checksum);

#undef WriteFM

	segment.data[11] = epilogue[0];
	segment.data[12] = epilogue[1];
	segment.data[13] = epilogue[2];

	return segment;
}

Storage::Disk::PCMSegment AppleGCR::five_and_three_data(uint8_t *source) {
	Storage::Disk::PCMSegment segment;

	segment.data.resize(410 + 7);
	segment.data[0] = header_prologue[0];
	segment.data[1] = header_prologue[1];
	segment.data[2] = header_prologue[2];

	std::size_t source_pointer = 0;
	std::size_t destination_pointer = 3;
	while(source_pointer < 255) {
		encode_five_and_three_block(&segment.data[destination_pointer], &source[source_pointer]);

		source_pointer += 5;
		destination_pointer += 8;
	}

	return segment;
}

Storage::Disk::PCMSegment AppleGCR::six_and_two_data(uint8_t *source) {
	Storage::Disk::PCMSegment segment;

	segment.data.resize(342 + 7);
	segment.data[0] = header_prologue[0];
	segment.data[1] = header_prologue[1];
	segment.data[2] = header_prologue[2];

	return segment;
}
