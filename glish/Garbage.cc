// $Id$
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "Garbage.h"
#include "Reporter.h"

garbage_list *Garbage::values = 0;

void Garbage::init() 		{ values = new garbage_list; }
void Garbage::finalize() 	{ delete values; }

void Garbage::collect( int report )
	{
	value_list garbage;
	glish_collecting_garbage = 1;
	int len = values->length();
	loop_over_list( *values, i )
		if ( (*values)[i]->isTaged() )
			(*values)[i]->clear();
		else
			{
			if ( garbage.is_member((*values)[i]->value) )
				cerr <<  "error: " << (void*) (*values)[i]->value << endl;
			else
				garbage.append((*values)[i]->value);
			}

	if ( report )
		message->Report( "removing", garbage.length(), "of", len, "values..." );

	loop_over_list( garbage, j )
		{
		Value *v = garbage[j];
		if ( v )
			for ( int c = v->RefCount(); c > 0; --c )
				Unref( v );
		}
	glish_collecting_garbage = 0;
	}
