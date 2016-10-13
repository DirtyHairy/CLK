//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "../../Outputs/CRT/CRT.hpp"

namespace Oric {

class VideoOutput {
	public:
		VideoOutput(uint8_t *memory);
		std::shared_ptr<Outputs::CRT::CRT> get_crt();
		void run_for_cycles(int number_of_cycles);

	private:
		uint8_t *_ram;
		std::shared_ptr<Outputs::CRT::CRT> _crt;

		// Counters
		int _counter, _frame_counter;

		// Output state
		enum State {
			Blank, Sync, Pixels
		} _state;
		unsigned int _cycles_in_state;
		uint8_t *_pixel_target;

		// Registers
		uint8_t _ink, _paper;

		int _character_set_base_address;
		inline void set_character_set_base_address()
		{
			if(_is_graphics_mode) _character_set_base_address = _use_alternative_character_set ? 0x9c00 : 0x9800;
			else _character_set_base_address = _use_alternative_character_set ? 0xb800 : 0xb400;
		}

		bool _is_graphics_mode;
		bool _is_sixty_hertz;
		bool _use_alternative_character_set;
		bool _use_double_height_characters;
		bool _blink_text;
};

}

#endif /* Video_hpp */
