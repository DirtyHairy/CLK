//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Video_hpp
#define Video_hpp

#include "../../../Outputs/CRT/CRT.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "DeferredAudio.hpp"

namespace Apple {
namespace Macintosh {

class Video {
	public:
		Video(uint16_t *ram, DeferredAudio &audio);
		void set_scan_target(Outputs::Display::ScanTarget *scan_target);
		void run_for(HalfCycles duration);

		void set_use_alternate_buffers(bool use_alternate_screen_buffer, bool use_alternate_audio_buffer);

		// TODO: feedback on blanks and syncs.
		bool vsync();
		bool is_outputting();

	private:
		DeferredAudio &audio_;
		uint16_t *ram_ = nullptr;

		Outputs::CRT::CRT crt_;

		HalfCycles frame_position_;

		size_t video_address_ = 0;
		size_t audio_address_ = 0;

		uint8_t *pixel_buffer_ = nullptr;

		bool use_alternate_screen_buffer_ = false;
		bool use_alternate_audio_buffer_ = false;
};

}
}

#endif /* Video_hpp */
