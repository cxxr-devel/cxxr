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

/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 2000-7  the R Development Core Team
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <Defn.h>

/* .Internal(lapply(X, FUN)) */

/* This is a special, so has unevaluated arguments.  It is called from a
   closure wrapper, so X and FUN are promises. */

SEXP attribute_hidden do_lapply(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP R_fcall, ans, names, X, XX, FUN;
    int i, n;
    PROTECT_INDEX px;

    checkArity(op, args);
    PROTECT_WITH_INDEX(X = CAR(args), &px);
    PROTECT(XX = eval(CAR(args), rho));
    FUN = CADR(args);  /* must be unevaluated for use in e.g. bquote */
    n = length(XX);
    if (n == NA_INTEGER) error(_("invalid length"));

    PROTECT(ans = allocVector(VECSXP, n));
    names = getAttrib(XX, R_NamesSymbol);
    if(!isNull(names)) setAttrib(ans, R_NamesSymbol, names);

    /* The R level code has ensured that XX is a vector.
       If it is atomic we can speed things up slightly by
       using the evaluated version.
    */
    {
	SEXP ind, tmp;
	/* Build call: FUN(XX[[<ind>]], ...) */

	/* Notice that it is OK to have one arg to LCONS do memory
	   allocation and not PROTECT the result (LCONS does memory
	   protection of its args internally), but not both of them,
	   since the computation of one may destroy the other */

	PROTECT(ind = allocVector(INTSXP, 1));
	if(isVectorAtomic(XX))
	    PROTECT(tmp = LCONS(R_Bracket2Symbol,
				CONS(XX, CONS(ind, R_NilValue))));
	else
	    PROTECT(tmp = LCONS(R_Bracket2Symbol,
				CONS(X, CONS(ind, R_NilValue))));
	PROTECT(R_fcall = LCONS(FUN,
				CONS(tmp, CONS(R_DotsSymbol, R_NilValue))));

	for(i = 0; i < n; i++) {
	    INTEGER(ind)[0] = i + 1;
	    SET_VECTOR_ELT(ans, i, eval(R_fcall, rho));
	}
	UNPROTECT(3);
    }

    UNPROTECT(3); /* X, XX, ans */
    return ans;
}

#ifdef UNUSED
/* .Internal(apply(X, X1, FUN)) */
/* X is a matrix, and the last dimension is the one we want to
   loop over */

SEXP attribute_hidden do_apply(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP R_fcall, X, Xd, X1, ans, FUN;
    int i, j, nr, nc, inr;

    checkArity(op, args);
    X = CAR(args); args = CDR(args);
    if(!isMatrix(X))
	error(_("first argument is not a matrix"));
    Xd = getAttrib(X, R_DimSymbol);
    nr = INTEGER(Xd)[0];
    nc = INTEGER(Xd)[1];
    X1 = CAR(args); args = CDR(args);
    FUN = CAR(args);

    PROTECT(R_fcall = LCONS(FUN, LCONS(X1, LCONS(R_DotsSymbol, R_NilValue))));
    PROTECT(ans = allocVector(VECSXP, nc));
    PROTECT(X1 = allocVector(TYPEOF(X), nr));
    SETCADR(R_fcall, X1);
    for(i = 0; i < nc; i++) {
	switch(TYPEOF(X)) {
	case REALSXP:
	    for (j = 0, inr = i*nr; j < nr; j++)
		REAL(X1)[j] = REAL(X)[j + inr];
	    break;
	case INTSXP:
	    for (j = 0, inr = i*nr; j < nr; j++)
		INTEGER(X1)[j] = INTEGER(X)[j + inr];
	    break;
	case LGLSXP:
	    for (j = 0, inr = i*nr; j < nr; j++)
		LOGICAL(X1)[j] = LOGICAL(X)[j + inr];
	    break;
	case CPLXSXP:
	    for (j = 0, inr = i*nr; j < nr; j++)
		COMPLEX(X1)[j] = COMPLEX(X)[j + inr];
	    break;
	case STRSXP:
	    for (j = 0, inr = i*nr; j < nr; j++)
		SET_STRING_ELT(X1, j, STRING_ELT(X, j + inr));
	    break;
	case RAWSXP:
	    for (j = 0, inr = i*nr; j < nr; j++)
		RAW(X1)[j] = RAW(X)[j + inr];
	    break;
	default:
	    UNIMPLEMENTED_TYPE("apply", X);
	}
	/* careful: we have altered X1 and might have FUN = function(x) x */
	SET_VECTOR_ELT(ans, i, duplicate(eval(R_fcall, rho)));
    }
    UNPROTECT(3);
    return ans;
}
#endif

