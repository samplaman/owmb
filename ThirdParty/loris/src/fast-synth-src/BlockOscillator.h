#ifndef Loris_BlockOscillator_h
#define Loris_BlockOscillator_h

/*
 * This is the Loris C++ Class Library, implementing analysis,
 * manipulation, and synthesis of digitized sounds using the Reassigned
 * Bandwidth-Enhanced Additive Sound Model.
 *
 * Loris is Copyright (c) 1999-2026 by Kelly Fitz and Lippold Haken
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * BlockOscillator.h
 *
 * Definition of class Loris::BlockOscillator, a Bandwidth-Enhanced
 * wavetable oscillator that uses a fixed block (frame) size (generates
 * the same number of samples every call, assumes uniform sampling of the
 * envelopes), and performs no bounds or aliasing checks (intended to be
 * fast and cheap). The bandlimited noise modulator samples are provided
 * for each block by the caller.
 *
 * Kelly Fitz, 6 Oct 2011
 * loris@cerlsoundgroup.org
 *
 * http://www.cerlsoundgroup.org/Loris/
 *
 */

#include "Breakpoint.h"

/*
 Definition of floating point type
 */
#if defined(FASTSYNTH_FLOAT_TYPE)
typedef FASTSYNTH_FLOAT_TYPE Fastsynth_Float_Type;
#else
typedef float Fastsynth_Float_Type;
#define FASTSYNTH_FLOAT_TYPE
#endif

//	begin namespace
namespace Loris
{

class BlockOscillator
{
    //	oscillator state
    Fastsynth_Float_Type mPhaseIdx, mFreqPhaseInc, mAmplitude, mBandwidth;

    unsigned int mBlockLenSamples;

    //	multipliers
    Fastsynth_Float_Type mOneOverBlockLen;
    // Fastsynth_Float_Type mOneOverSR;
    Fastsynth_Float_Type mPhaseIncOverF;

    //  wavetables are shared by all instances of the class
    static Fastsynth_Float_Type const *const CosineTab;
    static Fastsynth_Float_Type const *const CarrierAmpTab;
    static Fastsynth_Float_Type const *const ModIndexTab;

  public:
    static constexpr int TabSize = 1024;

    // Cannot store these in vectors without a default constructor.
    //
    BlockOscillator(void);

    //  Initialize state variables, associate with a wavetable.
    //	Default init-phase is 0.
    //
    BlockOscillator(unsigned int blockLenSamples,
                    Fastsynth_Float_Type sample_rate,
                    Fastsynth_Float_Type init_phase = 0);

    //  Set the instantaneous envelope parameters
    //  (frequency, amplitude, bandwidth, and phase).
    //	No checking is performed, except that phase is wrapped.
    //
    void set(const Loris::Breakpoint &bp);

    //  Initialize the oscillator to render an onset toward a target
    //  Breakpoint: zero amplitude, the target's frequency and bandwidth,
    //  and phase walked back one block at the target frequency, so that
    //  oscillating toward the target lands exactly on the target phase.
    //  Rendering an onset therefore never depends on a null Breakpoint
    //  having preceded the target in the frame stream.
    //
    void initOnset(const Loris::Breakpoint &bpTgt);

    //! Return the current amplitude, need access to know whether to
    //! set phase.
    Fastsynth_Float_Type
    amplitude(void) const
    {
        return mAmplitude;
    }

    //! Reset the phase. This is done when the amplitude of a Partial goes to
    //! zero, so that onsets are preserved in distilled and collated Partials.
    //!
    //!	\param phaseRadians is the desired oscillator phase in radians
    //
    void setPhase(Fastsynth_Float_Type phaseRadians);

    //  Accumulate a single block of sinusoidal samples into the buffer
    //  provided,
    //	modulating the oscillator state from its current values to the parameter
    //	values stored in a target Breakpoint.
    //
    //  The caller must ensure that putEmHere points to a block of samples, and
    //  that
    //	the current contents are zero or are valid (previously computed) samples
    // for 	this block (samples are accumulated, not replaced).
    //
    //	Target parameters are NOT bounds-checked.
    //
    void oscillate(const Loris::Breakpoint &bpTgt,
                   Fastsynth_Float_Type *putEmHere);

    //  Accumulate a single block of bandwidth-enhanced samples into the buffer
    //  provided,
    //	modulating the oscillator state from its current values to the parameter
    //	values stored in a target Breakpoint.
    //
    //  The caller must ensure that putEmHere points to a block of samples, and
    //  that
    //	the current contents are zero or are valid (previously computed) samples
    // for 	this block (samples are accumulated, not replaced).
    //
    //	Target parameters are NOT bounds-checked.
    //
    void oscillate(const Loris::Breakpoint &bpTgt,
                   Fastsynth_Float_Type *putEmHere,
                   const Fastsynth_Float_Type *modulator);
};
} // namespace Loris

#endif /* ndef Loris_BlockOscillator_h */
