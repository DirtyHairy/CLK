//
//  Electron.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Electron.hpp"

#include <algorithm>

using namespace Electron;

static const int cycles_per_line = 128;
static const int cycles_per_frame = 312*cycles_per_line;
static const int crt_cycles_multiplier = 8;
static const int crt_cycles_per_line = crt_cycles_multiplier * cycles_per_line;

Machine::Machine() :
	_interruptControl(0),
	_frameCycles(0),
	_outputPosition(0),
	_currentOutputLine(0)
{
	memset(_keyStates, 0, sizeof(_keyStates));
	memset(_palette, 0xf, sizeof(_palette));
	_crt = new Outputs::CRT(crt_cycles_per_line, 312, 1, 1);
	_interruptStatus = 0x02;
	setup6502();
}

Machine::~Machine()
{
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	unsigned int cycles = 1;

	if(address < 0x8000)
	{
		if(isReadOperation(operation))
		{
			*value = _ram[address];
		}
		else
		{
			// TODO: range check on address; a lot of the time the machine will be running code outside of
			// the screen area, meaning that no update is required.
			update_display();

			_ram[address] = *value;
		}

		// TODO: RAM timing for Modes 0–3
		cycles += (_frameCycles&1)^1;
	}
	else
	{
		if(address >= 0xc000)
		{
			if((address & 0xff00) == 0xfe00)
			{
//				printf("%c: %02x: ", isReadOperation(operation) ? 'r' : 'w', *value);

				switch(address&0xf)
				{
					case 0x0:
						if(isReadOperation(operation))
						{
							*value = _interruptStatus;
							_interruptStatus &= ~0x02;
						}
						else
						{
							_interruptControl = *value;
							evaluate_interrupts();
						}
					break;
					case 0x1:
					break;
					case 0x2:
						_startScreenAddress = (_startScreenAddress & 0xfe00) | (uint16_t)(((*value) & 0xe0) << 1);
					break;
					case 0x3:
						_startScreenAddress = (_startScreenAddress & 0x01ff) | (uint16_t)(((*value) & 0x3f) << 9);
					break;
					case 0x4:
						printf("Cassette\n");
					break;
					case 0x5:
						if(!isReadOperation(operation))
						{
							const uint8_t interruptDisable = (*value)&0xf0;
							if( interruptDisable )
							{
								if( interruptDisable&0x10 ) _interruptStatus &= ~InterruptDisplayEnd;
								if( interruptDisable&0x20 ) _interruptStatus &= ~InterruptRealTimeClock;
								if( interruptDisable&0x40 ) _interruptStatus &= ~InterruptHighToneDetect;
								evaluate_interrupts();
								// TODO: NMI (?)
							}
//							else
							{
								uint8_t nextROM = (*value)&0xf;

//								if(nextROM&0x08)
//								{
//									_activeRom = (Electron::ROMSlot)(nextROM&0x0e);
//									printf("%d -> Paged %d\n", nextROM, _activeRom);
//								}
								if(((_activeRom&12) != 8) || (nextROM&8))
								{
									_activeRom = (Electron::ROMSlot)nextROM;
								}
//								else
//								{
//									printf("Ignored!");
//								}
//								printf("%d -> Paged %d\n", nextROM, _activeRom);
							}
						}
					break;
					case 0x6:
						printf("Counter\n");
					break;
					case 0x7:
						if(!isReadOperation(operation))
						{
							_screenMode = ((*value) >> 3)&7;
							if(_screenMode == 7) _screenMode = 4;
							switch(_screenMode)
							{
								case 0: case 1: case 2: _screenModeBaseAddress = 0x3000; break;
								case 3: _screenModeBaseAddress = 0x4000; break;
								case 4: case 5: _screenModeBaseAddress = 0x5800; break;
								case 6: _screenModeBaseAddress = 0x6000; break;
							}
							printf("Misc. control\n");
						}
					break;
					default:
					{
						if(!isReadOperation(operation))
						{
							update_display();

							static const int registers[4][4] = {
								{10, 8, 2, 0},
								{14, 12, 6, 4},
								{15, 13, 7, 5},
								{11, 9, 3, 1},
							};
							const int index = (address >> 1)&3;
							const uint8_t colour = ~(*value);
							if(address&1)
							{
								_palette[registers[index][0]]	= (_palette[registers[index][0]]&3)	| ((colour >> 1)&4);
								_palette[registers[index][1]]	= (_palette[registers[index][1]]&3)	| ((colour >> 0)&4);
								_palette[registers[index][2]]	= (_palette[registers[index][2]]&3)	| ((colour << 1)&4);
								_palette[registers[index][3]]	= (_palette[registers[index][3]]&3)	| ((colour << 2)&4);

								_palette[registers[index][2]]	= (_palette[registers[index][2]]&5)	| ((colour >> 4)&2);
								_palette[registers[index][3]]	= (_palette[registers[index][3]]&5)	| ((colour >> 3)&2);
							}
							else
							{
								_palette[registers[index][0]]	= (_palette[registers[index][0]]&6)	| ((colour >> 7)&1);
								_palette[registers[index][1]]	= (_palette[registers[index][1]]&6)	| ((colour >> 6)&1);
								_palette[registers[index][2]]	= (_palette[registers[index][2]]&6)	| ((colour >> 5)&1);
								_palette[registers[index][3]]	= (_palette[registers[index][3]]&6)	| ((colour >> 4)&1);

								_palette[registers[index][0]]	= (_palette[registers[index][0]]&5)	| ((colour >> 2)&2);
								_palette[registers[index][1]]	= (_palette[registers[index][1]]&5)	| ((colour >> 1)&2);
							}
						}
					}
					break;
				}
			}
			else
			{
				if(isReadOperation(operation))
					*value = _os[address & 16383];
			}
		}
		else
		{
			if(isReadOperation(operation))
			{
				switch(_activeRom)
				{
					case ROMSlotBASIC:
					case ROMSlotBASIC+1:
						*value = _basic[address & 16383];
					break;
					case ROMSlotKeyboard:
					case ROMSlotKeyboard+1:
						*value = 0xf0;
						for(int address_line = 0; address_line < 14; address_line++)
						{
							if(!(address&(1 << address_line))) *value |= _keyStates[address_line];
						}
					break;
					default:
						*value = 0xff;
					break;
				}
			}
		}
	}

//	if(operation == CPU6502::BusOperation::ReadOpcode)
//	{
//		printf("%04x: %02x (%d)\n", address, *value, _frameCycles);
//	}

	_frameCycles += cycles;
	if(_frameCycles == cycles_per_frame)
	{
		update_display();
		_frameCycles = 0;
		_outputPosition = 0;
		_currentOutputLine = 0;
	}
	if(_frameCycles == 128*128) signal_interrupt(InterruptRealTimeClock);
	if(_frameCycles == 284*128) signal_interrupt(InterruptDisplayEnd);

	return cycles;
}

