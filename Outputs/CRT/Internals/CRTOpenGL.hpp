//
//  CRTOpenGL.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/02/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTOpenGL_h
#define CRTOpenGL_h

#include "../CRTTypes.hpp"
#include "CRTConstants.hpp"
#include "OpenGL.hpp"
#include "TextureTarget.hpp"
#include "Shader.hpp"

#include "ArrayBuilder.hpp"
#include "TextureBuilder.hpp"

#include "Shaders/OutputShader.hpp"
#include "Shaders/IntermediateShader.hpp"

#include <mutex>
#include <vector>

namespace Outputs {
namespace CRT {

class OpenGLOutputBuilder {
	private:
		// colour information
		ColourSpace _colour_space;
		unsigned int _colour_cycle_numerator;
		unsigned int _colour_cycle_denominator;
		OutputDevice _output_device;

		// timing information to allow reasoning about input information
		unsigned int _input_frequency;
		unsigned int _cycles_per_line;
		unsigned int _height_of_display;
		unsigned int _horizontal_scan_period;
		unsigned int _vertical_scan_period;
		unsigned int _vertical_period_divider;

		// The user-supplied visible area
		Rect _visible_area;

		// Other things the caller may have provided.
		char *_composite_shader;
		char *_rgb_shader;

		// Methods used by the OpenGL code
		void prepare_output_shader();
		void prepare_rgb_input_shaders();
		void prepare_composite_input_shaders();

		void prepare_output_vertex_array();
		void prepare_source_vertex_array();

		// the run and input data buffers
		std::unique_ptr<std::mutex> _output_mutex;
		std::unique_ptr<std::mutex> _draw_mutex;

		// transient buffers indicating composite data not yet decoded
		GLsizei _composite_src_output_y;

		std::unique_ptr<OpenGL::OutputShader> output_shader_program;
		std::unique_ptr<OpenGL::IntermediateShader> composite_input_shader_program, composite_separation_filter_program, composite_y_filter_shader_program, composite_chrominance_filter_shader_program;
		std::unique_ptr<OpenGL::IntermediateShader> rgb_input_shader_program, rgb_filter_shader_program;

		std::unique_ptr<OpenGL::TextureTarget> compositeTexture;	// receives raw composite levels
		std::unique_ptr<OpenGL::TextureTarget> separatedTexture;	// receives unfiltered Y in the R channel plus unfiltered but demodulated chrominance in G and B
		std::unique_ptr<OpenGL::TextureTarget> filteredYTexture;	// receives filtered Y in the R channel plus unfiltered chrominance in G and B
		std::unique_ptr<OpenGL::TextureTarget> filteredTexture;		// receives filtered YIQ or YUV

		std::unique_ptr<OpenGL::TextureTarget> framebuffer;			// the current pixel output

		GLuint output_vertex_array;
		GLuint source_vertex_array;

		unsigned int _last_output_width, _last_output_height;

		GLuint shadowMaskTextureName;

		GLuint defaultFramebuffer;

		void set_timing_uniforms();
		void set_colour_space_uniforms();

		void establish_OpenGL_state();
		void reset_all_OpenGL_state();

	public:
		TextureBuilder texture_builder;
		ArrayBuilder array_builder;

		OpenGLOutputBuilder(size_t bytes_per_pixel);
		~OpenGLOutputBuilder();

		inline void set_colour_format(ColourSpace colour_space, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator)
		{
			_output_mutex->lock();
			_colour_space = colour_space;
			_colour_cycle_numerator = colour_cycle_numerator;
			_colour_cycle_denominator = colour_cycle_denominator;
			set_colour_space_uniforms();
			_output_mutex->unlock();
		}

		inline void set_visible_area(Rect visible_area)
		{
			_visible_area = visible_area;
		}

		inline void lock_output()
		{
			_output_mutex->lock();
		}

		inline void unlock_output()
		{
			_output_mutex->unlock();
		}

		inline OutputDevice get_output_device()
		{
			return _output_device;
		}

		inline uint16_t get_composite_output_y()
		{
			return (uint16_t)_composite_src_output_y;
		}

		inline bool composite_output_buffer_is_full()
		{
			return _composite_src_output_y == IntermediateBufferHeight;
		}

		inline void increment_composite_output_y()
		{
			if(!composite_output_buffer_is_full())
				_composite_src_output_y++;
		}

		void draw_frame(unsigned int output_width, unsigned int output_height, bool only_if_dirty);
		void set_openGL_context_will_change(bool should_delete_resources);
		void set_composite_sampling_function(const char *shader);
		void set_rgb_sampling_function(const char *shader);
		void set_output_device(OutputDevice output_device);
		void set_timing(unsigned int input_frequency, unsigned int cycles_per_line, unsigned int height_of_display, unsigned int horizontal_scan_period, unsigned int vertical_scan_period, unsigned int vertical_period_divider);

		GLsync _fence;
};

}
}

#endif /* CRTOpenGL_h */
