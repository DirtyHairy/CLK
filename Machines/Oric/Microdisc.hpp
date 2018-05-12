//
//  Microdisc.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Microdisc_hpp
#define Microdisc_hpp

#include "../../Components/1770/1770.hpp"
#include "../../Activity/Observer.hpp"

#include <array>

namespace Oric {

class Microdisc: public WD::WD1770 {
	public:
		Microdisc();

		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive);
		void set_control_register(uint8_t control);
		uint8_t get_interrupt_request_register();
		uint8_t get_data_request_register();

		bool get_interrupt_request_line();

		void run_for(const Cycles cycles);
		using WD::WD1770::run_for;

		enum PagingFlags {
			BASICDisable	=	(1 << 0),
			MicrodscDisable	=	(1 << 1)
		};

		class Delegate: public WD1770::Delegate {
			public:
				virtual void microdisc_did_change_paging_flags(Microdisc *microdisc) = 0;
		};
		inline void set_delegate(Delegate *delegate)	{	delegate_ = delegate;	WD1770::set_delegate(delegate);	}
		inline int get_paging_flags()					{	return paging_flags_;									}

		void set_activity_observer(Activity::Observer *observer);

	private:
		void set_control_register(uint8_t control, uint8_t changes);
		void set_head_load_request(bool head_load);
		bool get_drive_is_ready();
		std::array<std::shared_ptr<Storage::Disk::Drive>, 4> drives_;
		size_t selected_drive_;
		bool irq_enable_ = false;
		int paging_flags_ = BASICDisable;
		int head_load_request_counter_ = -1;
		bool head_load_request_ = false;
		Delegate *delegate_ = nullptr;
		uint8_t last_control_ = 0;
		Activity::Observer *observer_ = nullptr;
};

}

#endif /* Microdisc_hpp */
