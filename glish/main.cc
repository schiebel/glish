// $Id$
//
// Copyright (c) 1993 The Regents of the University of California.
// All rights reserved.
// Copyright (c) 1997 Associated Universities Inc.
// All rights reserved.
//
// This code is derived from software contributed to Berkeley by
// Vern Paxson and software contributed to Associated Universities
// Inc. by Darrell Schiebel.
//
// The United States Government has rights in this work pursuant
// to contract no. DE-AC03-76SF00098 between the United States
// Department of Energy and the University of California, contract
// no. DE-AC02-89ER40486 between the United States Department of Energy
// and the Universities Research Association, Inc. and Cooperative
// Research Agreement #AST-9223814 between the United States National
// Science Foundation and Associated Universities, Inc.
//
// Redistribution and use in source and binary forms are permitted
// provided that: (1) source distributions retain this entire
// copyright notice and comment, and (2) distributions including
// binaries display the following acknowledgement:  ``This product
// includes software developed by the University of California,
// Berkeley, the National Radio Astronomy Observatory (NRAO), and
// their contributors'' in the documentation or other materials
// provided with the distribution and in all advertising materials
// mentioning features or use of this software.  Neither the names of
// the University or NRAO or the names of their contributors may be
// used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE.
//

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
	Sequencer::CurSeq()->System().AbortOccurred();
	}


#define DEFINE_SIG_FWD(NAME,STRING,SIGNAL)				\
void NAME( )								\
	{								\
	glish_cleanup( );						\
	fprintf(stderr,"\n[fatal error, '%s' (signal %d), exiting]\n",	\
			STRING, SIGNAL);				\
									\
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
		if ( Sequencer::ActiveAwait() )
			message->Report( Sequencer::ActiveAwait()->TerminateInfo() );
		Sequencer::TopLevelReset();
		unblock_signal(SIGINT);
		longjmp( glish_top_level, 1 );
		}

	glish_cleanup( );
	install_signal_handler( SIGINT, (signal_handler) SIG_DFL );
	kill(getpid(), SIGINT);
	}

class StringReporter : public Reporter {
    public:
	StringReporter( OStream* reporter_stream ) :
		Reporter( reporter_stream ) { loggable = 0; }
	void Epilog();
	void Prolog();
	};

void StringReporter::Epilog() { }
void StringReporter::Prolog() { }
static StringReporter *srpt = 0;

int main( int argc, char** argv )
	{
	srpt = new StringReporter( new SOStream );
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
	delete srpt;

	return 0;
	}

DEFINE_SIG_FWD(glish_sigsegv,"segmentation violation",SIGSEGV)
DEFINE_SIG_FWD(glish_sigbus,"bus error",SIGBUS)
DEFINE_SIG_FWD(glish_sigill,"illegal instruction",SIGILL)
#ifdef SIGEMT
DEFINE_SIG_FWD(glish_sigemt,"hardware fault",SIGEMT)
#endif
DEFINE_SIG_FWD(glish_sigfpe,"floating point exception",SIGFPE)
DEFINE_SIG_FWD(glish_sigtrap,"hardware fault",SIGTRAP)
#ifdef SIGSYS
DEFINE_SIG_FWD(glish_sigsys,"invalid system call",SIGSYS)
#endif

static void install_terminate_handlers()
	{
	(void) install_signal_handler( SIGSEGV, glish_sigsegv );
	(void) install_signal_handler( SIGBUS, glish_sigbus );
	(void) install_signal_handler( SIGILL, glish_sigill );
#ifdef SIGEMT
	(void) install_signal_handler( SIGEMT, glish_sigemt );
#endif
	(void) install_signal_handler( SIGFPE, glish_sigfpe );
	(void) install_signal_handler( SIGTRAP, glish_sigtrap );
#ifdef SIGSYS
	(void) install_signal_handler( SIGSYS, glish_sigsys );
#endif
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
			error->Stream() << "Not enough buffer size (in fmt_readline_str)"
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

#endif

DEFINE_CREATE_VALUE(IValue)

Value *copy_value( const Value *value )
	{
	return (Value*)copy_value( (const IValue*) value );
	}

int lookup_print_precision( )
	{
	return Sequencer::CurSeq()->System().PrintPrecision();
	}
int lookup_print_limit( )
	{
	return Sequencer::CurSeq()->System().PrintLimit();
	}

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
	srpt->Stream().reset();
	srpt->Report(m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
	IValue *ret = error_ivalue( ((SOStream&)srpt->Stream()).str() );
	if ( allwarn )
		{
		error->Stream() << "E[" << ++error_count << "]: ";
		ret->Describe( error->Stream() );
		error->Stream() << endl;
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
	srpt->Stream().reset();
	srpt->Report(m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
	IValue *ret = error_ivalue( ((SOStream&)srpt->Stream()).str(), file, line );
	if ( allwarn )
		{
		error->Stream() << "E[" << ++error_count << "]: ";
		ret->Describe( error->Stream() );
		error->Stream() << endl;
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
	srpt->Stream().reset();
	srpt->Report(m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
	if ( allwarn )
		error->Stream() << "E[" << ++error_count << "]: " <<
		  ((SOStream&)srpt->Stream()).str() << endl;
	return Str( (const char *) ((SOStream&)srpt->Stream()).str() );
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
	srpt->Stream().reset();
	srpt->Report(m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
	IValue *ret = error_ivalue( ((SOStream&)srpt->Stream()).str() );
	if ( allwarn )
		{
		error->Stream() << "E[" << ++error_count << "]: ";
		ret->Describe( error->Stream() );
		error->Stream() << endl;
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
	srpt->Stream().reset();
	srpt->Report(m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
	IValue *ret = error_ivalue( ((SOStream&)srpt->Stream()).str(), file, line );
	if ( allwarn )
		{
		error->Stream() << "E[" << ++error_count << "]: ";
		ret->Describe( error->Stream() );
		error->Stream() << endl;
		}
	Sequencer::SetErrorResult( ret );
	}

void log_output( const char *s )
	{
	if ( Sequencer::CurSeq()->System().OLog() )
		Sequencer::CurSeq()->System().DoOLog( s );
	}

int do_output_log()
	{
	return Sequencer::CurSeq()->System().OLog();
	}

void show_glish_stack( OStream &s )
	{
	Sequencer::CurSeq()->DescribeFrames( s );
	s << endl;
	}

static char copyright1[]  = "Copyright (c) 1993 The Regents of the University of California.";
static char copyright2[]  = "Copyright (c) 1997 Associated Universities Inc.";
