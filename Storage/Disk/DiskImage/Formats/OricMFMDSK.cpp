//
//  OricMFMDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "OricMFMDSK.hpp"

#include "../../Track/PCMTrack.hpp"
#include "../../Encodings/MFM/Constants.hpp"
#include "../../Encodings/MFM/Shifter.hpp"
#include "../../Encodings/MFM/Encoder.hpp"
#include "../../Track/TrackSerialiser.hpp"

using namespace Storage::Disk;

OricMFMDSK::OricMFMDSK(const char *file_name) :
		Storage::FileHolder(file_name) {
	if(!check_signature("MFM_DISK", 8))
		throw ErrorNotOricMFMDSK;

	head_count_ = fgetc32le();
	track_count_ = fgetc32le();
	geometry_type_ = fgetc32le();

	if(geometry_type_ < 1 || geometry_type_ > 2)
		throw ErrorNotOricMFMDSK;
}

unsigned int OricMFMDSK::get_head_position_count() {
	return track_count_;
}

unsigned int OricMFMDSK::get_head_count() {
	return head_count_;
}

bool OricMFMDSK::get_is_read_only() {
	return is_read_only_;
}

long OricMFMDSK::get_file_offset_for_position(unsigned int head, unsigned int position) {
	long seek_offset = 0;
	switch(geometry_type_) {
		case 1:
			seek_offset = (head * track_count_) + position;
		break;
		case 2:
			seek_offset = (position * track_count_ * head_count_) + head;
		break;
	}
	return (seek_offset * 6400) + 256;
}

std::shared_ptr<Track> OricMFMDSK::get_track_at_position(unsigned int head, unsigned int position) {
	PCMSegment segment;
	{
		std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
		fseek(file_, get_file_offset_for_position(head, position), SEEK_SET);

		// The file format omits clock bits. So it's not a genuine MFM capture.
		// A consumer must contextually guess when an FB, FC, etc is meant to be a control mark.
		size_t track_offset = 0;
		uint8_t last_header[6] = {0, 0, 0, 0, 0, 0};
		std::unique_ptr<Encodings::MFM::Encoder> encoder = Encodings::MFM::GetMFMEncoder(segment.data);
		bool did_sync = false;
		while(track_offset < 6250) {
			uint8_t next_byte = (uint8_t)fgetc(file_);
			track_offset++;

			switch(next_byte) {
				default: {
					encoder->add_byte(next_byte);
					if(did_sync) {
						switch(next_byte) {
							default: break;

							case 0xfe:
								for(int byte = 0; byte < 6; byte++) {
									last_header[byte] = (uint8_t)fgetc(file_);
									encoder->add_byte(last_header[byte]);
									track_offset++;
									if(track_offset == 6250) break;
								}
							break;

							case 0xfb:
								for(int byte = 0; byte < (128 << last_header[3]) + 2; byte++) {
									encoder->add_byte((uint8_t)fgetc(file_));
									track_offset++;
									if(track_offset == 6250) break;
								}
							break;
						}
					}

					did_sync = false;
				}
				break;

				case 0xa1:	// a synchronisation mark that implies a sector or header coming
					encoder->output_short(Storage::Encodings::MFM::MFMSync);
					did_sync = true;
				break;

				case 0xc2:	// an 'ordinary' synchronisation mark
					encoder->output_short(Storage::Encodings::MFM::MFMIndexSync);
				break;
			}
		}
	}

	segment.number_of_bits = (unsigned int)(segment.data.size() * 8);

	std::shared_ptr<PCMTrack> track(new PCMTrack(segment));
	return track;
}

void OricMFMDSK::set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track) {
	PCMSegment segment = Storage::Disk::track_serialisation(*track, Storage::Encodings::MFM::MFMBitLength);
	Storage::Encodings::MFM::Shifter shifter;
	shifter.set_is_double_density(true);
	shifter.set_should_obey_syncs(true);
	std::vector<uint8_t> parsed_track;
	int size = 0;
	int offset = 0;
	bool capture_size = false;

	for(unsigned int bit = 0; bit < segment.number_of_bits; ++bit) {
		shifter.add_input_bit(segment.bit(bit));
		if(shifter.get_token() == Storage::Encodings::MFM::Shifter::Token::None) continue;
		parsed_track.push_back(shifter.get_byte());

		if(offset) {
			offset--;
			if(!offset) {
				shifter.set_should_obey_syncs(true);
			}
			if(capture_size && offset == 2) {
				size = parsed_track.back();
				capture_size = false;
			}
		}

		if(	shifter.get_token() == Storage::Encodings::MFM::Shifter::Token::Data ||
			shifter.get_token() == Storage::Encodings::MFM::Shifter::Token::DeletedData) {
			offset = 128 << size;
			shifter.set_should_obey_syncs(false);
		}

		if(shifter.get_token() == Storage::Encodings::MFM::Shifter::Token::ID) {
			offset = 6;
			shifter.set_should_obey_syncs(false);
			capture_size = true;
		}
	}

	long file_offset = get_file_offset_for_position(head, position);

	std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
	fseek(file_, file_offset, SEEK_SET);
	size_t track_size = std::min((size_t)6400, parsed_track.size());
	fwrite(parsed_track.data(), 1, track_size, file_);
}
