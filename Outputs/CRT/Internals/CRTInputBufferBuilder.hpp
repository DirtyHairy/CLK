//
//  CRTInputBufferBuilder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTInputBufferBuilder_hpp
#define CRTInputBufferBuilder_hpp

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "CRTConstants.hpp"
#include "OpenGL.hpp"
#include <memory.h>

namespace Outputs {
namespace CRT {

struct CRTInputBufferBuilder {
	CRTInputBufferBuilder(size_t bytes_per_pixel);
	~CRTInputBufferBuilder();

	void allocate_write_area(size_t required_length);
	bool reduce_previous_allocation_to(size_t actual_length);

	uint16_t get_and_finalise_current_line();
	uint8_t *get_image_pointer();

	uint8_t *get_write_target();

	uint16_t get_last_write_x_position();

	uint16_t get_last_write_y_position();

	size_t get_bytes_per_pixel();

	private:
		// where pixel data will be put to the next time a write is requested
		uint16_t _next_write_x_position, _next_write_y_position;

		// the most recent position returned for pixel data writing
		uint16_t _write_x_position, _write_y_position;

		// details of the most recent allocation
		size_t _write_target_pointer;
		size_t _last_allocation_amount;

		// the buffer size
		size_t _bytes_per_pixel;

		// Storage for the amount of buffer uploaded so far; initialised correctly by the buffer
		// builder but otherwise entrusted to the CRT to update.
		unsigned int _last_uploaded_line;
		bool _is_full;

		uint8_t *_image;
};

}
}

#endif /* CRTInputBufferBuilder_hpp */