static SEXP do_one(SEXP X, SEXP FUN, SEXP classes, SEXP deflt,
		   Rboolean replace, SEXP rho)
{
    SEXP ans, names, klass, R_fcall;
    int i, j, n;
    Rboolean matched = FALSE;

    /* if X is a list, recurse.  Otherwise if it matches classes call f */
    if(isNewList(X)) {
	n = length(X);
	PROTECT(ans = allocVector(VECSXP, n));
	names = getAttrib(X, R_NamesSymbol);
	/* or copy attributes if replace = TRUE? */
	if(!isNull(names)) setAttrib(ans, R_NamesSymbol, names);
	for(i = 0; i < n; i++)
	    SET_VECTOR_ELT(ans, i, do_one(VECTOR_ELT(X, i), FUN, classes,
					  deflt, replace, rho));
	UNPROTECT(1);
	return ans;
    }
    if(strcmp(CHAR(STRING_ELT(classes, 0)), "ANY") == 0) /* ASCII */
	matched = TRUE;
    else {
	PROTECT(klass = R_data_class(X, FALSE));
	for(i = 0; i < LENGTH(klass); i++)
	    for(j = 0; j < length(classes); j++)
		if(Seql(STRING_ELT(klass, i), STRING_ELT(classes, j)))
		    matched = TRUE;
	UNPROTECT(1);
    }
    if(matched) {
	/* PROTECT(R_fcall = lang2(FUN, X)); */
	PROTECT(R_fcall = lang3(FUN, X, R_DotsSymbol));
	ans = eval(R_fcall, rho);
	UNPROTECT(1);
	return(ans);
    } else if(replace) return duplicate(X);
    else return duplicate(deflt);
}

SEXP attribute_hidden do_rapply(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP X, FUN, classes, deflt, how, ans, names;
    int i, n;
    Rboolean replace;

    checkArity(op, args);
    X = CAR(args); args = CDR(args);
    FUN = CAR(args); args = CDR(args);
    if(!isFunction(FUN)) error(_("invalid '%s' argument"), "f");
    classes = CAR(args); args = CDR(args);
    if(!isString(classes)) error(_("invalid '%s' argument"), "classes");
    deflt = CAR(args); args = CDR(args);
    how = CAR(args);
    if(!isString(how)) error(_("invalid '%s' argument"), "how");
    replace = Rboolean(strcmp(CHAR(STRING_ELT(how, 0)), "replace") == 0); /* ASCII */
    n = length(X);
    PROTECT(ans = allocVector(VECSXP, n));
    names = getAttrib(X, R_NamesSymbol);
    /* or copy attributes if replace = TRUE? */
    if(!isNull(names)) setAttrib(ans, R_NamesSymbol, names);
    for(i = 0; i < n; i++)
	SET_VECTOR_ELT(ans, i, do_one(VECTOR_ELT(X, i), FUN, classes, deflt,
				      replace, rho));
    UNPROTECT(1);
    return ans;
}

static Rboolean islistfactor(SEXP X)
{
    int i, n = length(X);

    if(n == 0) return FALSE;
    switch(TYPEOF(X)) {
    case VECSXP:
	for(i = 0; i < LENGTH(X); i++)
	    if(!islistfactor(VECTOR_ELT(X, i))) return FALSE;
	return TRUE;
	break;
    case EXPRSXP:
	for(i = 0; i < LENGTH(X); i++)
	    if(!islistfactor(XVECTOR_ELT(X, i))) return FALSE;
	return TRUE;
	break;
    default:  // -Wswitch
	break;
    }
    return isFactor(X);
}


/* is this a tree with only factor leaves? */

SEXP attribute_hidden do_islistfactor(SEXP call, SEXP op, SEXP args, SEXP rho)
{
    SEXP X;
    Rboolean lans = TRUE, recursive;
    int i, n;

    checkArity(op, args);
    X = CAR(args);
    recursive = Rboolean(asLogical(CADR(args)));
    n = length(X);
    if(n == 0 || !isVectorList(X)) {
	lans = FALSE;
	goto do_ans;
    }
    if(!recursive) {
    for(i = 0; i < LENGTH(X); i++)
	if(!isFactor(VECTOR_ELT(X, i))) {
	    lans = FALSE;
	    break;
	}
    } else {
	switch(TYPEOF(X)) {
	case VECSXP:
	    for(i = 0; i < LENGTH(X); i++)
		if(!islistfactor(VECTOR_ELT(X, i))) {
		    lans = FALSE;
		    break;
		}
	    break;
	case EXPRSXP:
	    for(i = 0; i < LENGTH(X); i++)
		if(!islistfactor(XVECTOR_ELT(X, i))) {
		    lans = FALSE;
		    break;
		}
	    break;
	default:
	    break;
	}
    }
do_ans:
    return ScalarLogical(lans);
}
