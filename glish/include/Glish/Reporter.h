// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997,1998 Associated Universities Inc.
#ifndef reporter_h
#define reporter_h

#include "Glish/Object.h"

class OStream;
class SOStream;
class Value;
class Reporter;

class RMessage GC_FINAL_CLASS {
    public:
	RMessage( const GlishObject* message_object );
	RMessage( const char* message_string );
	RMessage( int message_int );
	RMessage( void* message_void );

	// Writes its value to the given stream.  leading_space true means
	// that if appropriate (i.e., if the Rmessage object is an GlishObject
	// and not a string), then a leading space should first be written;
	// similarly, trailing_space indicates whether or not a trailing
	// space should be written for an GlishObject.
	//  The character returned is that *last* character written, if
	// known (it will be known in the string case but not in the
	// GlishObject case), or '\0' if not known.
	char Write( OStream&, int leading_space, int trailing_space, const ioOpt &opt ) const;

	// Returns the *first* character which would be written, if
	// known (it will be known in the string case but not in the
	// GlishObject case), or '\0' if not known.
	char FirstChar() const;

    protected:
	const GlishObject* object;
	const char* str;
	int int_val;
	void *void_val;
	};


extern RMessage EndMessage;


class Reporter GC_FINAL_CLASS {
    public:
	Reporter( OStream *reporter_stream );
	virtual ~Reporter( );

	virtual void report( const ioOpt &opt, const RMessage&,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage,
			     const RMessage& = EndMessage, const RMessage& = EndMessage 
			   );

	void Report( const RMessage &m0,
		     const RMessage &m1 = EndMessage, const RMessage &m2 = EndMessage,
		     const RMessage &m3 = EndMessage, const RMessage &m4 = EndMessage,
		     const RMessage &m5 = EndMessage, const RMessage &m6 = EndMessage,
		     const RMessage &m7 = EndMessage, const RMessage &m8 = EndMessage,
		     const RMessage &m9 = EndMessage, const RMessage &m10 = EndMessage,
		     const RMessage &m11 = EndMessage, const RMessage &m12 = EndMessage,
		     const RMessage &m13 = EndMessage, const RMessage &m14 = EndMessage,
		     const RMessage &m15 = EndMessage, const RMessage &m16 = EndMessage 
		   )
		{ report( ioOpt(), m0, m1, m2, m3, m4, m5, m6, m7,
			  m8, m9, m10, m11, m12, m13, m14, m15, m16 ); }

	// Count of how many times this reporter has generated a message
	int Count()			{ return count; }
	void SetCount( int new_count )	{ count = new_count; }

	OStream &Stream() { return stream; }

    protected:
	int loggable;
	int do_log;
	static SOStream *sout;
	virtual void Prolog(const ioOpt&);
	virtual void Epilog(const ioOpt&);

	int count;

	OStream& stream;
	};


// Unfortunately "error", "fatal", and "warn" are popular names and their
// use has led to a number of hard-to-debug name clashes.
#define error glish_error
#define warn glish_warn
#define fatal glish_fatal
#define message glish_message

extern Reporter* error;
extern Reporter* warn;
extern Reporter* fatal;
extern Reporter* message;

extern void init_reporters();
extern void finalize_reporters();

extern void report_error( const RMessage&, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage 
		);

extern void report_error( const char *file, int line,
		const RMessage&, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage 
		);

extern Value *generate_error( const RMessage&, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage 
		);
extern Value *generate_error( const char *file, int line,
		const RMessage&, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage 
		);

extern const Str generate_error_str( const RMessage&, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage 
		);
#endif	/* reporter_h */
