//
//  DisplayMetrics.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef DisplayMetrics_hpp
#define DisplayMetrics_hpp

#include "ScanTarget.hpp"
#include <chrono>

namespace Outputs {
namespace Display {

/*!
	A class to derive various metrics about the input to a ScanTarget,
	based purely on empirical observation. In particular it is intended
	to allow for host-client frame synchronisation.
*/
class Metrics {
	public:
		void announce_event(ScanTarget::Event event);

		void announce_did_resize();
		void announce_draw_status(size_t lines, std::chrono::high_resolution_clock::duration duration, bool complete);
};

}
}

#endif /* DisplayMetrics_hpp */
