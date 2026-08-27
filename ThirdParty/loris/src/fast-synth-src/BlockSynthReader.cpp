//
//  BlockSynthReader.cpp
//  Loris
//
//  Created by Kelly Fitz on 11/8/11.
//  Loris is Copyright (c) 1999-2026 by Kelly Fitz and Lippold Haken
//

#include "BlockSynthReader.h"

#include "Breakpoint.h"
#include "PartialUtils.h"
#include "Resampler.h"

#include <cmath> // for std::floor

//	begin namespace
namespace Loris
{

BlockSynthReader::BlockSynthReader(Loris::PartialList &partials,
                                   Fastsynth_Float_Type BlockInterval_seconds) :
    mBlocksPerSecond(1.0 / BlockInterval_seconds)
{
    //	Need ultimately to separate this out into a reader class that can return
    //	a frame to be synthesized over the next block, or to be morphed or
    // otherwise 	modified before rendering.

    //	const Fastsynth_Float_Type BlockInterval_seconds = blockLenSamples /
    // sample_rate;
    //
    const Fastsynth_Float_Type OneOverInterval = 1.0 / BlockInterval_seconds;

    const unsigned int NumPartials = partials.size();

    const Fastsynth_Float_Type dur_seconds =
        Loris::PartialUtils::timeSpan(partials.begin(), partials.end()).second;

    const unsigned int NumFrames =
        1 + 1 +
        (unsigned int)((dur_seconds * OneOverInterval) + 0.5 /* round */);

    Loris::Resampler resampler(BlockInterval_seconds);
    resampler.setPhaseCorrect(true);

    //	resize mBpFrames to contain NumFrames empty frames, each frame
    //	of sizepartials.size()
    mBpFrames.resize(NumFrames, FrameType(NumPartials));

    //	prepare the Partials, and fill in the Breakpoints
    unsigned int partialNum = 0;
    for (Loris::PartialList::const_iterator it = partials.begin();
         it != partials.end(); ++it)
    {

        Loris::Partial p = *it;

        //  use a Resampler to quantize the Breakpoint times and
        //  correct the phases:
        resampler.resample(p);

        p.fadeIn(BlockInterval_seconds);
        p.fadeOut(BlockInterval_seconds);

        Loris::Partial::const_iterator pos = p.begin();
        Assert(pos.time() >=
               0.0); //	alternatively, skip over Breakpoints at time < 0.0
        unsigned int frameNum =
            std::floor((pos.time() * OneOverInterval) + 0.5 /* round */);
        while (pos != p.end())
        {
            mBpFrames[frameNum++][partialNum] = pos.breakpoint();
            ++pos;
        }

        ++partialNum;
    }
}

BlockSynthReader::FrameType &
BlockSynthReader::getFrame(unsigned int frameNum)
{
    Assert(frameNum < mBpFrames.size());

    return mBpFrames[frameNum];
}

BlockSynthReader::FrameType &
BlockSynthReader::getFrameAtTime(double frameTime)
{

    int frameNum = mBlocksPerSecond * frameTime;
    if (0 > frameNum)
    {
        frameNum = 0;
    }
    else if (mBpFrames.size() < frameNum)
    {
        frameNum = mBpFrames.size();
    }

    return mBpFrames[frameNum];
}

} // namespace Loris
