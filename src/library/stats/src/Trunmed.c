/*CXXR $Id$
 *CXXR
 *CXXR This file is part of CXXR, a project to refactor the R interpreter
 *CXXR into C++.  It may consist in whole or in part of program code and
 *CXXR documentation taken from the R project itself, incorporated into
 *CXXR CXXR (and possibly MODIFIED) under the terms of the GNU General Public
 *CXXR Licence.
 *CXXR 
 *CXXR CXXR is Copyright (C) 2008-9 Andrew R. Runnalls, subject to such other
 *CXXR copyrights and copyright restrictions as may be stated below.
 *CXXR 
 *CXXR CXXR is not part of the R project, and bugs and other issues should
 *CXXR not be reported via r-bugs or other R project channels; instead refer
 *CXXR to the CXXR website.
 *CXXR */

/* Copyright (C) 1995   Berwin A. Turlach <berwin@alphasun.anu.edu.au>
 * Copyright (C) 2000-2 Martin Maechler <maechler@stat.math.ethz.ch>
 * Copyright (C) 2003   The R Foundation

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, a copy is available at
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* These routines implement a running median smoother according to the
 * algorithm described in Haerdle und Steiger (1995).
 *
 * A tech-report of that paper is available under
 * ftp://amadeus.wiwi.hu-berlin.de/pub/papers/sfb/sfb1994/dpsfb940015.ps.Z
 *
 * The current implementation does not use any global variables!
 */

/* Changes for R port by Martin Maechler ((C) above):
 *
 *  s/long/int/			R uses int, not long (as S does)
 *  s/void/static void/		most routines are internal
 *
 * Added  print_level  and end_rule  arguments
 */

/* this includes R.h, hence lots of system headers.
   Avoid their name, e.g. for heapsort() */
#include "modreg.h"

/* Variable	name	descri- | Identities from paper
 * name here	paper	ption   | (1-indexing)
 * ---------    -----	-----------------------------------
 * window[]      H      the array containing the double heap
 * data[]        X      the data (left intact)
 * ...		 i	1st permuter  H[i[m]]    == X[i + m]
 * ...		 j	2nd permuter  X[i +j[m]] == H[m]
 */

static void
swap(int l, int r, double *window, int *outlist, int *nrlist, int print_level)
{
    /* swap positions `l' and `r' in window[] and nrlist[]
     *
     * ---- Used in R_heapsort() and many other routines
     */
    int nl, nr;
    double tmp;

    if(print_level >= 3) Rprintf("SW(%d,%d) ", l,r);
    tmp = window[l]; window[l]	    = window[r];  window[r] = tmp;
    nl	= nrlist[l]; nrlist[l] = (nr= nrlist[r]); nrlist[r] = nl;

    outlist[nl/* = nrlist[r] */] = r;
    outlist[nr/* = nrlist[l] */] = l;
}

static void
siftup(int l, int r, double *window, int *outlist, int *nrlist, int print_level)
{
/* Used only in R_heapsort() */
    int i, j, nrold;
    double x;

    if(print_level >= 2) Rprintf("siftup(%d,%d) ", l,r);
    i	  = l;
    j	  = 2*i;
    x	  = window[i];
    nrold = nrlist[i];
    while (j <= r) {
	if (j < r)
	    if (window[j] < window[j+1])
		j++;
	if (x >= window[j])
	    break;

	window[i]	   = window[j];
	outlist[nrlist[j]] = i;
	nrlist[i]	   = nrlist[j];
	i = j;
	j = 2*i;
    }
    window[i]	   = x;
    outlist[nrold] = i;
    nrlist[i]	   = nrold;
}

static void
R_heapsort(int low, int up, double *window, int *outlist, int *nrlist,
	 int print_level)
{
    int l, u;

    l = (up/2) + 1;
    u = up;
    while(l > low) {
	l--;
	siftup(l, u, window, outlist, nrlist, print_level);
    }
    while(u > low) {
	swap(l, u, window, outlist, nrlist, print_level);
	u--;
	siftup(l, u, window, outlist, nrlist, print_level);
    }
}

