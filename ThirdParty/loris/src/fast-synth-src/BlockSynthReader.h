//
//  BlockSynthReader.h
//  Loris
//
//  Created by Kelly Fitz on 11/8/11.
//  Loris is Copyright (c) 1999-2026 by Kelly Fitz and Lippold Haken
//

#ifndef Loris_BlockSynthReader_h
#define Loris_BlockSynthReader_h

#include "PartialList.h"

#include <vector>

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

class BlockSynthReader
{
  private:
    Fastsynth_Float_Type mBlocksPerSecond;

    std::vector<std::vector<Loris::Breakpoint>> mBpFrames;
    //	this is the tricky bit, how to allocate this, what kind of storage to
    // use?

  public:
    typedef std::vector<Loris::Breakpoint> FrameType;

    //	lifecycle

    //	Initialize a Partial reader at a specified resampling interval.
    BlockSynthReader(Loris::PartialList &partials,
                     Fastsynth_Float_Type BlockInterval_seconds);

    //  access

    unsigned int
    numFrames(void) const
    {
        return mBpFrames.size();
    }
    unsigned int
    numPartials(void) const
    {
        return mBpFrames.front().size();
    }

    FrameType &getFrame(unsigned int frameNum);
    FrameType &getFrameAtTime(double frameTime);

}; // end of class BlockSynthReader
} // namespace Loris

#endif
