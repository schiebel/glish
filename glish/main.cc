// $Id$
//
// Copyright (c) 1993 The Regents of the University of California.
// All rights reserved.
// Copyright (c) 1997,1998,2000 Associated Universities Inc.
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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <strstream.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <termio.h>

#if HAVE_OSFCN_H
#include <osfcn.h>
#endif

#ifdef HAVE_SIGINFO_H
#include <siginfo.h>
#endif
#ifdef HAVE_UCONTEXT_H
#include <ucontext.h>
#endif
#ifdef HAVE_FLOATINGPOINT_H
#include <floatingpoint.h>
#endif
#ifdef HAVE_MACHINE_FPU_H
#include <machine/fpu.h>
#endif

#include "input.h"
#include "glishlib.h"
#include "Glish/Reporter.h"
#include "Sequencer.h"
#include "IValue.h"
#include "Task.h"

static Sequencer* s = 0;
// used to recover from a ^C typed while glish
// is executing an instruction
int glish_top_jmpbuf_set = 0;
jmp_buf glish_top_jmpbuf;
int glish_include_jmpbuf_set = 0;
jmp_buf glish_include_jmpbuf;
int glish_silent = 0;
int glish_collecting_garbage = 0;

int allwarn = 0;
static unsigned int error_count = 0;

#if USE_EDITLINE
extern "C" {
	char *readline( const char * );
	char *nb_readline( const char * );
	extern char *rl_data_incomplete;
	void add_history( char * );
	void nb_readline_cleanup();
	void *create_editor( );
	void *set_editor( void * );
}
#endif

static void glish_dump_core( const char * );
static int handling_fatal_signal = 0;

static void install_terminate_handlers();

void glish_cleanup( )
	{
#if USE_EDITLINE
	nb_readline_cleanup();
#endif
	set_term_unchar_mode();
	Sequencer::CurSeq()->AbortOccurred();
	}


#define DEFINE_SIG_FWD(NAME,STRING,SIGNAL,COREDUMP)			\
void NAME( )								\
	{								\
	glish_cleanup( );						\
	fprintf(stderr,"\n[fatal error, '%s' (signal %d), exiting]\n",	\
			STRING, SIGNAL);				\
									\
	install_signal_handler( SIGNAL, (signal_handler) SIG_DFL );	\
									\
	COREDUMP							\
									\
	kill( getpid(), SIGNAL );					\
	}


DEFINE_SIG_FWD(glish_sighup,"hangup signal",SIGHUP,)
DEFINE_SIG_FWD(glish_sigterm,"terminate signal",SIGTERM,)
DEFINE_SIG_FWD(glish_sigabrt,"abort signal",SIGABRT,)

void glish_sigquit( )
	{
	glish_cleanup( );
	fprintf(stderr,"exiting on ^\\ ...\n");
	fflush(stderr);
	exit(0);
	}

extern void yyrestart( FILE * );
void glish_sigint( )
	{
	if ( glish_include_jmpbuf_set || glish_top_jmpbuf_set )
		{
		if ( Sequencer::ActiveAwait() )
			message->Report( Sequencer::ActiveAwait()->TerminateInfo() );
		Sequencer::TopLevelReset();
		unblock_signal(SIGINT);
		if ( glish_include_jmpbuf_set )
			longjmp( glish_include_jmpbuf, 1 );
		else
			longjmp( glish_top_jmpbuf, 1 );
		}

	char answ = 0;
	struct termio tbuf, tbufsave;
	int did_ioctl = 0;

	fprintf(stdout,"\nexit glish (y/n)? ");
	fflush(stdout);

	if ( ioctl( fileno(stdin), TCGETA, &tbuf) != -1 )
		{
		tbufsave = tbuf;
		tbuf.c_lflag &= ~ICANON;
		tbuf.c_cc[4] = 1;		/* MIN */
		tbuf.c_cc[5] = 9;		/* TIME */
		if ( ioctl( fileno(stdin), TCSETAF, &tbuf ) != -1 )
			did_ioctl = 1;
		}

	read( fileno(stdin), &answ, 1 );

	if ( did_ioctl )
		ioctl( fileno(stdin), TCSETAF, &tbufsave );

	fputc('\n',stdout);
	fflush(stdout);

	if ( answ == 'y' || answ == 'Y' )
		{
		glish_cleanup( );
		install_signal_handler( SIGINT, (signal_handler) SIG_DFL );
		kill(getpid(), SIGINT);
		}

	unblock_signal(SIGINT);
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

	evalOpt opt;
	s->Exec(opt);

	glish_cleanup();

	delete s;

#ifdef LINT
	delete srpt;
#endif

	return 0;
	}

