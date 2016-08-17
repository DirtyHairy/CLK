//
//  TapePRG.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TapePRG.hpp"

#include <sys/stat.h>

using namespace Storage;

TapePRG::TapePRG(const char *file_name) : _file(nullptr), _bitPhase(3), _filePhase(FilePhaseLeadIn), _phaseOffset(0), _copy_mask(0x80)
{
	struct stat file_stats;
	stat(file_name, &file_stats);

	// There's really no way to validate other than that if this file is larger than 64kb,
	// of if load address + length > 65536 then it's broken.
	if(file_stats.st_size >= 65538 || file_stats.st_size < 3)
		throw ErrorBadFormat;

	_file = fopen(file_name, "rb");
	if(!_file) throw ErrorBadFormat;

	_load_address = (uint16_t)fgetc(_file);
	_load_address |= (uint16_t)fgetc(_file) << 8;
	_length = (uint16_t)(file_stats.st_size - 2);

				fseek(_file, 2, SEEK_SET);


	if (_load_address + _length >= 65536)
		throw ErrorBadFormat;
}

TapePRG::~TapePRG()
{
	if(_file) fclose(_file);
}

Tape::Pulse TapePRG::get_next_pulse()
{
	// these are all microseconds per pole
	static const unsigned int leader_zero_length = 179;
	static const unsigned int zero_length = 169;
	static const unsigned int one_length = 247;
	static const unsigned int marker_length = 328;

	_bitPhase = (_bitPhase+1)&3;
	if(!_bitPhase) get_next_output_token();

	Tape::Pulse pulse;
	switch(_outputToken)
	{
		case Leader:		pulse.length.length = leader_zero_length;							break;
		case Zero:			pulse.length.length = (_bitPhase&2) ? one_length : zero_length;		break;
		case One:			pulse.length.length = (_bitPhase&2) ? zero_length : one_length;		break;
		case WordMarker:	pulse.length.length = (_bitPhase&2) ? one_length : marker_length;	break;
		case EndOfBlock:	pulse.length.length = (_bitPhase&2) ? zero_length : marker_length;	break;
	}
	pulse.length.clock_rate = 1000000;
	pulse.type = (_bitPhase&1) ? Pulse::High : Pulse::Low;
	return pulse;
}

void TapePRG::reset()
{
	_bitPhase = 3;
	fseek(_file, 2, SEEK_SET);
	_filePhase = FilePhaseLeadIn;
	_phaseOffset = 0;
	_copy_mask = 0x80;
}

void TapePRG::get_next_output_token()
{
	static const int block_length = 192;	// not counting the checksum
	static const int countdown_bytes = 9;

	// the lead-in is 20,000 instances of the lead-in pair; every other phase begins with 5000
	// before doing whatever it should be doing
	if((_filePhase == FilePhaseLeadIn || _filePhase == FilePhaseHeaderDataGap) || _phaseOffset < 50)
	{
		_outputToken = Leader;
		_phaseOffset++;
		if(
			(_filePhase == FilePhaseLeadIn && _phaseOffset == 20000) ||
			(_filePhase == FilePhaseHeaderDataGap && _phaseOffset == 5586)
		)
		{
			_phaseOffset = 0;
			_filePhase = (_filePhase == FilePhaseLeadIn) ? FilePhaseHeader : FilePhaseData;
		}
		return;
	}

	// determine whether a new byte needs to be queued up
	int block_offset = _phaseOffset - 50;
	int bit_offset = block_offset % 10;
	int byte_offset = block_offset / 10;
	_phaseOffset++;

	if(byte_offset == block_length + countdown_bytes + 1) // i.e. after the checksum
	{
		_outputToken = EndOfBlock;
		_phaseOffset = 0;

		switch(_filePhase)
		{
			default: break;
//			case FilePhaseLeadIn:
//				_filePhase = FilePhaseHeader;
//			break;
			case FilePhaseHeader:
				_copy_mask ^= 0x80;
				if(_copy_mask) _filePhase = FilePhaseHeaderDataGap;
			break;
			case FilePhaseData:
				if(feof(_file))
				{
					_copy_mask ^= 0x80;
					fseek(_file, 2, SEEK_SET);
				}
			break;
		}
		printf("\n===\n");
		return;
	}

	if(bit_offset == 0)
	{
		// the first nine bytes are countdown; the high bit is set if this is a header
		if(byte_offset < countdown_bytes)
		{
			_output_byte = (uint8_t)(countdown_bytes - byte_offset) | _copy_mask;
		}
		else if(byte_offset == countdown_bytes + block_length)
		{
			_output_byte = _check_digit;
		}
		else
		{
			if(byte_offset == countdown_bytes) _check_digit = 0;
			if(_filePhase == FilePhaseHeader)
			{
				switch(byte_offset - countdown_bytes)
				{
					case 0:	_output_byte = 0x03;										break;
					case 1: _output_byte = _load_address & 0xff;						break;
					case 2: _output_byte = (_load_address >> 8)&0xff;					break;
					case 3: _output_byte = (_load_address + _length) & 0xff;			break;
					case 4: _output_byte = ((_load_address + _length) >> 8) & 0xff;		break;

					case 5: _output_byte = 0x50;	break; // P
					case 6: _output_byte = 0x52;	break; // R
					case 7: _output_byte = 0x47;	break; // G
					default:
						_output_byte = 0x20;
					break;
				}
			}
			else
			{
				_output_byte = (uint8_t)fgetc(_file);
				if(feof(_file)) _output_byte = 0x00;
			}

			_check_digit ^= _output_byte;
		}

		printf(" %02x", _output_byte);
	}

	switch(bit_offset)
	{
		case 0:
			_outputToken = WordMarker;
		break;
		default:	// i.e. 1–8
			_outputToken = (_output_byte & (1 << (bit_offset - 1))) ? One : Zero;
		break;
		case 9:
		{
			uint8_t parity = _output_byte;
			parity ^= (parity >> 4);
			parity ^= (parity >> 2);
			parity ^= (parity >> 1);
			_outputToken = (parity&1) ? Zero : One;
			printf("[%d]", parity&1);
		}
		break;
	}
}
