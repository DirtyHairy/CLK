//
//  CSMachine+Subclassing.h
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSMachine.h"
#include "CRT.hpp"
#include "Speaker.hpp"

@interface CSMachine (Subclassing)

- (void)setSpeakerDelegate:(Outputs::Speaker::Delegate *)delegate;
- (void)setCRTDelegate:(Outputs::CRT::Delegate *)delegate;

- (void)doRunForNumberOfCycles:(int)numberOfCycles;
- (void)perform:(dispatch_block_t)action;

- (void)crt:(Outputs::CRT *)crt didEndFrame:(CRTFrame *)frame didDetectVSync:(BOOL)didDetectVSync;

@end
