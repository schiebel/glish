// $Id$
// Copyright (c) 1997 Associated Universities Inc.
//

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <memory.h>
#include <string.h>

#include "Glish/Value.h"
#include "Glish/VecRef.h"
#include "Reporter.h"
#include "system.h"

extern int glish_collecting_garbage;

glish_implement(SubVecRef,glish_bool)
glish_implement(SubVecRef,byte)
glish_implement(SubVecRef,short)
glish_implement(SubVecRef,int)
glish_implement(SubVecRef,float)
glish_implement(SubVecRef,double)
glish_implement(SubVecRef,complex)
glish_implement(SubVecRef,dcomplex)
glish_implement2(SubVecRef,charptr,strdup)

VecRef::TranslateIndex( int index, int* error ) const 
	{
	if ( error )
		*error = 0;

	if ( Type() == TYPE_RECORD )
	    return index;

	int offset = indices[index];
	if ( error && offset > val->Length() )
		*error = 1;

	return offset - 1;
	}

VecRef::VecRef( Value* value, int arg_indices[], int num, int arg_max_index,
		int take_indices )
	{
	len = num;
	max_index = arg_max_index;
	ref = 0;
	vec = 0;
	is_subvec_ref = 0;
	description = "vecref";

	Value* v = value->VecRefDeref();

	if ( ! v->IsNumeric() && v->Type() != TYPE_STRING)
		  error->Report( "bad type in VecRef::VecRef()" );

	if ( ! take_indices )
		{
		indices = (int*) alloc_memory( sizeof(int)*len );
		memcpy(indices, arg_indices, len * sizeof(int));
		}
	else
		indices = arg_indices;

	val = FindIndices( value, indices, num );
	Ref( val );
	}

VecRef::VecRef( Value* ref_value, int* index, int num, int arg_max_index,
		void* values, glish_type t )
	{
	val = ref_value;
	Ref( val );
	indices = index;
	is_subvec_ref = 1;
	len = num; 
	max_index = arg_max_index;
	vec = values;
	subtype = t;
	ref = 0;
	description = "vecref";
	}

VecRef::~VecRef()
	{
	if ( ! is_subvec_ref )
		// We "own" the indices.
		free_memory( indices );

	Unref( ref );
	if ( ! glish_collecting_garbage )
		Unref( val );
	}

Value* VecRef::FindIndices( Value* value, int* Indices, int num )
	{
	if ( value->IsVecRef() )
		{
		VecRef* vecref = value->VecRefPtr();
		for ( int i = 0; i < num; ++i )
			Indices[i] = vecref->indices[Indices[i] - 1];
		return FindIndices( vecref->Val(), Indices, num );
		}

	else if ( value->IsRef() )
		return FindIndices( value->Deref(), Indices, num );

	else
		return value;
	}

#define SUBVEC_SUBSCRIPT_ACTION(name,type,tag,accessor)			\
SubVecRef(type)* VecRef::name()						\
	{								\
	if ( val->Type() != tag )					\
		fatal->Report( "bad type in VecRef::name" );		\
									\
	if ( ref && ref->Type() == tag )				\
		/* We already have a suitable SubVecRef. */		\
		return (SubVecRef(type) *) ref;				\
									\
	vec = val->accessor();						\
	Unref( ref );							\
	ref = new SubVecRef(type)( val, indices, len,			\
					max_index, vec, tag );		\
	return (SubVecRef(type) *) ref;					\
	}

SUBVEC_SUBSCRIPT_ACTION(BoolRef,glish_bool,TYPE_BOOL,BoolPtr)
SUBVEC_SUBSCRIPT_ACTION(ByteRef,byte,TYPE_BYTE,BytePtr)
SUBVEC_SUBSCRIPT_ACTION(ShortRef,short,TYPE_SHORT,ShortPtr)
SUBVEC_SUBSCRIPT_ACTION(IntRef,int,TYPE_INT,IntPtr)
SUBVEC_SUBSCRIPT_ACTION(FloatRef,float,TYPE_FLOAT,FloatPtr)
SUBVEC_SUBSCRIPT_ACTION(DoubleRef,double,TYPE_DOUBLE,DoublePtr)
SUBVEC_SUBSCRIPT_ACTION(ComplexRef,complex,TYPE_COMPLEX,ComplexPtr)
SUBVEC_SUBSCRIPT_ACTION(DcomplexRef,dcomplex,TYPE_DCOMPLEX,DcomplexPtr)
SUBVEC_SUBSCRIPT_ACTION(StringRef,charptr,TYPE_STRING,StringPtr)
