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
#include <setjmp.h>
#include <strstream.h>

#include "input.h"
#include "glishlib.h"
#include "Reporter.h"
#include "Sequencer.h"
#include "IValue.h"

#if defined(GLISHTK)
#include "TkAgent.h"
#endif

static Sequencer* s = 0;
int glish_jmpbuf_set = 0;
jmp_buf glish_top_level;
int allwarn = 0;
static unsigned int error_count = 0;

#if USE_EDITLINE
extern "C" void nb_readline_cleanup();
#endif

static void install_terminate_handlers();

void glish_cleanup( )
	{
#if USE_EDITLINE
	nb_readline_cleanup();
#endif
	set_term_unchar_mode();
	}


#define DEFINE_SIG_FWD(NAME,STRING,SIGNAL)				\
void NAME( )								\
	{								\
	glish_cleanup( );						\
	fprintf(stderr,"\n[fatal error, '%s' (signal %d), exiting]\n",	\
			STRING, SIGNAL);				\
									\
	if ( s ) delete s;						\
	install_signal_handler( SIGNAL, (signal_handler) SIG_DFL );	\
	kill( getpid(), SIGNAL );					\
	}


DEFINE_SIG_FWD(glish_sighup,"hangup signal",SIGHUP)
DEFINE_SIG_FWD(glish_sigterm,"terminate signal",SIGTERM)
DEFINE_SIG_FWD(glish_sigabrt,"abort signal",SIGABRT)
DEFINE_SIG_FWD(glish_sigquit,"quit signal",SIGQUIT)

void glish_sigint( )
	{
	if ( glish_jmpbuf_set )
		{
		Sequencer::TopLevelReset();
		unblock_signal(SIGINT);
		longjmp( glish_top_level, 1 );
		}

	if ( s ) delete s;
	install_signal_handler( SIGINT, (signal_handler) SIG_DFL );
	kill(getpid(), SIGINT);
	}

int main( int argc, char** argv )
	{
	install_terminate_handlers();

	(void) install_signal_handler( SIGINT, glish_sigint );
	(void) install_signal_handler( SIGHUP, glish_sighup );
	(void) install_signal_handler( SIGTERM, glish_sigterm );
	(void) install_signal_handler( SIGABRT, glish_sigabrt );
	(void) install_signal_handler( SIGQUIT, glish_sigquit );

	seed_random_number_generator();

	s = new Sequencer( argc, argv );

	s->Exec();

	glish_cleanup();

	delete s;

	return 0;
	}

DEFINE_SIG_FWD(glish_sigsegv,"segmentation violation",SIGSEGV)
DEFINE_SIG_FWD(glish_sigbus,"bus error",SIGBUS)
DEFINE_SIG_FWD(glish_sigill,"illegal instruction",SIGILL)
DEFINE_SIG_FWD(glish_sigemt,"hardware fault",SIGEMT)
DEFINE_SIG_FWD(glish_sigfpe,"floating point exception",SIGFPE)
DEFINE_SIG_FWD(glish_sigtrap,"hardware fault",SIGTRAP)
DEFINE_SIG_FWD(glish_sigsys,"invalid system call",SIGSYS)

