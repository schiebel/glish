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

extern void sos_do_unref( GcRef * );

class GcRef GC_FINAL_CLASS {
    public:
	GcRef() : unref(0), mask(0), ref_count(1) { }
	GcRef( const GcRef & );

	virtual ~GcRef();

	// Return the ref count so other classes can do intelligent copying.
	unsigned short RefCount() const		 { return ref_count; }

	// Register probable cycles
	static void AddCycleRoot( GcRef * );

	void SetUnref( GcRef *r, int propagate_only=0 );
	void ClearUnref( );

	void MarkFrame( ) { mask |= mCALLFRAME(); }
	void ClearFrame( ) { mask &= ~ mCALLFRAME(); }
	int FrameMember( ) const { return mCALLFRAME(mask); }

	virtual int SoftDelete( );

    protected:
	inline refmode_t mUNREF( refmode_t mask=~((refmode_t) 0) ) const { return mask & 1<<0; }
	inline refmode_t mUNREF_REVERT( refmode_t mask=~((refmode_t) 0) ) const { return mask & 1<<1; }
	inline refmode_t mPROPAGATE( refmode_t mask=~((refmode_t) 0) ) const { return mask & 1<<2; }
	inline refmode_t mCALLFRAME( refmode_t mask=~((refmode_t) 0) ) const { return mask & 1<<3; }

	inline refmode_t get_revert_count( ) const { return mask>>4; }
	inline void set_revert_count( unsigned short value ) { mask = mask & 0xF | value<<4; }

	friend inline void Ref( GcRef* object );
	friend inline void Unref( GcRef* object );
	friend void sos_do_unref( GcRef * );

	void unref_revert( );

	int doUnref( ) const { return mUNREF(mask) | mUNREF_REVERT(mask); }
	int doRevert( ) const { return mUNREF_REVERT(mask); }
	int doPropagate( ) const { return mPROPAGATE(mask); }

	GcRef *unref;
	refmode_t mask;
	unsigned short ref_count;
	static ref_list *cycle_roots;
};

inline void Ref( GcRef* object )
	{
	++object->ref_count;
	if ( object->unref && ! object->doUnref() )
		object->unref_revert( );
	}

inline void Unref( GcRef* object )
	{
	if ( object && --object->ref_count == 0 )
		sos_do_unref( object );
	}

#include <sos/list.h>

#endif
