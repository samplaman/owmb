#ifndef Loris_BlockSynthBwe_h
#define Loris_BlockSynthBwe_h

/*
 *  BlockSynthBwe.h
 *  Loris
 *
 *  Created by Pop on 10/3/11.
 *  Loris is Copyright (c) 1999-2026 by Kelly Fitz and Lippold Haken
 *
 */

#include "BlockOscillator.h"
#include "Breakpoint.h"
#include "PartialList.h"

#include <vector>
//
// extern "C" {
//}

/*
 Definition of floating point type
 */
#if defined(FASTSYNTH_FLOAT_TYPE)
typedef FASTSYNTH_FLOAT_TYPE Fastsynth_Float_Type;
#else
typedef float Fastsynth_Float_Type;
#define FASTSYNTH_FLOAT_TYPE
#endif

// ------------------------------------
//	class BlockSynthBwe
// ------------------------------------

//	begin namespace
namespace Loris
{

class BlockSynthBwe
{
  private:
    //	static std::vector< Fastsynth_Float_Type > mCosineTab;
    //	static std::vector< Fastsynth_Float_Type > mCarrierAmpTab;
    //	static std::vector< Fastsynth_Float_Type > mModIndexTab;

    unsigned int mBlockLenSamples;
    unsigned int mNoiseBufferIndex;

    //	multipliers
    Fastsynth_Float_Type mOneOverBlockLen;
    Fastsynth_Float_Type mOneOverSR;

    Fastsynth_Float_Type mRadians2WavetablePhase;

    std::vector<BlockOscillator> mOscils;

  public:
    static constexpr int TabSize = 1024;
    static constexpr int MaxDelay = 101;

    //	lifecycle

    //	construct from block length and sample rate, which must then be
    //	fixed for the lifetime of this oscillator.
    BlockSynthBwe(unsigned int blockLenSamples,
                  Fastsynth_Float_Type sample_rate, unsigned int numOscils);

    static void buildTables(void);

    // void prepareFrames( Loris::PartialList & partials );
    void allocateOscils(unsigned int howMany, Fastsynth_Float_Type sample_rate);

    void render(std::vector<Loris::Breakpoint> &thisFrame,
                Fastsynth_Float_Type *putEmHere);

}; //	end of class BlockSynthBwe
} // namespace Loris

#endif /* ndef Loris_BlockSynthBwe_h */
