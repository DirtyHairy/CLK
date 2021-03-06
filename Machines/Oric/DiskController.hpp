//
//  DiskController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef DiskController_h
#define DiskController_h

namespace Oric {

class DiskController: public WD::WD1770 {
	public:
		DiskController(WD::WD1770::Personality personality, int clock_rate, Storage::Disk::Drive::ReadyType ready_type) :
			WD::WD1770(personality), clock_rate_(clock_rate), ready_type_(ready_type) {}

		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int d) {
			const size_t drive = size_t(d);
			if(!drives_[drive]) {
				drives_[drive] = std::make_unique<Storage::Disk::Drive>(clock_rate_, 300, 2, ready_type_);
				if(drive == selected_drive_) set_drive(drives_[drive]);
			}
			drives_[drive]->set_disk(disk);
		}

		enum class PagedItem {
			DiskROM,
			BASIC,
			RAM
		};

		struct Delegate: public WD1770::Delegate {
			virtual void disk_controller_did_change_paged_item(DiskController *controller) = 0;
		};
		inline void set_delegate(Delegate *delegate)	{
			delegate_ = delegate;
			WD1770::set_delegate(delegate);
			if(delegate) delegate->disk_controller_did_change_paged_item(this);
		}
		inline PagedItem get_paged_item() {
			return paged_item_;
		}

	protected:
		std::array<std::shared_ptr<Storage::Disk::Drive>, 4> drives_;
		size_t selected_drive_ = 0;
		void select_drive(size_t drive) {
			if(drive != selected_drive_) {
				selected_drive_ = drive;
				set_drive(drives_[selected_drive_]);
			}
		}
		Delegate *delegate_ = nullptr;

		bool enable_overlay_ram_ = false;
		bool disable_basic_rom_ = false;
		void select_paged_item() {
			PagedItem item = PagedItem::RAM;
			if(!enable_overlay_ram_) {
				item = disable_basic_rom_ ? PagedItem::DiskROM : PagedItem::BASIC;
			}
			set_paged_item(item);
		}

	private:
		PagedItem paged_item_ = PagedItem::DiskROM;
		int clock_rate_;
		Storage::Disk::Drive::ReadyType ready_type_;

		inline void set_paged_item(PagedItem item) {
			if(paged_item_ == item) return;
			paged_item_ = item;
			if(delegate_) {
				delegate_->disk_controller_did_change_paged_item(this);
			}
		}
};


};


#endif /* DiskController_h */
