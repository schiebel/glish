//======================================================================
// str.cc
//
// $Id$
//
//======================================================================
#include "sos/sos.h"
RCSID("@(#) $Id$")
#include "sos/str.h"

str_kernel::str_kernel( const char *s ) : cnt(1), len(1)
	{
	ary = (char**) alloc_zero_memory(len*sizeof(char*));
	if ( s && *s )
		{
		unsigned int s_len = ::strlen(s);
		ary[0] = (char*) alloc_memory(s_len+5);
		*((unsigned int*)ary[0]) = s_len;
		memcpy(&ary[0][4],s,s_len+1);
		}
	}

void str_kernel::set( unsigned int off, const char *s )
	{
	if ( s && *s )
		{
		unsigned int s_len = ::strlen(s);
		if ( ary[off] )
			{
			if ( *((unsigned int*)ary[off]) < s_len )
				ary[off] = (char*) realloc_memory(ary[off],s_len+5);
			}
		else
			ary[off] = (char*) alloc_memory(s_len+5);

		*((unsigned int*)ary[off]) = s_len;
		memcpy(&ary[off][4],s,s_len+1);
		}
	else if ( ary[off] )
		*((unsigned int*)ary[off]) = 0;
	}

void str_kernel::raw_set( unsigned int off, const char *s )
	{
	if ( s )
		{
		if ( ary[off] )

			if ( *((unsigned int*)ary[off]) < *((unsigned int*)s) )
				ary[off] = (char*) realloc_memory(ary[off],*((unsigned int*)s)+5);
			else
				ary[off] = (char*) alloc_memory(*((unsigned int*)s)+5);

		memcpy(ary[off],s,*((unsigned int*)s)+5);
		}
	else if ( ary[off] )
		*((unsigned int*)ary[off]) = 0;
	}

str_kernel *str_kernel::clone() const
	{
	str_kernel *nk = new str_kernel(len);
	for ( unsigned int i=0; i < len; i++ )
		if ( ary[i] && *((unsigned int*)ary[i]) )
			{
			nk->ary[i] = (char*) alloc_memory(*((unsigned int*)ary[i])+5);
			memcpy(nk->ary[i],ary[i],*((unsigned int*)ary[i])+5);
			}
	return nk;
	}

void str_kernel::grow( unsigned int size )
	{
	if ( size <= len )
		len = size;
	else
		{
		ary = (char**) realloc_memory(ary,size*sizeof(char*));
		for ( unsigned int i = len; i < size; i++ )
			ary[i] = 0;
		len = size;
		}
	}

str_kernel::~str_kernel( )
	{
	for ( unsigned int x = 0; x < len; x++ )
		if ( ary[x] ) free_memory( ary[x] );
	free_memory(ary);
	}

void str::do_copy()
	{
	if ( kernel->count() > 1 )
		{
		kernel->unref();
		kernel = kernel->clone();
		}
	}
