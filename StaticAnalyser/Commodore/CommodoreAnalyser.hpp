//
//  CommodoreAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CommodoreAnalyser_hpp
#define CommodoreAnalyser_hpp

#include "../StaticAnalyser.hpp"

namespace StaticAnalyser {
namespace Commodore {

void AddTargets(
	const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
	const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
	const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
	std::list<Target> &destination
);

}
}

#endif /* CommodoreAnalyser_hpp */
