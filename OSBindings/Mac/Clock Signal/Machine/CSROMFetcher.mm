//
//  CSROMFetcher.m
//  Clock Signal
//
//  Created by Thomas Harte on 01/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>
#include "CSROMFetcher.hpp"

#import "NSBundle+DataResource.h"
#import "NSData+StdVector.h"

#include <string>

ROMMachine::ROMFetcher CSROMFetcher() {
	return [] (const std::string &machine, const std::vector<ROMMachine::ROM> &roms) -> std::vector<std::unique_ptr<std::vector<std::uint8_t>>> {
		NSString *subdirectory = [@"ROMImages/" stringByAppendingString:[NSString stringWithUTF8String:machine.c_str()]];
		std::vector<std::unique_ptr<std::vector<std::uint8_t>>> results;
		for(const auto &rom: roms) {
			NSData *fileData = [[NSBundle mainBundle]
				dataForResource:[NSString stringWithUTF8String:rom.file_name.c_str()]
				withExtension:nil
				subdirectory:subdirectory];

			if(!fileData)
				results.emplace_back(nullptr);
			else {
				std::unique_ptr<std::vector<std::uint8_t>> data(new std::vector<std::uint8_t>);
				*data = fileData.stdVector8;
				results.emplace_back(std::move(data));
			}
		}

		return results;
	};
}
