#ifndef INCLUDE_PTRCOPYONWRITE_H
#define INCLUDE_PTRCOPYONWRITE_H
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
 * PtrCopyOnWrite.h
 *
 * Template reference-counting copy-on-write pointer class, adapted from the
 * Ptr template in "Accelerated C++" by Koenig and Moo.
 *
 * Kelly Fitz, 25 Feb 2011
 * loris@cerlsoundgroup.org
 *
 * http://www.cerlsoundgroup.org/Loris/
 *
 */

#include <cstddef>
#include <stdexcept>

//  begin namespace
namespace Loris
{

// ---------------------------------------------------------------------------
//  template class Ptr
//
//! Reference counting smart pointer template class supporting copy-on-write
//! semantics, mostly copied from Koenig and Moo, Accelerated C++.

template <class T> class Ptr
{

  public:
    //  --- lifecycle ---

    //! Construct a new pointer to nothing
    Ptr() :
        refptr(new size_t(1)),
        p(0)
    {
    }

    //! Construct a new pointer an initialize it to point to something,
    //! first counted reference.
    Ptr(T *t) :
        refptr(new size_t(1)),
        p(t)
    {
    }

    //! Construct a new pointer and initialize it to point to a shared
    //! resource, increment the reference count.
    Ptr(const Ptr &h) :
        refptr(h.refptr),
        p(h.p)
    {
        ++*refptr;
    }

    //! Assignment
    //! Release any previously-managed resources, if there were no other
    //! references, and share a reference to a resource with rhs.
    Ptr &operator=(const Ptr &);

    //! Destructor
    //! Release any managed resources, if there were no other references,
    //! otherwise just decrement the reference count.
    ~Ptr();

    //! Conversion to bool, return true if Ptr is bound to some object,
    //! false otherwise.
    operator bool(void) const
    {
        return p;
    }

    //! Dereference
    //! Non-const dereference triggers a copy of a shared resource through
    //! make_unique. Const dereference does not, shared resource remains shared.
    T &operator*(void);

    //! Dereference
    //! Non-const dereference triggers a copy of a shared resource through
    //! make_unique. Const dereference does not, shared resource remains shared.
    T *operator->(void);

    //! Dereference
    //! Non-const dereference triggers a copy of a shared resource through
    //! make_unique. Const dereference does not, shared resource remains shared.
    const T &operator*(void) const;

    //! Dereference
    //! Non-const dereference triggers a copy of a shared resource through
    //! make_unique. Const dereference does not, shared resource remains shared.
    const T *operator->(void) const;

  private:
    //	-- implementation --

    T *p; //! managed resource

#ifdef _MSC_VER
    size_t *refptr; //! shared reference counter
#else
    std::size_t *refptr; //! shared reference counter
#endif

    //! Private member to copy the shared resource
    //! conditionally when needed. Invoked automatically
    //! when (before) non-const access is granted.
    //! Invokes template clone() function (non-member) which
    //! must be implemented for the managed type. Default
    //! implementation invokes a clone() member function.
    void
    make_unique(void)
    {
        if (*refptr != 1)
        {
            --*refptr;
            refptr = new size_t(1);
            p = p ? clone(p) : 0;
        }
    }

}; //  end of class Ptr

// ---------------------------------------------------------------------------
//  clone - default template implementation
// ---------------------------------------------------------------------------
//  Implement this function for any managed (by Ptr) type that does not
//  support a clone() operation.
//
template <class T>
T *
clone(const T *tp)
{
    return tp->clone();
}

// ---------------------------------------------------------------------------
//  pointer dereference
// ---------------------------------------------------------------------------
//  Non-const dereference triggers a copy of a shared resource through
//  make_unique. Const dereference does not, shared resource remains shared.

template <class T>
T &
Ptr<T>::operator*(void)
{
    make_unique();
    if (p)
    {
        return *p;
    }
    throw std::runtime_error("unbound Ptr");
}

template <class T>
T *
Ptr<T>::operator->(void)
{
    make_unique();
    if (p)
    {
        return p;
    }
    throw std::runtime_error("unbound Ptr");
}

template <class T>
const T &
Ptr<T>::operator*(void) const
{
    if (p)
    {
        return *p;
    }
    throw std::runtime_error("unbound Ptr");
}

template <class T>
const T *
Ptr<T>::operator->(void) const
{
    if (p)
    {
        return p;
    }
    throw std::runtime_error("unbound Ptr");
}

// ---------------------------------------------------------------------------
//  assignment
// ---------------------------------------------------------------------------
//  Release any previously-managed resources, if there were no other
//  references, and share a reference to a resource with rhs.
//
template <class T>
Ptr<T> &
Ptr<T>::operator=(const Ptr &rhs)
{
    ++*rhs.refptr;
    // free the lhs, destroying pointers if appropriate
    if (--*refptr == 0)
    {
        delete refptr;
        delete p;
    }

    // copy in values from the right-hand side
    refptr = rhs.refptr;
    p = rhs.p;
    return *this;
}

// ---------------------------------------------------------------------------
//  destructor
// ---------------------------------------------------------------------------
//  Release any managed resources, if there were no other references,
//  otherwise just decrement the reference count.
//
template <class T> Ptr<T>::~Ptr()
{
    if (--*refptr == 0)
    {
        delete refptr;
        delete p;
    }
}

} //  end of namespace Loris

#endif /* def INCLUDE_PTRCOPYONWRITE_H */
