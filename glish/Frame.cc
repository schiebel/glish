// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <iostream.h>
#include "IValue.h"
#include "Frame.h"
#include "Reporter.h"
#include "system.h"


Frame::Frame( int frame_size, IValue* param_info, scope_type s )
	{
	scope = s;
	size = frame_size;
	missing = param_info ? param_info : empty_ivalue();
	values = (IValue**) alloc_memory( sizeof(IValue*)*size );

	for ( int i = 0; i < size; ++i )
		values[i] = 0;

	description = "<frame>";
	}


Frame::~Frame()
	{
	Unref( missing );

	for ( int i = 0; i < size; ++i )
		Unref( values[i] );
	free_memory( values );
	}


IValue*& Frame::FrameElement( int offset )
	{
	if ( offset < 0 || offset >= size )
		fatal->Report( "bad offset in Frame::FrameElement" );

	return values[offset];
	}
