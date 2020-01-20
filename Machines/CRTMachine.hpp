//
//  CRTMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/05/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef CRTMachine_hpp
#define CRTMachine_hpp

#include "../Outputs/ScanTarget.hpp"
#include "../Outputs/Speaker/Speaker.hpp"
#include "../ClockReceiver/ClockReceiver.hpp"
#include "../ClockReceiver/TimeTypes.hpp"
#include "ROMMachine.hpp"

#include "../Configurable/StandardOptions.hpp"

#include <cmath>

// TODO: rename.
namespace CRTMachine {

/*!
	A CRTMachine::Machine is a mostly-abstract base class for machines that connect to a CRT,
	that optionally provide a speaker, and that nominate a clock rate and can announce to a delegate
	should that clock rate change.
*/
class Machine {
	public:
		/*!
			Causes the machine to set up its display and, if it has one, speaker.

			The @c scan_target will receive all video output; the caller guarantees
			that it is non-null.
		*/
		virtual void set_scan_target(Outputs::Display::ScanTarget *scan_target) = 0;

		/// @returns The speaker that receives this machine's output, or @c nullptr if this machine is mute.
		virtual Outputs::Speaker::Speaker *get_speaker() = 0;

		/// @returns The confidence that this machine is running content it understands.
		virtual float get_confidence() { return 0.5f; }
		virtual std::string debug_type() { return ""; }

		/// Runs the machine for @c duration seconds.
		virtual void run_for(Time::Seconds duration) {
			const double cycles = (duration * clock_rate_) + clock_conversion_error_;
			clock_conversion_error_ = std::fmod(cycles, 1.0);
			run_for(Cycles(static_cast<int>(cycles)));
		}

		/*!
			Runs for the machine for at least @c duration seconds, and then until @c condition is true.

			@returns The amount of time run for.
		*/
		Time::Seconds run_until(Time::Seconds minimum_duration, std::function<bool()> condition) {
			Time::Seconds total_runtime = minimum_duration;
			run_for(minimum_duration);
			while(!condition()) {
				// Advance in increments of one 500th of a second until the condition
				// is true; that's 1/10th of a 50Hz frame, but more like 1/8.33 of a
				// 60Hz frame. Though most machines aren't exactly 50Hz or 60Hz, and some
				// are arbitrary other refresh rates. So those observations are merely
				// for scale.
				run_for(0.002);
				total_runtime += 0.002;
			}
			return total_runtime;
		}

		enum class MachineEvent {
			/// At least one new packet of audio has been delivered to the spaker's delegate.
			NewSpeakerSamplesGenerated
		};

		/*!
			Runs for at least @c duration seconds, and then until @c event has occurred at least once since this
			call to @c run_until_event.

			@returns The amount of time run for.
		*/
		Time::Seconds run_until(Time::Seconds minimum_duration, MachineEvent event) {
			switch(event) {
				case MachineEvent::NewSpeakerSamplesGenerated: {
					const auto speaker = get_speaker();
					if(!speaker) {
						run_for(minimum_duration);
						return minimum_duration;
					}

					const int sample_sets = speaker->completed_sample_sets();
					return run_until(minimum_duration, [sample_sets, speaker]() {
						return speaker->completed_sample_sets() != sample_sets;
					});
				} break;
			}
		}

	protected:
		/// Runs the machine for @c cycles.
		virtual void run_for(const Cycles cycles) = 0;
		void set_clock_rate(double clock_rate) {
			clock_rate_ = clock_rate;
		}
		double get_clock_rate() {
			return clock_rate_;
		}

		/*!
			Maps from Configurable::Display to Outputs::Display::VideoSignal and calls
			@c set_display_type with the result.
		*/
		void set_video_signal_configurable(Configurable::Display type) {
			Outputs::Display::DisplayType display_type;
			switch(type) {
				default:
				case Configurable::Display::RGB:
					display_type = Outputs::Display::DisplayType::RGB;
				break;
				case Configurable::Display::SVideo:
					display_type = Outputs::Display::DisplayType::SVideo;
				break;
				case Configurable::Display::CompositeColour:
					display_type = Outputs::Display::DisplayType::CompositeColour;
				break;
				case Configurable::Display::CompositeMonochrome:
					display_type = Outputs::Display::DisplayType::CompositeMonochrome;
				break;
			}
			set_display_type(display_type);
		}

		/*!
			Forwards the video signal to the target returned by get_crt().
		*/
		virtual void set_display_type(Outputs::Display::DisplayType display_type) {}

	private:
		double clock_rate_ = 1.0;
		double clock_conversion_error_ = 0.0;
};

}

#endif /* CRTMachine_hpp */
