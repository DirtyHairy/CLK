//
//  G64.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "G64.hpp"

using namespace Storage;

G64::G64(const char *file_name)
{
	_file = fopen(file_name, "rb");

	if(!_file)
		throw ErrorNotGCR;

	// read and check the file signature
	char signature[8];
	if(fread(signature, 1, 8, _file) != 8)
		throw ErrorNotGCR;

	if(memcmp(signature, "GCR-1541", 8))
		throw ErrorNotGCR;

	// check the version number
	int version = fgetc(_file);
	if(version != 0)
	{
		throw ErrorUnknownVersion;
	}

	// get the number of tracks and track size
	_number_of_tracks = (uint8_t)fgetc(_file);
	_maximum_track_size = (uint16_t)fgetc(_file);
	_maximum_track_size |= (uint16_t)fgetc(_file) << 8;
}

G64::~G64()
{
	if(_file) fclose(_file);
}

unsigned int G64::get_head_position_count()
{
	// give at least 84 tracks, to yield the normal geometry but,
	// if there are more, shove them in
	return _number_of_tracks > 84 ? _number_of_tracks : 84;
}

std::shared_ptr<Track> G64::get_track_at_position(unsigned int position)
{
	std::shared_ptr<Track> resulting_track;

	// if there's definitely no track here, return the empty track
	// (TODO: should be supplying one with an index hole?)
	if(position >= _number_of_tracks) return resulting_track;

	// seek to this track's entry in the track table
	fseek(_file, (long)((position * 4) + 0xc), SEEK_SET);

	// read the track offset
	uint32_t track_offset;
	track_offset = (uint32_t)fgetc(_file);
	track_offset |= (uint32_t)fgetc(_file) << 8;
	track_offset |= (uint32_t)fgetc(_file) << 16;
	track_offset |= (uint32_t)fgetc(_file) << 24;

	// if the track offset is zero, this track doesn't exist, so...
	if(!track_offset) return resulting_track;

	// seek to the track start
	fseek(_file, (int)track_offset, SEEK_SET);

	// get the real track length
	uint16_t track_length;
	track_length = (uint16_t)fgetc(_file);
	track_length |= (uint16_t)fgetc(_file) << 8;

	// grab the byte contents of this track
	std::unique_ptr<uint8_t> track_contents(new uint8_t[track_length]);
	fread(track_contents.get(), 1, track_length, _file);

	// seek to this track's entry in the speed zone table
	fseek(_file, (long)((position * 4) + 0x15c), SEEK_SET);

	// read the speed zone offsrt
	uint32_t speed_zone_offset;
	speed_zone_offset = (uint32_t)fgetc(_file);
	speed_zone_offset |= (uint32_t)fgetc(_file) << 8;
	speed_zone_offset |= (uint32_t)fgetc(_file) << 16;
	speed_zone_offset |= (uint32_t)fgetc(_file) << 24;

	// if the speed zone is not constant, create a track based on the whole table; otherwise create one that's constant
	if(speed_zone_offset > 3)
	{
		// seek to start of speed zone
		fseek(_file, (int)speed_zone_offset, SEEK_SET);

		uint16_t speed_zone_length = (track_length + 3) >> 2;

		// read the speed zone bytes
		uint8_t speed_zone_contents[speed_zone_length];
		fread(speed_zone_contents, 1, speed_zone_length, _file);

		// TODO: divide into individual PCMSegments (per byte, if necessary), shove into a PCMTrack
	}
	else
	{
		PCMSegment segment;
		segment.duration.length = track_length * 8;
		segment.duration.clock_rate = track_length * 8;
		segment.data = std::move(track_contents);

		resulting_track.reset(new PCMTrack(std::move(segment)));
	}

	return resulting_track;
}

#pragma mark - PCMTrack

unsigned int greatest_common_divisor(unsigned int a, unsigned int b)
{
	if(a < b)
	{
		unsigned int swap = b;
		b = a;
		a = swap;
	}

	while(1) {
		if(!a) return b;
		if(!b) return a;

		unsigned int remainder = a%b;
		a = b;
		b = remainder;
	}
}

unsigned int least_common_multiple(unsigned int a, unsigned int b)
{
	unsigned int gcd = greatest_common_divisor(a, b);
	return (a*b) / gcd;
}

PCMTrack::PCMTrack(std::vector<PCMSegment> segments)
{
	_segments = std::move(segments);
	fix_length();
}

PCMTrack::PCMTrack(PCMSegment segment)
{
	_segments.push_back(std::move(segment));
	fix_length();
}

PCMTrack::Event PCMTrack::get_next_event()
{
	// find the next 1 in the input stream, keeping count of length as we go, and assuming it's going
	// to be a flux transition
	_next_event.type = Track::Event::FluxTransition;
	_next_event.length.length = 0;
	while(_segment_pointer < _segments.size())
	{
		unsigned int clock_multiplier = _track_clock_rate / _segments[_segment_pointer].duration.clock_rate;
		const uint8_t *segment_data = _segments[_segment_pointer].data.get();
		while(_bit_pointer < _segments[_segment_pointer].duration.length)
		{
			// for timing simplicity, bits are modelled as happening at the end of their window
			// TODO: should I account for the converse bit ordering? Or can I assume MSB first?
			int bit = segment_data[_bit_pointer >> 3] & (0x80 >> (_bit_pointer&7));
			_bit_pointer++;
			_next_event.length.length += clock_multiplier;

			if(bit) return _next_event;
		}
		_bit_pointer = 0;
		_segment_pointer++;
	}

	// check whether we actually reached the index hole
	if(_segment_pointer == _segments.size())
	{
		_segment_pointer = 0;
		_next_event.type = Track::Event::IndexHole;
	}

	return _next_event;
}

void PCMTrack::fix_length()
{
	// find the least common multiple of all segment clock rates
	_track_clock_rate = _segments[0].duration.clock_rate;
	for(size_t c = 1; c < _segments.size(); c++)
	{
		_track_clock_rate = least_common_multiple(_track_clock_rate, _segments[c].duration.clock_rate);
	}

	// therby determine the total length, storing it to next_event as the divisor
	_next_event.length.clock_rate = 0;
	for(size_t c = 0; c < _segments.size(); c++)
	{
		unsigned int multiplier = _track_clock_rate / _segments[c].duration.clock_rate;
		_next_event.length.clock_rate += _segments[c].duration.length * multiplier;
	}

	_segment_pointer = _bit_pointer = 0;
}
