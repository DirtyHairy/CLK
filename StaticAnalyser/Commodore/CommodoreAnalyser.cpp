//
//  CommodoreAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CommodoreAnalyser.hpp"

#include "Tape.hpp"

using namespace StaticAnalyser::Commodore;

void StaticAnalyser::Commodore::AddTargets(
	const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
	const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
	const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
	std::list<StaticAnalyser::Target> &destination)
{
	Target target;
	target.machine = Target::Vic20;	// TODO: machine estimation
	target.probability = 1.0; // TODO: a proper estimation

	// strip out inappropriate cartridges
//	target.cartridges = AcornCartridgesFrom(cartridges);

	// if there are any tapes, attempt to get data from the first
	if(tapes.size() > 0)
	{
		std::shared_ptr<Storage::Tape::Tape> tape = tapes.front();
		tape->reset();
		std::list<File> files = GetFiles(tape);
		tape->reset();

		// continue if there are any files
		if(files.size())
		{
			target.tapes = tapes;

			target.vic20.memory_model = Vic20MemoryModel::Unexpanded;
			if(files.front().is_basic())
			{
				target.loadingCommand = "LOAD\"\",1,0\nRUN\n";
			}
			else
			{
				target.loadingCommand = "LOAD\"\",1,1\nRUN\n";
			}

			// make a first guess based on loading address
			switch(files.front().starting_address)
			{
				case 0x1001:
				default: break;
				case 0x1201:
					target.vic20.memory_model = Vic20MemoryModel::ThirtyTwoKB;
				break;
				case 0x0401:
					target.vic20.memory_model = Vic20MemoryModel::EightKB;
				break;
			}

			// General approach: increase memory size conservatively such that the largest file found will fit.
			for(File &file : files)
			{
				size_t file_size = file.data.size();
				//bool is_basic = file.is_basic();

				/*if(is_basic)
				{
					// BASIC files may be relocated, so the only limit is size.
					//
					// An unexpanded machine has 3583 bytes free for BASIC;
					// a 3kb expanded machine has 6655 bytes free.
					if(file_size > 6655)
						target.vic20.memory_model = Vic20MemoryModel::ThirtyTwoKB;
					else if(target.vic20.memory_model == Vic20MemoryModel::Unexpanded && file_size > 3583)
						target.vic20.memory_model = Vic20MemoryModel::EightKB;
				}
				else
				{*/
//				if(!file.type == File::NonRelocatableProgram)
//				{
					// Non-BASIC files may be relocatable but, if so, by what logic?
					// Given that this is unknown, take starting address as literal
					// and check against memory windows.
					//
					// (ignoring colour memory...)
					// An unexpanded Vic has memory between 0x0000 and 0x0400; and between 0x1000 and 0x2000.
					// A 3kb expanded Vic fills in the gap and has memory between 0x0000 and 0x2000.
					// A 32kb expanded Vic has memory in the entire low 32kb.
					uint16_t starting_address = file.starting_address;

					// If anything above the 8kb mark is touched, mark as a 32kb machine; otherwise if the
					// region 0x0400 to 0x1000 is touched and this is an unexpanded machine, mark as 3kb.
					if(starting_address + file_size > 0x2000)
						target.vic20.memory_model = Vic20MemoryModel::ThirtyTwoKB;
					else if(target.vic20.memory_model == Vic20MemoryModel::Unexpanded && !(starting_address >= 0x1000 || starting_address+file_size < 0x0400))
						target.vic20.memory_model = Vic20MemoryModel::ThirtyTwoKB;
//				}
			//	}
			}
		}
	}

	if(target.tapes.size() || target.cartridges.size() || target.disks.size())
		destination.push_back(target);
}
