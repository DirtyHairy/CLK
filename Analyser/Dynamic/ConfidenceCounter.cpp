//
//  ConfidenceCounter.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "ConfidenceCounter.hpp"

using namespace DynamicAnalyser;

float ConfidenceCounter::get_probability() {
	return static_cast<float>(hits_) / static_cast<float>(hits_ + misses_);
}

void ConfidenceCounter::add_hit() {
	hits_++;
}

void ConfidenceCounter::add_miss() {
	misses_++;
}

void ConfidenceCounter::add_equivocal() {
	hits_++;
	misses_++;
}
