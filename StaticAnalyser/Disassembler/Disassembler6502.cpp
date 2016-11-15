//
//  Disassembler6502.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Disassembler6502.hpp"
#include <map>

using namespace StaticAnalyser::MOS6502;

struct PartialDisassembly {
	Disassembly disassembly;
	std::vector<uint16_t> remaining_entry_points;
};

static void AddToDisassembly(PartialDisassembly &disassembly, const std::vector<uint8_t> &memory, uint16_t start_address, uint16_t entry_point)
{
	uint16_t address = entry_point;
	while(1)
	{
		uint16_t local_address = address - start_address;
		if(local_address >= memory.size()) return;

		struct Instruction instruction;
		instruction.address = address;
		address++;

		// get operation
		uint8_t operation = memory[local_address];

		// decode addressing mode
		switch(operation&0x1f)
		{
			case 0x00:
				if(operation >= 0x80) instruction.addressing_mode = Instruction::Immediate;
				else if(operation == 0x20) instruction.addressing_mode = Instruction::Absolute;
				else instruction.addressing_mode = Instruction::Implied;
			break;
			case 0x08:	case 0x18:	case 0x0a:	case 0x1a:	case 0x12:
				instruction.addressing_mode = Instruction::Implied;
			break;
			case 0x10:
				instruction.addressing_mode = Instruction::Relative;
			break;
			case 0x01: case 0x03:
				instruction.addressing_mode = Instruction::IndexedIndirectX;
			break;
			case 0x02:	case 0x09:	case 0x0b:
				instruction.addressing_mode = Instruction::Immediate;
			break;
			case 0x04:	case 0x05:	case 0x06:	case 0x07:
				instruction.addressing_mode = Instruction::ZeroPage;
			break;
			case 0x0c:	case 0x0d:	case 0x0e:	case 0x0f:
				instruction.addressing_mode = (operation == 0x6c) ? Instruction::Indirect : Instruction::Absolute;
			break;
			case 0x11:	case 0x13:
				instruction.addressing_mode = Instruction::IndirectIndexedY;
			break;
			case 0x14:	case 0x15:	case 0x16:	case 0x17:
				instruction.addressing_mode =
					(operation == 0x96 || operation == 0xb6 || operation == 0x97 || operation == 0xb7)
						? Instruction::ZeroPageY : Instruction::ZeroPageX;
			break;
			case 0x19:	case 0x1b:
				instruction.addressing_mode = Instruction::AbsoluteY;
			break;
			case 0x1c:	case 0x1d:	case 0x1e: case 0x1f:
				instruction.addressing_mode =
					(operation == 0x9e || operation == 0xbe || operation == 0x9f || operation == 0xbf)
						? Instruction::AbsoluteY : Instruction::AbsoluteX;
			break;
		}

		// decode operation
#define RM_INSTRUCTION(base, op)	\
	case base+0x09: case base+0x05: case base+0x15: case base+0x01: case base+0x11: case base+0x0d: case base+0x1d: case base+0x19:	\
		instruction.operation = op;	\
	break;

#define URM_INSTRUCTION(base, op)	\
	case base+0x07: case base+0x17: case base+0x03: case base+0x13: case base+0x0f: case base+0x1f: case base+0x1b:	\
		instruction.operation = op;	\
	break;

#define M_INSTRUCTION(base, op)	\
	case base+0x0a: case base+0x06: case base+0x16: case base+0x0e: case base+0x1e:	\
		instruction.operation = op;	\
	break;

#define IM_INSTRUCTION(base, op)	\
	case base:	instruction.operation = op; break;
		switch(operation)
		{
			IM_INSTRUCTION(0x00, Instruction::BRK)
			IM_INSTRUCTION(0x20, Instruction::JSR)
			IM_INSTRUCTION(0x40, Instruction::RTI)
			IM_INSTRUCTION(0x60, Instruction::RTS)
			case 0x4c: case 0x6c:
				instruction.operation = Instruction::JMP;
			break;

			IM_INSTRUCTION(0x10, Instruction::BPL)
			IM_INSTRUCTION(0x30, Instruction::BMI)
			IM_INSTRUCTION(0x50, Instruction::BVC)
			IM_INSTRUCTION(0x70, Instruction::BVS)
			IM_INSTRUCTION(0x90, Instruction::BCC)
			IM_INSTRUCTION(0xb0, Instruction::BCS)
			IM_INSTRUCTION(0xd0, Instruction::BNE)
			IM_INSTRUCTION(0xf0, Instruction::BEQ)

			IM_INSTRUCTION(0xca, Instruction::DEX)
			IM_INSTRUCTION(0x88, Instruction::DEY)
			IM_INSTRUCTION(0xe8, Instruction::INX)
			IM_INSTRUCTION(0xc8, Instruction::INY)

			IM_INSTRUCTION(0xaa, Instruction::TAX)
			IM_INSTRUCTION(0x8a, Instruction::TXA)
			IM_INSTRUCTION(0xa8, Instruction::TAY)
			IM_INSTRUCTION(0x98, Instruction::TYA)
			IM_INSTRUCTION(0xba, Instruction::TSX)
			IM_INSTRUCTION(0x9a, Instruction::TXS)

			IM_INSTRUCTION(0x68, Instruction::PLA)
			IM_INSTRUCTION(0x48, Instruction::PHA)
			IM_INSTRUCTION(0x28, Instruction::PLP)
			IM_INSTRUCTION(0x08, Instruction::PHP)

			IM_INSTRUCTION(0x18, Instruction::CLC)
			IM_INSTRUCTION(0x38, Instruction::SEC)
			IM_INSTRUCTION(0xd8, Instruction::CLD)
			IM_INSTRUCTION(0xf8, Instruction::SED)
			IM_INSTRUCTION(0x58, Instruction::CLI)
			IM_INSTRUCTION(0x78, Instruction::SEI)
			IM_INSTRUCTION(0xb8, Instruction::CLV)

			URM_INSTRUCTION(0x00, Instruction::SLO)
			URM_INSTRUCTION(0x20, Instruction::RLA)
			URM_INSTRUCTION(0x40, Instruction::SRE)
			URM_INSTRUCTION(0x60, Instruction::RRA)

			RM_INSTRUCTION(0x00, Instruction::ORA)
			RM_INSTRUCTION(0x20, Instruction::AND)
			RM_INSTRUCTION(0x40, Instruction::EOR)
			case 0x24: case 0x2c:
				instruction.operation = Instruction::BIT;
			break;
			RM_INSTRUCTION(0x60, Instruction::ADC)
			RM_INSTRUCTION(0xc0, Instruction::CMP)
			RM_INSTRUCTION(0xe0, Instruction::SBC)

			M_INSTRUCTION(0x00, Instruction::ASL)
			M_INSTRUCTION(0x20, Instruction::ROL)
			M_INSTRUCTION(0x40, Instruction::LSR)
			M_INSTRUCTION(0x60, Instruction::ROR)

			case 0xe0: case 0xe4: case 0xec:			instruction.operation =	Instruction::CPX;	break;
			case 0xc0: case 0xc4: case 0xcc:			instruction.operation = Instruction::CPY;	break;
			case 0xc6: case 0xd6: case 0xce: case 0xde:	instruction.operation = Instruction::DEC;	break;
			case 0xe6: case 0xf6: case 0xee: case 0xfe:	instruction.operation = Instruction::INC;	break;

			RM_INSTRUCTION(0xa0, Instruction::LDA)
			case 0x85: case 0x95: case 0x81: case 0x91: case 0x8d: case 0x9d: case 0x99:
				instruction.operation = Instruction::STA;
			break;
			case 0xa2: case 0xa6: case 0xb6: case 0xae: case 0xbe:
				instruction.operation = Instruction::LDX;
			break;
			case 0x86: case 0x96: case 0x8e:
				instruction.operation = Instruction::STX;
			break;
			case 0xa0: case 0xa4: case 0xb4: case 0xac: case 0xbc:
				instruction.operation = Instruction::LDY;
			break;
			case 0x84: case 0x94: case 0x8c:
				instruction.operation = Instruction::STY;
			break;

			case 0x04: case 0x0c: case 0x14: case 0x1a: case 0x1c:
			case 0x34: case 0x3a: case 0x3c: case 0x44: case 0x54: case 0x5a: case 0x5c:
			case 0x64: case 0x74: case 0x7a: case 0x7c:
			case 0x80: case 0x82: case 0x89:
			case 0xc2: case 0xd4: case 0xda: case 0xdc:
			case 0xe2: case 0xea: case 0xf4: case 0xfa: case 0xfc:
				instruction.operation = Instruction::NOP;
			break;

			case 0x87: case 0x97: case 0x83: case 0x8f:
				instruction.operation = Instruction::SAX;
			break;
			case 0xa7: case 0xb7: case 0xa3: case 0xb3: case 0xaf: case 0xbf:
				instruction.operation = Instruction::LAX;
			break;
			URM_INSTRUCTION(0xc0, Instruction::DCP)
			URM_INSTRUCTION(0xe0, Instruction::ISC)

			case 0x0b: case 0x2b:
				instruction.operation = Instruction::ANC;
			break;

			IM_INSTRUCTION(0x4b, Instruction::ALR)
			IM_INSTRUCTION(0x6b, Instruction::ARR)
			IM_INSTRUCTION(0x8b, Instruction::XAA)
			IM_INSTRUCTION(0xab, Instruction::LAX)
			IM_INSTRUCTION(0xcb, Instruction::AXS)
			IM_INSTRUCTION(0xeb, Instruction::SBC)
			case 0x93: case 0x9f:
				instruction.operation = Instruction::AHX;
			break;
			IM_INSTRUCTION(0x9c, Instruction::SHY)
			IM_INSTRUCTION(0x9e, Instruction::SHX)
			IM_INSTRUCTION(0x9b, Instruction::TAS)
			IM_INSTRUCTION(0xbb, Instruction::LAS)
		}
#undef RM_INSTRUCTION
#undef URM_INSTRUCTION
#undef M_INSTRUCTION
#undef IM_INSTRUCTION

		// get operand
		switch(instruction.addressing_mode)
		{
			// zero-byte operands
			case Instruction::Implied:
				instruction.operand = 0;
			break;

			// one-byte operands
			case Instruction::Immediate:
			case Instruction::ZeroPage: case Instruction::ZeroPageX: case Instruction::ZeroPageY:
			case Instruction::IndexedIndirectX: case Instruction::IndirectIndexedY:
			case Instruction::Relative:
			{
				uint16_t operand_address = address - start_address;
				if(operand_address >= memory.size()) return;
				address++;

				instruction.operand = memory[operand_address];
			}
			break;

			// two-byte operands
			case Instruction::Absolute: case Instruction::AbsoluteX: case Instruction::AbsoluteY:
			case Instruction::Indirect:
			{
				uint16_t operand_address = address - start_address;
				if(operand_address >= memory.size()-1) return;
				address += 2;

				instruction.operand = memory[operand_address] | (uint16_t)(memory[operand_address+1] << 8);
			}
			break;
		}

		// store the instruction away
		disassembly.disassembly.instructions_by_address[instruction.address] = instruction;

		// TODO: something wider-ranging than this
		if(instruction.addressing_mode == Instruction::Absolute && (instruction.operand < start_address || instruction.operand >= start_address + memory.size()))
		{
			if(	instruction.operation == Instruction::STY ||
				instruction.operation == Instruction::STX ||
				instruction.operation == Instruction::STA)
					disassembly.disassembly.external_stores.insert(instruction.operand);
			if(	instruction.operation == Instruction::LDY ||
				instruction.operation == Instruction::LDX ||
				instruction.operation == Instruction::LDA)
					disassembly.disassembly.external_loads.insert(instruction.operand);
		}

		// decide on overall flow control
		if(instruction.operation == Instruction::RTS || instruction.operation == Instruction::RTI) return;
		if(instruction.operation == Instruction::BRK) return;	// TODO: check whether IRQ vector is within memory range
		if(instruction.operation == Instruction::JSR)
		{
			disassembly.remaining_entry_points.push_back(instruction.operand);
		}
		if(instruction.operation == Instruction::JMP)
		{
			if(instruction.addressing_mode == Instruction::Absolute)
				disassembly.remaining_entry_points.push_back(instruction.operand);
			return;
		}
		if(instruction.addressing_mode == Instruction::Relative)
		{
			uint16_t destination = (uint16_t)(address + (int8_t)instruction.operand);
			disassembly.remaining_entry_points.push_back(destination);
		}
	}
}

Disassembly StaticAnalyser::MOS6502::Disassemble(const std::vector<uint8_t> &memory, uint16_t start_address, std::vector<uint16_t> entry_points)
{
	PartialDisassembly partialDisassembly;
	partialDisassembly.remaining_entry_points = entry_points;

	while(!partialDisassembly.remaining_entry_points.empty())
	{
		// pull the next entry point from the back of the vector
		uint16_t next_entry_point = partialDisassembly.remaining_entry_points.back();
		partialDisassembly.remaining_entry_points.pop_back();

		// if that address has already bene visited, forget about it
		if(partialDisassembly.disassembly.instructions_by_address.find(next_entry_point) != partialDisassembly.disassembly.instructions_by_address.end()) continue;

		// if it's outgoing, log it as such and forget about it; otherwise disassemble
		if(next_entry_point < start_address || next_entry_point >= start_address + memory.size())
			partialDisassembly.disassembly.outward_calls.insert(next_entry_point);
		else
			AddToDisassembly(partialDisassembly, memory, start_address, next_entry_point);
	}

	return std::move(partialDisassembly.disassembly);
}