static void
inittree(int n, int k, int k2, const double *data, double *window,
	 int *outlist, int *nrlist, int print_level)
{
    int i, k2p1;
    double big;

    for(i=1; i <= k; i++) { /* use 1-indexing for our three arrays !*/
	window[i] = data[i-1];
	nrlist[i] = outlist[i] = i;
    }

    /* sort the window[] -- sort *only* called here */
    R_heapsort(1, k, window, outlist, nrlist, print_level);

    big = fabs(window[k]);
    if (big < fabs(window[1]))
	big = fabs(window[1]);
    /* big := max | X[1..k] | */
    for(i=k; i < n; i++)
	if (big < fabs(data[i]))
	    big = fabs(data[i]);
    /* big == max(|data_i|,  i = 1,..,n) */
    big = 1 + 2. * big;/* such that -big < data[] < +big (strictly !) */

    for(i=k; i > 0; i--) {
	window[i+k2] = window[i];
	nrlist[i+k2] = nrlist[i]-1;
    }

    for(i=0; i<k; i++)
	outlist[i]=outlist[i+1]+k2;

    k2p1 = k2+1;
    for(i=0; i<k2p1; i++) {
	window[i]	 = -big;
	window[k+k2p1+i] =  big;
    }
} /* inittree*/

static void
toroot(int outvirt, int k, int nrnew, int outnext,
       const double *data, double *window, int *outlist, int *nrlist,
       int print_level)
{
    int father;

    if(print_level >= 2) Rprintf("toroot(%d, %d,%d) ", k, nrnew, outnext);

    do {
	father			  = outvirt/2;
	window[outvirt+k]	  = window[father+k];
	outlist[nrlist[father+k]] = outvirt+k;
	nrlist[outvirt+k]	  = nrlist[father+k];
	outvirt			  = father;
    } while (father != 0);
    window[k]	     = data[nrnew];
    outlist[outnext] = k;
    nrlist[k]	     = outnext;
}

static void
downtoleave(int outvirt, int k,
	    double *window, int *outlist, int *nrlist, int print_level)
{
    int childl, childr;

    if(print_level >= 2) Rprintf("\n downtoleave(%d, %d)\n   ", outvirt,k);
    for(;;) {
	childl = outvirt*2;
	childr = childl-1;
	if (window[childl+k] < window[childr+k])
	    childl = childr;
	if (window[outvirt+k] >= window[childl+k])
	    break;
	/* seg.fault happens here: invalid outvirt/childl ? */
	swap(outvirt+k, childl+k, window, outlist, nrlist, print_level);
	outvirt = childl;
    }
}

static void
uptoleave(int outvirt, int k,
	  double *window, int *outlist, int *nrlist, int print_level)
{
    int childl, childr;

    if(print_level >= 2) Rprintf("\n uptoleave(%d, %d)\n   ", outvirt,k);
    for(;;) {
	childl = outvirt*2;
	childr = childl+1;
	if (window[childl+k] > window[childr+k])
	    childl = childr;
	if (window[outvirt+k] <= window[childl+k])
	    break;
	swap(outvirt+k, childl+k, window, outlist, nrlist, print_level);
	outvirt = childl;
    }
}

static void
upperoutupperin(int outvirt, int k,
		double *window, int *outlist, int *nrlist, int print_level)
{
    int father;

    if(print_level >= 2) Rprintf("\nUpperoutUPPERin(%d, %d)\n  ", outvirt,k);
    uptoleave(outvirt, k, window, outlist, nrlist, print_level);
    father = outvirt/2;
    while (window[outvirt+k] < window[father+k]) {
	swap(outvirt+k, father+k, window, outlist, nrlist, print_level);
	outvirt = father;
	father	= outvirt/2;
    }
    if(print_level >= 2) Rprintf("\n");
}

static void
upperoutdownin(int outvirt, int k, int nrnew, int outnext,
	       const double *data, double *window, int *outlist, int *nrlist,
	       int print_level)
{
    if(print_level >= 2) Rprintf("\n__upperoutDOWNin(%d, %d)\n  ", outvirt,k);
    toroot(outvirt, k, nrnew, outnext, data, window, outlist, nrlist, print_level);
    if(window[k] < window[k-1]) {
	swap(k, k-1, window, outlist, nrlist, print_level);
	downtoleave(/*outvirt = */ -1, k, window, outlist, nrlist, print_level);
    }
}

static void
downoutdownin(int outvirt, int k,
	      double *window, int *outlist, int *nrlist, int print_level)
{
    int father;

    if(print_level >= 2) Rprintf("\nDownoutDOWNin(%d, %d)\n  ", outvirt,k);
    downtoleave(outvirt, k, window, outlist, nrlist, print_level);
    father = outvirt/2;
    while (window[outvirt+k] > window[father+k]) {
	swap(outvirt+k, father+k, window, outlist, nrlist, print_level);
	outvirt = father;
	father	= outvirt/2;
    }
    if(print_level >= 2) Rprintf("\n");
}

static void
downoutupperin(int outvirt, int k, int nrnew, int outnext,
	       const double *data, double *window, int *outlist, int *nrlist,
	       int print_level)
{
    if(print_level >= 2) Rprintf("\n__downoutUPPERin(%d, %d)\n  ", outvirt,k);
    toroot(outvirt, k, nrnew, outnext, data, window, outlist, nrlist, print_level);
    if(window[k] > window[k+1]) {
	swap(k, k+1, window, outlist, nrlist, print_level);
	uptoleave(/*outvirt = */ +1, k, window, outlist, nrlist, print_level);
    }
}

