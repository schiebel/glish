// $Header$

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <stream.h>
#include "IValue.h"
#include "Frame.h"
#include "Reporter.h"


Frame::Frame( int frame_size, IValue* param_info, scope_type s )
	{
	scope = s;
	size = frame_size;
	missing = param_info ? param_info : empty_ivalue();
	values = new IValue*[size];

	for ( int i = 0; i < size; ++i )
		values[i] = 0;

	description = "<frame>";
	}


Frame::~Frame()
	{
	Unref( missing );

	for ( int i = 0; i < size; ++i )
		Unref( values[i] );
	delete values;
	}


IValue*& Frame::FrameElement( int offset )
	{
	if ( offset < 0 || offset >= size )
		fatal->Report( "bad offset in Frame::FrameElement" );

	return values[offset];
	}
