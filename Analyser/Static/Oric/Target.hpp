//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/03/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Target_h
#define Target_h

namespace Analyser {
namespace Static {
namespace Oric {

struct Target: public ::Analyser::Static::Target {
	bool use_atmos_rom;
	bool has_microdisc;
};

}
}
}

#endif /* Target_h */
