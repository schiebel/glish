// $Header$

// Copyright (c) 1993 The Regents of the University of California.
// All rights reserved.
//
// This code is derived from software contributed to Berkeley by
// Vern Paxson.
//
// The United States Government has rights in this work pursuant
// to contract no. DE-AC03-76SF00098 between the United States
// Department of Energy and the University of California, and
// contract no. DE-AC02-89ER40486 between the United States
// Department of Energy and the Universities Research Association, Inc.
//
// Redistribution and use in source and binary forms are permitted
// provided that: (1) source distributions retain this entire
// copyright notice and comment, and (2) distributions including
// binaries display the following acknowledgement:  ``This product
// includes software developed by the University of California,
// Berkeley and its contributors'' in the documentation or other
// materials provided with the distribution and in all advertising
// materials mentioning features or use of this software.  Neither the
// name of the University nor the names of its contributors may be
// used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include <signal.h>
#include <osfcn.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "input.h"
#include "glishlib.h"
#include "Reporter.h"
#include "Sequencer.h"
#include "IValue.h"

#if defined(GLISHTK)
#include "Rivet/tcl.h"
#include "Rivet/tk.h"
#endif

static Sequencer* s;

#if USE_EDITLINE
extern "C" void nb_readline_cleanup();
#endif

void glish_cleanup()
	{
#if USE_EDITLINE
	nb_readline_cleanup();
#endif
	set_term_unchar_mode();
	exit( 0 );
	}

int main( int argc, char** argv )
	{
	(void) install_signal_handler( SIGINT, glish_cleanup );
	(void) install_signal_handler( SIGHUP, glish_cleanup );
	(void) install_signal_handler( SIGTERM, glish_cleanup );

	s = new Sequencer( argc, argv );

	s->Exec();

	// We don't delete s because presently the Sequencer class does
	// not reclaim its sundry memory upon deletion, so Purify complains
	// about zillions of memory leaks.  This is also the reason why
	// s is a static and not a local.

	glish_cleanup();

	return 0;
	}

#if USE_EDITLINE

extern "C" {
	char *readline( const char * );
	char *nb_readline( const char * );
	extern char *rl_data_incomplete;
	void add_history( char * );
}

static int fmt_readline_str( char* to_buf, int max_size, char* from_buf )
	{
	if ( from_buf )
		{
		char* from_buf_start = from_buf;

		while ( isspace(*from_buf_start) )
			++from_buf_start;

		if ( strlen( from_buf_start ) <= max_size )
			to_buf = strcpy( to_buf, from_buf_start );
		else
			{
			cerr << "Not enough buffer size (in fmt_readline_str)"
			     << endl;
			free_memory( (void*) from_buf );
			return 0;
			}
		  
		if ( *to_buf )
			add_history( to_buf );

		sprintf( to_buf, "%s\n", from_buf_start );

		if ( from_buf )
			free_memory( (void*) from_buf );

		return strlen( to_buf );
		}

	else
		return 0;
	}

int interactive_read( FILE* /* file */, const char prompt[], char buf[],
			int max_size )
	{
#ifndef __GNUC__
        static int did_sync = 0;
        if ( ! did_sync )
		{
		ios::sync_with_stdio();
		did_sync = 1;
		}
#endif

	char* ret;
#if defined( GLISHTK )
	if ( current_sequencer->ActiveClients() || tk_NumMainWindows > 0 )
#else
	if ( current_sequencer->ActiveClients() )
#endif
		{
		ret = nb_readline( prompt );

		while ( ret == rl_data_incomplete )
			{
			current_sequencer->EventLoop();
			ret = nb_readline( prompt );
			}
		}
	else
		{
		current_sequencer->EventLoop();
		ret = readline( prompt );
		}

	return fmt_readline_str( buf, max_size, ret );
	}

DEFINE_CREATE_VALUE(IValue)

Value *copy_value( const Value *value )
	{
	return (Value*)copy_value( (const IValue*) value );
	}

const Value *lookup_sequencer_value( const char *id )
	{
	return Sequencer::LookupVal( id );
	}

#endif
