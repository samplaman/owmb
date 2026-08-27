/*
 *  BlockSynthBwe.cpp
 *  Loris
 *
 *  Created by Pop on 10/3/11.
 *  Loris is Copyright (c) 1999-2026 by Kelly Fitz and Lippold Haken
 *
 */

#include "BlockSynthBwe.h"

#include "Breakpoint.h"
#include "Filter.h"
#include "LorisExceptions.h"
#include "PartialUtils.h"
#include "Resampler.h"
#include "r250.h"

#include <cmath>

//	begin namespace
namespace Loris
{

const Fastsynth_Float_Type TwoPi = 2 * 3.14159265358979324;

//  prototype, defined at bottom of this file

static void generate_randi(unsigned int decimation,
                           Fastsynth_Float_Type *output, int howmany);

/*
 make_cos_table

 Function to allocate and fill a cosine wavetable. Make it one
 extra sample long, so that interpolating and rounding index
 are safe.
 */
static Fastsynth_Float_Type *
make_cos_table(int N)
{
    using std::cos;

    int i;
    Fastsynth_Float_Type *table =
        (Fastsynth_Float_Type *)malloc((N + 1) * sizeof(Fastsynth_Float_Type));

    /* fill it with a cosine wave */
    for (i = 0; i < N + 1; ++i)
    {
        table[i] = cos(Fastsynth_Float_Type(i) * TwoPi / N);
    }
    return table;
}

/*
 make_carrier_amp_table

 Function to allocate and fill a lookup table for Partial amplitude
 scale as a function of bandwidth. The table is filled with samples of

 F(bw) = sqrt( 1. - bw )

 for 0.0 <= bw <= 1.0. Carrier amplitude is F(bw) * amp.

 Make it one extra sample long, so that interpolating and rounding index
 are safe. Lookup as F(bw) = tab[ round(bw*N) ].
 */
static Fastsynth_Float_Type *
make_carrier_amp_table(int N)
{
    using std::sqrt;

    int i;
    Fastsynth_Float_Type *table =
        (Fastsynth_Float_Type *)malloc((N + 1) * sizeof(Fastsynth_Float_Type));

    /* fill it with a cosine wave */
    for (i = 0; i < N + 1; ++i)
    {
        table[i] = sqrt(1.0 - (Fastsynth_Float_Type(i) / N));
    }
    return table;
}

/*
 make_mod_index_table

 Function to allocate and fill a lookup table for Partial modulation index
 scale as a function of bandwidth. The table is filled with samples of

 F(bw) = sqrt( 2. * bw )

 for 0.0 <= bw <= 1.0. Stochastic modulator is F(bw) * amp * noise.

 Make it one extra sample long, so that interpolating and rounding index
 are safe. Lookup as F(bw) = tab[ round(bw*N) ].
 */
static Fastsynth_Float_Type *
make_mod_index_table(int N)
{
    using std::sqrt;

    int i;
    Fastsynth_Float_Type *table =
        (Fastsynth_Float_Type *)malloc((N + 1) * sizeof(Fastsynth_Float_Type));

    /* fill it with a cosine wave */
    for (i = 0; i < N + 1; ++i)
    {
        table[i] = sqrt(2.0 * (Fastsynth_Float_Type(i) / N));
    }
    return table;
}

//	these are temporary

static Fastsynth_Float_Type NoiseBuffer[BlockSynthBwe::TabSize];

BlockSynthBwe::BlockSynthBwe(unsigned int blockLenSamples,
                             Fastsynth_Float_Type sample_rate,
                             unsigned int numOscils) :
    mBlockLenSamples(blockLenSamples),
    mNoiseBufferIndex(0),
    mOneOverBlockLen(1.0 / blockLenSamples),
    mOneOverSR(1.0 / sample_rate),
    mRadians2WavetablePhase(TabSize / TwoPi)
{
    allocateOscils(numOscils, sample_rate);

    //  initialize the random number generator
    r250_init(1);

    //  pre-fill the noise buffer
    generate_randi(50, NoiseBuffer, TabSize);
}

//	\pre mBpFrames must already be constructed, initial oscillator state is set
// to
void
BlockSynthBwe::allocateOscils(unsigned int howMany,
                              Fastsynth_Float_Type sample_rate)
{
    BlockOscillator proto = BlockOscillator(mBlockLenSamples, sample_rate);

    // mOscils is a std::vector< BlockOscillator >
    mOscils.resize(howMany, proto);
}

void
BlockSynthBwe::render(std::vector<Loris::Breakpoint> &thisFrame,
                      Fastsynth_Float_Type *putEmHere)
{
    generate_randi(50, NoiseBuffer + mNoiseBufferIndex, mBlockLenSamples);
    mNoiseBufferIndex += mBlockLenSamples;
    if (TabSize <= (mNoiseBufferIndex + mBlockLenSamples))
    {
        mNoiseBufferIndex = 0;
    }
    //  TODO: need a decorrelating delay here! NOT YET IMPLEMENTED

    for (unsigned int partialNum = 0; partialNum < mOscils.size(); ++partialNum)
    {
        BlockOscillator &osc = mOscils[partialNum];

        Loris::Breakpoint &nxtBp = thisFrame[partialNum];

        //  skip over all of this if all samples will be zero
        if (0 < nxtBp.amplitude() || 0 < osc.amplitude())
        {
            //	Onset from silence: initialize the oscillator from the
            //	target Breakpoint (phase walked back one block at the
            //	target frequency), so that correct rendering never
            //	depends on a null Breakpoint having preceded the target
            //	in the frame stream.
            if (0 == osc.amplitude())
            {
                osc.initOnset(nxtBp);
            }
            osc.oscillate(nxtBp, putEmHere, NoiseBuffer);
        }

        //  park the oscillator on the target if current amp is zero --
        // 	resetting only phase is not sufficient/equivalent if
        //	oscillation was skipped.
        //	(Note: osc.amplitude is zero iff nxtBp.amplitude is zero)
        if (0 == osc.amplitude())
        {
            osc.set(nxtBp);
        }
    }
}

// ------------------------------------------------
// temporary hacks to make a noise generator
// ------------------------------------------------

inline double
uniform(void)
{
    return dr250();
}

// ---------------------------------------------------------------------------
//	gaussian_normal
// ---------------------------------------------------------------------------
//	Approximate the normal distribution using the polar form of the
//  Box-Muller transformation.
//	This is a better approximation and faster algorithm than the
//  central limit theorem method.
//
double
gaussian_normal(void)
{
    static bool use_saved = false; //	boolean really, now member variables
    static double saved_val;

    double r = 1., fac, v1, v2;

    if (use_saved)
    {
        use_saved = false;
        return saved_val;
    }
    else
    {
        v1 = 2. * uniform() - 1.;
        v2 = 2. * uniform() - 1.;
        r = v1 * v1 + v2 * v2;
        while (r >= 1.)
        {
            // v1 = 2. * uniform() - 1.;
            // actually may only need one new uniform sample
            v1 = v2;
            v2 = 2. * uniform() - 1.;
            r = v1 * v1 + v2 * v2;
        }

        fac = std::sqrt(-2. * std::log(r) / r);
        saved_val = v1 * fac;
        use_saved = true;
        return v2 * fac;
    }
}

// ---------------------------------------------------------------------------
//  apply protoype filter
// ---------------------------------------------------------------------------
//  Static local function for obtaining a prototype Filter
//  to use in Oscillator construction. Eventually, allow
//  external (client) specification of the Filter prototype.
//
static inline double
apply_filter(double sample)
{
    //  Chebychev order 3, cutoff 500, ripple -1.
    //
    //  Coefficients obtained from http://www.cs.york.ac.uk/~fisher/mkfilter/
    //  Digital filter designed by mkfilter/mkshape/gencode   A.J. Fisher
    //
    static const double Gain = 4.663939184e+04;
    static const double ExtraScaling = 6.;
    static const double MaCoefs[] = {1., 3., 3., 1.};
    static const double ArCoefs[] = {1., -2.9258684252, 2.8580608586,
                                     -0.9320209046};

    static Loris::Filter proto(MaCoefs, MaCoefs + 4, ArCoefs, ArCoefs + 4,
                               ExtraScaling / Gain);

    return proto.apply(sample);
}

static void
generate_randi(unsigned int decimation, Fastsynth_Float_Type *output,
               int howmany)
{
    static int step = decimation;
    static Fastsynth_Float_Type value = gaussian_normal();
    static Fastsynth_Float_Type dvalue = (gaussian_normal() - value) / step;

    while (howmany-- > 0)
        *output++ = 0;
    //    {
    //		/* compute the output sample */
    //		*output++ = apply_filter( value );
    //
    //		/* update the noise sample */
    //		if ( --step <= 0 )
    //		{
    //            step = decimation;
    //            dvalue = (gaussian_normal() - value) / step;
    //		}
    //		value += dvalue;
    //	}
}

} // namespace Loris
