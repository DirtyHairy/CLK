//
//  CRT.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"
#include <stdarg.h>

static const int bufferWidth = 512;
static const int bufferHeight = 512;

static const int syncCapacityLineChargeThreshold = 3;
static const int millisecondsHorizontalRetraceTime = 16;
static const int scanlinesVerticalRetraceTime = 26;

using namespace Outputs;

CRT::CRT(int cycles_per_line, int height_of_display, int number_of_buffers, ...)
{
	// store fundamental display configuration properties
	_height_of_display = height_of_display;
	_cycles_per_line = cycles_per_line;

	// generate timing values implied by the given arbuments
	_hsync_error_window = cycles_per_line >> 5;

	// generate buffers for signal storage as requested — format is
	// number of buffers, size of buffer 1, size of buffer 2...
	_numberOfBuffers = number_of_buffers;
	_bufferSizes = new int[_numberOfBuffers];
	_buffers = new uint8_t *[_numberOfBuffers];

	va_list va;
	va_start(va, number_of_buffers);
	for(int c = 0; c < _numberOfBuffers; c++)
	{
		_bufferSizes[c] = va_arg(va, int);
		_buffers[c] = new uint8_t[bufferHeight * bufferWidth * _bufferSizes[c]];
	}
	va_end(va);

	// reset pointer into output buffers
	_write_allocation_pointer = 0;

	// reset the run buffer pointer
	_run_pointer = 0;

	// reset raster position
	_horizontalOffset = 0.0f;
	_verticalOffset = 0.0f;

	// reset flywheel sync
	_expected_next_hsync = cycles_per_line;
	_horizontal_counter = 0;

	// reset the vertical charge capacitor
	_sync_capacitor_charge_level = 0;

	// start off not in horizontal sync, not receiving a sync signal
	_is_receiving_sync = false;
	_is_in_hsync = false;
}

CRT::~CRT()
{
	delete[] _bufferSizes;
	for(int c = 0; c < _numberOfBuffers; c++)
	{
		delete[] _buffers[c];
	}
	delete[] _buffers;
}

#pragma mark - Sync loop

CRT::SyncEvent CRT::advance_to_next_sync_event(bool hsync_is_requested, bool vsync_is_charging, int cycles_to_run_for, int *cycles_advanced)
{
	// do we recognise this hsync, thereby adjusting time expectations?
	if ((_horizontal_counter < _hsync_error_window || _horizontal_counter >= _expected_next_hsync - _hsync_error_window) && hsync_is_requested) {
		_did_detect_hsync = true;

		int time_now = (_horizontal_counter < _hsync_error_window) ? _expected_next_hsync + _horizontal_counter : _horizontal_counter;
		_expected_next_hsync = (_expected_next_hsync + time_now) >> 1;
//		printf("to %d for %d\n", _expected_next_hsync, time_now);
	}

	SyncEvent proposedEvent = SyncEvent::None;
	int proposedSyncTime = cycles_to_run_for;

	// have we overrun the maximum permitted number of horizontal syncs for this frame?
	if (_hsync_counter > _height_of_display + 10) {
		*cycles_advanced = 0;
		return SyncEvent::StartHSync;
	}

	// will we end an ongoing hsync?
	const int endOfHSyncTime = (millisecondsHorizontalRetraceTime*_cycles_per_line) >> 6;
	if (_horizontal_counter < endOfHSyncTime && _horizontal_counter+proposedSyncTime >= endOfHSyncTime) {
		proposedSyncTime = endOfHSyncTime - _horizontal_counter;
		proposedEvent = SyncEvent::EndHSync;
	}

	// will we start an hsync?
	if (_horizontal_counter + proposedSyncTime >= _expected_next_hsync) {
		proposedSyncTime = _expected_next_hsync - _horizontal_counter;
		proposedEvent = SyncEvent::StartHSync;
	}

	// will an acceptable vertical sync be triggered?
	if (vsync_is_charging && !_vretrace_counter) {
		const int startOfVSyncTime = syncCapacityLineChargeThreshold*_cycles_per_line;

		 if (_sync_capacitor_charge_level < startOfVSyncTime && _sync_capacitor_charge_level + proposedSyncTime >= startOfVSyncTime) {
			proposedSyncTime = startOfVSyncTime - _sync_capacitor_charge_level;
			proposedEvent = SyncEvent::StartVSync;
		 }
	}

	// will an ongoing vertical sync end?
	if (_vretrace_counter > 0) {
		if (_vretrace_counter < proposedSyncTime) {
			proposedSyncTime = _vretrace_counter;
			proposedEvent = SyncEvent::EndVSync;
		}
	}

	*cycles_advanced = proposedSyncTime;
	return proposedEvent;
}