static void install_terminate_handlers()
	{
	(void) install_signal_handler( SIGSEGV, glish_sigsegv );
	(void) install_signal_handler( SIGBUS, glish_sigbus );
	(void) install_signal_handler( SIGILL, glish_sigill );
	(void) install_signal_handler( SIGEMT, glish_sigemt );
	(void) install_signal_handler( SIGFPE, glish_sigfpe );
	(void) install_signal_handler( SIGTRAP, glish_sigtrap );
	(void) install_signal_handler( SIGSYS, glish_sigsys );
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
	if ( current_sequencer->ActiveClients() || TkFrame::TopLevelCount() > 0 )
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

class StringReporter : public Reporter {
    public:
	StringReporter( ostream& reporter_stream ) :
		Reporter( reporter_stream ) { }
	void Epilog();
	void Prolog();
	};

void StringReporter::Epilog() { }
void StringReporter::Prolog() { }

Value *generate_error( const RMessage& m0,
		       const RMessage& m1, const RMessage& m2,
		       const RMessage& m3, const RMessage& m4,
		       const RMessage& m5, const RMessage& m6,
		       const RMessage& m7, const RMessage& m8,
		       const RMessage& m9, const RMessage& m10,
		       const RMessage& m11, const RMessage& m12,
		       const RMessage& m13, const RMessage& m14,
		       const RMessage& m15, const RMessage& m16
		    )
	{
	strstream sout;
	StringReporter srpt( sout );

	srpt.Report(m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
	sout << '\0';
	IValue *ret = error_ivalue( sout.str() );
	if ( allwarn )
		{
		cerr << "E[" << ++error_count << "]: ";
		ret->Describe( cerr );
		cerr << endl;
		}
	return ret;
	}

Value *generate_error( const char *file, int line,
		       const RMessage& m0,
		       const RMessage& m1, const RMessage& m2,
		       const RMessage& m3, const RMessage& m4,
		       const RMessage& m5, const RMessage& m6,
		       const RMessage& m7, const RMessage& m8,
		       const RMessage& m9, const RMessage& m10,
		       const RMessage& m11, const RMessage& m12,
		       const RMessage& m13, const RMessage& m14,
		       const RMessage& m15, const RMessage& m16
		    )
	{
	strstream sout;
	StringReporter srpt( sout );

	srpt.Report(m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
	sout << '\0';
	IValue *ret = error_ivalue( sout.str(), file, line );
	if ( allwarn )
		{
		cerr << "E[" << ++error_count << "]: ";
		ret->Describe( cerr );
		cerr << endl;
		}
	return ret;
	}

const Str generate_error_str( const RMessage& m0,
		       const RMessage& m1, const RMessage& m2,
		       const RMessage& m3, const RMessage& m4,
		       const RMessage& m5, const RMessage& m6,
		       const RMessage& m7, const RMessage& m8,
		       const RMessage& m9, const RMessage& m10,
		       const RMessage& m11, const RMessage& m12,
		       const RMessage& m13, const RMessage& m14,
		       const RMessage& m15, const RMessage& m16
		    )
	{
	strstream sout;
	StringReporter srpt( sout );

	srpt.Report(m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
	sout << '\0';
	if ( allwarn )
		cerr << "E[" << ++error_count << "]: " <<
		  sout.str() << endl;
	return Str( (const char *) sout.str() );
	}

void report_error( const RMessage& m0,
		       const RMessage& m1, const RMessage& m2,
		       const RMessage& m3, const RMessage& m4,
		       const RMessage& m5, const RMessage& m6,
		       const RMessage& m7, const RMessage& m8,
		       const RMessage& m9, const RMessage& m10,
		       const RMessage& m11, const RMessage& m12,
		       const RMessage& m13, const RMessage& m14,
		       const RMessage& m15, const RMessage& m16
		    )
	{
	strstream sout;
	StringReporter srpt( sout );

	srpt.Report(m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
	sout << '\0';
	IValue *ret = error_ivalue( sout.str() );
	if ( allwarn )
		{
		cerr << "E[" << ++error_count << "]: ";
		ret->Describe( cerr );
		cerr << endl;
		}
	Sequencer::SetErrorResult( ret );
	}

void report_error( const char *file, int line,
		       const RMessage& m0,
		       const RMessage& m1, const RMessage& m2,
		       const RMessage& m3, const RMessage& m4,
		       const RMessage& m5, const RMessage& m6,
		       const RMessage& m7, const RMessage& m8,
		       const RMessage& m9, const RMessage& m10,
		       const RMessage& m11, const RMessage& m12,
		       const RMessage& m13, const RMessage& m14,
		       const RMessage& m15, const RMessage& m16
		    )
	{
	strstream sout;
	StringReporter srpt( sout );

	srpt.Report(m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
	sout << '\0';
	IValue *ret = error_ivalue( sout.str(), file, line );
	if ( allwarn )
		{
		cerr << "E[" << ++error_count << "]: ";
		ret->Describe( cerr );
		cerr << endl;
		}
	Sequencer::SetErrorResult( ret );
	}
#endif
