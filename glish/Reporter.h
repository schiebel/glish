// $Header$

#ifndef reporter_h
#define reporter_h

#include "Glish/Object.h"

class ostream;
class Value;

class RMessage {
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
	char Write( ostream&, int leading_space, int trailing_space ) const;

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


class Reporter {
    public:
	Reporter( ostream& reporter_stream );

	void Report( const RMessage&,
		     const RMessage& = EndMessage, const RMessage& = EndMessage,
		     const RMessage& = EndMessage, const RMessage& = EndMessage,
		     const RMessage& = EndMessage, const RMessage& = EndMessage,
		     const RMessage& = EndMessage, const RMessage& = EndMessage,
		     const RMessage& = EndMessage, const RMessage& = EndMessage,
		     const RMessage& = EndMessage, const RMessage& = EndMessage,
		     const RMessage& = EndMessage, const RMessage& = EndMessage,
		     const RMessage& = EndMessage, const RMessage& = EndMessage 
		   );

	// Count of how many times this reporter has generated a message
	int Count()			{ return count; }
	void SetCount( int new_count )	{ count = new_count; }

    protected:
	virtual void Prolog();
	virtual void Epilog();

	int count;

	ostream& stream;
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
