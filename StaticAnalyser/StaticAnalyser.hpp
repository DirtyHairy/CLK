//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_hpp
#define StaticAnalyser_hpp

#include "../Storage/Tape/Tape.hpp"
#include "../Storage/Disk/Disk.hpp"
#include "../Storage/Cartridge/Cartridge.hpp"

#include <string>
#include <list>
#include <vector>

namespace StaticAnalyser {

enum class Vic20MemoryModel {
	Unexpanded,
	EightKB,
	ThirtyTwoKB
};

enum class Atari2600PagingModel {
	None,
	CommaVid,
	Atari8k,
	Atari16k,
	Atari32k,
	ActivisionStack,
	ParkerBros,
	Tigervision,
	CBSRamPlus,
	MNetwork,
	MegaBoy,
	Pitfall2
};

/*!
	A list of disks, tapes and cartridges plus information about the machine to which to attach them and its configuration,
	and instructions on how to launch the software attached, plus a measure of confidence in this target's correctness.
*/
struct Target {
	enum  {
		Atari2600,
		Electron,
		Vic20,
		Oric,
		ZX80
	} machine;
	float probability;

	union {
		struct {
			bool has_adfs;
			bool has_dfs;
			bool should_shift_restart;
		} acorn;

		struct {
			Atari2600PagingModel paging_model;
			bool uses_superchip;
		} atari;

		struct {
			bool use_atmos_rom;
			bool has_microdisc;
		} oric;

		struct {
			Vic20MemoryModel memory_model;
			bool has_c1540;
		} vic20;
	};

	std::string loadingCommand;

	std::list<std::shared_ptr<Storage::Disk::Disk>> disks;
	std::list<std::shared_ptr<Storage::Tape::Tape>> tapes;
	std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> cartridges;
};

/*!
	Attempts, through any available means, to return a list of potential targets for the file with the given name.
	
	@returns The list of potential targets, sorted from most to least probable.
*/
std::list<Target> GetTargets(const char *file_name);

}

#endif /* StaticAnalyser_hpp */
