// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "sos/sos.h"
RCSID("@(#) $Id$")
#include "sos/alloc.h"

#include <stdlib.h>
#include <iostream.h>

#include "sos/list.h"

static const int DEFAULT_CHUNK_SIZE = 10;

// Print message on stderr and exit.
void default_error_handler(char* s)
	{
	cerr << s << "\n";
	exit(1);
	}

PFC BaseList::set_error_handler(PFC handler)
	{
	PFC old = error_handler;

	if ( handler == 0 )
		error_handler = default_error_handler;
	else
		error_handler = handler;

	return old;
	}

BaseList::BaseList(int size, PFC handler)
	{

	if ( size <= 0 )
		chunk_size = DEFAULT_CHUNK_SIZE;
	else
		chunk_size = size;

	if ( size < 0 )
		{
		num_entries = max_entries = 0;
		entry = 0;
		}
	else
		{
		num_entries = 0;
		if ( (entry = (ent*) sos_alloc_memory(sizeof(ent) * chunk_size)) )
			max_entries = chunk_size;
		else
			max_entries = 0;
		}

	if ( handler == 0 )
		error_handler = default_error_handler;
	else
		error_handler = handler;
	}


BaseList::BaseList(BaseList& b)
	{
	max_entries = b.max_entries;
	chunk_size = b.chunk_size;
	num_entries = b.num_entries;
	error_handler = b.error_handler;

	if ( max_entries )
		entry = (ent*) sos_alloc_memory( sizeof(ent)*max_entries );
	else
		entry = 0;

	for ( int i = 0; i < num_entries; i++ )
		entry[i] = b.entry[i];
	}

void BaseList::operator=(BaseList& b)
	{
	if ( this == &b )
		return;	// i.e., this already equals itself

	sos_free_memory( entry );

	max_entries = b.max_entries;
	chunk_size = b.chunk_size;
	num_entries = b.num_entries;
	error_handler = b.error_handler;

	if ( max_entries )
		entry = (ent*) sos_alloc_memory( sizeof(ent)*max_entries );
	else
		entry = 0;

	for ( int i = 0; i < num_entries; i++ )
		entry[i] = b.entry[i];
	}

void BaseList::insert(ent a)
	{
	if ( num_entries == max_entries )
		resize( );	// make more room

	for ( int i = num_entries; i > 0; i-- )	
		entry[i] = entry[i-1];	// move all pointers up one

	num_entries++;
	entry[0] = a;
	}

void BaseList::insert_nth(int off, ent a)
	{
	if ( num_entries == max_entries )
		resize( );	// make more room

	if ( off > num_entries )
		off = num_entries + 1;

	if ( off < 0 ) off = 0;

	for ( int i = num_entries; i > off; i-- )	
		entry[i] = entry[i-1];	// move all pointers up one

	num_entries++;
	entry[off] = a;
	}

ent BaseList::remove(ent a)
	{
	int i = 0;
	for ( ; i < num_entries && a != entry[i]; i++ )
		;

	return remove_nth(i);
	}

ent BaseList::remove_nth(int n)
	{
	if ( n < 0 || n >= num_entries )
		return 0;

	ent old_ent = entry[n];
	--num_entries;

	for ( ; n < num_entries; n++ )
		entry[n] = entry[n+1];

	entry[n] = 0;	// for debugging
	return old_ent;
	}

// Get and remove from the end of the list.
ent BaseList::get()
	{
	if ( num_entries == 0 )
		{
		error_handler("get from empty BaseList");
		return 0;
		}

	return entry[--num_entries];
	}


void BaseList::clear()
	{
	sos_free_memory( entry );
	entry = 0;
	num_entries = max_entries = 0;
	chunk_size = DEFAULT_CHUNK_SIZE;
	}

ent BaseList::replace(int ent_index,ent new_ent)
	{
	if ( ent_index < 0 || ent_index > num_entries-1 )
		{
		return 0;
		}
	else
		{
		ent old_ent = entry[ent_index];
		entry[ent_index] = new_ent;
		return old_ent;
		}
	}

int BaseList::resize( )
	{
	max_entries += chunk_size;
	chunk_size *= 2;
	entry = (ent*) sos_realloc_memory( (void*) entry, sizeof( ent ) * max_entries );
	return max_entries;
	}

ent BaseList::is_member(ent e) const
	{
	int i = 0;
	for ( ; i < length() && e != entry[i]; i++ )
		;

	return (i == length()) ? 0 : e;
	}
