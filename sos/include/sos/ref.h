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

#if defined(ENABLE_GC)
#include <gcmem/gc_cpp.h>
#endif
#include "sos/generic.h"
#include "sos/alloc.h"

class GcRef;
class sos_name2(GcRef,PList);
typedef sos_name2(GcRef,PList) ref_list;

typedef unsigned short refmode_t;

class GcRef GC_FINAL_CLASS {
    public:
	enum cycle_type { NONROOT, ROOT };
	GcRef() : zero(0), unref(0), mask(0), ref_count(1) { }
	GcRef( const GcRef & );

	virtual ~GcRef()		{ if ( zero ) do_zero(); }

	// Return the ref count so other classes can do intelligent copying.
	unsigned short RefCount() const		 { return ref_count; }

	// Register probable cycles
	static void AddCycleRoot( GcRef * );

	// Add the pointer to be zeroed when
	// this value is deleted.
	void AddZero( void *p );
	void RemoveZero( void *p=0 );

	void SetUnref( GcRef *r, int propagate_only=0 );
	void ClearUnref( );

	// Should we carefully carry along a mirror to this
	// value when it is copied?
	virtual int MirrorSet( ) const;
	void MarkMirror( );
	void ClearMirror( );

	// How this object creates cycles (if it does)
	virtual cycle_type CycleMode( ) const;

/*     protected: */
	inline refmode_t mZERO( refmode_t mask=~((refmode_t) 0) ) const { return mask & 1<<0; }
	inline refmode_t mZEROLIST( refmode_t mask=~((refmode_t) 0) ) const { return mask & 1<<1; }
	inline refmode_t mUNREF( refmode_t mask=~((refmode_t) 0) ) const { return mask & 1<<2; }
	inline refmode_t mPROPAGATE( refmode_t mask=~((refmode_t) 0) ) const { return mask & 1<<3; }
	inline refmode_t mMIRROR( refmode_t mask=~((refmode_t) 0) ) const { return mask & 1<<4; }

	friend inline void Ref( GcRef* object );
	friend inline void Unref( GcRef* object );

	void do_zero( );

	int doUnref( ) const { return mUNREF(mask); }
	int doPropagate( ) const { return mPROPAGATE(mask); }

	void *zero;
	GcRef *unref;
	refmode_t mask;
	unsigned short ref_count;
	static ref_list *cycle_roots;
};

extern void sos_do_unref( GcRef * );

inline void Ref( GcRef* object )
	{
	++object->ref_count;
	}

inline void Unref( GcRef* object )
	{
	if ( object && --object->ref_count == 0 )
		{
		GcRef *unref = object->doUnref() ? object->unref : 0;
		if ( object->zero ) object->do_zero( );
		delete object;
		if ( unref ) sos_do_unref( unref );
		}
	}

#include <sos/list.h>

#endif
