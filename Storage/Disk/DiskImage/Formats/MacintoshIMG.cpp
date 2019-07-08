//
//  DiskCopy42.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "MacintoshIMG.hpp"

#include <cstring>

#include "../../Track/PCMTrack.hpp"
#include "../../Encodings/AppleGCR/Encoder.hpp"

/*
	File format specifications as referenced below are largely
	sourced from the documentation at
	https://wiki.68kmla.org/DiskCopy_4.2_format_specification
*/

using namespace Storage::Disk;

MacintoshIMG::MacintoshIMG(const std::string &file_name) :
	file_(file_name) {

	// Test 1: is this a raw secctor dump? If so it'll start with
	// the magic word 0x4C4B6000 (big endian) and be exactly
	// 819,200 bytes long if double sided, or 409,600 bytes if
	// single sided.
	//
	// Luckily, 0x4c is an invalid string length for the proper
	// DiskCopy 4.2 format, so there's no ambiguity here.
	const auto name_length = file_.get8();
	if(name_length == 0x4c) {
		if(file_.stats().st_size != 819200 && file_.stats().st_size != 409600)
			throw Error::InvalidFormat;

		uint32_t magic_word = file_.get24be();
		if(magic_word != 0x4b6000)
			throw Error::InvalidFormat;

		file_.seek(0, SEEK_SET);
		if(file_.stats().st_size == 819200) {
			encoding_ = Encoding::GCR800;
			format_ = 0x22;
			data_ = file_.read(819200);
		} else {
			encoding_ = Encoding::GCR400;
			format_ = 0x2;
			data_ = file_.read(409600);
		}
	} else {
		// DiskCopy 4.2 it is then:
		//
		// File format starts with 64 bytes dedicated to the disk name;
		// this is a Pascal-style string though there is apparently a
		// bug in one version of Disk Copy that can cause the length to
		// be one too high.
		//
		// Validate the length, then skip the rest of the string.
		if(name_length > 64)
			throw Error::InvalidFormat;

		// Get the length of the data and tag blocks.
		file_.seek(64, SEEK_SET);
		const auto data_block_length = file_.get32be();
		const auto tag_block_length = file_.get32be();
		const auto data_checksum = file_.get32be();
		const auto tag_checksum = file_.get32be();

		// Don't continue with no data.
		if(!data_block_length)
			throw Error::InvalidFormat;

		// Check that this is a comprehensible disk encoding.
		const auto encoding = file_.get8();
		switch(encoding) {
			default: throw Error::InvalidFormat;

			case 0:	encoding_ = Encoding::GCR400;	break;
			case 1:	encoding_ = Encoding::GCR800;	break;
			case 2:	encoding_ = Encoding::MFM720;	break;
			case 3:	encoding_ = Encoding::MFM1440;	break;
		}
		format_ = file_.get8();

		// Check the magic number.
		const auto magic_number = file_.get16be();
		if(magic_number != 0x0100)
			throw Error::InvalidFormat;

		// Read the data and tags, and verify that enough data
		// was present.
		data_ = file_.read(data_block_length);
		tags_ = file_.read(tag_block_length);

		if(data_.size() != data_block_length || tags_.size() != tag_block_length)
			throw Error::InvalidFormat;

		// Verify the two checksums.
		const auto computed_data_checksum = checksum(data_);
		const auto computed_tag_checksum = checksum(tags_, 12);

		if(computed_tag_checksum != tag_checksum || computed_data_checksum != data_checksum)
			throw Error::InvalidFormat;
	}
}

uint32_t MacintoshIMG::checksum(const std::vector<uint8_t> &data, size_t bytes_to_skip) {
	uint32_t result = 0;

	// Checksum algorith is: take each two bytes as a big-endian word; add that to a
	// 32-bit accumulator and then rotate the accumulator right one position.
	for(size_t c = bytes_to_skip; c < data.size(); c += 2) {
		const uint16_t next_word = uint16_t((data[c] << 8) | data[c+1]);
		result += next_word;
		result = (result >> 1) | (result << 31);
	}

	return result;
}

HeadPosition MacintoshIMG::get_maximum_head_position() {
	return HeadPosition(80);
}

int MacintoshIMG::get_head_count() {
	// Bit 5 in the format field indicates whether this disk is double
	// sided, regardless of whether it is GCR or MFM.
	return 1 + ((format_ & 0x20) >> 5);
}

bool MacintoshIMG::get_is_read_only() {
	return true;
}

std::shared_ptr<::Storage::Disk::Track> MacintoshIMG::get_track_at_position(::Storage::Disk::Track::Address address) {
	/*
		The format_ byte has the following meanings:

		GCR:
			This byte appears on disk as the GCR format nibble in every sector tag.
			The low five bits are an interleave factor, either:

				'2' for 0 8 1 9 2 10 3 11 4 12 5 13 6 14 7 15; or
				'4' for 0 4 8 12 1 5 9 13 2 6 10 14 3 7 11 15.

			Bit 5 indicates double sided or not.

		MFM:
			The low five bits provide sector size as a multiple of 256 bytes.
			Bit 5 indicates double sided or not.
	*/

	if(encoding_ == Encoding::GCR400 || encoding_ == Encoding::GCR800) {
		// Perform a GCR encoding.
		const auto included_sectors = Storage::Encodings::AppleGCR::Macintosh::sectors_in_track(address.position.as_int());
		const size_t start_sector = size_t(included_sectors.start * get_head_count() + included_sectors.length * address.head);

		if(start_sector*512 >= data_.size()) return nullptr;

		uint8_t *sector = &data_[512 * start_sector];
		uint8_t *tags = tags_.size() ? &tags_[12 * start_sector] : nullptr;

		Storage::Disk::PCMSegment segment;
		segment += Encodings::AppleGCR::six_and_two_sync(24);

		for(int c = 0; c < included_sectors.length; ++c) {
//			const int interleave_scale = ((format_ & 0x1f) == 4) ? 8 : 4;
			uint8_t sector_id = uint8_t(c);//uint8_t((c == included_sectors.length - 1) ? c : (c * interleave_scale)%included_sectors.length);

			uint8_t sector_plus_tags[524];

			// Copy in the tags, if provided; otherwise generate them.
			if(tags) {
				memcpy(sector_plus_tags, tags, 12);
				tags += 12;
			} else {
				// TODO: fill in tags properly.
				memset(sector_plus_tags, 0, 12);
			}

			// Copy in the sector body.
			memcpy(&sector_plus_tags[12], sector, 512);
			sector += 512;

			// NB: sync lengths below are identical to those for
			// the Apple II, as I have no idea whatsoever what they
			// should be.

			segment += Encodings::AppleGCR::Macintosh::header(
				format_,
				uint8_t(address.position.as_int()),
				sector_id,
				!!address.head
			);
			segment += Encodings::AppleGCR::six_and_two_sync(7);
			segment += Encodings::AppleGCR::Macintosh::data(sector_id, sector_plus_tags);
			segment += Encodings::AppleGCR::six_and_two_sync(20);
		}

		// TODO: is there inter-track skew?

		return std::make_shared<PCMTrack>(segment);
	}

	return nullptr;
}
