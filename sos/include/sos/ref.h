//======================================================================
// sos/ref.h
//
// $Id$
//
// Copyright (c) 1997 Associated Universities Inc.
//
//======================================================================
#ifndef sos_ref_h_
#define sos_ref_h_

class SosRef {
    public:
	SosRef() : ref_count(1) 	{ }
	virtual ~SosRef()		{ }

	// Return the ref count so other classes can do intelligent copying.
	int RefCount() const	{ return ref_count; }
    protected:
	friend inline void Ref( SosRef* object );
	friend inline void Unref( SosRef* object );

	int ref_count;
};

inline void Ref( SosRef* object )
	{
	++object->ref_count;
	}

inline void Unref( SosRef* object )
	{
	if ( object && --object->ref_count == 0 )
		delete object;
	}

#endif
