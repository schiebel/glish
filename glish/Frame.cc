// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997,1998 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <iostream.h>
#include "IValue.h"
#include "Frame.h"
#include "Glish/Reporter.h"
#include "system.h"

const char *Frame::Description() const
	{
	return "<frame>";
	}

Frame::Frame( int frame_size, IValue* param_info, scope_type s )
	{
	scope = s;
	size = frame_size;
	missing = param_info ? param_info : empty_ivalue();
	values = (IValue**) alloc_memory( sizeof(IValue*)*size );

	for ( int i = 0; i < size; ++i )
		values[i] = 0;
	}


Frame::~Frame() { clear(); }

void Frame::clear()
	{
#ifdef GGC
	if ( ! glish_collecting_garbage )
#endif
		{
		Unref( missing );
		missing = 0;

		for ( int i = 0; i < size; ++i )
			Unref( values[i] );
		}

	free_memory( values );
	values = 0;
	size = 0;
	}

IValue*& Frame::FrameElement( int offset )
	{
	if ( offset < 0 || offset >= size )
		fatal->Report( "bad offset in Frame::FrameElement" );

	return values[offset];
	}

#ifdef MEMFREE
unsigned int Frame::CountRefs( recordptr r ) const
	{
	//
	// here we lie because if some other function is
	// pointing to us we must protect our values...
	//
	if ( RefCount() > 1 ) return 0;

	unsigned int count = 0;
	for ( int i = 0; i < size; ++i )
		if ( values[i] )
			count += values[i]->CountRefs(r);

	return count;
	}
	
int Frame::CountRefs( Frame *f ) const
	{
	int count = 0;
	for ( int i = 0; i < size; ++i )
		if ( values[i] )
			count += values[i]->CountRefs(f);

	return count;
	}
#endif
	
#ifdef GGC
void Frame::TagGC( )
	{
	for ( int i = 0; i < size; ++i )
		if ( values[i] )
			values[i]->TagGC( );
	if ( missing)
		missing->TagGC( );
	}
#endif
