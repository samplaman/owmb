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
 * BlockOscillator.C
 *
 * Implementation of class Loris::BlockOscillator, a Bandwidth-Enhanced
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

#include "BlockOscillator.h"

#include "Notifier.h"
#include "Partial.h"

#include <cmath>
#include <vector>

const Fastsynth_Float_Type Pi = M_PI;
const Fastsynth_Float_Type TwoPi = 2 * Pi;
const Fastsynth_Float_Type OneOverTwoPi = 1.0 / TwoPi;

using Loris::Breakpoint;

//	begin namespace
namespace Loris
{

// ---------------------------------------------------------------------------
//  fill_cos_table
// ---------------------------------------------------------------------------
// Function to allocate and fill a cosine wavetable. Make it one
// extra sample long, so that interpolating and rounding index
// are safe.
//
static Fastsynth_Float_Type *
fill_cos_table(void)
{
    using std::cos;

    static Fastsynth_Float_Type table[BlockOscillator::TabSize + 1];

    /* fill it with a cosine wave */
    const Fastsynth_Float_Type TwoPiOverN = TwoPi / BlockOscillator::TabSize;
    for (int i = 0; i <= BlockOscillator::TabSize; ++i)
    {
        table[i] = cos(Fastsynth_Float_Type(i) * TwoPiOverN);
    }
    return table;
}

// ---------------------------------------------------------------------------
//  fill_carrier_amp_table
// ---------------------------------------------------------------------------
// Function to allocate and fill a lookup table for Partial amplitude
// scale as a function of bandwidth. The table is filled with samples of
//
// F(bw) = sqrt( 1. - bw )
//
// for 0.0 <= bw <= 1.0. Carrier amplitude is F(bw) * amp.
//
// Make it one extra sample long, so that interpolating and rounding index
// are safe. Lookup as F(bw) = tab[ round(bw*N) ].
//
static Fastsynth_Float_Type *
fill_carrier_amp_table(void)
{
    using std::sqrt;

    static Fastsynth_Float_Type table[BlockOscillator::TabSize + 1];

    const Fastsynth_Float_Type OneOverN = 1.0 / BlockOscillator::TabSize;

    for (int i = 0; i <= BlockOscillator::TabSize; ++i)
    {
        table[i] = sqrt(1.0 - (Fastsynth_Float_Type(i) * OneOverN));
    }
    return table;
}

// ---------------------------------------------------------------------------
//  fill_mod_index_table
// ---------------------------------------------------------------------------
// Function to allocate and fill a lookup table for Partial modulation index
// scale as a function of bandwidth. The table is filled with samples of
//
// F(bw) = sqrt( 2. * bw )
//
// for 0.0 <= bw <= 1.0. Stochastic modulator is F(bw) * amp * noise.
//
// Make it one extra sample long, so that interpolating and rounding index
// are safe. Lookup as F(bw) = tab[ round(bw*N) ].
//
static Fastsynth_Float_Type *
fill_mod_index_table(void)
{
    using std::sqrt;

    static Fastsynth_Float_Type table[BlockOscillator::TabSize + 1];

    const Fastsynth_Float_Type OneOverN = 1.0 / BlockOscillator::TabSize;

    for (int i = 0; i <= BlockOscillator::TabSize; ++i)
    {
        table[i] = sqrt(2.0 * (Fastsynth_Float_Type(i) * OneOverN));
    }
    return table;
}

//  wavetables are shared by all instances of the class
Fastsynth_Float_Type const *const BlockOscillator::CosineTab = fill_cos_table();
Fastsynth_Float_Type const *const BlockOscillator::CarrierAmpTab =
    fill_carrier_amp_table();
Fastsynth_Float_Type const *const BlockOscillator::ModIndexTab =
    fill_mod_index_table();

// ---------------------------------------------------------------------------
//  phaseToTableIndex
// ---------------------------------------------------------------------------
//  Convert phase in radians to a FRACTIONAL wavetable index.
//
static inline Fastsynth_Float_Type
phaseToTableIndex(double phRadians)
{
    const Fastsynth_Float_Type Nfloat = BlockOscillator::TabSize;

    // convert radians to samples
    Fastsynth_Float_Type ph = phRadians * Nfloat * OneOverTwoPi;

    //	wrap the index onto a valid range
    while (ph >= Nfloat)
    {
        ph -= Nfloat;
    }
    while (ph < 0.0)
    {
        ph += Nfloat;
    }
    return ph;
}

// ---------------------------------------------------------------------------
//  BlockOscillator default construction
// ---------------------------------------------------------------------------
// Cannot store these in vectors without a default constructor.
//
BlockOscillator::BlockOscillator(void) :
    mPhaseIdx(0),
    //	phase is stored as wavetable index
    mFreqPhaseInc(0),
    //	frequency is stored as phase increment
    mAmplitude(0),
    mBandwidth(0),
    mBlockLenSamples(1),
    mOneOverBlockLen(1.0),
    mPhaseIncOverF(Fastsynth_Float_Type(TabSize))
{
}

// ---------------------------------------------------------------------------
//  BlockOscillator construction
// ---------------------------------------------------------------------------
//  Initialize state variables, associate with a wavetable.
//	Default init-phase is 0.
//
BlockOscillator::BlockOscillator(unsigned int blockLenSamples,
                                 Fastsynth_Float_Type sample_rate,
                                 Fastsynth_Float_Type init_phase) :
    mPhaseIdx(phaseToTableIndex(init_phase)),
    //	phase is stored as wavetable index
    mFreqPhaseInc(0),
    //	frequency is stored as phase increment
    mAmplitude(0),
    mBandwidth(0),
    mBlockLenSamples(blockLenSamples),
    mOneOverBlockLen(1.0 / blockLenSamples),
    mPhaseIncOverF(Fastsynth_Float_Type(TabSize) / sample_rate)
{
}

// ---------------------------------------------------------------------------
//  set
// ---------------------------------------------------------------------------
//  Set the instantaneous envelope parameters
//  (frequency, amplitude, bandwidth, and phase).
//	No checking is performed, except that phase is wrapped.
//
void
BlockOscillator::set(const Breakpoint &bp)
{
    mFreqPhaseInc = bp.frequency() * mPhaseIncOverF;
    mAmplitude = bp.amplitude();
    mBandwidth = bp.bandwidth();
    mPhaseIdx = phaseToTableIndex(bp.phase());
}

// ---------------------------------------------------------------------------
//  initOnset
// ---------------------------------------------------------------------------
//  Initialize the oscillator to render an onset toward a target Breakpoint:
//  zero amplitude, the target's frequency and bandwidth, and phase walked
//  back one block at the target frequency, so that oscillating toward the
//  target lands exactly on the target phase (the same state a preceding
//  null Breakpoint would have established).
//
void
BlockOscillator::initOnset(const Breakpoint &bpTgt)
{
    mFreqPhaseInc = bpTgt.frequency() * mPhaseIncOverF;
    mAmplitude = 0;
    mBandwidth = bpTgt.bandwidth();

    //  walk the phase back one block at the target frequency
    //  (in wavetable index units), and wrap it into the table
    const Fastsynth_Float_Type N = TabSize;
    Fastsynth_Float_Type ph =
        phaseToTableIndex(bpTgt.phase()) - (mFreqPhaseInc * mBlockLenSamples);
    ph -= N * std::floor(ph / N);
    mPhaseIdx = ph;
}

// ---------------------------------------------------------------------------
//  setPhase
// ---------------------------------------------------------------------------
//! Reset the phase. This is done when the amplitude of a Partial goes to
//! zero, so that onsets are preserved in distilled and collated Partials.
//!
//!	\param phaseRadians is the desired oscillator phase in radians
//
void
BlockOscillator::setPhase(Fastsynth_Float_Type phaseRadians)
{
    mPhaseIdx = phaseToTableIndex(phaseRadians);
}

// ---------------------------------------------------------------------------
//  oscillate
// ---------------------------------------------------------------------------
//  Accumulate a single block of sinusoidal samples into the buffer provided,
//	modulating the oscillator state from its current values to the parameter
//	values stored in a target Breakpoint.
//
//  The caller must ensure that putEmHere points to a block of samples, and that
//	the current contents are zero or are valid (previously computed) samples for
//	this block (samples are accumulated, not replaced).
//
//	Target parameters are NOT bounds-checked.
//
void
BlockOscillator::oscillate(const Breakpoint &bpTgt,
                           Fastsynth_Float_Type *putEmHere)
{
    Fastsynth_Float_Type targetFreq = bpTgt.frequency() * mPhaseIncOverF;
    // mPhaseIncOverF = Fastsynth_Float_Type(TabSize) / sample_rate

    Fastsynth_Float_Type targetAmp = bpTgt.amplitude();

    //  Use local variables for speed.
    Fastsynth_Float_Type ph = mPhaseIdx;
    Fastsynth_Float_Type freq = mFreqPhaseInc;
    Fastsynth_Float_Type a = mAmplitude;

    unsigned int idx;

    //  compute trajectories:
    const Fastsynth_Float_Type dFreq = (targetFreq - freq) * mOneOverBlockLen;
    freq = mFreqPhaseInc + (0.5 * dFreq);
    //  freq is only used to update phase, advance by half a sample here,
    //  then by a whole step each time through the loop, so that the phase
    //  update is the mean frequency between two samples

    const Fastsynth_Float_Type dAmp =
        (targetAmp - mAmplitude) * mOneOverBlockLen;
    // const Fastsynth_Float_Type dBw = (targetBw - mBandwidth)  *
    // mOneOverBlockLen;

    const Fastsynth_Float_Type N = TabSize;
    //	for phase wrapping

    for (int howmany = 0; howmany < mBlockLenSamples; ++howmany)
    {
        //  compute a sample and add it into the buffer:
        idx = ph + 0.5; // cheap rounding
        *putEmHere++ += a * CosineTab[idx];

        //  update and wrap the phase
        ph += freq;
        if (ph > N)
        {
            ph -= N;
        }
        // ph += ( Fastsynth_Float_Type(ph<0) - Fastsynth_Float_Type(ph>N) ) *
        // N;
        //	phase wrapping that accommodates negative frequencies

        //  update the instantaneous oscillator state:
        freq += dFreq; //  update phase before updating frequency
        a += dAmp;

    } // end of sample computation loop

    //  set the state variables to their target values:
    mFreqPhaseInc = targetFreq;
    mAmplitude = targetAmp;
    mPhaseIdx = ph;
}

// ---------------------------------------------------------------------------
//  oscillate
// ---------------------------------------------------------------------------
//  Accumulate a single block of bandwidth-enhanced samples into the buffer
//  provided,
//	modulating the oscillator state from its current values to the parameter
//	values stored in a target Breakpoint.
//
//  The caller must ensure that putEmHere points to a block of samples, and that
//	the current contents are zero or are valid (previously computed) samples for
//	this block (samples are accumulated, not replaced).
//
//	Target parameters are NOT bounds-checked.
//
void
BlockOscillator::oscillate(const Breakpoint &bpTgt,
                           Fastsynth_Float_Type *putEmHere,
                           Fastsynth_Float_Type const *noise)
{
    Fastsynth_Float_Type targetFreq = bpTgt.frequency() * mPhaseIncOverF;
    // mPhaseIncOverF = Fastsynth_Float_Type(TabSize) / sample_rate

    Fastsynth_Float_Type targetAmp = bpTgt.amplitude();
    Fastsynth_Float_Type targetBw = bpTgt.bandwidth();

    //  Use local variables for speed.
    Fastsynth_Float_Type ph = mPhaseIdx;
    Fastsynth_Float_Type freq = mFreqPhaseInc;
    Fastsynth_Float_Type a = mAmplitude;
    Fastsynth_Float_Type bw = mBandwidth;

    unsigned int idx;

    //  compute trajectories:
    const Fastsynth_Float_Type dFreq = (targetFreq - freq) * mOneOverBlockLen;
    freq = mFreqPhaseInc + (0.5 * dFreq);
    //  freq is only used to update phase, advance by half a sample here,
    //  then by a whole step each time through the loop, so that the phase
    //  update is the mean frequency between two samples

    const Fastsynth_Float_Type dAmp =
        (targetAmp - mAmplitude) * mOneOverBlockLen;
    const Fastsynth_Float_Type dBw = (targetBw - mBandwidth) * mOneOverBlockLen;

    const Fastsynth_Float_Type TableMaxIdx = TabSize;
    //	for phase wrapping

    //  local variables for amplitude modulation
    Fastsynth_Float_Type car, mod, am;

    for (int howmany = 0; howmany < mBlockLenSamples; ++howmany)
    {
        //  compute a sample and add it into the buffer:
        idx = (unsigned int)((bw * TableMaxIdx) + 0.5); // cheap rounding
        car = CarrierAmpTab[idx];
        mod = ModIndexTab[idx];
        am = car + (*noise++ * mod);

        idx = ph + 0.5; // cheap rounding
        *putEmHere++ += a * am * CosineTab[idx];

        //  update and wrap the phase
        ph += freq;
        if (ph > TableMaxIdx)
        {
            ph -= TableMaxIdx;
        }
        // ph += ( Fastsynth_Float_Type(ph<0) -
        // Fastsynth_Float_Type(ph>TableMaxIdx) ) * TableMaxIdx;
        //	phase wrapping that accommodates negative frequencies

        //  update the instantaneous oscillator state:
        freq += dFreq; //  update phase before updating frequency
        a += dAmp;
        bw += dBw;

    } // end of sample computation loop

    //  set the state variables to their target values:
    mFreqPhaseInc = targetFreq;
    mAmplitude = targetAmp;
    mBandwidth = targetBw;
    mPhaseIdx = ph;
}

} // namespace Loris
