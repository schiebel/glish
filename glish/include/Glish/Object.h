// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef object_h
#define object_h
#include "Glish/Str.h"
#include "sos/ref.h"

// GlishObject is the root of the class hierarchy.  GlishObjects know how to
// describe themselves.

//
// Line number and file to associate with newly created objects..
//
extern int line_num;
extern Str *file_name;
class OStream;
class Value;
class RMessage;
extern RMessage EndMessage;
extern Str glish_errno;

typedef const char* charptr;
typedef SosRef GlishRef;

class GlishObject : public GlishRef {
    public:
	GlishObject() : file( file_name && file_name->chars() ?
			      new Str(*file_name) : 0 ),
			line(line_num), description(0) { }
	GlishObject(const char *d) : file( file_name && file_name->chars() ?
			      new Str(*file_name) : 0 ),
			line(line_num), description(d) { }
	virtual ~GlishObject()	{ if ( file ) delete file; }

	int Line()		{ return line; }

	// Generate a long description of the object to the
	// given stream.  This typically includes descriptions of
	// subobjects as well as this object.
	virtual void Describe( OStream& ) const;

	// Generate a short description of the object to the
	// given stream. Returns non-zero if something was actually
	// written to the stream.
	virtual int DescribeSelf( OStream&, charptr prefix = 0 ) const;

	// Non-virtual, non-const versions of Describe() and DescribeSelf().
	// We add it here so that if when deriving a subclass of GlishObject we
	// forget the "const" declaration on the Describe/DescribeSelf
	// member functions, we'll hopefully get a warning message that
	// we're shadowing a non-virtual function.
	void Describe( OStream& stream )
		{ ((const GlishObject*) this)->Describe( stream ); }
	int DescribeSelf( OStream& stream, charptr prefix = 0 )
		{ return ((const GlishObject*) this)->DescribeSelf( stream, prefix ); }
	// Get a quick (minimal) description of the object. This is
	// used in CallExpr::Eval() to get the name of the function.
	// Getting it via a stream is just too much overhead.
	const char *Description() const { return description; }

	const Str strFail( const RMessage&, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage 
		) const;

	Value *Fail( const RMessage&, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage, const RMessage& = EndMessage,
		const RMessage& = EndMessage 
		) const;

	Value *Fail( Value * ) const;
	Value *Fail( ) const;

    protected:
	const char* description;
	int line;
	Str *file;
	};

#endif	/* object_h */
