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
 * PartialList.C
 *
 * Definition of Loris::PartialList class members.
 *
 * Kelly Fitz, 15 Feb 2011
 * loris@cerlsoundgroup.org
 *
 * http://www.cerlsoundgroup.org/Loris/
 *
 */

#include "PartialList.h"

#include "Notifier.h"

//	begin namespace
namespace Loris
{

// ---------------------------------------------------------------------------
//	constructor (default)
// ---------------------------------------------------------------------------
//! Construct an empty PartialList
//
PartialList::PartialList(void) :
    mList(new list_of_Partials_type)
{
    // debugger << " -- PartialList default constructor" << endl;
}

// ---------------------------------------------------------------------------
//	constructor (copy)
// ---------------------------------------------------------------------------
//! Construct a PartialList that is a copy of another.
//! Partials are not immediately copied, the underlying
//! container is shared throught the smart pointer until
//! non-const access is required (through any non-const
//! member function).
//
PartialList::PartialList(const PartialList &rhs) :
    mList(rhs.mList)
{
    // debugger << " -- PartialList copy " << rhs.size() << " Partials" << endl;
}

// ---------------------------------------------------------------------------
//	destructor
// ---------------------------------------------------------------------------
//! Destroy a PartialList. The underlying container is
//! destroyed only if it is not referenced by any other
//! PartialList.
//
PartialList::~PartialList(void)
{
    // debugger << " -- PartialList destroy " << size() << " Partials" << endl;
}

// ---------------------------------------------------------------------------
//	operator= (assignment)
// ---------------------------------------------------------------------------
//! Assign the contents of a PartialList to this PartialList.
//! Partials are not immediately copied, the underlying
//! container is shared throught the smart pointer until
//! non-const access is required (through any non-const
//! member function).
//
PartialList &
PartialList::operator=(const PartialList &rhs)
{
    // debugger << " -- PartialList assign " << rhs.size() << " Partials" <<
    // endl;

    mList = rhs.mList;

    return *this;
}

// ---------------------------------------------------------------------------
//	extract
// ---------------------------------------------------------------------------
//! Remove a range of Partials from this List and return a new List containing
//! those Partials.
//!
//! \param  b beginning of a range of Partials in this PartialList
//! \param  e end of a range of Partials in this PartialList
//! \return a new PartialList containing the Partials in the half-open range
//! [b,e)
//! \post   Partials in the range [b,e) are removed from this List
//! \pre    [b,e) must describe a valid range of Partials in this List
//
PartialList
PartialList::extract(iterator b, iterator e)
{
    PartialList ret;
    ret.mList->splice(ret.begin(), *mList, b, e);

    // debugger << " -- PartialList extract " << ret.size() << " Partials" <<
    // endl;

    return ret;
}

} // namespace Loris
