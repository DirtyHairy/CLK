//
//  ScanTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/10/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Outputs_CRT_ScanTarget_h
#define Outputs_CRT_ScanTarget_h

namespace Outputs {
namespace CRT {

enum class ColourSpace {
	/// YIQ is the NTSC colour space.
	YIQ,

	/// YUV is the PAL colour space.
	YUV
};

/*!
	Provides an abstract target for 'scans' i.e. continuous sweeps of output data,
	which are identified by 2d start and end coordinates, and the PCM-sampled data
	that is output during the sweep.

	Additional information is provided to allow decoding (and/or encoding) of a
	composite colour feed.

	Otherwise helpful: the ScanTarget vends all allocated memory. That should allow
	for use of shared memory where available.
*/
struct ScanTarget {

	/*
		This top section of the interface deals with modal settings. A ScanTarget can
		assume that the modals change very infrequently.
	*/

		struct Modals {
			/*!
				Enumerates the potential formats of input data.
			*/
			enum class DataType {

				// The luminance types can be used to feed only two video pipelines:
				// black and white video, or composite colour.

				Luminance1,				// 1 byte/pixel; any bit set => white; no bits set => black.
				Luminance8,				// 1 byte/pixel; linear scale.

				// The luminance plus phase types describe a luminance and the phase offset
				// of a colour subcarrier. So they can be used to generate a luminance signal,
				// or an s-video pipeline.

				Phase4Luminance4,		// 1 byte/pixel; top nibble is a phase offset, bottom nibble is luminance.
				Phase8Luminance8,		// 1 bytes/pixel; first is phase, second is luminance.

				// The RGB types can directly feed an RGB pipeline, naturally, or can be mapped
				// to phase+luminance, or just to luminance.

				Red1Green1Blue1,		// 1 byte/pixel; bit 0 is blue on or off, bit 1 is green, bit 2 is red.
				Red2Green2Blue2,		// 1 byte/pixel; bits 0 and 1 are blue, bits 2 and 3 are green, bits 4 and 5 are blue.
				Red4Green4Blue4,		// 2 bytes/pixel; first nibble is red, second is green, third is blue.
				Red8Green8Blue8,		// 4 bytes/pixel; first is red, second is green, third is blue, fourth is vacant.
			} source_data_type;

			// If being fed composite data, this defines the colour space in use.
			ColourSpace composite_colour_space;
		};

		/// Sets the total format of input data.
		virtual void set_modals(Modals);


	/*
		This second section of the interface allows provision of the streamed data, plus some control
		over the streaming.
	*/

		/*!
			Defines a scan in terms of its two endpoints.
		*/
		struct Scan {
			struct EndPoint {
				/// Provide the coordinate of this endpoint. These are fixed point, purely fractional
				/// numbers: 0 is the extreme left or top of the scanned rectangle, and 65535 is the
				/// extreme right or bottom.
				uint16_t x, y;

				/// Provides the offset, in samples, into the most recently allocated write area, of data
				/// at this end point.
				uint16_t data_offset;

				/// For composite video, provides the angle of the colour subcarrier at this endpoint.
				///
				/// This is a slightly weird fixed point, being:
				///
				///		* a six-bit fractional part;
				///		* a nine-bit integral part; and
				///		* a sign.
				///
				/// Positive numbers indicate that the colour subcarrier is 'running positively' on this
				/// line; i.e. it is any NTSC line or an appropriate swing PAL line, encoded as
				/// x*cos(a) + y*sin(a).
				///
				/// Negative numbers indicate a 'negative running' colour subcarrier; i.e. it is one of
				/// the phase alternated lines of PAL, encoded as x*cos(a) - y*sin(a), or x*cos(-a) + y*sin(-a),
				/// whichever you prefer.
				///
				/// It will produce undefined behaviour if signs differ on a single scan.
				int16_t composite_angle;
			} end_points[2];

			/// For composite video, dictates the amplitude of the colour subcarrier as a proportion of
			/// the whole, as determined from the colour burst.
			uint8_t composite_amplitude;
		};

		/// Requests a new scan to populate.
		///
		/// @return A valid pointer, or @c nullptr if insufficient further storage is available.
		Scan *get_scan();

		/// Finds the first available space of at least @c required_length pixels in size which is suitably aligned
		/// for writing of @c required_alignment number of pixels at a time.
		///
		/// Calls will be paired off with calls to @c reduce_previous_allocation_to.
		///
		/// @returns a pointer to the allocated space if any was available; @c nullptr otherwise.
		virtual uint8_t *allocate_write_area(std::size_t required_length, std::size_t required_alignment = 1) = 0;

		/// Announces that the owner is finished with the region created by the most recent @c allocate_write_area
		/// and indicates that its actual final size was @c actual_length.
		virtual void reduce_previous_allocation_to(std::size_t actual_length) = 0;

		/// Announces that all endpoint pairs and write areas obtained since the last @c submit have now been
		/// populated with appropriate data.
		///
		/// The ScanTarget isn't bound to take any drawing action immediately; it may sit on submitted data for
		/// as long as it feels is appropriate subject to an @c flush.
		virtual void submit() = 0;

		/// Discards all data and endpoints supplied since the last @c submit. This is generally used when
		/// failures in either get_endpoing_pair of allocate_write_area mean that proceeding would produce
		/// a faulty total output.
		virtual void reset() = 0;

		/// Announces that any submitted data not yet output should be output now, but needn't block while
		/// doing so. This generally communicates that processing is now otherwise 'up to date', so no
		/// further delay should be allowed.
		virtual void flush() = 0;


	/*
		ScanTargets also receive notification of certain events that may be helpful in processing, particularly
		for synchronising internal output to the outside world.
	*/

		enum class Event {
			HorizontalRetrace,
			VerticalRetrace
		};

		/// Provides a hint that the named event has occurred.
		virtual void announce(Event event) = 0;
};

}
}

#endif /* Outputs_CRT_ScanTarget_h */
