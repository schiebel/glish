/* $Id$
** Copyright (c) 1997 Associated Universities Inc.
*/

#include "Glish/glish.h"
RCSID("@(#) $Id$")

#ifdef __alpha

#define DIV(NAME,TYPE)							\
void NAME( TYPE *lhs, TYPE *rhs, int lhs_len, int rhs_incr )		\
	{								\
	int i,j;							\
	for ( i = 0, j = 0; i < lhs_len; ++i, j += rhs_incr )		\
		lhs[i] /= rhs[j];					\
	}

DIV(glish_fdiv,float)
DIV(glish_ddiv,double)

void glish_func_loop( double (*fn)( double ), double *lhs, double *arg, int len )
	{
	int i;
	for ( i=0; i < len; i++ )
		lhs[i] = (*fn)( arg[i] );
	}

#endif
