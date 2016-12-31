//
//  MFM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "MFM.hpp"

#include "../PCMTrack.hpp"
#include "../../../NumberTheory/CRC.hpp"

using namespace Storage::Encodings::MFM;

class MFMEncoder: public Encoder {
	public:
		MFMEncoder(std::vector<uint8_t> &target) : Encoder(target) {}

		void add_byte(uint8_t input) {
			crc_generator_.add(input);
			uint16_t spread_value =
				(uint16_t)(
					((input & 0x01) << 0) |
					((input & 0x02) << 1) |
					((input & 0x04) << 2) |
					((input & 0x08) << 3) |
					((input & 0x10) << 4) |
					((input & 0x20) << 5) |
					((input & 0x40) << 6) |
					((input & 0x80) << 7)
				);
			uint16_t or_bits = (uint16_t)((spread_value << 1) | (spread_value >> 1) | (last_output_ << 15));
			uint16_t output = spread_value | ((~or_bits) & 0xaaaa);
			output_short(output);
		}

		void add_index_address_mark() {
			for(int c = 0; c < 3; c++) output_short(MFMIndexSync);
			add_byte(MFMIndexAddressByte);
		}

		void add_ID_address_mark() {
			output_sync();
			add_byte(MFMIDAddressByte);
		}

		void add_data_address_mark() {
			output_sync();
			add_byte(MFMDataAddressByte);
		}

		void add_deleted_data_address_mark() {
			output_sync();
			add_byte(MFMDeletedDataAddressByte);
		}

	private:
		uint16_t last_output_;
		void output_short(uint16_t value) {
			last_output_ = value;
			Encoder::output_short(value);
		}

		void output_sync() {
			for(int c = 0; c < 3; c++) output_short(MFMSync);
			crc_generator_.set_value(MFMPostSyncCRCValue);
		}
};

class FMEncoder: public Encoder {
	// encodes each 16-bit part as clock, data, clock, data [...]
	public:
		FMEncoder(std::vector<uint8_t> &target) : Encoder(target) {}

		void add_byte(uint8_t input) {
			crc_generator_.add(input);
			output_short(
				(uint16_t)(
					((input & 0x01) << 0) |
					((input & 0x02) << 1) |
					((input & 0x04) << 2) |
					((input & 0x08) << 3) |
					((input & 0x10) << 4) |
					((input & 0x20) << 5) |
					((input & 0x40) << 6) |
					((input & 0x80) << 7) |
					0xaaaa
				));
		}

		void add_index_address_mark()
		{
			crc_generator_.reset();
			crc_generator_.add(MFMIndexAddressByte);
			output_short(FMIndexAddressMark);
		}

		void add_ID_address_mark()
		{
			crc_generator_.reset();
			crc_generator_.add(MFMIDAddressByte);
			output_short(FMIDAddressMark);
		}

		void add_data_address_mark()
		{
			crc_generator_.reset();
			crc_generator_.add(MFMDataAddressByte);
			output_short(FMDataAddressMark);
		}

		void add_deleted_data_address_mark()
		{
			crc_generator_.reset();
			crc_generator_.add(MFMDeletedDataAddressByte);
			output_short(FMDeletedDataAddressMark);
		}
};

static uint8_t logarithmic_size_for_size(size_t size)
{
	switch(size)
	{
		default:	return 0;
		case 256:	return 1;
		case 512:	return 2;
		case 1024:	return 3;
		case 2048:	return 4;		std::vector<uint8_t> get_track(uint8_t track);

		case 4196:	return 5;
	}
}

template<class T> std::shared_ptr<Storage::Disk::Track>
	GetTrackWithSectors(
		const std::vector<Sector> &sectors,
		size_t post_index_address_mark_bytes, uint8_t post_index_address_mark_value,
		size_t pre_address_mark_bytes, size_t post_address_mark_bytes,
		size_t pre_data_mark_bytes, size_t post_data_bytes,
		size_t inter_sector_gap,
		size_t expected_track_bytes)
{
	Storage::Disk::PCMSegment segment;
	segment.data.reserve(expected_track_bytes);
	T shifter(segment.data);

	// output the index mark
	shifter.add_index_address_mark();

	// add the post-index mark
	for(int c = 0; c < post_index_address_mark_bytes; c++) shifter.add_byte(post_index_address_mark_value);

	// add sectors
	for(const Sector &sector : sectors)
	{
		// gap
		for(int c = 0; c < pre_address_mark_bytes; c++) shifter.add_byte(0x00);

		// sector header
		shifter.add_ID_address_mark();
		shifter.add_byte(sector.track);
		shifter.add_byte(sector.side);
		shifter.add_byte(sector.sector);
		uint8_t size = logarithmic_size_for_size(sector.data.size());
		shifter.add_byte(size);
		shifter.add_crc();

		// gap
		for(int c = 0; c < post_address_mark_bytes; c++) shifter.add_byte(0x4e);
		for(int c = 0; c < pre_data_mark_bytes; c++) shifter.add_byte(0x00);

		// data
		shifter.add_data_address_mark();
		for(size_t c = 0; c < sector.data.size(); c++)
		{
			shifter.add_byte(sector.data[c]);
		}
		shifter.add_crc();

		// gap
		for(int c = 0; c < post_data_bytes; c++) shifter.add_byte(0x00);
		for(int c = 0; c < inter_sector_gap; c++) shifter.add_byte(0x4e);
	}

	while(segment.data.size() < expected_track_bytes) shifter.add_byte(0x00);

	segment.number_of_bits = (unsigned int)(segment.data.size() * 8);
	return std::shared_ptr<Storage::Disk::Track>(new Storage::Disk::PCMTrack(std::move(segment)));
}

