//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Atari_ST_Video_hpp
#define Atari_ST_Video_hpp

#include "../../../Outputs/CRT/CRT.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"

namespace Atari {
namespace ST {

enum class FieldFrequency {
	Fifty = 0, Sixty = 1, SeventyTwo = 2
};

class Video {
	public:
		Video();

		/*!
			Sets the target device for video data.
		*/
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);

		/*!
			Produces the next @c duration period of pixels.
		*/
		void run_for(HalfCycles duration);

		/*!
			@returns the number of cycles until there is next a change in the hsync,
			vsync or display_enable outputs.
		*/
		HalfCycles get_next_sequence_point();

		bool hsync();
		bool vsync();
		bool display_enabled();

		void set_ram(uint16_t *, size_t size);

		uint16_t read(int address);
		void write(int address, uint16_t value);

	private:
		Outputs::CRT::CRT crt_;

		uint16_t raw_palette_[16];
		uint16_t palette_[16];
		int base_address_ = 0;
		int current_address_ = 0;

		uint16_t *ram_;
		uint16_t line_buffer_[256];

		int x_ = 0, y_ = 0, next_y_ = 0;

		uint16_t video_mode_ = 0;
		uint16_t sync_mode_ = 0;

		FieldFrequency field_frequency_ = FieldFrequency::Fifty;
		enum class OutputBpp {
			One, Two, Four
		} output_bpp_;
		void update_output_mode();

		struct HorizontalState {
			bool enable = false;
			bool blank = false;
			bool sync = false;
		} horizontal_;
		struct VerticalState {
			bool enable = false;
			bool blank = false;

			enum class SyncSchedule {
				/// No sync events this line.
				None,
				/// Sync should begin during this horizontal line.
				Begin,
				/// Sync should end during this horizontal line.
				End,
			} sync_schedule = SyncSchedule::None;
			bool sync = false;
		} vertical_, next_vertical_;
		int line_length_ = 1024;

		int data_latch_position_ = 0;
		uint16_t data_latch_[4];
		void latch_word();

		class Shifter {
			public:
				Shifter(Outputs::CRT::CRT &crt, uint16_t *palette) : crt_(crt), palette_(palette) {}
				void output_blank(int duration);
				void output_sync(int duration);
				void output_border(int duration, OutputBpp bpp);
				void output_pixels(int duration, OutputBpp bpp);

				void load(uint64_t value);

			private:
				int duration_ = 0;
				enum class OutputMode {
					Sync, Blank, Border, Pixels
				} output_mode_ = OutputMode::Sync;
				uint16_t border_colour_;
				OutputBpp bpp_;
				union {
					uint64_t output_shifter_;
					uint32_t shifter_halves_[2];
				};

				void flush_output(OutputMode next_mode);

				uint16_t *pixel_buffer_;
				size_t pixel_pointer_ = 0;

				Outputs::CRT::CRT &crt_;
				uint16_t *palette_ = nullptr;
		} shifter_;
};

}
}

#endif /* Atari_ST_Video_hpp */
