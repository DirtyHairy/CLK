//
//  DiskController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Disk_Controller_hpp
#define Storage_Disk_Controller_hpp

#include "Drive.hpp"
#include "DigitalPhaseLockedLoop.hpp"
#include "PCMSegment.hpp"
#include "PCMPatchedTrack.hpp"

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/Sleeper.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides the shell for emulating a disk controller — something that is connected to a disk drive and uses a
	phase locked loop ('PLL') to decode a bit stream from the surface of the disk.

	Partly abstract; it is expected that subclasses will provide methods to deal with receiving a newly-recognised
	bit from the PLL and with crossing the index hole.

	TODO: communication of head size and permissible stepping extents, appropriate simulation of gain.
*/
class Controller: public DigitalPhaseLockedLoop::Delegate, public Drive::EventDelegate, public Sleeper, public Sleeper::SleepObserver {
	protected:
		/*!
			Constructs a @c DiskDrive that will be run at @c clock_rate and runs its PLL at @c clock_rate*clock_rate_multiplier,
			spinning inserted disks at @c revolutions_per_minute.
		*/
		Controller(Cycles clock_rate, int clock_rate_multiplier, int revolutions_per_minute);

		/*!
			Communicates to the PLL the expected length of a bit as a fraction of a second.
		*/
		void set_expected_bit_length(Time bit_length);

		/*!
			Advances the drive by @c number_of_cycles cycles.
		*/
		void run_for(const Cycles cycles);

		/*!
			Sets the current drive. This drive is the one the PLL listens to.
		*/
		void set_drive(std::shared_ptr<Drive> drive);

		/*!
			Should be implemented by subclasses; communicates each bit that the PLL recognises.
		*/
		virtual void process_input_bit(int value) = 0;

		/*!
			Should be implemented by subclasses; communicates that the index hole has been reached.
		*/
		virtual void process_index_hole() = 0;

		/*!
			Should be implemented by subclasses if they implement writing; communicates that
			all bits supplied to write_bit have now been written.
		*/
		virtual void process_write_completed();

		/*!
			Puts the drive returned by get_drive() into write mode, supplying the current bit length.

			@param clamp_to_index_hole If @c true then writing will automatically be truncated by
			the index hole. Writing will continue over the index hole otherwise.
		*/
		void begin_writing(bool clamp_to_index_hole);

		/*!
			Returns the connected drive or, if none is connected, an invented one. No guarantees are
			made about the lifetime or the exclusivity of the invented drive.
		*/
		Drive &get_drive();

		bool is_sleeping();

	private:
		Time bit_length_;
		int clock_rate_;
		int clock_rate_multiplier_;
		Time rotational_multiplier_;

		std::shared_ptr<DigitalPhaseLockedLoop> pll_;
		std::shared_ptr<Drive> drive_;

		std::shared_ptr<Drive> empty_drive_;

		void set_component_is_sleeping(void *component, bool is_sleeping);

		// for Drive::EventDelegate
		void process_event(const Track::Event &event);
		void advance(const Cycles cycles);

		// to satisfy DigitalPhaseLockedLoop::Delegate
		void digital_phase_locked_loop_output_bit(int value);
};

}
}

#endif /* DiskDrive_hpp */
