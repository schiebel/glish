//======================================================================
// sos/ref.h
//
// $Id$
//
// Copyright (c) 1997,1998 Associated Universities Inc.
//
//======================================================================
#ifndef sos_ref_h_
#define sos_ref_h_

class SosRef {
    public:
	SosRef() : ref_count(1), flags(0)	{ }
	virtual ~SosRef()			{ }

	// Return the ref count so other classes can do intelligent copying.
	unsigned short RefCount() const		{ return ref_count; }

	void MarkFinal( ) 			{ flags |= 1; }
	void ClearFinal( )			{ flags &= ~1; }
	int doFinal( ) const			{ return flags & 1; }

	virtual int Finalize( );

    protected:
	friend inline void Ref( SosRef* object );
	friend inline void Unref( SosRef* object );

	unsigned short ref_count;
	unsigned short flags;
};

inline void Ref( SosRef* object )
	{
	++object->ref_count;
	}

inline void Unref( SosRef* object )
	{
	if ( object && --object->ref_count == 0 )
		if ( ! object->doFinal( ) || object->Finalize( ) )
			delete object;
	}

#endif
