// $Header$

#ifndef object_h
#define object_h
#include "Glish/Str.h"

// GlishObject is the root of the class hierarchy.  GlishObjects know how to
// describe themselves.

//
// Line number and file to associate with newly created objects..
//
extern int line_num;
extern Str *file_name;
class ostream;
class Value;
class RMessage;
extern RMessage EndMessage;
extern Str glish_errno;

class GlishRef {
    public:
	GlishRef() : ref_count(1) 	{ }
	virtual ~GlishRef()		{ }

	// Return the ref count so other classes can do intelligent copying.
	int RefCount() const	{ return ref_count; }
    protected:
	friend inline void Ref( GlishRef* object );
	friend inline void Unref( GlishRef* object );

	int ref_count;
};

class GlishObject : public GlishRef {
    public:
	GlishObject() : file( file_name ? new Str(*file_name) : 0 ),
			line(line_num) { }
	virtual ~GlishObject()	{ }

	int Line()		{ return line; }

	// Generate a long description of the object to the
	// given stream.  This typically includes descriptions of
	// subobjects as well as this object.
	virtual void Describe( ostream& ) const;

	// Generate a short description of the object to the
	// given stream.
	virtual void DescribeSelf( ostream& ) const;

	// Non-virtual, non-const versions of Describe() and DescribeSelf().
	// We add it here so that if when deriving a subclass of GlishObject we
	// forget the "const" declaration on the Describe/DescribeSelf
	// member functions, we'll hopefully get a warning message that
	// we're shadowing a non-virtual function.
	void Describe( ostream& stream )
		{ ((const GlishObject*) this)->Describe( stream ); }
	void DescribeSelf( ostream& stream )
		{ ((const GlishObject*) this)->DescribeSelf( stream ); }

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


inline void Ref( GlishRef* object )
	{
	++object->ref_count;
	}

inline void Unref( GlishRef* object )
	{
	if ( object && --object->ref_count == 0 )
		delete object;
	}

#endif	/* object_h */