#define DUMP_CORE						\
	if ( ! handling_fatal_signal )				\
		{						\
		handling_fatal_signal = 1;			\
		glish_dump_core( "glish.core" );		\
		}

DEFINE_SIG_FWD(glish_sigsegv,"segmentation violation",SIGSEGV,DUMP_CORE)
DEFINE_SIG_FWD(glish_sigbus,"bus error",SIGBUS,DUMP_CORE)
DEFINE_SIG_FWD(glish_sigill,"illegal instruction",SIGILL,DUMP_CORE)
#ifdef SIGEMT
DEFINE_SIG_FWD(glish_sigemt,"hardware fault",SIGEMT,DUMP_CORE)
#endif
DEFINE_SIG_FWD(glish_sigtrap,"hardware fault",SIGTRAP,DUMP_CORE)
#ifdef SIGSYS
DEFINE_SIG_FWD(glish_sigsys,"invalid system call",SIGSYS,DUMP_CORE)
#endif

static void install_sigfpe();
static void install_terminate_handlers()
	{
	(void) install_signal_handler( SIGSEGV, glish_sigsegv );
	(void) install_signal_handler( SIGBUS, glish_sigbus );
	(void) install_signal_handler( SIGILL, glish_sigill );
#ifdef SIGEMT
	(void) install_signal_handler( SIGEMT, glish_sigemt );
#endif
	install_sigfpe();
	(void) install_signal_handler( SIGTRAP, glish_sigtrap );
#ifdef SIGSYS
	(void) install_signal_handler( SIGSYS, glish_sigsys );
#endif
	}


#if USE_EDITLINE

static int fmt_readline_str( char* to_buf, int max_size, char* from_buf )
	{
	if ( from_buf )
		{
		char* from_buf_start = from_buf;

		while ( isspace(*from_buf_start) )
			++from_buf_start;

		if ( (int) strlen( from_buf_start ) <= max_size )
			to_buf = strcpy( to_buf, from_buf_start );
		else
			{
			error->Stream() << "Not enough buffer size (in fmt_readline_str)"
			     << endl;
			free_memory( (void*) from_buf );
			return 0;
			}
		  
		sprintf( to_buf, "%s\n", from_buf_start );

		if ( from_buf )
			free_memory( (void*) from_buf );

		return strlen( to_buf );
		}

	else
		return 0;
	}

char *readline_read( const char *prompt, int new_editor )
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
	void *last_editor = 0;

	//
	// tell readline to create a new editor environment?
	//
	if ( new_editor ) last_editor = set_editor( create_editor( ) );

	ret = nb_readline( prompt );

	while ( ret == rl_data_incomplete )
		{
		int clc = current_sequencer->EventLoop();
		ret = clc ? nb_readline( prompt ) : readline( prompt );
		}

	if ( ret && *ret ) add_history( ret );

	if ( last_editor ) free_memory( set_editor( last_editor ) );

	return ret;
	}

int interactive_read( FILE* /* file */, const char prompt[], char buf[],
			int max_size )
	{
	return fmt_readline_str( buf, max_size, readline_read(prompt,0) );
	}

#endif

DEFINE_CREATE_VALUE(IValue)

Value *copy_value( const Value *value )
	{
	return (Value*)copy_value( (const IValue*) value );
	}

