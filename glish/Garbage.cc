// $Id$
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "Garbage.h"
#include "Reporter.h"

garbage_list *Garbage::values = 0;

void Garbage::init() 		{ values = new garbage_list; }
void Garbage::finalize() 	{ delete values; }

void Garbage::collect( )
	{
	value_list garbage;
	glish_collecting_garbage = 1;
	loop_over_list( *values, i )
		if ( (*values)[i]->isTaged() )
			(*values)[i]->clear();
		else
			garbage.append((*values)[i]->value);

	message->Report( "removing", garbage->length(), "values..." );
	loop_over_list( garbage, j )
		{
		Value *v = garbage[j];
		for ( int c = v->RefCount(); c > 0; --c )
			Unref( v );
		}
	glish_collecting_garbage = 0;
	}
