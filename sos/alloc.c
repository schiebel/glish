//======================================================================
// alloc.cc
//
// $Id$
//
//======================================================================
#include "sos/sos.h"
RCSID("@(#) $Id$")
#include "config_p.h"
#include "sos/alloc.h"
#include <stdlib.h>

void* sos_alloc_memory( unsigned int size )
	{
#if defined(_AIX) || defined(__alpha__)
	if ( ! size ) size += 8;
#endif
	return (void*) malloc( size );
	}

void* sos_alloc_zero_memory( unsigned int size )
	{
#if defined(_AIX) || defined(__alpha__)
	if ( ! size ) size += 8;
#endif
	return (void*) calloc( 1, size );
	}

void* sos_realloc_memory( void* ptr, unsigned int new_size )
	{
	return (void*) realloc( (malloc_t) ptr, new_size );
	}

void sos_free_memory( void* ptr )
	{
	if ( ptr ) free( (malloc_t) ptr );
	}

