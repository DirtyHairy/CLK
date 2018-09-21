//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/09/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

Analyser::Static::TargetList Analyser::Static::Sega::GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms) {
	TargetList targets;
	std::unique_ptr<Target> target(new Target);
	target->machine = Machine::MasterSystem;
	target->media.cartridges = media.cartridges;
	if(!target->media.empty())
		targets.push_back(std::move(target));
	return targets;
}
