// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <string.h>
#include "Glish/Dict.h"
#include "config.h"


// If the mean bucket length exceeds the following then Insert() will
// increase the size of the hash table.
#define TOO_CROWDED 3

// This determines how many slots buckets have initially
#define INITIAL_BUCKET_CHUNK  2

class DictEntry {
public:
	DictEntry( const char* k, void* val )
		{ key = k; value = val; }

	const char* key;
	void* value;
	};

// The value of an iteration cookie is the bucket and offset within the
// bucket at which to start looking for the next value to return.
class IterCookie {
public:
	IterCookie( int b, int o ) : bucket(b), offset(o) { }
	void Set( int b, int o )
		{
		bucket = b;
		offset = o;
		}
	int bucket, offset;
	};


Dictionary::Dictionary( dict_order ordering, int initial_size )
	{
	Init( initial_size );

	if ( ordering == ORDERED )
		order = new PList(DictEntry)(INITIAL_BUCKET_CHUNK);
	else
		order = 0;

	stale_cookie = 0;
	}

Dictionary::~Dictionary()
	{
	for ( int i = 0; i < num_buckets; ++i )
		if ( tbl[i] )
			{
			while ( tbl[i]->length() )
				delete tbl[i]->remove_nth( tbl[i]->length() - 1 );

			delete tbl[i];
			}

	delete [] tbl;

	if ( order )
		delete order;
	if ( stale_cookie )
		delete stale_cookie;
	}


void* Dictionary::Insert( const char* key, void* value )
	{
	DictEntry* new_entry = new DictEntry( key, value );
	void* old_val = Insert( new_entry );

	if ( old_val )
		{
		// We didn't need the new DictEntry, the key was already
		// present.
		delete new_entry;
		}

	else if ( order )
		order->append( new_entry );

	if ( num_entries / num_buckets >= TOO_CROWDED )
		ChangeSize( num_buckets * 2 );

	return old_val;
	}


void* Dictionary::Lookup( const char* key ) const
	{
	int h = Hash( key, num_buckets );
	PList(DictEntry)* chain = tbl[h];

	if ( chain )
		{
		for ( int i = 0; i < chain->length(); ++i )
			{
			DictEntry* entry = (*chain)[i];

			if ( ! strcmp( key, entry->key ) )
				return entry->value;
			}
		}

	return NotFound;
	}

void* Dictionary::NthEntry( int n, const char*& key ) const
	{
	if ( ! order || n < 0 || n >= Length() )
		return 0;

	DictEntry* entry = (*order)[n];
	key = entry->key;
	return entry->value;
	}

int Dictionary::Sizeof( ) const
	{
	int size = 0;
	
	for ( int i = 0; i < num_buckets; ++i )
		{
		PList(DictEntry) *chain = tbl[i];
		if ( chain ) size += sizeof(BaseList) +
			       chain->curlen() * sizeof(void*) +
			       chain->length() * sizeof(DictEntry);
		}

	return sizeof(Dictionary) + (order ? sizeof(BaseList) + order->curlen() * sizeof(void*) : 0)
				  + num_buckets * sizeof(void*) + size;
	}


char* Dictionary::Remove( const char* key )
	{
	int h = Hash( key, num_buckets );
	PList(DictEntry)* chain = tbl[h];

	if ( chain )
		{
		for ( int i = 0; i < chain->length(); ++i )
			{
			DictEntry* entry = (*chain)[i];

			if ( ! strcmp( key, entry->key ) )
				{
				char* entry_key = (char*) entry->key;

				chain->remove( entry );

				if ( order )
					order->remove( entry );

				delete entry;
				--num_entries;
				return entry_key;
				}
			}
		}

	return 0;
	}


IterCookie* Dictionary::InitForIteration() const
	{
	IterCookie *ret = 0;
	if ( stale_cookie )
		{
		ret = stale_cookie;
		ret->Set(0,0);
		((Dictionary*)this)->stale_cookie = 0;
		}
	else
		ret = new IterCookie( 0, 0 );

	return ret;
	}

void* Dictionary::NextEntry( const char*& key, IterCookie*& cookie ) const
	{
	int b = cookie->bucket;
	int o = cookie->offset;
	DictEntry* entry;

	if ( tbl[b] && tbl[b]->length() > o )
		{
		entry = (*tbl[b])[o];
		++cookie->offset;
		key = entry->key;
		return entry->value;
		}

	++b;
	while ( b < num_buckets && (! tbl[b] || tbl[b]->length() == 0) )
		++b;

	if ( b >= num_buckets )
		{ // All done.
		if ( ! stale_cookie )
			((Dictionary*)this)->stale_cookie = cookie;
		else
			delete cookie;
		return 0;
		}

	entry = (*tbl[b])[0];
	key = entry->key;

	cookie->bucket = b;
	cookie->offset = 1;

	return entry->value;
	}


void Dictionary::Init( int size )
	{
	num_buckets = NextPrime( size );
	tbl = new PList(DictEntry)*[num_buckets];

	for ( int i = 0; i < num_buckets; ++i )
		tbl[i] = 0;

	num_entries = 0;
	}

void* Dictionary::Insert( DictEntry* new_entry )
	{
	int h = Hash( new_entry->key, num_buckets );
	PList(DictEntry)* chain = tbl[h];

	if ( chain )
		{
		for ( int i = 0; i < chain->length(); ++i )
			{
			DictEntry* entry = (*chain)[i];

			if ( ! strcmp( new_entry->key, entry->key ) )
				{
				void* old_value = entry->value;
				entry->value = new_entry->value;
				return old_value;
				}
			}
		}

	else
		{ // Create new chain.
		chain = tbl[h] = new PList(DictEntry)(INITIAL_BUCKET_CHUNK);
		}

	// We happen to know (:-() that appending is more efficient
	// on lists than prepending.
	chain->append( new_entry );

	++num_entries;

	return 0;
	}

int Dictionary::Hash( register const char* str, int hash_size ) const
	{
	register unsigned int hashval = 1;

	while ( *str )
		hashval = (hashval << 1) + *str++;

	return (int)(hashval % (unsigned int) hash_size);
	}

int Dictionary::NextPrime( int n ) const
	{
	while ( ! IsPrime( n ) )
		++n;
	return n;
	}

int Dictionary::IsPrime( int n ) const
	{
	if ( (n & 0x1) == 0 )
		// Even.
		return 0;

	for ( int j = 3; j * j <= n; ++j )
		if ( n % j == 0 )
			return 0;

	return 1;
	}

void Dictionary::ChangeSize( int new_size )
	{
	// First collect the current contents into a list.
	PList(DictEntry)* current;

	if ( order )
		current = order;
	else
		current = new PList(DictEntry)(INITIAL_BUCKET_CHUNK);

	for ( int i = 0; i < num_buckets; ++i )
		{
		PList(DictEntry)* chain = tbl[i];

		if ( chain )
			{
			if ( ! order )
				{
				for ( int j = 0; j < chain->length(); ++j )
					current->append( (*chain)[j] );
				}

			delete chain;
			}
		}

	delete [] tbl;
	Init( new_size );

	for ( LOOPDECL i = 0; i < current->length(); ++i )
		Insert( (*current)[i] );

	if ( ! order )
		delete current;
	}
