// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <stdlib.h>
#include <iostream.h>
#include <string.h>

#include "Reporter.h"
#include "Glish/Stream.h"
#include "input.h"
#include "system.h"

int interactive = 0;
SOStream *Reporter::sout = 0;

extern void show_glish_stack( OStream& );
extern void log_output( const char * );
extern int do_output_log();
extern int glish_silent;

class WarningReporter : public Reporter {
    public:
	WarningReporter() : Reporter( new ProxyStream(cerr) )	{ }
	virtual void Prolog( const ioOpt & );
	};


class ErrorReporter : public Reporter {
    public:
	ErrorReporter() : Reporter( new ProxyStream(cerr) )	{ }
	virtual void Prolog( const ioOpt & );
	};


class FatalReporter : public Reporter {
    public:
	FatalReporter() : Reporter( new ProxyStream(cerr) )	{ }
	virtual void Prolog( const ioOpt & );
	virtual void Epilog( const ioOpt & );
	};


class MessageReporter : public Reporter {
    public:
	MessageReporter() : Reporter( new ProxyStream(cout) )	{ }
	virtual void Prolog( const ioOpt & );
	};


RMessage EndMessage( "" );

// Due to bugs in gcc-2.1, we don't initialize these here but rather
// have Sequencer::Sequencer do it.

Reporter* warn = 0;
Reporter* error = 0;
Reporter* fatal = 0;
Reporter* message = 0;


RMessage::RMessage( const GlishObject* message_object )
	{
	object = message_object;
	str = 0;
	void_val = 0;
	}

RMessage::RMessage( const char* message_string )
	{
	str = message_string;
	object = 0;
	void_val = 0;
	}

RMessage::RMessage( void *message_void )
	{
	str = 0;
	object = 0;
	void_val = message_void;
	}

RMessage::RMessage( int message_int )
	{
	str = 0;
	object = 0;
	void_val = 0;
	int_val = message_int;
	}

char RMessage::Write( OStream& s, int leading_space, int trailing_space, const ioOpt &opt ) const
	{
	if ( object )
		{
		// If opt.sep() is 0, it means that there won't be any
		// inter-element separation. If this is the case, don't add
		// leading or trailing spaces either...
		if ( leading_space && opt.sep() )
			s << " ";

		object->Describe( s, opt );

		if ( trailing_space && opt.sep() )
			s << " ";

		return '\0';
		}

	else if ( str )
		{
		if ( str[0] )
			{
			s << str;
			return str[strlen( str ) - 1];
			}

		else
			return '\0';
		}

	else if ( void_val )
		{
		if ( leading_space )
			s << " ";

		s << void_val;

		if ( trailing_space )
			s << " ";

		return '\0';
		}
	else
		{
		if ( leading_space )
			s << " ";

		s << int_val;

		if ( trailing_space )
			s << " ";

		return '\0';
		}
	}

char RMessage::FirstChar() const
	{
	if ( object )
		return '\0';

	else if ( str )
		return str[0];

	else
		return '\0';
	}


Reporter::Reporter( OStream *reporter_stream ) : stream( *reporter_stream )
	{
	count = 0;
	loggable = 1;
	do_log = 0;
	if ( ! sout )
		sout = new SOStream;
	else
		Ref(sout);
	}

Reporter::~Reporter( )
	{
	Unref( &stream );
	Unref( sout );
	}

void Reporter::report( const ioOpt &opt, const RMessage& m0,
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

	if ( glish_silent ) return;

	const int max_messages = 50;
	const RMessage* messages[max_messages];

	messages[0] = &m0;
	messages[1] = &m1;
	messages[2] = &m2;
	messages[3] = &m3;
	messages[4] = &m4;
	messages[5] = &m5;
	messages[6] = &m6;
	messages[7] = &m7;
	messages[8] = &m8;
	messages[9] = &m9;
	messages[10] = &m10;
	messages[11] = &m11;
	messages[12] = &m12;
	messages[13] = &m13;
	messages[14] = &m14;
	messages[15] = &m15;
	messages[16] = &m16;
	messages[11] = &EndMessage;

	if ( (do_log = loggable && do_output_log()) )
		sout->reset();

	Prolog(opt);

	const char* suppress_following_blank = " ([{#@$%-`'\"";
	const char* suppress_preceding_blank = " )]},:;.'\"";

	int leading_space = 0;
	int trailing_space;

	for ( int i = 0; i < max_messages; ++i )
		{
		char c;

		if ( messages[i] == &EndMessage )
			break;

		if ( i == max_messages - 1 )
			trailing_space = 0;

		else
			{
			char c = messages[i+1]->FirstChar();
			if ( c && strchr( suppress_preceding_blank, c ) )
				trailing_space = 0;
			else
				trailing_space = 1;
			}

		c = messages[i]->Write( stream, leading_space, trailing_space, opt );
		if ( do_log )
			messages[i]->Write( *sout, leading_space, trailing_space, opt );

		leading_space = ! (c && strchr( suppress_following_blank, c ));
		}

	if ( do_log )
		log_output( sout->str() );

	Epilog(opt);

	++count;
	}

void Reporter::Prolog( const ioOpt &opt )
	{
	if ( ! interactive )
		{
		if ( file_name && glish_files )
			{
			stream << "\"" << (*glish_files)[file_name] << "\", ";
			if ( do_log )
				*sout << "\"" << (*glish_files)[file_name] << "\", ";
			}

		if ( line_num > 0 )
			{
			stream << "line " << line_num << ": ";
			if ( do_log )
				*sout << "line " << line_num << ": ";
			}
		}
	}

void Reporter::Epilog( const ioOpt &opt )
	{
	if ( ! opt.flags(ioOpt::NO_NEWLINE()) )
		stream << "\n";
	stream.flush();
	}


void WarningReporter::Prolog( const ioOpt &opt )
	{
	Reporter::Prolog(opt);
	stream << "warning, ";
	if ( do_log ) *sout << "warning, ";
	}


void ErrorReporter::Prolog( const ioOpt &opt )
	{
	Reporter::Prolog(opt);
	stream << "error, ";
	if ( do_log ) *sout << "error, ";
	}


void FatalReporter::Prolog( const ioOpt &opt )
	{
	show_glish_stack( stream );
	Reporter::Prolog(opt);
	stream << "fatal internal error, ";
	if ( do_log ) *sout << "fatal internal error, ";
	}

void FatalReporter::Epilog( const ioOpt &opt )
	{
	Reporter::Epilog(opt);
	glish_cleanup();
	exit( 1 );
	}


void MessageReporter::Prolog( const ioOpt & )
	{
	}


void init_reporters()
	{
	static int did_init = 0;
	if ( ! did_init )
		{
		warn = new WarningReporter;
		error = new ErrorReporter;
		fatal = new FatalReporter;
		message = new MessageReporter;
		did_init = 1;
		}
	}

void finalize_reporters()
	{
	static int did_final = 0;
	if ( ! did_final )
		{
		if ( warn ) delete warn;
		if ( error ) delete error;
		if ( fatal ) delete fatal;
		if ( message ) delete message;
		did_final = 1;
		}
	}

