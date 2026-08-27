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
 * Resampler.C
 *
 * Implementation of class Resampler, for converting reassigned Partial
 * envelopes into more conventional additive synthesis envelopes, having data
 * points at regular time intervals. The benefits of reassigned analysis are NOT
 * lost in this process, since the elimination of unreliable data and the
 * reduction of temporal smearing are reflected in the resampled data.
 *
 * Lippold, 7 Aug 2003
 * loris@cerlsoundgroup.org
 *
 * http://www.cerlsoundgroup.org/Loris/
 *
 *
 * Phase correction added by Kelly 13 Dec 2005.
 * Interface updated by Kelly Aug 2010.
 */
#include "Resampler.h"

#include "Breakpoint.h"
#include "LinearEnvelope.h"
#include "LorisExceptions.h"
#include "Notifier.h"
#include "Partial.h"
#include "phasefix.h"

#include <algorithm>
#include <cmath>

//	begin namespace
namespace Loris
{

/*
TODO
    X remove empties (currently handled automatically in the Python module)

    X simplify interface, operate on containers, not iterator ranges
        - only two operations, resample and quantize, taking either a
        Partial or a PartialList, remove the static member too

    X remove insert_resampled_at (??)

    - phase correct with timing?

    - timing envelope as class member? Like morphing envelopes?

    - in-place operation?

    - fade time (for amplitude envelope sampling) - equal to interval? half?
        wtf???
*/

// ---------------------------------------------------------------------------
//	constructor - sampling interval
// ---------------------------------------------------------------------------
//! Initialize a Resampler having the specified uniform sampling
//! interval. Enable phase-correct resampling, in which frequencies
//! of resampled Partials are modified (using fixFrequency) such
//! that the resampled phases are achieved in synthesis. Phase-
//! correct resampling can be disabled using setPhaseCorrect.
//!
//! Resampled Partials will be composed of Breakpoints at every
//! integer multiple of the resampling interval.
//!
//! \sa setPhaseCorrect
//! \sa fixFrequency
//!
//! \param  sampleInterval is the resampling interval in seconds,
//!         Breakpoint data is computed at integer multiples of
//!         sampleInterval seconds.
//! \throw  InvalidArgument if sampleInterval is not positive.
//
Resampler::Resampler(double sampleInterval) :
    interval_(sampleInterval),
    phaseCorrect_(true)
{
    if (sampleInterval <= 0.)
    {
        Throw(InvalidArgument, "Resampler sample interval must be positive.");
    }
}

// ---------------------------------------------------------------------------
//	setPhaseCorrect
// ---------------------------------------------------------------------------
//! Specify phase-corrected resampling, or not. If phase
//! correct, Partial frequencies are altered slightly
//! to match, as nearly as possible, the Breakpoint
//! phases after resampling. Phases are updated so that
//! the Partial frequencies and phases are consistent after
//! resampling.
//!
//! \param  correctPhase is a boolean flag specifying that
//!         (if true) frequency/phase correction should be
//!         applied after resampling.
void
Resampler::setPhaseCorrect(bool correctPhase)
{
    phaseCorrect_ = correctPhase;
}

// ---------------------------------------------------------------------------
//	resample
// ---------------------------------------------------------------------------
//! Resample the specified Partial using the stored quanitization interval.
//! The Breakpoint times will comprise a contiguous sequence of all integer
//! multiples of the sampling interval, starting and ending with the nearest
//! multiples to the ends of the Partial. If phase correct resampling is
//! specified (the default)≤ frequencies and phases are corrected to be in
//! agreement and to match as nearly as possible the resampled phases.
//!
//! Resampling is performed in-place.
//!
//! \param  p is the Partial to resample
//
void
Resampler::resample(Partial &p) const
{
    //  for phase-correct resampling, first make the phases correct by
    //  fixing them from the initial phase (ideally this should have
    //  no effect but there's no way to be phase-correct after resampling
    //  unless the phases start correct), then resample the Breakpoint
    //  times, then afterwards, adjust the frequencies to match
    //  the interpolated phases:
    if (phaseCorrect_)
    {
        fixPhaseForward(p.begin(), --p.end());
    }

    //	create the new Partial:
    Partial newp;
    newp.setLabel(p.label());

    //  find time of first and last breakpoint for the resampled envelope:
    double firstInsertTime = interval_ * int(0.5 + p.startTime() / interval_);
    double lastInsertTime = p.endTime() + (0.5 * interval_);

    //  resample:
    for (double insertTime = firstInsertTime; insertTime <= lastInsertTime;
         insertTime += interval_)
    {
        //  sample time is same as the insert time:
        double sampleTime = insertTime;

        //  make a resampled Breakpoint:
        Breakpoint newbp = p.parametersAt(sampleTime);

        newp.insert(insertTime, newbp);
    }

    //	store the new Partial:
    p = newp;

    if (phaseCorrect_)
    {
        fixFrequency(p); // use default maxFixPct
    }
}

// ---------------------------------------------------------------------------
//	resample
// ---------------------------------------------------------------------------
//! Resample the specified Partial using the stored quanitization interval.
//! The Breakpoint times will comprise a contiguous sequence of all integer
//! multiples of the sampling interval, starting and ending with the nearest
//! multiples to the ends of the Partial. If phase correct resampling is
//! specified (the default)≤ frequencies and phases are corrected to be in
//! agreement and to match as nearly as possible the resampled phases.
//!
//! The timing envelope represents a warping of the time axis that is
//! applied during resampling. The Breakpoint times in resampled Partials
//! will a comprise contiguous sequence of all integer multiples of the
//! sampling interval between the first and last breakpoints in the timing
//! envelope, and each Breakpoint will represent the parameters of the
//! original Partial at the time that is the value of the timing envelope
//! at that instant.
//!
//! Resampling is performed in-place.
//!
//! \param  p is the Partial to resample
//!
//! \param  timingEnv is the timing envelope, a map of Breakpoint
//!         times in resampled Partials onto parameter sampling
//!         instants in the original Partials.
//!
//! \throw  InvalidArgument if timingEnv has any negative breakpoint
//!         times or values.
//
void
Resampler::resample(Partial &p, const LinearEnvelope &timingEnv) const
{
    Assert(0 != timingEnv.size());

    //	create the new Partial:
    Partial newp;
    newp.setLabel(p.label());

    //  find the extent of the timing envelope, if specified, otherwise
    //  the insert time range is the same as the sample time range:
    double firstInsertTime =
        interval_ * int(0.5 + timingEnv.begin()->first / interval_);
    double lastInsertTime = (--timingEnv.end())->first + (0.5 * interval_);

    //  resample:
    for (double insertTime = firstInsertTime; insertTime <= lastInsertTime;
         insertTime += interval_)
    {
        //  sample time is obtained from the timing envelope:
        double sampleTime = timingEnv.valueAt(insertTime);

        //  make a resampled Breakpoint:
        Breakpoint newbp = p.parametersAt(sampleTime);

        newp.insert(insertTime, newbp);
    }

    //  remove excess null Breakpoints at the ends of the newly-formed
    //  Partial, no simple way to anticipate these, without evaluating
    //  the timing envelope at all points.
    //
    //  Also runs of nulls in the middle?
    Partial::iterator it = newp.begin();
    while (it != newp.end() && 0 == it->amplitude())
    {
        ++it;
    }
    newp.erase(newp.begin(), it);

    it = newp.end();
    while (it != newp.begin() && 0 == (--it)->amplitude())
    {
    }
    if (it != newp.end())
    {
        newp.erase(++it, newp.end());
    }

    //  is this a good idea? generally not.
    if (phaseCorrect_ && (0 != newp.numBreakpoints()))
    {
        fixFrequency(newp); // use default maxFixPct
    }

    //	store the new Partial:
    p = newp;
}

// ---------------------------------------------------------------------------
//	quantize
// ---------------------------------------------------------------------------
//! Quantize the Breakpoint times using the specified Partial using the
//! stored quanitization interval. Each Breakpoint in the Partial is
//! replaced by a Breakpoint constructed by resampling the Partial at
//! the nearest integer multiple of the of the resampling interval.
//!
//! Quantization is performed in-place.
//!
//! \param  p is the Partial to resample
//
void
Resampler::quantize(Partial &p) const
{
    //  for phase-correct quantization, first make the phases correct by
    //  fixing them from the initial phase (ideally this should have
    //  no effect but there's no way to be phase-correct after quantization
    //  unless the phases start correct), then quantize the Breakpoint
    //  times, then afterwards, adjust the frequencies to match
    //  the interpolated phases:
    if (phaseCorrect_)
    {
        fixPhaseForward(p.begin(), --p.end());
    }

    //	create the new Partial:
    Partial newp;
    newp.setLabel(p.label());

    Partial::const_iterator iter = p.begin();
    while (iter != p.end())
    {
        const Breakpoint &bp = iter.breakpoint();
        double bpt = iter.time();

        //  find the nearest multiple of the quantization interval:
        long qstep = long(0.5 + (bpt / interval_));

        long endstep = qstep - 1; //  guarantee first insertion
        if (newp.numBreakpoints() != 0)
        {
            endstep = long(0.5 + (newp.endTime() / interval_));
        }

        //  insert a new Breakpoint if it does not duplicate
        //  a previous insertion, or if it is a Null (needed
        //  for phase-correction):
        if ((endstep != qstep) || (0 == bp.amplitude()))
        {
            double qt = interval_ * qstep;

            //  insert another Breakpoint and advance the Breakpoint
            //  iterator and the current time:
            //
            //  sample the Partial with a long fade time so that
            //  the amplitudes at the ends keep their original values:
            const double a_long_time = 1.;
            Breakpoint newbp = p.parametersAt(qt, a_long_time);
            Partial::iterator new_pos = newp.insert(qt, newbp);

            //  tricky: if the quantized position (iter) is a null Breakpoint,
            //  we had better made the new position a null also, very important
            //  for making phase resets happen at synthesis time.
            //
            //  Also, if new_pos is earlier than iter, the phase should be
            //  rolled back from iter, rather than interpolated. If new_pos is
            //  later than iter, then its phase will have been correctly
            //  interpolated.
            if (0 == bp.amplitude())
            {
                new_pos.breakpoint().setAmplitude(0);

                if (new_pos.time() < bpt)
                {
                    double dp = phaseTravel(new_pos.breakpoint(), bp,
                                            bpt - new_pos.time());
                    new_pos.breakpoint().setPhase(bp.phase() - dp);
                }
            }
        }
        ++iter;
    }

    //  for phase-correct quantization, adjust the frequencies to match
    //  the interpolated phases:
    if (phaseCorrect_)
    {
        fixFrequency(newp, 5);
    }

    //	store the new Partial:
    p = newp;
}

// ---------------------------------------------------------------------------
//	is_empty_Partial (helper)
// ---------------------------------------------------------------------------
// Predicate returning true if a Partial has no Breakpoints, used to prune
// away empties after resampling.

static bool
is_empty_Partial(Partial &p)
{
    return 0 == p.numBreakpoints();
}

// ---------------------------------------------------------------------------
//	resample (sequence of Partials)
// ---------------------------------------------------------------------------
//! Resample all Partials in the specified PartialList using this
//! Resampler's stored quanitization interval. The Breakpoint times in
//! resampled Partials will comprise a contiguous sequence of all integer
//! multiples of the sampling interval, starting and ending with the nearest
//! multiples to the ends of the Partial. If phase correct resampling is
//! specified (the default)≤ frequencies and phases are corrected to be in
//! agreement and to match as nearly as possible the resampled phases.
//!
//! Resampling is performed in-place (the PartialList is modified).
//!
//!	\param plist is the container of Partials to resample
//
void
Resampler::resample(PartialList &plist) const
{
    std::for_each(plist.begin(), plist.end(), *this);
    /*
    for ( PartialList::iterator it = plist.begin(); it != plist.end(); ++ it )
    {
        resample( *it );
    }
    */

    //  prune away empties
    plist.erase(std::remove_if(plist.begin(), plist.end(), is_empty_Partial),
                plist.end());
}

// ---------------------------------------------------------------------------
//	resample (sequence of Partials, with timing envelope)
// ---------------------------------------------------------------------------
//! Resample all Partials in the specified PartialList using this
//! Resampler's stored sampling interval, so that the Breakpoints in
//! the Partial envelopes will all lie on a common temporal grid.
//! The Breakpoint times in the resampled Partial will comprise a
//! contiguous sequence of integer multiples of the sampling interval,
//! beginning with the multiple nearest to the Partial's start time and
//! ending with the multiple nearest to the Partial's end time. Resampling
//! is performed in-place.
//!
//!	\param plist is the container of Partials to resample
//! \param  timingEnv is the timing envelope, a map of Breakpoint
//!         times in resampled Partials onto parameter sampling
//!         instants in the original Partials.
//!
//
void
Resampler::resample(PartialList &plist, const LinearEnvelope &timingEnv) const
{
    for (PartialList::iterator it = plist.begin(); it != plist.end(); ++it)
    {
        resample(*it, timingEnv);
    }

    //  prune away empties
    plist.erase(std::remove_if(plist.begin(), plist.end(), is_empty_Partial),
                plist.end());
}

// ---------------------------------------------------------------------------
//	quantize (sequence of Partials)
// ---------------------------------------------------------------------------
//! Quantize all Partials in the specified PartialList.
//! Each Breakpoint in the Partials is replaced by a Breakpoint
//! constructed by resampling the Partial at the nearest
//! integer multiple of the of the resampling interval.
//!
//!	\param plist is the container of Partials to quantize
//!
//
void
Resampler::quantize(PartialList &plist) const
{
    for (PartialList::iterator it = plist.begin(); it != plist.end(); ++it)
    {
        quantize(*it);
    }

    //  prune away empties
    plist.erase(std::remove_if(plist.begin(), plist.end(), is_empty_Partial),
                plist.end());
}

} // namespace Loris
