/*CXXR $Id$
 *CXXR
 *CXXR This file is part of CXXR, a project to refactor the R interpreter
 *CXXR into C++.  It may consist in whole or in part of program code and
 *CXXR documentation taken from the R project itself, incorporated into
 *CXXR CXXR (and possibly MODIFIED) under the terms of the GNU General Public
 *CXXR Licence.
 *CXXR 
 *CXXR CXXR is Copyright (C) 2008 Andrew R. Runnalls, subject to such other
 *CXXR copyrights and copyright restrictions as may be stated below.
 *CXXR 
 *CXXR CXXR is not part of the R project, and bugs and other issues should
 *CXXR not be reported via r-bugs or other R project channels; instead refer
 *CXXR to the CXXR website.
 *CXXR */

/*
 *  R : A Computer Language for Statistical Data Analysis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, a copy is available at
 *  http://www.r-project.org/Licenses/
 */

/** @file RAllocStack.cpp
 *
 * Implementation of class RAllocStack and related functions.
 */

#include "CXXR/RAllocStack.h"

#include <stdexcept>
#include "R_ext/Error.h"
#include "Rvalgrind.h"
#include "localization.h"
#include "CXXR/MemoryBank.hpp"

using namespace std;
using namespace CXXR;

// Force generation of non-inline embodiments of functions in the C
// interface:
namespace CXXR {
    namespace ForceNonInline {
	unsigned int (*vmaxgetp)(void) = vmaxget;
	void (*vmaxsetp)(unsigned int) = vmaxset;
    }
}

RAllocStack::Stack RAllocStack::s_stack;

void* RAllocStack::allocate(size_t sz)
{
    Pair pr(sz, MemoryBank::allocate(sz));
    s_stack.push(pr);
    void* ans = s_stack.top().second;
#if VALGRIND_LEVEL == 1
    // If VALGRIND_LEVEL > 1 this will be done by CXXR::MemoryBank.
    VALGRIND_MAKE_MEM_UNDEFINED(ans, sz);
#endif
    return ans;
}

void RAllocStack::restoreSize(size_t new_size)
{
    if (new_size > s_stack.size())
	throw out_of_range("RAllocStack::restoreSize: requested size"
			   " greater than current size.");
    while (s_stack.size() > new_size) {
	Pair& top = s_stack.top();
	MemoryBank::deallocate(top.second, top.first);
	s_stack.pop();
    }
}

// ***** C interface *****

char* R_alloc(size_t num_elts, int elt_size)
{
    if (elt_size <= 0)
	Rf_error(_("R_alloc: element size must be positive."));
    size_t size = num_elts*elt_size;
    // Check for integer overflow:
    if (size/elt_size != num_elts)
	Rf_error(_("R_alloc: requested allocation is impossibly large."));
    return static_cast<char*>(RAllocStack::allocate(size));
}

char* S_alloc(long num_elts, int elt_size)
{
    if (num_elts < 0)
	Rf_error(_("S_alloc: number of elements must be non-negative."));
    if (elt_size <= 0)
	Rf_error(_("S_alloc: element size must be positive."));
    size_t size = num_elts*elt_size;
    // Check for integer overflow:
    if (size/elt_size != size_t(num_elts))
	Rf_error(_("R_alloc: requested allocation is impossibly large."));
    char* ans = static_cast<char*>(RAllocStack::allocate(size));
    memset(ans, 0, size);
    return ans;
}
    
char* S_realloc(char *prev_block, long new_sz, long old_sz, int elt_size)
{
    if (new_sz <= old_sz)
	return prev_block;
    char* ans = S_alloc(new_sz, elt_size);
    size_t old_bytes = old_sz*elt_size;
    memcpy(ans, prev_block, old_bytes);
    memset(ans + old_sz, 0, new_sz*elt_size - old_bytes);
    return ans;
}
