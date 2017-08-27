//
//  Atari2600.m
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "CSAtari2600.h"

#include "Atari2600.hpp"
#import "CSMachine+Subclassing.h"

@implementation CSAtari2600 {
	std::unique_ptr<Atari2600::Machine> _atari2600;
}

- (instancetype)init {
	Atari2600::Machine *machine = Atari2600::Machine::Atari2600();
	self = [super initWithMachine:machine];
	if(self) {
		_atari2600.reset(machine);
	}
	return self;
}

- (void)setDirection:(CSJoystickDirection)direction onPad:(NSUInteger)pad isPressed:(BOOL)isPressed {
	Atari2600DigitalInput input;
	switch(direction)
	{
		case CSJoystickDirectionUp:		input = pad ? Atari2600DigitalInputJoy2Up : Atari2600DigitalInputJoy1Up;		break;
		case CSJoystickDirectionDown:	input = pad ? Atari2600DigitalInputJoy2Down : Atari2600DigitalInputJoy1Down;	break;
		case CSJoystickDirectionLeft:	input = pad ? Atari2600DigitalInputJoy2Left : Atari2600DigitalInputJoy1Left;	break;
		case CSJoystickDirectionRight:	input = pad ? Atari2600DigitalInputJoy2Right : Atari2600DigitalInputJoy1Right;	break;
	}
	@synchronized(self) {
		_atari2600->set_digital_input(input, isPressed ? true : false);
	}
}

- (void)setButtonAtIndex:(NSUInteger)button onPad:(NSUInteger)pad isPressed:(BOOL)isPressed {
	@synchronized(self) {
		_atari2600->set_digital_input(pad ? Atari2600DigitalInputJoy2Fire : Atari2600DigitalInputJoy1Fire, isPressed ? true : false);
	}
}

- (void)setResetLineEnabled:(BOOL)enabled {
	@synchronized(self) {
		_atari2600->set_reset_switch(enabled ? true : false);
	}
}

- (void)setupOutputWithAspectRatio:(float)aspectRatio {
	@synchronized(self) {
		[super setupOutputWithAspectRatio:aspectRatio];
	}
}

#pragma mark - Switches

- (void)setColourButton:(BOOL)colourButton {
	_colourButton = colourButton;
	@synchronized(self) {
		_atari2600->set_switch_is_enabled(Atari2600SwitchColour, colourButton);
	}
}

- (void)setLeftPlayerDifficultyButton:(BOOL)leftPlayerDifficultyButton {
	_leftPlayerDifficultyButton = leftPlayerDifficultyButton;
	@synchronized(self) {
		_atari2600->set_switch_is_enabled(Atari2600SwitchLeftPlayerDifficulty, leftPlayerDifficultyButton);
	}
}

- (void)setRightPlayerDifficultyButton:(BOOL)rightPlayerDifficultyButton {
	_rightPlayerDifficultyButton = rightPlayerDifficultyButton;
	@synchronized(self) {
		_atari2600->set_switch_is_enabled(Atari2600SwitchRightPlayerDifficulty, rightPlayerDifficultyButton);
	}
}

- (void)toggleSwitch:(Atari2600Switch)toggleSwitch {
	@synchronized(self) {
		_atari2600->set_switch_is_enabled(toggleSwitch, true);
	}
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
		@synchronized(self) {
			_atari2600->set_switch_is_enabled(toggleSwitch, false);
		}
	});
}

- (void)pressResetButton {
	[self toggleSwitch:Atari2600SwitchReset];
}

- (void)pressSelectButton {
	[self toggleSwitch:Atari2600SwitchSelect];
}

- (NSString *)userDefaultsPrefix {	return @"atari2600";	}

@end