int write_agent( sos_out &sos, Value *val_, sos_header &head, const ProxyId &proxy_id )
	{
	if ( val_->Type() != TYPE_AGENT )
		return 0;

	IValue *val = (IValue*) val_;
	Agent *agent = val->AgentVal();
	if ( ! agent || ! agent->IsProxy() )
		{
// 		warn->Report( "non-proxy agent" );
		return 0;
		}

	ProxyTask *pxy = (ProxyTask*) agent;
	if ( pxy->Id().interp() != proxy_id.interp() ||
	     pxy->Id().task() != proxy_id.task() )
		{
		warn->Report( "attempt to pass proxy agent to client which did not create it" );
		return 0;
		}

	sos.put( (int*) pxy->Id().array(), ProxyId::len(), head, sos_sink::COPY );
	return 1;
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
	srpt->report( ioOpt(ioOpt::NO_NEWLINE(), 3 ), m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
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
	srpt->report( ioOpt( ioOpt::NO_NEWLINE(), 3 ), m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
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
	srpt->report( ioOpt(ioOpt::NO_NEWLINE(), 3 ), m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
	if ( allwarn )
		error->Stream() << "E[" << ++error_count << "]: " <<
		  ((SOStream&)srpt->Stream()).str() << endl;
	return Str( (const char *) ((SOStream&)srpt->Stream()).str() );
	}

void describe_value( IValue *v )
	{
	message->Report(v);
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
	srpt->report( ioOpt(ioOpt::NO_NEWLINE(), 3 ), m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
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
	srpt->report( ioOpt(ioOpt::NO_NEWLINE(), 3 ), m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,m10,m11,m12,m13,m14,m15,m16);
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

static void glish_dump_core( const char *file )
	{
	glish_silent = 1;
	int fd = open(file,O_WRONLY|O_CREAT|O_TRUNC,0600);
	if ( fd < 0 ) return;

	// 's' above has not *yet* been assigned
	Scope *scope = Sequencer::CurSeq()->GetScope( );
	int len = scope->Length();

	char **fields = alloc_charptr( len );
	const IValue **vals = (const IValue**) alloc_ivalueptr( len );
	if ( ! fields || ! vals ) return;

	IterCookie *c = scope->InitForIteration();
	const Expr *member;
	const char *key;

	sos_fd_sink sink( fd );
	sos_out sos( &sink );

	c = scope->InitForIteration( );

	int alen = 0;
	while ( (member = scope->NextEntry( key, c )) )
		{
		if ( key && key[0] == '*' && key[strlen(key)-1] == '*' )
			continue;

		if ( member && ((VarExpr*)member)->Access() == USE_ACCESS )
			{
			// "val" may be LEAKED, but we don't care;
			//  we're dumping core
			evalOpt opt;
			const IValue *val = ((Expr*)member)->ReadOnlyEval( opt );

			glish_type type = val->Type();
			if ( type != TYPE_FUNC && type != TYPE_AGENT &&
			     (type != TYPE_RECORD || ! val->IsAgentRecord()) )
				{
				fields[alen] = (char*) key;
				vals[alen++] = val;
				}
			}
		}

	sos.put_record_start( alen );
	sos.put( fields, alen );
	for ( int i=0; i < alen; i++ )
		write_value( sos, (Value*) vals[i], "" );

	sos.flush( );
	}

//
//  Handle SIGFPE, the complications are:
//
//	o  solaris 2.* must use sigfpe()
//	o  alpha must compile with -ieee and isolate portions
//		which need -ieee due to performance hit
//
//  note that these signal handlers are only used to trap integer
//  division problems, floats should happen with IEEE NaN and Inf
//
int glish_abort_on_fpe = 1;
int glish_sigfpe_trap = 0;
#if defined(__alpha) || defined(__alpha__)
int glish_alpha_sigfpe_init = 0;
#endif

//
// ONLY SOLARIS 2.*, I believe...
//
#if defined(HAVE_SIGFPE) && defined(FPE_INTDIV)
//
//  Catch integer division exception to prevent "1 % 0" from
//  crashing glish...
//
void glish_sigfpe( int, siginfo_t *, ucontext_t *uap )
	{
	glish_sigfpe_trap = 1;
	/*
	**  Increment program counter; ieee_handler does this by
	**  default, but here we have to use sigfpe() to set up the
	**  signal handler for integer divide by 0.
	*/
	uap->uc_mcontext.gregs[REG_PC] = uap->uc_mcontext.gregs[REG_nPC];

	if ( glish_abort_on_fpe )
		{
		glish_cleanup( );
		fprintf(stderr,"\n[fatal error, 'floating point exception' (signal %d), exiting]\n", SIGFPE );
		sigfpe(FPE_INTDIV, (RETSIGTYPE (*)(...)) SIGFPE_DEFAULT);
		kill( getpid(), SIGFPE );
		}
	}

//
//  Currently for solaris "as_short(1/0)" et al. doesn't give the right
//  result. There doesn't seem to be a good fix for this because casting
//  doesn't generate an exception; division is not the problem.
//
static void install_sigfpe() { sigfpe(FPE_INTDIV, (RETSIGTYPE (*)(...)) glish_sigfpe ); }
#elif defined(__alpha) || defined(__alpha__)
//
// for the alpha, this should be defined in "alpha.c"
//
extern "C" void glish_sigfpe();
static void install_sigfpe()
	{
	glish_alpha_sigfpe_init = 1;
	install_signal_handler( SIGFPE, glish_sigfpe );
	ieee_set_fp_control(IEEE_TRAP_ENABLE_INV);
	}
#else
void glish_sigfpe( )
	{
	glish_sigfpe_trap = 1;

	if ( glish_abort_on_fpe )
		{
		glish_cleanup( );
		fprintf(stderr,"\n[fatal error, 'floating point exception' (signal %d), exiting]\n", SIGFPE );
		install_signal_handler( SIGFPE, (signal_handler) SIG_DFL );
		kill( getpid(), SIGFPE );
		}
	}

static void install_sigfpe() { install_signal_handler( SIGFPE, glish_sigfpe ); }
#endif

static char copyright1[]  = "Copyright (c) 1993 The Regents of the University of California.";
static char copyright2[]  = "Copyright (c) 1997,1998,1999 Associated Universities Inc.";