void CRT::advance_cycles(int number_of_cycles, bool hsync_requested, const bool vsync_charging, const CRTRun::Type type, const char *data_type)
{
	// this is safe to keep locally because it accumulates over this run of cycles only
	int buffer_offset = 0;

	while(number_of_cycles) {

		// get the next sync event and its timing; hsync request is instantaneous (being edge triggered) so
		// set it to false for the next run through this loop (if any)
		int next_run_length;
		SyncEvent next_event = advance_to_next_sync_event(hsync_requested, vsync_charging, number_of_cycles, &next_run_length);
		hsync_requested = false;

		// get a run from the allocated list, allocating more if we're about to overrun
		if(_run_pointer >= _all_runs.size())
		{
			_all_runs.resize((_all_runs.size() * 2)+1);
		}

		CRTRun *nextRun = &_all_runs[_run_pointer];
		_run_pointer++;

		// set the type, initial raster position and type of this run
		nextRun->type = type;
		nextRun->start_point.dst_x = _horizontalOffset;
		nextRun->start_point.dst_y = _verticalOffset;
		nextRun->data_type = data_type;

		// if this is a data or level run then store a starting data position
		if(type == CRTRun::Type::Data || type == CRTRun::Type::Level)
		{
			nextRun->start_point.src_x = (_write_target_pointer + buffer_offset) & (bufferWidth - 1);
			nextRun->start_point.dst_x = (_write_target_pointer + buffer_offset) / bufferWidth;
		}

		// advance the raster position as dictated by current sync status
		if (_vretrace_counter > 0)
		{
			_verticalOffset = std::max(0.0f, _verticalOffset - (float)number_of_cycles / (float)(scanlinesVerticalRetraceTime * _cycles_per_line));
		}
		else
		{
			_verticalOffset = std::min(1.0f, _verticalOffset + (float)number_of_cycles / (float)(_height_of_display * _cycles_per_line));
		}

		if (_is_in_hsync)
		{
			_horizontalOffset = std::max(0.0f, _horizontalOffset - (float)(((millisecondsHorizontalRetraceTime * _cycles_per_line) >> 6) * number_of_cycles) / (float)_cycles_per_line);
		}
		else
		{
			_horizontalOffset = std::min(1.0f, _horizontalOffset + (float)((((64 - millisecondsHorizontalRetraceTime) * _cycles_per_line) >> 6) * number_of_cycles) / (float)_cycles_per_line);
		}

		// store the final raster position
		nextRun->end_point.dst_x = _horizontalOffset;
		nextRun->end_point.dst_y = _verticalOffset;

		// if this is a data run then advance the buffer pointer
		if(type == CRTRun::Type::Data)
		{
			buffer_offset += next_run_length;
		}

		// if this is a data or level run then store the end point
		if(type == CRTRun::Type::Data || type == CRTRun::Type::Level)
		{
			nextRun->end_point.src_x = (_write_target_pointer + buffer_offset) & (bufferWidth - 1);
			nextRun->end_point.dst_x = (_write_target_pointer + buffer_offset) / bufferWidth;
		}

		// decrement the number of cycles left to run for and increment the
		// horizontal counter appropriately
		number_of_cycles -= next_run_length;
		_horizontal_counter += next_run_length;

		// either charge or deplete the vertical retrace capacitor (making sure it stops at 0)
		if (vsync_charging)
			_sync_capacitor_charge_level += next_run_length;
		else
			_sync_capacitor_charge_level = std::max(_sync_capacitor_charge_level - next_run_length, 0);

		// decrement the vertical retrace counter, making sure it stops at 0
		_vretrace_counter = std::max(_vretrace_counter - next_run_length, 0);

		// react to the incoming event...
		switch(next_event) {

			// start of hsync: zero the scanline counter, note that we're now in
			// horizontal sync, increment the lines-in-this-frame counter
			case SyncEvent::StartHSync:
				_horizontal_counter = 0;
				_is_in_hsync = true;
				_hsync_counter++;
			break;

			// end of horizontal sync: update the flywheel's velocity, note that we're no longer
			// in horizontal sync
			case SyncEvent::EndHSync:
				if (!_did_detect_hsync) {
					_expected_next_hsync = (_expected_next_hsync + (_hsync_error_window >> 1) + _cycles_per_line) >> 1;
				}
				_did_detect_hsync = false;
				_is_in_hsync = false;
			break;

			// start of vertical sync: reset the lines-in-this-frame counter,
			// load the retrace counter with the amount of time it'll take to retrace
			case SyncEvent::StartVSync:
				_vretrace_counter = scanlinesVerticalRetraceTime * _cycles_per_line;
				_hsync_counter = 0;
			break;

			// end of vertical sync: tell the delegate that we finished vertical sync,
			// releasing all runs back into the common pool
			case SyncEvent::EndVSync:
				if(_delegate != nullptr)
					_delegate->crt_did_start_vertical_retrace_with_runs(&_all_runs[0], _run_pointer);
				_run_pointer = 0;
			break;

			default:		break;
		}
	}
}

