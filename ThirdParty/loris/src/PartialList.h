#ifndef INCLUDE_PARTIALLIST_H
#define INCLUDE_PARTIALLIST_H
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
 * PartialList.h
 *
 * Definition of class Loris::PartialList class, mostly a wrapper for
 * std::list< Partial >, to which most operations are forwarded.
 *
 * Kelly Fitz, 15 Feb 2011
 * loris@cerlsoundgroup.org
 *
 * http://www.cerlsoundgroup.org/Loris/
 *
 */

#include "Notifier.h"
#include "Partial.h"
#include "PtrCopyOnWrite.h"

#include <functional>
#include <list>

//	begin namespace
namespace Loris
{

// ---------------------------------------------------------------------------
//	clone (non-member)
// ---------------------------------------------------------------------------
//! Cloning operation used by the reference-counting copy-on-write pointer
//! class that is used to maintain the std::list and avoid unnecessary
//! copying. Specialization of the template cone function in PtrCopyOnWrite.h.
//! This is the operation that is invoked when the underlying container needs
//! to be duplicated, any time non-const access is required of a shared
//! instance.
//
template <>
inline std::list<Partial> *
clone<std::list<Partial>>(const std::list<Partial> *tp)
{
    debugger << " +++ cloning list of " << tp->size() << " Partials" << endl;
    return new std::list<Partial>(*tp);
}

// ---------------------------------------------------------------------------
//	class PartialList
//
//!	PartialList is a wrapper for a std::list<> of Loris Partials.
//! PartialList implements many members of the std::list interface
//! by simply forwarding them to the underlying container.
//!
//!	The associated bidirectional iterators are also defined as
//!	PartialListIterator and PartialListConstIterator.
//
class PartialList
{

  private:
    //  --- private types ---

    typedef std::list<Partial> list_of_Partials_type;
    typedef Ptr<list_of_Partials_type> list_ptr_type;

    //  --- member variables ---

    //! Smart pointer with copy-on-write behavior, wrapping the
    //! underlying container of Partials.
    list_ptr_type mList;

  public:
    //  --- types ---

    typedef std::list<Partial>::size_type size_type;
    typedef std::list<Partial>::iterator iterator;
    typedef std::list<Partial>::const_iterator const_iterator;
    typedef std::list<Partial>::reference reference;
    typedef std::list<Partial>::const_reference const_reference;
    typedef std::list<Partial>::value_type value_type;

    //  --- lifecycle ---

    //!  Construct an empty PartialList
    PartialList(void);

    //! Construct a PartialList containing copies of the Partials in the
    //! range [b,e). Partials in the specified range are copied (immediately),
    //! not shared through the smart pointer.
    //!
    //! Same as std::list range constructor.
    template <class InIt>
    PartialList(InIt b, InIt e) :
        mList(new list_of_Partials_type(b, e))
    {
    }

    //! Construct a PartialList that is a copy of another.
    //! Partials are not immediately copied, the underlying
    //! container is shared throught the smart pointer until
    //! non-const access is required (through any non-const
    //! member function).
    PartialList(const PartialList &rhs);

    //! Destroy a PartialList. The underlying container is
    //! destroyed only if it is not referenced by any other
    //! PartialList.
    ~PartialList(void);

    //! Assign the contents of a PartialList to this PartialList.
    //! Partials are not immediately copied, the underlying
    //! container is shared throught the smart pointer until
    //! non-const access is required (through any non-const
    //! member function).
    PartialList &operator=(const PartialList &rhs);

    //  --- access and mutation ---

    //  extract
    //! Remove a range of Partials from this List and return a new List
    //! containing those Partials.
    //!
    //! \param  b beginning of a range of Partials in this PartialList
    //! \param  e end of a range of Partials in this PartialList
    //! \return a new PartialList containing the Partials in the half-open range
    //! [b,e)
    //! \post   Partials in the range [b,e) are removed from this List
    //! \pre    [b,e) must describe a valid range of Partials in this List
    PartialList extract(iterator b, iterator e);

    //  --- std::list interface ---

    //  PartialList implements many members of the std::list interface
    //  by simply forwarding them to the underlying container.

    //  iterator access

    //! Same as the corresponding member of std::list.
    iterator
    begin(void)
    {
        return mList->begin();
    }
    //! Same as the corresponding member of std::list.
    iterator
    end(void)
    {
        return mList->end();
    }

    //! Same as the corresponding member of std::list.
    const_iterator
    begin(void) const
    {
        return mList->begin();
    }
    //! Same as the corresponding member of std::list.
    const_iterator
    end(void) const
    {
        return mList->end();
    }

