//
//  Commodore1540.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Commodore1540_hpp
#define Commodore1540_hpp

#include "../../../Processors/6502/CPU6502.hpp"
#include "../../../Components/6522/6522.hpp"

#include "../SerialBus.hpp"

#include "../../../Storage/Disk/Disk.hpp"
#include "../../../Storage/Disk/DiskController.hpp"

namespace Commodore {
namespace C1540 {

/*!
	An implementation of the serial-port VIA in a Commodore 1540 — the VIA that facilitates all
	IEC bus communications.

	It is wired up such that Port B contains:
		Bit 0:		data input; 1 if the line is low, 0 if it is high;
		Bit 1:		data output; 1 if the line should be low, 0 if it should be high;
		Bit 2:		clock input; 1 if the line is low, 0 if it is high;
		Bit 3:		clock output; 1 if the line is low, 0 if it is high;
		Bit 4:		attention acknowledge output; exclusive ORd with the attention input and ORd onto the data output;
		Bits 5/6:	device select input; the 1540 will act as device 8 + [value of bits]
		Bit 7:		attention input; 1 if the line is low, 0 if it is high

	The attention input is also connected to CA1, similarly inverted — the CA1 wire will be high when the bus is low and vice versa.
*/
class SerialPortVIA: public MOS::MOS6522<SerialPortVIA>, public MOS::MOS6522IRQDelegate {
	public:
		using MOS6522IRQDelegate::set_interrupt_status;

		SerialPortVIA();

		uint8_t get_port_input(Port port);

		void set_port_output(Port port, uint8_t value, uint8_t mask);
		void set_serial_line_state(::Commodore::Serial::Line line, bool value);

		void set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serialPort);

	private:
		uint8_t _portB;
		std::weak_ptr<::Commodore::Serial::Port> _serialPort;
		bool _attention_acknowledge_level, _attention_level_input, _data_level_output;

		void update_data_line();
};

/*!
	An implementation of the drive VIA in a Commodore 1540 — the VIA that is used to interface with the disk.

	It is wired up such that Port B contains:
		Bits 0/1:	head step direction
		Bit 2:		motor control
		Bit 3:		LED control (TODO)
		Bit 4:		write protect photocell status (TODO)
		Bits 5/6:	read/write density
		Bit 7:		0 if sync marks are currently being detected, 1 otherwise.

	... and Port A contains the byte most recently read from the disk or the byte next to write to the disk, depending on data direction.

	It is implied that CA2 might be used to set processor overflow, CA1 a strobe for data input, and one of the CBs being definitive on
	whether the disk head is being told to read or write, but it's unclear and I've yet to investigate. So, TODO.
*/
class DriveVIA: public MOS::MOS6522<DriveVIA>, public MOS::MOS6522IRQDelegate {
	public:
		class Delegate {
			public:
				virtual void drive_via_did_step_head(void *driveVIA, int direction) = 0;
				virtual void drive_via_did_set_data_density(void *driveVIA, int density) = 0;
		};
		void set_delegate(Delegate *delegate);

		using MOS6522IRQDelegate::set_interrupt_status;

		DriveVIA();

		uint8_t get_port_input(Port port);

		void set_sync_detected(bool sync_detected);
		void set_data_input(uint8_t value);
		bool get_should_set_overflow();
		bool get_motor_enabled();

		void set_control_line_output(Port port, Line line, bool value);

		void set_port_output(Port port, uint8_t value, uint8_t direction_mask);

	private:
		uint8_t _port_b, _port_a;
		bool _should_set_overflow;
		bool _drive_motor;
		uint8_t _previous_port_b_output;
		Delegate *_delegate;
};

/*!
	An implementation of the C1540's serial port; this connects incoming line levels to the serial-port VIA.
*/
class SerialPort : public ::Commodore::Serial::Port {
	public:
		void set_input(::Commodore::Serial::Line line, ::Commodore::Serial::LineLevel level);
		void set_serial_port_via(std::shared_ptr<SerialPortVIA> serialPortVIA);

	private:
		std::weak_ptr<SerialPortVIA> _serialPortVIA;
};

/*!
	Provides an emulation of the C1540.
*/
class Machine:
	public CPU6502::Processor<Machine>,
	public MOS::MOS6522IRQDelegate::Delegate,
	public DriveVIA::Delegate,
	public Storage::Disk::Controller {

	public:
		Machine();

		/*!
			Sets the ROM image to use for this drive; it is assumed that the buffer provided will be at least 16 kb in size.
		*/
		void set_rom(const uint8_t *rom);

		/*!
			Sets the serial bus to which this drive should attach itself.
		*/
		void set_serial_bus(std::shared_ptr<::Commodore::Serial::Bus> serial_bus);

		void run_for_cycles(int number_of_cycles);
		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk);

		// to satisfy CPU6502::Processor
		unsigned int perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value);

		// to satisfy MOS::MOS6522::Delegate
		virtual void mos6522_did_change_interrupt_status(void *mos6522);

		// to satisfy DriveVIA::Delegate
		void drive_via_did_step_head(void *driveVIA, int direction);
		void drive_via_did_set_data_density(void *driveVIA, int density);

	private:
		uint8_t _ram[0x800];
		uint8_t _rom[0x4000];

		std::shared_ptr<SerialPortVIA> _serialPortVIA;
		std::shared_ptr<SerialPort> _serialPort;
		DriveVIA _driveVIA;

		std::shared_ptr<Storage::Disk::Disk> _disk;

		int _shift_register, _bit_window_offset;
		virtual void process_input_bit(int value, unsigned int cycles_since_index_hole);
		virtual void process_index_hole();
};

}
}

#endif /* Commodore1540_hpp */