Encoder::Encoder(std::vector<uint8_t> &target) :
	crc_generator_(0x1021, 0xffff),
	target_(target)
{}

void Encoder::output_short(uint16_t value)
{
	target_.push_back(value >> 8);
	target_.push_back(value & 0xff);
}

void Encoder::add_crc()
{
	uint16_t crc_value = crc_generator_.get_value();
	add_byte(crc_value >> 8);
	add_byte(crc_value & 0xff);
}

std::shared_ptr<Storage::Disk::Track> Storage::Encodings::MFM::GetFMTrackWithSectors(const std::vector<Sector> &sectors)
{
	return GetTrackWithSectors<FMEncoder>(
		sectors,
		16, 0x00,
		6, 0,
		17, 14,
		0,
		6250);	// i.e. 250kbps (including clocks) * 60 = 15000kpm, at 300 rpm => 50 kbits/rotation => 6250 bytes/rotation
}

std::shared_ptr<Storage::Disk::Track> Storage::Encodings::MFM::GetMFMTrackWithSectors(const std::vector<Sector> &sectors)
{
	return GetTrackWithSectors<MFMEncoder>(
		sectors,
		50, 0x4e,
		12, 22,
		12, 18,
		32,
		12500);	// unintelligently: double the single-density bytes/rotation (or: 500kps @ 300 rpm)
}

std::unique_ptr<Encoder> Storage::Encodings::MFM::GetMFMEncoder(std::vector<uint8_t> &target)
{
	return std::unique_ptr<Encoder>(new MFMEncoder(target));
}

std::unique_ptr<Encoder> Storage::Encodings::MFM::GetFMEncoder(std::vector<uint8_t> &target)
{
	return std::unique_ptr<Encoder>(new FMEncoder(target));
}

#pragma mark - Parser

Parser::Parser(bool is_mfm) :
	Storage::Disk::Controller(4000000, 1, 300),
	crc_generator_(0x1021, 0xffff),
	shift_register_(0), track_(0), is_mfm_(is_mfm)
{
	Storage::Time bit_length;
	bit_length.length = 1;
	bit_length.clock_rate = is_mfm ? 500000 : 250000;	// i.e. 250 kbps (including clocks)
	set_expected_bit_length(bit_length);

	drive.reset(new Storage::Disk::Drive);
	set_drive(drive);
	set_motor_on(true);
}

Parser::Parser(bool is_mfm, const std::shared_ptr<Storage::Disk::Disk> &disk) :
	Parser(is_mfm)
{
	drive->set_disk(disk);
}

Parser::Parser(bool is_mfm, const std::shared_ptr<Storage::Disk::Track> &track) :
	Parser(is_mfm)
{
	drive->set_disk_with_track(track);
}

void Parser::seek_to_track(uint8_t track)
{
	int difference = (int)track - (int)track_;
	track_ = track;

	if(difference)
	{
		int direction = difference < 0 ? -1 : 1;
		difference *= direction;

		for(int c = 0; c < difference; c++) step(direction);
	}
}

std::shared_ptr<Storage::Encodings::MFM::Sector> Parser::get_sector(uint8_t track, uint8_t sector)
{
	seek_to_track(track);
	return get_sector(sector);
}

std::vector<uint8_t> Parser::get_track(uint8_t track)
{
	seek_to_track(track);
	return get_track();
}

void Parser::process_input_bit(int value, unsigned int cycles_since_index_hole)
{
	shift_register_ = ((shift_register_ << 1) | (unsigned int)value) & 0xffff;
	bit_count_++;
}

void Parser::process_index_hole()
{
	index_count_++;
}

uint8_t Parser::get_byte_for_shift_value(uint16_t value)
{
	return (uint8_t)(
		((value&0x0001) >> 0) |
		((value&0x0004) >> 1) |
		((value&0x0010) >> 2) |
		((value&0x0040) >> 3) |
		((value&0x0100) >> 4) |
		((value&0x0400) >> 5) |
		((value&0x1000) >> 6) |
		((value&0x4000) >> 7));}

uint8_t Parser::get_next_byte()
{
	bit_count_ = 0;
	while(bit_count_ < 16) run_for_cycles(1);
	uint8_t byte = get_byte_for_shift_value((uint16_t)shift_register_);
	crc_generator_.add(byte);
	return byte;
}

