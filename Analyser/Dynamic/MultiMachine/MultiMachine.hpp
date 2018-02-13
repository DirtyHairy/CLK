//
//  MultiMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef MultiMachine_hpp
#define MultiMachine_hpp

#include "../../../Machines/DynamicMachine.hpp"

#include "Implementation/MultiConfigurable.hpp"
#include "Implementation/MultiConfigurationTarget.hpp"
#include "Implementation/MultiCRTMachine.hpp"
#include "Implementation/MultiJoystickMachine.hpp"
#include "Implementation/MultiKeyboardMachine.hpp"

#include <memory>
#include <mutex>
#include <vector>

namespace Analyser {
namespace Dynamic {

/*!
	Provides the same interface as to a single machine, while multiplexing all
	underlying calls to an array of real dynamic machines.

	Calls to crt_machine->get_crt will return that for the first machine.

	Following each crt_machine->run_for, reorders the supplied machines by
	confidence.

	If confidence for any machine becomes disproportionately low compared to
	the others in the set, that machine is removed from the array.
*/
class MultiMachine: public ::Machine::DynamicMachine, public MultiCRTMachine::Delegate {
	public:
		MultiMachine(std::vector<std::unique_ptr<DynamicMachine>> &&machines);

		ConfigurationTarget::Machine *configuration_target() override;
		CRTMachine::Machine *crt_machine() override;
		JoystickMachine::Machine *joystick_machine() override;
		KeyboardMachine::Machine *keyboard_machine() override;
		Configurable::Device *configurable_device() override;
		void *raw_pointer() override;

		void multi_crt_did_run_machines() override;


	private:
		std::vector<std::unique_ptr<DynamicMachine>> machines_;
		std::mutex machines_mutex_;

		MultiConfigurable configurable_;
		MultiConfigurationTarget configuration_target_;
		MultiCRTMachine crt_machine_;
		MultiJoystickMachine joystick_machine_;
		MultiKeyboardMachine keyboard_machine_;

		void pick_first();
		bool has_picked_ = false;
};

}
}

#endif /* MultiMachine_hpp */
