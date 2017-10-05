//
//  CSBestEffortUpdater.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "CSBestEffortUpdater.h"

#include "BestEffortUpdater.hpp"

struct UpdaterDelegate: public Concurrency::BestEffortUpdater::Delegate {
	__weak id<CSBestEffortUpdaterDelegate> delegate;
	NSLock *delegateLock;

	void update(Concurrency::BestEffortUpdater *updater, int cycles, bool did_skip_previous_update) {
		[delegateLock lock];
		[delegate bestEffortUpdater:nil runForCycles:(NSUInteger)cycles didSkipPreviousUpdate:did_skip_previous_update];
		[delegateLock unlock];
	}
};

@implementation CSBestEffortUpdater {
	Concurrency::BestEffortUpdater _updater;
	UpdaterDelegate _updaterDelegate;
	NSLock *_delegateLock;
}

- (instancetype)init {
	self = [super init];
	if(self) {
		_delegateLock = [[NSLock alloc] init];
		_updaterDelegate.delegateLock = _delegateLock;
		_updater.set_delegate(&_updaterDelegate);
	}
	return self;
}

- (void)dealloc {
	_updater.flush();
}

- (void)update {
	_updater.update();
}

- (void)flush {
	_updater.flush();
}

- (void)setClockRate:(double)clockRate {
	_clockRate = clockRate;
	_updater.set_clock_rate(clockRate);
}

- (void)setDelegate:(id<CSBestEffortUpdaterDelegate>)delegate {
	[_delegateLock lock];
	_updaterDelegate.delegate = delegate;
	[_delegateLock unlock];
}

- (id<CSBestEffortUpdaterDelegate>)delegate {
	id<CSBestEffortUpdaterDelegate> delegate;

	[_delegateLock lock];
	delegate = _updaterDelegate.delegate;
	[_delegateLock unlock];

	return delegate;
}

@end