static void
wentoutone(int k, double *window, int *outlist, int *nrlist, int print_level)
{
    if(print_level >= 2) Rprintf("\nwentOUT_1(%d)\n  ", k);
    swap(k, k+1, window, outlist, nrlist, print_level);
    uptoleave(/*outvirt = */ +1, k, window, outlist, nrlist, print_level);
}

static void
wentouttwo(int k, double *window, int *outlist, int *nrlist, int print_level)
{
    if(print_level >= 2) Rprintf("\nwentOUT_2(%d)\n  ", k);
    swap(k, k-1, window, outlist, nrlist, print_level);
    downtoleave(/*outvirt = */ -1, k, window, outlist, nrlist, print_level);
}

/* For Printing `diagnostics' : */
#define Rm_PR(_F_,_A_)		for(j = 0; j <= 2*k; j++) Rprintf(_F_, _A_)
#define RgPRINT_j(A_J)		Rm_PR("%6g", A_J); Rprintf("\n")
#define RdPRINT_j(A_J)		Rm_PR("%6d", A_J); Rprintf("\n")

#define R_PRINT_4vec()							\
	Rprintf(" %9s: ","j");        RdPRINT_j(j);			\
	Rprintf(" %9s: ","window []");RgPRINT_j(window[j]);		\
	Rprintf(" %9s: "," nrlist[]");RdPRINT_j(nrlist[j]);		\
	Rprintf(" %9s: ","outlist[]");RdPRINT_j((j <= k2|| j > k+k2)? -9\
						: outlist[j - k2])

static void
runmedint(int n, int k, int k2, const double *data, double *median,
	  double *window, int *outlist, int *nrlist,
	  int end_rule, int print_level)
{
    /* Running Median of `k' ,  k == 2*k2 + 1 *
     * end_rule == 0: leave values at the end,
     *          otherwise: "constant" end values
     */
    int i, nrnew, outnext, out, outvirt;

    if(end_rule)
	for(i = 0; i <= k2; median[i++] = window[k]);
    else {
	for(i = 0; i <  k2; median[i] = data[i], i++);
	median[k2] = window[k];
    }
    outnext = 0;
    for(i = k2+1; i < n-k2; i++) {/* compute (0-index) median[i] == X*_{i+1} */
	out	    = outlist[outnext];
	nrnew	    = i+k2;
	window[out] = data[nrnew];
	outvirt	    = out-k;
	if (out > k)
	    if(data[nrnew] >= window[k])
		upperoutupperin(outvirt, k, window, outlist, nrlist, print_level);
	    else
		upperoutdownin(outvirt, k, nrnew, outnext,
			       data, window, outlist, nrlist, print_level);
	else if(out < k)
	    if(data[nrnew] < window[k])
		downoutdownin(outvirt, k, window, outlist, nrlist, print_level);
	    else
		downoutupperin(outvirt, k, nrnew, outnext,
			       data, window, outlist, nrlist, print_level);
	else if(window[k] > window[k+1])
	    wentoutone(k, window, outlist, nrlist, print_level);
	else if(window[k] < window[k-1])
	    wentouttwo(k, window, outlist, nrlist, print_level);
	median[i] = window[k];
	outnext	  = (outnext+1)%k;
    }
    if(end_rule)
	for(i = n-k2; i < n; median[i++] = window[k]);
    else
	for(i = n-k2; i < n; median[i] = data[i], i++);
}/* runmedint() */

/* This is the function called from R or S: */
void Trunmed(Sint *nn,/* = length(data) */
	     Sint *kk,/* is odd <= nn */
	     const double *data,
	     double *median, /* (n) */
	     Sint   *outlist,/* (k+1) */
	     Sint   *nrlist,/* (2k+1) */
	     double *window,/* (2k+1) */
	     Sint   *end_rule,
	     Sint   *print_level)
{
    int k = (int)*kk,
	k2 = (k - 1)/2, /* k == *kk == 2 * k2 + 1 */
	j;

    inittree ((int)*nn, k, k2, data,
	      /* initialize these: */
	      window, (int *)outlist, (int *)nrlist, (int) *print_level);

    /* window[], outlist[], and nrlist[] are all 1-based (indices) */

    if(*print_level) {
	Rprintf("After inittree():\n");
	R_PRINT_4vec();
    }
    runmedint((int)*nn, k, k2, data, median,
	      window, (int *)outlist, (int *)nrlist,
	      (int) *end_rule, (int) *print_level);
}