    //  container access and mutation

    //! Same as the corresponding member of std::list.
    Partial &
    front(void)
    {
        return mList->front();
    }
    //! Same as the corresponding member of std::list.
    const Partial &
    front(void) const
    {
        return mList->front();
    }

    //! Same as the corresponding member of std::list.
    Partial &
    back(void)
    {
        return mList->back();
    }
    //! Same as the corresponding member of std::list.
    const Partial &
    back(void) const
    {
        return mList->back();
    }

    //! Same as the corresponding member of std::list.
    void
    push_back(const Partial &val)
    {
        mList->push_back(val);
    }
    //! Same as the corresponding member of std::list.
    void
    push_front(const Partial &val)
    {
        mList->push_front(val);
    }

    //! Same as the corresponding member of std::list.
    iterator
    insert(iterator where, const Partial &val)
    {
        return mList->insert(where, val);
    }

    //! Same as the corresponding member of std::list.
    template <class InIt>
    void
    insert(iterator where, InIt first, InIt last)
    {
        mList->insert(where, first, last);
    }

    //! Same as the corresponding member of std::list.
    iterator
    erase(iterator where)
    {
        return mList->erase(where);
    }

    //! Same as the corresponding member of std::list.
    iterator
    erase(iterator first, iterator last)
    {
        return mList->erase(first, last);
    }

    //! Same as the corresponding member of std::list.
    void
    clear(void)
    {
        // mList->clear();
        //   possibly more efficient to construct a new list, if the
        //   Partials are shared with another PartialList, clear will
        //   trigger a copy immediately before erasing all of the
        //   Partials.
        mList = list_ptr_type(new list_of_Partials_type);
    }

    //  query

    //! Same as the corresponding member of std::list.
    bool
    empty(void) const
    {
        return mList->empty();
    }

    //! Same as the corresponding member of std::list.
    size_type
    size(void) const
    {
        return mList->size();
    }

    //  sorting

    //! Same as the corresponding member of std::list.
    template <class Comparitor>
    void
    sort(Comparitor c)
    {
        mList->sort(c);
    }

    //  splicing

    //! Same as the corresponding member of std::list.
    //! Transfer the Partials from one List to this List, same as
    //! std::list::.splice
    //!
    //! \param  pos is the position in this List at which to insert the absorbed
    //!         Partials
    //! \param  other is the List of Partials to absorb into this List
    //!
    //! \post   other is an empty List, its former contents have been transfered
    //!         to this List
    //! \pre    pos is a valid position in this List
    //!
    //! \sa std::list::splice
    //
    void
    splice(iterator pos, PartialList &other)
    {
        mList->splice(pos, *other.mList);
    }

    //! Same as the corresponding member of std::list.
    //! Transfer the Partials from one List to this List, same as
    //! std::list::.splice
    //!
    //! \param  pos is the position in this List at which to insert the absorbed
    //!         Partials
    //! \param  other is the List of Partials to absorb into this List
    //! \param  first is the beginning of the sequence in other to transfer to
    //! this list
    //!
    //! \post   other is an empty List, its former contents have been transfered
    //!         to this List
    //! \pre    pos is a valid position in this List
    //! \pre    first is a valid position on other
    //!
    //! \sa std::list::splice
    //
    void
    splice(iterator pos, PartialList &other, iterator first)
    {
        mList->splice(pos, *other.mList, first);
    }

    //! Same as the corresponding member of std::list.
    //! Transfer the Partials from one List to this List, same as
    //! std::list::.splice
    //!
    //! \param  pos is the position in this List at which to insert the absorbed
    //!         Partials
    //! \param  other is the List of Partials to absorb into this List
    //! \param  first is the beginning of the sequence in other to transfer to
    //! this list
    //! \param  last is the end of the sequence in other to transfer to this
    //! list
    //!
    //! \post   other is an empty List, its former contents have been transfered
    //!         to this List
    //! \pre    pos is a valid position in this List
    //! \pre    [first,last) is a valid sequence in other
    //!
    //! \sa std::list::splice
    //
    void
    splice(iterator pos, PartialList &other, iterator first, iterator last)
    {
        mList->splice(pos, *other.mList, first, last);
    }

}; //   end of class PartialList

// --- typedefs for iterators ---

typedef PartialList::iterator PartialListIterator;
typedef PartialList::const_iterator PartialListConstIterator;

} // namespace Loris

#endif /* ndef INCLUDE_PARTIALLIST_H */
