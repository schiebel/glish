/* $Id$
** Copyright (c) 1997 Associated Universities Inc.
*/

#include "Glish/glish.h"
RCSID("@(#) $Id$")

#if defined(__alpha) || defined(__alpha__)

#define DIV(NAME,TYPE)							\
void NAME( TYPE *lhs, TYPE *rhs, int lhs_len, int rhs_incr )		\
	{								\
	for ( int i = 0, j = 0; i < lhs_len; ++i, j += rhs_incr )	\
		lhs[i] /= rhs[j];					\
	}

DIV(glish_fdiv,float)
DIV(glish_ddiv,double)

#endif
