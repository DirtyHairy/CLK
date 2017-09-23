//
//  MFMDiskController.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "MFMDiskController.hpp"

#include "../Encodings/MFM.hpp"

using namespace Storage::Disk;

MFMController::MFMController(Cycles clock_rate) :
	Storage::Disk::Controller(clock_rate),
	crc_generator_(0x1021, 0xffff),
	data_mode_(DataMode::Scanning),
	is_awaiting_marker_value_(false) {
}

void MFMController::process_index_hole() {
	posit_event((int)Event::IndexHole);
}

void MFMController::process_write_completed() {
	posit_event((int)Event::DataWritten);
}

void MFMController::set_is_double_density(bool is_double_density) {
	is_double_density_ = is_double_density;
	Storage::Time bit_length;
	bit_length.length = 1;
	bit_length.clock_rate = is_double_density ? 500000 : 250000;
	set_expected_bit_length(bit_length);

	if(!is_double_density) is_awaiting_marker_value_ = false;
}

bool MFMController::get_is_double_density() {
	return is_double_density_;
}

void MFMController::set_data_mode(DataMode mode) {
	data_mode_ = mode;
}

MFMController::Token MFMController::get_latest_token() {
	return latest_token_;
}

NumberTheory::CRC16 &MFMController::get_crc_generator() {
	return crc_generator_;
}

void MFMController::process_input_bit(int value) {
	if(data_mode_ == DataMode::Writing) return;

	shift_register_ = (shift_register_ << 1) | value;
	bits_since_token_++;

	if(data_mode_ == DataMode::Scanning) {
		Token::Type token_type = Token::Byte;
		if(!is_double_density_) {
			switch(shift_register_ & 0xffff) {
				case Storage::Encodings::MFM::FMIndexAddressMark:
					token_type = Token::Index;
					crc_generator_.reset();
					crc_generator_.add(latest_token_.byte_value = Storage::Encodings::MFM::IndexAddressByte);
				break;
				case Storage::Encodings::MFM::FMIDAddressMark:
					token_type = Token::ID;
					crc_generator_.reset();
					crc_generator_.add(latest_token_.byte_value = Storage::Encodings::MFM::IDAddressByte);
				break;
				case Storage::Encodings::MFM::FMDataAddressMark:
					token_type = Token::Data;
					crc_generator_.reset();
					crc_generator_.add(latest_token_.byte_value = Storage::Encodings::MFM::DataAddressByte);
				break;
				case Storage::Encodings::MFM::FMDeletedDataAddressMark:
					token_type = Token::DeletedData;
					crc_generator_.reset();
					crc_generator_.add(latest_token_.byte_value = Storage::Encodings::MFM::DeletedDataAddressByte);
				break;
				default:
				break;
			}
		} else {
			switch(shift_register_ & 0xffff) {
				case Storage::Encodings::MFM::MFMIndexSync:
					bits_since_token_ = 0;
					is_awaiting_marker_value_ = true;

					token_type = Token::Sync;
					latest_token_.byte_value = Storage::Encodings::MFM::MFMIndexSyncByteValue;
				break;
				case Storage::Encodings::MFM::MFMSync:
					bits_since_token_ = 0;
					is_awaiting_marker_value_ = true;
					crc_generator_.set_value(Storage::Encodings::MFM::MFMPostSyncCRCValue);

					token_type = Token::Sync;
					latest_token_.byte_value = Storage::Encodings::MFM::MFMSyncByteValue;
				break;
				default:
				break;
			}
		}

		if(token_type != Token::Byte) {
			latest_token_.type = token_type;
			bits_since_token_ = 0;
			posit_event((int)Event::Token);
			return;
		}
	}

	if(bits_since_token_ == 16) {
		latest_token_.type = Token::Byte;
		latest_token_.byte_value = (uint8_t)(
			((shift_register_ & 0x0001) >> 0) |
			((shift_register_ & 0x0004) >> 1) |
			((shift_register_ & 0x0010) >> 2) |
			((shift_register_ & 0x0040) >> 3) |
			((shift_register_ & 0x0100) >> 4) |
			((shift_register_ & 0x0400) >> 5) |
			((shift_register_ & 0x1000) >> 6) |
			((shift_register_ & 0x4000) >> 7));
		bits_since_token_ = 0;

		if(is_awaiting_marker_value_ && is_double_density_) {
			is_awaiting_marker_value_ = false;
			switch(latest_token_.byte_value) {
				case Storage::Encodings::MFM::IndexAddressByte:
					latest_token_.type = Token::Index;
				break;
				case Storage::Encodings::MFM::IDAddressByte:
					latest_token_.type = Token::ID;
				break;
				case Storage::Encodings::MFM::DataAddressByte:
					latest_token_.type = Token::Data;
				break;
				case Storage::Encodings::MFM::DeletedDataAddressByte:
					latest_token_.type = Token::DeletedData;
				break;
				default: break;
			}
		}

		crc_generator_.add(latest_token_.byte_value);
		posit_event((int)Event::Token);
		return;
	}
}

