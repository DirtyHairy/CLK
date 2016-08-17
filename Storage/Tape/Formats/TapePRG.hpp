//
//  TapePRG.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef TapePRG_hpp
#define TapePRG_hpp

#include "../Tape.hpp"
#include <stdint.h>

namespace Storage {

/*!
	Provides a @c Tape containing a .PRG, which is a direct local file.
*/
class TapePRG: public Tape {
	public:
		/*!
			Constructs a @c T64 containing content from the file with name @c file_name, of type @c type.

			@param file_name The name of the file to load.
			@param type The type of data the file should contain.
			@throws ErrorBadFormat if this file could not be opened and recognised as the specified type.
		*/
		TapePRG(const char *file_name);
		~TapePRG();

		enum {
			ErrorBadFormat
		};

		// implemented to satisfy @c Tape
		Pulse get_next_pulse();
		void reset();
	private:
		FILE *_file;
		uint16_t _load_address;
		uint16_t _length;

		enum FilePhase {
			FilePhaseLeadIn,
			FilePhaseHeader,
			FilePhaseHeaderDataGap,
			FilePhaseData,
		} _filePhase;
		int _phaseOffset;

		int _bitPhase;
		enum OutputToken {
			Leader,
			Zero,
			One,
			WordMarker,
			EndOfBlock
		} _outputToken;
		void get_next_output_token();
		uint8_t _output_byte;
		uint8_t _check_digit;
		uint8_t _copy_mask;
};

}

#endif /* T64_hpp */