#pragma mark - delegate

void CRT::set_crt_delegate(CRTDelegate *delegate)
{
	_delegate = delegate;
}

#pragma mark - stream feeding methods

/*
	These all merely channel into advance_cycles, supplying appropriate arguments
*/
void CRT::output_sync(int number_of_cycles)
{
	bool _hsync_requested = !_is_receiving_sync;	// ensure this really is edge triggered; someone calling output_sync twice in succession shouldn't trigger it twice
	_is_receiving_sync = true;
	advance_cycles(number_of_cycles, _hsync_requested, true, CRTRun::Type::Sync, nullptr);
}

void CRT::output_blank(int number_of_cycles)
{
	_is_receiving_sync = false;
	advance_cycles(number_of_cycles, false, false, CRTRun::Type::Blank, nullptr);
}

void CRT::output_level(int number_of_cycles, const char *type)
{
	_is_receiving_sync = false;
	advance_cycles(number_of_cycles, false, false, CRTRun::Type::Level, type);
}

void CRT::output_data(int number_of_cycles, const char *type)
{
	_is_receiving_sync = false;
	advance_cycles(number_of_cycles, false, false, CRTRun::Type::Data, type);
}

#pragma mark - Buffer supply

void CRT::allocate_write_area(int required_length)
{
	int xPos = _write_allocation_pointer & (bufferWidth - 1);
	if (xPos + required_length > bufferWidth)
	{
		_write_allocation_pointer &= ~(bufferWidth - 1);
		_write_allocation_pointer = (_write_allocation_pointer + bufferWidth) & ((bufferHeight-1) * bufferWidth);
	}

	_write_target_pointer = _write_allocation_pointer;
	_write_allocation_pointer += required_length;
}

uint8_t *CRT::get_write_target_for_buffer(int buffer)
{
	return &_buffers[buffer][_write_target_pointer * _bufferSizes[buffer]];
}