void MFMController::write_bit(int bit) {
	if(is_double_density_) {
		get_drive().write_bit(!bit && !last_bit_);
		get_drive().write_bit(!!bit);
		last_bit_ = bit;
	} else {
		get_drive().write_bit(true);
		get_drive().write_bit(!!bit);
	}
}

void MFMController::write_byte(uint8_t byte) {
	for(int c = 0; c < 8; c++) write_bit((byte << c)&0x80);
	crc_generator_.add(byte);
}

void MFMController::write_raw_short(uint16_t value) {
	for(int c = 0; c < 16; c++) {
		get_drive().write_bit(!!((value << c)&0x8000));
	}
}

void MFMController::write_crc() {
	uint16_t crc = get_crc_generator().get_value();
	write_byte(crc >> 8);
	write_byte(crc & 0xff);
}

void MFMController::write_n_bytes(int quantity, uint8_t value) {
	while(quantity--) write_byte(value);
}

void MFMController::write_id_joiner() {
	if(get_is_double_density()) {
		write_n_bytes(12, 0x00);
		for(int c = 0; c < 3; c++) write_raw_short(Storage::Encodings::MFM::MFMSync);
		get_crc_generator().set_value(Storage::Encodings::MFM::MFMPostSyncCRCValue);
		write_byte(Storage::Encodings::MFM::IDAddressByte);
	} else {
		write_n_bytes(6, 0x00);
		get_crc_generator().reset();
		write_raw_short(Storage::Encodings::MFM::FMIDAddressMark);
	}
}

void MFMController::write_id_data_joiner(bool is_deleted, bool skip_first_gap) {
	if(get_is_double_density()) {
		if(!skip_first_gap) write_n_bytes(22, 0x4e);
		write_n_bytes(12, 0x00);
		for(int c = 0; c < 3; c++) write_raw_short(Storage::Encodings::MFM::MFMSync);
		get_crc_generator().set_value(Storage::Encodings::MFM::MFMPostSyncCRCValue);
		write_byte(is_deleted ? Storage::Encodings::MFM::DeletedDataAddressByte : Storage::Encodings::MFM::DataAddressByte);
	} else {
		if(!skip_first_gap) write_n_bytes(11, 0xff);
		write_n_bytes(6, 0x00);
		get_crc_generator().reset();
		get_crc_generator().add(is_deleted ? Storage::Encodings::MFM::DeletedDataAddressByte : Storage::Encodings::MFM::DataAddressByte);
		write_raw_short(is_deleted ? Storage::Encodings::MFM::FMDeletedDataAddressMark : Storage::Encodings::MFM::FMDataAddressMark);
	}
}

void MFMController::write_post_data_gap() {
	if(get_is_double_density()) {
		write_n_bytes(54, 0x4e);
	} else {
		write_n_bytes(27, 0xff);
	}
}

void MFMController::write_start_of_track() {
	if(get_is_double_density()) {
		write_n_bytes(80, 0x4e);
		write_n_bytes(12, 0x00);
		for(int c = 0; c < 3; c++) write_raw_short(Storage::Encodings::MFM::MFMIndexSync);
		write_byte(Storage::Encodings::MFM::IndexAddressByte);
		write_n_bytes(50, 0x4e);
	} else {
		write_n_bytes(40, 0xff);
		write_n_bytes(6, 0x00);
		write_raw_short(Storage::Encodings::MFM::FMIndexAddressMark);
		write_n_bytes(26, 0xff);
	}
}
