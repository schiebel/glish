// $Header$

#include <stream.h>
#include "Glish/Value.h"
#include "Frame.h"
#include "Reporter.h"


Frame::Frame( int frame_size )
	{
	size = frame_size;

	typedef Value* value_ptr;
	values = new value_ptr[size];

	for ( int i = 0; i < size; ++i )
		values[i] = 0;

	description = "<frame>";
	}


Frame::~Frame()
	{
	for ( int i = 0; i < size; ++i )
		Unref( values[i] );

	delete values;
	}


Value*& Frame::FrameElement( int offset )
	{
	if ( offset < 0 || offset >= size )
		fatal->Report( "bad offset in Frame::FrameElement" );

	return values[offset];
	}