void Machine::set_rom(ROMSlot slot, size_t length, const uint8_t *data)
{
	uint8_t *target = nullptr;
	switch(slot)
	{
		case ROMSlotBASIC:	target = _basic;	break;
		case ROMSlotOS:		target = _os;		break;
		default: return;
	}

	memcpy(target, data, std::min((size_t)16384, length));
}

inline void Machine::signal_interrupt(Electron::Interrupt interrupt)
{
	_interruptStatus |= interrupt;
	evaluate_interrupts();
}

inline void Machine::evaluate_interrupts()
{
	if(_interruptStatus & _interruptControl)
	{
		_interruptStatus |= 1;
	}
	else
	{
		_interruptStatus &= ~1;
	}
	set_irq_line(_interruptStatus & 1);
}

inline void Machine::update_display()
{
	const int lines_of_hsync = 3;
	const int end_of_hsync = lines_of_hsync * cycles_per_line;
	const int first_graphics_line = 28;

	if(_frameCycles >= end_of_hsync)
	{
		// assert sync for the first three lines of the display, with a break at the end for horizontal alignment
		if(_outputPosition < end_of_hsync)
		{
			for(int c = 0; c < lines_of_hsync; c++)
			{
				_crt->output_sync(119 * crt_cycles_multiplier);
				_crt->output_blank(9 * crt_cycles_multiplier);
			}
			_outputPosition = end_of_hsync;
		}

		while(_outputPosition >= end_of_hsync && _outputPosition < _frameCycles)
		{
			const int current_line = _outputPosition >> 7;
			const int line_position = _outputPosition & 127;

			// all lines then start with 9 cycles of sync
			if(!line_position)
			{
				_crt->output_sync(9 * crt_cycles_multiplier);
				_outputPosition += 9;
			}
			else
			{
				bool isBlankLine =
					((_screenMode == 3) || (_screenMode == 6)) ?
						((current_line < first_graphics_line || current_line >= first_graphics_line+248) || (((current_line - first_graphics_line)%10) > 7)) :
						((current_line < first_graphics_line || current_line >= first_graphics_line+256));

				if(isBlankLine)
				{
					if(line_position == 9)
					{
						_crt->output_blank(119 * crt_cycles_multiplier);
						_outputPosition += 119;
					}
				}
				else
				{
					// there are then 15 cycles of blank, 80 cycles of pixels, and 24 further cycles of blank
					if(line_position == 9)
					{
						_crt->output_blank(15 * crt_cycles_multiplier);
						_outputPosition += 15;

						_crt->allocate_write_area(80 * crt_cycles_multiplier);
						_currentLine = (uint8_t *)_crt->get_write_target_for_buffer(0);

						if(current_line == first_graphics_line)
							_startLineAddress = _startScreenAddress;
						_currentScreenAddress = _startLineAddress;
					}

					if(line_position >= 24 && line_position < 104)
					{
						if(_currentLine && ((_screenMode < 4) || !(line_position&1)))
						{
							if(_currentScreenAddress&32768)
							{
								_currentScreenAddress = _screenModeBaseAddress + (_currentScreenAddress&32767);
							}
							uint8_t pixels = _ram[_currentScreenAddress];
							_currentScreenAddress = _currentScreenAddress+8;
							int output_ptr = (line_position - 24) << 3;

							switch(_screenMode)
							{
								case 0:
								case 3:
									for(int c = 0; c < 8; c++)
									{
										uint8_t colour = (pixels&0x80) >> 4;
										_currentLine[output_ptr + c] = _palette[colour];
										pixels <<= 1;
									}
								break;

								case 1:
									for(int c = 0; c < 8; c += 2)
									{
										uint8_t colour = ((pixels&0x80) >> 4) | ((pixels&0x08) >> 2);
										_currentLine[output_ptr + c + 0] = _currentLine[output_ptr + c + 1] = _palette[colour];
										pixels <<= 1;
									}
								break;

								case 2:
									for(int c = 0; c < 8; c += 4)
									{
										uint8_t colour = ((pixels&0x80) >> 4) | ((pixels&0x20) >> 3) | ((pixels&0x08) >> 2) | ((pixels&0x02) >> 1);
										_currentLine[output_ptr + c + 0] = _currentLine[output_ptr + c + 1] =
										_currentLine[output_ptr + c + 2] = _currentLine[output_ptr + c + 3] = _palette[colour];
										pixels <<= 1;
									}
								break;

								case 5:
									for(int c = 0; c < 16; c += 4)
									{
										uint8_t colour = ((pixels&0x80) >> 4) | ((pixels&0x08) >> 2);
										_currentLine[output_ptr + c + 0] = _currentLine[output_ptr + c + 1] =
										_currentLine[output_ptr + c + 2] = _currentLine[output_ptr + c + 3] = _palette[colour];
										pixels <<= 1;
									}
								break;

								default:
								case 4:
								case 6:
									for(int c = 0; c < 16; c += 2)
									{
										uint8_t colour = (pixels&0x80) >> 4;
										_currentLine[output_ptr + c] = _currentLine[output_ptr + c + 1] = _palette[colour];
										pixels <<= 1;
									}
								break;
							}
						}
						_outputPosition++;
					}

					if(line_position == 104)
					{
						_currentOutputLine++;
						if(!(_currentOutputLine&7))
						{
							_startLineAddress += ((_screenMode < 4) ? 80 : 40)*8 - 7;
						}
						else
							_startLineAddress++;

						_currentLine = nullptr;
						_crt->output_data(80 * crt_cycles_multiplier);
						_crt->output_blank(24 * crt_cycles_multiplier);
						_outputPosition += 24;
					}
				}
			}
		}
	}
}

const char *Machine::get_signal_decoder()
{
	return
		"vec4 sample(vec2 coordinate)\n"
		"{\n"
			"float texValue = texture(texID, coordinate).r;\n"
			"return vec4( step(4.0/256.0, mod(texValue, 8.0/256.0)), step(2.0/256.0, mod(texValue, 4.0/256.0)), step(1.0/256.0, mod(texValue, 2.0/256.0)), 1.0);\n"
		"}";
}

void Machine::set_key_state(Key key, bool isPressed)
{
	if(key == KeyBreak)
	{
		set_reset_line(isPressed);
	}
	else
	{
		if(isPressed)
			_keyStates[key >> 4] |= key&0xf;
		else
			_keyStates[key >> 4] &= ~(key&0xf);
	}
}
