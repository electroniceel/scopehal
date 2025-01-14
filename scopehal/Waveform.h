/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal v0.1                                                                                                     *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg and contributors                                                         *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file
	@author Andrew D. Zonenberg
	@brief Declaration of Waveform
 */

#ifndef Waveform_h
#define Waveform_h

#include <vector>
#include <AlignedAllocator.h>

#include "StandardColors.h"
#include "AcceleratorBuffer.h"

/**
	@brief Base class for all Waveform specializations

	One waveform contains a time-series of sample objects as well as scale information etc. The samples may
	or may not be at regular intervals depending on whether the Oscilloscope uses RLE compression.

	The WaveformBase contains all metadata, but the actual samples are stored in a derived class member.
 */
class WaveformBase
{
public:
	WaveformBase()
		: m_timescale(0)
		, m_startTimestamp(0)
		, m_startFemtoseconds(0)
		, m_triggerPhase(0)
		, m_densePacked(false)
		, m_flags(0)
		, m_revision(0)
	{
		m_offsets.PrepareForCpuAccess();
		m_durations.PrepareForCpuAccess();
	}

	//empty virtual destructor in case any derived classes need one
	virtual ~WaveformBase()
	{}

	/**
		@brief The time scale, in femtoseconds per timestep, used by this channel.

		This is used as a scaling factor for individual sample time values as well as to compute the maximum zoom value
		for the time axis.
	 */
	int64_t m_timescale;

	///@brief Start time of the acquisition, rounded to nearest second
	time_t	m_startTimestamp;

	///@brief Fractional start time of the acquisition (femtoseconds since m_startTimestamp)
	int64_t m_startFemtoseconds;

	/**
		@brief Offset, in femtoseconds, from the trigger to the sampling clock.

		This is most commonly the output of a time-to-digital converter and ranges from 0 to 1 sample, but this
		should NOT be assumed to be the case in all waveforms.

		LeCroy oscilloscopes, for example, can have negative trigger phases of 150ns or more on digital channels
		since the digital waveform can start significantly before the analog waveform!
	 */
	int64_t m_triggerPhase;

	/**
		@brief True if the waveform is "dense packed".

		This means that m_durations is always 1, and m_offsets ranges from 0 to m_offsets.size()-1.

		If dense packed, we can often perform various optimizations to avoid excessive copying of waveform data.

		Most oscilloscopes output dense packed waveforms natively.
	 */
	bool m_densePacked;

	/**
		@brief Flags that apply to this waveform. Bitfield.

		WAVEFORM_CLIPPING: Scope indicated that this waveform is clipped.
	 */
	uint8_t m_flags;

	/**
		@brief Revision number

		This is a monotonically increasing counter that indicates waveform data has changed. Filters may choose to
		cache pre-processed versions of input data (for example, resampled versions of raw input) as long as the
		pointer and revision number have not changed.
	 */
	uint64_t m_revision;

	enum
	{
		WAVEFORM_CLIPPING = 1
	};

	///@brief Start timestamps of each sample
	AcceleratorBuffer<int64_t> m_offsets;

	///@brief Durations of each sample
	AcceleratorBuffer<int64_t> m_durations;

	virtual void clear()
	{
		m_offsets.clear();
		m_durations.clear();
	}

	virtual void Resize(size_t size)
	{
		m_offsets.resize(size);
		m_durations.resize(size);
	}

	virtual std::string GetText(size_t /*i*/)
	{
		return "(unimplemented)";
	}

	virtual Gdk::Color GetColor(size_t /*i*/)
	{
		return StandardColors::colors[StandardColors::COLOR_ERROR];
	}

	virtual void PrepareForCpuAccess()
	{
		m_offsets.PrepareForCpuAccess();
		m_durations.PrepareForCpuAccess();
	}

	virtual void PrepareForGpuAccess()
	{
		m_offsets.PrepareForGpuAccess();
		m_durations.PrepareForGpuAccess();
	}

	/**
		@brief Copies offsets/durations from one waveform to another.

		Must have been resized to match rhs first.
	 */
	void CopyTimestamps(const WaveformBase* rhs)
	{
		m_offsets.CopyFrom(rhs->m_offsets);
		m_durations.CopyFrom(rhs->m_durations);
	}

	virtual void MarkSamplesModifiedFromCpu()
	{

	}

	virtual void MarkSamplesModifiedFromGpu()
	{
	}

	virtual void MarkTimestampsModifiedFromCpu()
	{

	}

	virtual void MarkTimestampsModifiedFromGpu()
	{
	}
};

/**
	@brief A waveform that contains actual data
 */
template<class S>
class Waveform : public WaveformBase
{
public:

	Waveform()
	{
		//Default waveform data to CPU/GPU mirror
		//and sample data to pinned memory
		m_samples.SetCpuAccessHint(AcceleratorBuffer<S>::HINT_LIKELY);
		m_samples.SetGpuAccessHint(AcceleratorBuffer<S>::HINT_LIKELY);
	}

	///@brief Sample data
	AcceleratorBuffer<S> m_samples;

	virtual void Resize(size_t size)
	{
		m_offsets.resize(size);
		m_durations.resize(size);
		m_samples.resize(size);
	}

	virtual void clear()
	{
		m_offsets.clear();
		m_durations.clear();
		m_samples.clear();
	}

	virtual void PrepareForCpuAccess()
	{
		m_offsets.PrepareForCpuAccess();
		m_durations.PrepareForCpuAccess();
		m_samples.PrepareForCpuAccess();
	}

	virtual void PrepareForGpuAccess()
	{
		m_offsets.PrepareForGpuAccess();
		m_durations.PrepareForGpuAccess();
		m_samples.PrepareForGpuAccess();
	}

	virtual void MarkTimestampsModifiedFromCpu()
	{
		m_offsets.MarkModifiedFromCpu();
		m_durations.MarkModifiedFromCpu();
	}

	virtual void MarkTimestampsModifiedFromGpu()
	{
		m_offsets.MarkModifiedFromGpu();
		m_durations.MarkModifiedFromGpu();
	}

	virtual void MarkSamplesModifiedFromCpu()
	{
		m_samples.MarkModifiedFromCpu();
	}

	virtual void MarkSamplesModifiedFromGpu()
	{
		m_samples.MarkModifiedFromGpu();
	}
};

typedef Waveform<bool>	DigitalWaveform;
typedef Waveform<float>	AnalogWaveform;

typedef Waveform< std::vector<bool> > 	DigitalBusWaveform;

#endif