std::vector<uint8_t> Parser::get_track()
{
	std::vector<uint8_t> result;
	size_t number_of_bits = 0;
	bool is_clock = false;

	// align to the next index hole
	index_count_ = 0;
	while(!index_count_) run_for_cycles(1);

	// capture every other bit until the next index hole
	index_count_ = 0;
	while(1)
	{
		// wait until either another bit or the index hole arrives
		bit_count_ = 0;
		while(!bit_count_ && !index_count_) run_for_cycles(1);

		// if that was the index hole then finish
		if(index_count_) break;

		// otherwise, add another bit to the collection if it wasn't a clock
		if(!is_clock)
		{
			int bit = number_of_bits & 7;
			if(!bit)
			{
				if(!result.empty()) printf("[%02x]", result.back());
				result.push_back(0);
			}
			result[number_of_bits >> 3] |= (shift_register_&1) << (7 - bit);
			number_of_bits++;
		}
		is_clock ^= true;

		// if a synchronisation is detected then align
		uint16_t low_shift_register = (shift_register_&0xffff);
		bool is_sync = false;
		if(is_mfm_)
		{
			is_sync = (low_shift_register == MFMIndexSync) || (low_shift_register == MFMSync);
		}
		else
		{
			is_sync =
				(low_shift_register == FMIndexAddressMark) ||
				(low_shift_register == FMIDAddressMark) ||
				(low_shift_register == FMDataAddressMark) ||
				(low_shift_register == FMDeletedDataAddressMark);
		}
		if(is_sync)
		{
			if(number_of_bits&7) number_of_bits += 8 - (number_of_bits&7);
			result.push_back(get_byte_for_shift_value((uint16_t)shift_register_));
			number_of_bits += 8;
			is_clock = true;
		}
	}

	return result;
}


std::shared_ptr<Storage::Encodings::MFM::Sector> Parser::get_next_sector()
{
	std::shared_ptr<Storage::Encodings::MFM::Sector> sector(new Storage::Encodings::MFM::Sector);
	index_count_ = 0;

	while(index_count_ < 2)
	{
		// look for an ID address mark
		bool id_found = false;
		while(!id_found)
		{
			run_for_cycles(1);
			if(is_mfm_)
			{
				while(shift_register_ == Storage::Encodings::MFM::MFMSync)
				{
					uint8_t mark = get_next_byte();
					if(mark == Storage::Encodings::MFM::MFMIDAddressByte)
					{
						crc_generator_.set_value(MFMPostSyncCRCValue);
						id_found = true;
						break;
					}
				}
			}
			else
			{
				if(shift_register_ == Storage::Encodings::MFM::FMIDAddressMark)
				{
					crc_generator_.reset();
					id_found = true;
				}
			}
			if(index_count_ >= 2) return nullptr;
		}

		crc_generator_.add(MFMIDAddressByte);
		sector->track = get_next_byte();
		sector->side = get_next_byte();
		sector->sector = get_next_byte();
		uint8_t size = get_next_byte();
		uint16_t header_crc = crc_generator_.get_value();
		if((header_crc >> 8) != get_next_byte()) continue;
		if((header_crc & 0xff) != get_next_byte()) continue;

		// look for data mark
		bool data_found = false;
		while(!data_found)
		{
			run_for_cycles(1);
			if(is_mfm_)
			{
				while(shift_register_ == Storage::Encodings::MFM::MFMSync)
				{
					uint8_t mark = get_next_byte();
					if(mark == Storage::Encodings::MFM::MFMDataAddressByte)
					{
						crc_generator_.set_value(MFMPostSyncCRCValue);
						data_found = true;
						break;
					}
					if(mark == Storage::Encodings::MFM::MFMIDAddressByte) return nullptr;
				}
			}
			else
			{
				if(shift_register_ == Storage::Encodings::MFM::FMDataAddressMark)
				{
					crc_generator_.reset();
					data_found = true;
				}
				if(shift_register_ == Storage::Encodings::MFM::FMIDAddressMark) return nullptr;
			}
			if(index_count_ >= 2) return nullptr;
		}
		crc_generator_.add(MFMDataAddressByte);

		size_t data_size = (size_t)(128 << size);
		sector->data.reserve(data_size);
		for(size_t c = 0; c < data_size; c++)
		{
			sector->data.push_back(get_next_byte());
		}
		uint16_t data_crc = crc_generator_.get_value();
		if((data_crc >> 8) != get_next_byte()) continue;
		if((data_crc & 0xff) != get_next_byte()) continue;

		return sector;
	}

	return nullptr;
}

std::shared_ptr<Storage::Encodings::MFM::Sector> Parser::get_sector(uint8_t sector)
{
	std::shared_ptr<Storage::Encodings::MFM::Sector> first_sector;
	index_count_ = 0;
	while(!first_sector && index_count_ < 2) first_sector = get_next_sector();
	if(!first_sector) return first_sector;
	if(first_sector->sector == sector) return first_sector;

	while(1)
	{
		std::shared_ptr<Storage::Encodings::MFM::Sector> next_sector = get_next_sector();
		if(!next_sector) continue;
		if(next_sector->sector == first_sector->sector) return nullptr;
		if(next_sector->sector == sector) return next_sector;
	}
}
