// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "Glish/Complex.h"

// Defined in "Value.cc".
extern dcomplex text_to_dcomplex( const char text[], int& successful );

complex atocpx( const char text[] )
	{
	return complex( atodcpx( text ) );
	}

dcomplex atodcpx( const char text[] )
	{
	int successful;
	dcomplex dr = text_to_dcomplex( text, successful );
	return successful ? dr : dcomplex( 0.0, 0.0 );
	}

#define sqr(x) ((x) * (x))

#ifdef _AIX
static float local_zero = 0.0;
#define DIV_BY_ZERO (1.0/local_zero)
#else
#define DIV_BY_ZERO (1.0/0.0)
#endif
#define COMPLEX_DIV_OP(type,cast)					\
type div( const type divd, const type dsor )				\
	{								\
	double y = sqr( dsor.r ) + sqr( dsor.i );			\
	double p = divd.r * dsor.r + divd.i * dsor.i;			\
	double q = divd.i * dsor.r - divd.r * dsor.i;			\
									\
	if ( y < 1.0 )							\
		{							\
		double w = HUGE * y;					\
		/*** OVERFLOW ***/					\
		if ( fabs( p ) > w || fabs( q ) > w || y == 0.0 )	\
			return type( cast(DIV_BY_ZERO), cast(DIV_BY_ZERO) ); \
		}							\
	return type( cast( p / y ), cast( q / y ) );			\
	}

COMPLEX_DIV_OP(dcomplex,double)
COMPLEX_DIV_OP(complex,float)

dcomplex exp( const dcomplex v ) 
	{
	double r = exp( v.r );
	return dcomplex( r * cos(v.i), r * sin(v.i) );
	}

dcomplex log( const dcomplex v )
	{
	double h = hypot( v.r, v.i );
	/* THROW EXCEPTION if h <= 0*/
	return dcomplex( log(h), atan2(v.i, v.r) );
	}

dcomplex log10( const dcomplex v )
	{
	double log10e = 0.4342944819032518276511289;
	dcomplex l = log(v);
	return dcomplex( l.r * log10e, l.i * log10e );
	}

dcomplex sin( const dcomplex v )
	{
	return dcomplex( sin(v.r) * cosh(v.i), cos(v.r) * sinh(v.i) );
	}

dcomplex cos( const dcomplex v )
	{
	return dcomplex( cos(v.r) * cosh(v.i), -sin(v.r) * sinh(v.i) );
	}

dcomplex sqrt( const dcomplex v )
	{
	return v.r == 0 && v.i == 0 ? dcomplex(0,0) : pow( v, dcomplex( 0.5 ) );
	}

dcomplex pow( const dcomplex x, const dcomplex y )
	{
	dcomplex z = log( x );
	z = mul( z, y );
	return exp( z );
	}
