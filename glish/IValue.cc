// $Header$

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"

#include <string.h>
#include <stream.h>
#include <stdlib.h>

#include "Sds/sdsgen.h"
#include "IValue.h"

#include "glish_event.h"
#include "BinOpExpr.h"
#include "Func.h"
#include "Reporter.h"
#include "Sequencer.h"
#include "Agent.h"

#define AGENT_MEMBER_NAME "*agent*"

#define DEFINE_SINGLETON_CONSTRUCTOR(constructor_type)			\
IValue::IValue( constructor_type value )					\
	{								\
	InitValue();							\
	SetValue( &value, 1, COPY_ARRAY );				\
	}

#define DEFINE_ARRAY_CONSTRUCTOR(constructor_type)			\
IValue::IValue( constructor_type value[], int len, array_storage_type storage )\
	{								\
	InitValue();							\
	SetValue( value, len, storage );				\
	}

DEFINE_SINGLETON_CONSTRUCTOR(funcptr)
DEFINE_ARRAY_CONSTRUCTOR(funcptr)

IValue::IValue( agentptr value, array_storage_type storage )
	{
	InitValue();
	if ( storage != COPY_ARRAY )
		{
		agentptr *ary = new agentptr[1];
		copy_array(&value,ary,1,agentptr);
		SetValue( ary, 1, storage );
		}
	else
		SetValue( &value, 1, storage );
	}

IValue::IValue( recordptr value, Agent* agent )
	{
	InitValue();
	SetValue( value, agent );
	}

void IValue::SetValue( agentptr array[], int len, array_storage_type arg_storage )
	{
	SetType( TYPE_AGENT );
	max_size = length = len;
	storage = arg_storage;
	if ( storage == COPY_ARRAY ) {
		values = copy_values(array, agentptr);
		for (int i = 0; i < len; i++)
			Ref(array[i]);
	} else
		values = array;
	}

#define DEFINE_ARRAY_SET_VALUE(type, glish_type)			\
void IValue::SetValue( type array[], int len, array_storage_type arg_storage )\
	{								\
	SetType( glish_type );						\
	max_size = length = len;					\
	storage = arg_storage;						\
	values = storage == COPY_ARRAY ? copy_values( array, type ) : array;\
	}

DEFINE_ARRAY_SET_VALUE(funcptr,TYPE_FUNC)

void IValue::SetValue( recordptr value )
	{
	Value::SetValue( value );
	}

void IValue::SetValue( recordptr value, Agent* agent )
	{
	Value::SetValue( value );

	RecordPtr()->Insert( strdup( AGENT_MEMBER_NAME ),
					new IValue( agent, TAKE_OVER_ARRAY ) );
	}

void IValue::DeleteValue()
	{
	if ( type == TYPE_AGENT )
		{
		// Here we rely on the fact that Agent is derived
		// GlishObject, which has a virtual destructor.
		Unref( (GlishObject*) AgentVal() );
		}

	Value::DeleteValue();
	type = TYPE_ERROR;
	}

IValue::~IValue()
	{
	DeleteValue();
	}

int IValue::IsAgentRecord() const
	{
	if ( VecRefDeref()->Type() == TYPE_RECORD &&
	     (*RecordPtr())[AGENT_MEMBER_NAME] )
		return 1;
	else
		return 0;
	}


#define DEFINE_CONST_ACCESSOR(name,tag,type)				\
type IValue::name() const						\
	{								\
	if ( IsVecRef() ) 						\
		return ((const IValue*) VecRefPtr()->Val())->name();	\
	else if ( Type() != tag )					\
		fatal->Report( "bad use of const accessor" );		\
	return (type) values;						\
	}

DEFINE_CONST_ACCESSOR(FuncPtr,TYPE_FUNC,funcptr*)
DEFINE_CONST_ACCESSOR(AgentPtr,TYPE_AGENT,agentptr*)


#define DEFINE_ACCESSOR(name,tag,type)					\
type IValue::name()							\
	{								\
	if ( IsVecRef() ) 						\
		return ((IValue*)VecRefPtr()->Val())->name();		\
	if ( Type() != tag )						\
		Polymorph( tag );					\
	return (type) values;						\
	}

DEFINE_ACCESSOR(FuncPtr,TYPE_FUNC,funcptr*)
DEFINE_ACCESSOR(AgentPtr,TYPE_AGENT,agentptr*)

Agent* IValue::AgentVal() const
	{
	if ( type == TYPE_AGENT )
		return AgentPtr()[0];

	if ( VecRefDeref()->Type() == TYPE_RECORD )
		{
		Value* member = (*RecordPtr())[AGENT_MEMBER_NAME];

		if ( member )
			return ((IValue*)member)->AgentVal();
		}

	error->Report( this, " is not an agent value" );
	return 0;
	}

Func* IValue::FuncVal() const
	{
	if ( type != TYPE_FUNC )
		{
		error->Report( this, " is not a function value" );
		return 0;
		}

	if ( length == 0 )
		{
		error->Report( "empty function array" );
		return 0;
		}

	if ( length > 1 )
		warn->Report( "more than one function element in", this,
				", excess ignored" );

	return FuncPtr()[0];
	}


funcptr* IValue::CoerceToFuncArray( int& is_copy, int size, funcptr* result ) const
	{
	if ( type != TYPE_FUNC )
		fatal->Report( "non-func type in CoerceToFuncArray()" );

	if ( size != length )
		fatal->Report( "size != length in CoerceToFuncArray()" );

	if ( result )
		fatal->Report( "prespecified result in CoerceToFuncArray()" );

	is_copy = 0;
	return FuncPtr();
	}



void IValue::AssignArrayElements( int* indices, int num_indices, Value* value,
				int rhs_len )
	{
	if ( IsRef() )
		{
		Deref()->AssignArrayElements(indices,num_indices,value,rhs_len);
		return;
		}

	int max_index;
	if ( ! IndexRange( indices, num_indices, max_index ) )
		return;

	if ( max_index > Length() )
		if ( ! Grow( (unsigned int) max_index ) )
			return;

	switch ( Type() )
		{
#define ASSIGN_ARRAY_ELEMENTS_ACTION(tag,lhs_type,rhs_type,accessor,coerce_func,copy_func,delete_old_value)		\
	case tag:							\
		{							\
		int rhs_copy;						\
		rhs_type rhs_array = ((IValue*)value)->coerce_func( rhs_copy,	\
							rhs_len );	\
		lhs_type lhs = accessor;				\
		for ( int i = 0; i < num_indices; ++i )		\
			{						\
			delete_old_value				\
			lhs[indices[i]-1] = copy_func(rhs_array[i]);	\
			}						\
									\
		if ( rhs_copy )						\
			delete rhs_array;				\
									\
		break;							\
		}

ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_BOOL,glish_bool*,glish_bool*,BoolPtr(),
	CoerceToBoolArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_BYTE,byte*,byte*,BytePtr(),
	CoerceToByteArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_SHORT,short*,short*,ShortPtr(),
	CoerceToShortArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_INT,int*,int*,IntPtr(),
	CoerceToIntArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_FLOAT,float*,float*,FloatPtr(),
	CoerceToFloatArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_DOUBLE,double*,double*,DoublePtr(),
	CoerceToDoubleArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_COMPLEX,complex*,complex*,ComplexPtr(),
	CoerceToComplexArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_DCOMPLEX,dcomplex*,dcomplex*,DcomplexPtr(),
	CoerceToDcomplexArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_STRING,charptr*,charptr*,StringPtr(),
	CoerceToStringArray, strdup, delete (char*) (lhs[indices[i]-1]);)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_FUNC,funcptr*,funcptr*,FuncPtr(),
	CoerceToFuncArray,,)

		case TYPE_SUBVEC_CONST:
		case TYPE_SUBVEC_REF:
			switch ( VecRefPtr()->Type() )
				{
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_BOOL,glish_boolref&,glish_bool*,BoolRef(),
	CoerceToBoolArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_BYTE,byteref&,byte*,ByteRef(),
	CoerceToByteArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_SHORT,shortref&,short*,ShortRef(),
	CoerceToShortArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_INT,intref&,int*,IntRef(),
	CoerceToIntArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_FLOAT,floatref&,float*,FloatRef(),
	CoerceToFloatArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_DOUBLE,doubleref&,double*,DoubleRef(),
	CoerceToDoubleArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_COMPLEX,complexref&,complex*,ComplexRef(),
	CoerceToComplexArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_DCOMPLEX,dcomplexref&,dcomplex*,DcomplexRef(),
	CoerceToDcomplexArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_STRING,charptrref&,charptr*,StringRef(),
	CoerceToStringArray, strdup, delete (char*) (lhs[indices[i]-1]);)

				default:
					fatal->Report(
			"bad subvec type in IValue::AssignArrayElements()" );
				}
			break;

		default:
			fatal->Report(
				"bad type in IValue::AssignArrayElements()" );
		}
	}

void IValue::AssignArrayElements( Value* value )
	{
	if ( IsRef() )
		{
		Deref()->AssignArrayElements(value);
		return;
		}

	int max_index = Length();
	int val_len = value->Length();

	if ( Length() > val_len )
		{
		warn->Report( "partial assignment to \"",this,"\"" );
		max_index = val_len;
		}

	else if ( Length() < val_len )
		if ( ! Grow( (unsigned int) val_len ) )
			warn->Report( "partial assignment from \"",value,"\"" );

	switch ( Type() )
		{
#define ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(tag,type_lhs,type_rhs,accessor,coerce_func,copy_func,delete_old_value)	\
	case tag:							\
		{							\
		int rhs_copy;						\
		type_rhs rhs_array = ((IValue*)value->Deref())->coerce_func(	\
						rhs_copy, max_index );	\
		type_lhs lhs = accessor;				\
		for ( int i = 0; i < max_index; ++i )			\
			{						\
			delete_old_value				\
			lhs[i] = copy_func( rhs_array[i] );		\
			}						\
									\
		if ( rhs_copy )						\
			delete rhs_array;				\
									\
		break;							\
		}

ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_BOOL,glish_bool*,glish_bool*,BoolPtr(),
	CoerceToBoolArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_BYTE,byte*,byte*,BytePtr(),
	CoerceToByteArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_SHORT,short*,short*,ShortPtr(),
	CoerceToShortArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_INT,int*,int*,IntPtr(),
	CoerceToIntArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_FLOAT,float*,float*,FloatPtr(),
	CoerceToFloatArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_DOUBLE,double*,double*,DoublePtr(),
	CoerceToDoubleArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_COMPLEX,complex*,complex*,
	ComplexPtr(),CoerceToComplexArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_DCOMPLEX,dcomplex*,dcomplex*,
	DcomplexPtr(),CoerceToDcomplexArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_STRING,charptr*,charptr*,StringPtr(),
	CoerceToStringArray,strdup, delete (char*) (lhs[i]);)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_FUNC,funcptr*,funcptr*,FuncPtr(),
	CoerceToFuncArray,,)

		case TYPE_SUBVEC_REF:
			switch ( VecRefPtr()->Type() )
				{
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_BOOL,glish_boolref&,glish_bool*,
	BoolRef(), CoerceToBoolArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_BYTE,byteref&,byte*,ByteRef(),
	CoerceToByteArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_SHORT,shortref&,short*,ShortRef(),
	CoerceToShortArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_INT,intref&,int*,IntRef(),
	CoerceToIntArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_FLOAT,floatref&,float*,FloatRef(),
	CoerceToFloatArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_DOUBLE,doubleref&,double*,DoubleRef(),
	CoerceToDoubleArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_COMPLEX,complexref&,complex*,
	ComplexRef(),CoerceToComplexArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_DCOMPLEX,dcomplexref&,dcomplex*,
	DcomplexRef(), CoerceToDcomplexArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_STRING,charptrref&,charptr*,StringRef(),
	CoerceToStringArray, strdup, delete (char*) (lhs[i]);)

				default:
					fatal->Report(
		"bad sub-array reference in IValue::AssignArrayElements()" );
				}
			break;
		default:
			fatal->Report(
				"bad type in IValue::AssignArrayElements()" );
		}
	}

#define DEFINE_XXX_ARITH_OP_COMPUTE(name,type,coerce_func)		\
void IValue::name( const IValue* value, int lhs_len, ArithExpr* expr )	\
	{								\
	int lhs_copy, rhs_copy;					\
	type* lhs_array = coerce_func( lhs_copy, lhs_len );		\
	type* rhs_array = ((IValue*)value)->coerce_func( rhs_copy, value->Length() );\
									\
	int rhs_incr = value->Length() == 1 ? 0 : 1;			\
									\
	expr->Compute( lhs_array, rhs_array, lhs_len, rhs_incr );	\
									\
	if ( lhs_copy )							\
		{							\
		Value* attr = TakeAttributes();				\
		/* Change our value to the new result. */		\
		SetValue( lhs_array, lhs_len );				\
		if ( attr )						\
			AssignAttributes( attr );			\
		else							\
			CopyAttributes( value );			\
		}							\
									\
	if ( rhs_copy )							\
		delete rhs_array;					\
	}


DEFINE_XXX_ARITH_OP_COMPUTE(ByteOpCompute,byte,CoerceToByteArray)
DEFINE_XXX_ARITH_OP_COMPUTE(ShortOpCompute,short,CoerceToShortArray)
DEFINE_XXX_ARITH_OP_COMPUTE(IntOpCompute,int,CoerceToIntArray)
DEFINE_XXX_ARITH_OP_COMPUTE(FloatOpCompute,float,CoerceToFloatArray)
DEFINE_XXX_ARITH_OP_COMPUTE(DoubleOpCompute,double,CoerceToDoubleArray)
DEFINE_XXX_ARITH_OP_COMPUTE(ComplexOpCompute,complex,CoerceToComplexArray)
DEFINE_XXX_ARITH_OP_COMPUTE(DcomplexOpCompute,dcomplex,CoerceToDcomplexArray)


void IValue::Polymorph( glish_type new_type )
	{
	if ( type == new_type )
		return;

	if ( type == TYPE_SUBVEC_REF || type == TYPE_SUBVEC_CONST )
		{
		// ### hmmm, seems polymorphing a const subvec should be an
		// error ...
		VecRefPtr()->Val()->Polymorph( new_type );
		return;
		}

	if ( new_type == TYPE_INT && type == TYPE_BOOL )
		{
		// ### We do bool -> int conversions in place, relying on
		// the fact that bools are actually implemented as int's
		// with a value of either 0 (F) or 1 (T).  Note that this
		// is probably *buggy* since presently the internal "bool"
		// type is defined using an enumeration instead of as "int",
		// so a compiler might choose a smaller type.  Fixing this
		// is on the to-do list.
		type = new_type;
		return;
		}

	switch ( new_type )
		{
#define POLYMORPH_ACTION(tag,type,coerce_func)				\
	case tag:							\
		{							\
		int is_copy;						\
		type* new_val = coerce_func( is_copy, length );		\
		if ( is_copy )						\
			{						\
			Value* attr = TakeAttributes();			\
			SetValue( new_val, length );			\
			AssignAttributes( attr );			\
			}						\
		break;							\
		}

POLYMORPH_ACTION(TYPE_BOOL,glish_bool,CoerceToBoolArray)
POLYMORPH_ACTION(TYPE_BYTE,byte,CoerceToByteArray)
POLYMORPH_ACTION(TYPE_SHORT,short,CoerceToShortArray)
POLYMORPH_ACTION(TYPE_INT,int,CoerceToIntArray)
POLYMORPH_ACTION(TYPE_FLOAT,float,CoerceToFloatArray)
POLYMORPH_ACTION(TYPE_DOUBLE,double,CoerceToDoubleArray)
POLYMORPH_ACTION(TYPE_COMPLEX,complex,CoerceToComplexArray)
POLYMORPH_ACTION(TYPE_DCOMPLEX,dcomplex,CoerceToDcomplexArray)
POLYMORPH_ACTION(TYPE_STRING,charptr,CoerceToStringArray)
POLYMORPH_ACTION(TYPE_FUNC,funcptr,CoerceToFuncArray)

		case TYPE_RECORD:
			if ( length > 1 )
				warn->Report(
			"array values lost due to conversion to record type" );

			SetValue( create_record_dict() );

			break;

		default:
			fatal->Report( "bad type in IValue::Polymorph()" );
		}
	}

void IValue::DescribeSelf( ostream& s ) const
	{
	if ( type == TYPE_FUNC )
		{
		// ### what if we're an array of functions?
		FuncVal()->Describe( s );
		}
	else
		Value::DescribeSelf( s );
	}

IValue* copy_value( const IValue* value )
	{
	if ( value->IsRef() )
		return copy_value( (const IValue*) value->RefPtr() );

	IValue* copy;
	switch ( value->Type() )
		{
#define COPY_VALUE(tag,accessor)					\
	case tag:							\
		copy = new IValue( value->accessor, value->Length(),	\
					COPY_ARRAY );			\
		break;

COPY_VALUE(TYPE_BOOL,BoolPtr())
COPY_VALUE(TYPE_BYTE,BytePtr())
COPY_VALUE(TYPE_SHORT,ShortPtr())
COPY_VALUE(TYPE_INT,IntPtr())
COPY_VALUE(TYPE_FLOAT,FloatPtr())
COPY_VALUE(TYPE_DOUBLE,DoublePtr())
COPY_VALUE(TYPE_COMPLEX,ComplexPtr())
COPY_VALUE(TYPE_DCOMPLEX,DcomplexPtr())
COPY_VALUE(TYPE_STRING,StringPtr())
COPY_VALUE(TYPE_FUNC,FuncPtr())

		case TYPE_AGENT:
			copy = new IValue( value->AgentVal() );
			break;

		case TYPE_RECORD:
			{
			if ( value->IsAgentRecord() )
				{
				warn->Report(
		"cannot copy agent record value; returning reference instead" );
				return new IValue( (Value*) value, VAL_REF );
				}

			recordptr rptr = value->RecordPtr();
			recordptr new_record = create_record_dict();

			for ( int i = 0; i < rptr->Length(); ++i )
				{
				const char* key;
				Value* member = rptr->NthEntry( i, key );

				new_record->Insert( strdup( key ),
							copy_value( (IValue*) member ) );
				}

			copy = new IValue( new_record );
			break;
			}

		case TYPE_OPAQUE:
			{
			// _AIX requires a temporary
			SDS_Index tmp(value->SDS_IndexVal());
			copy = new IValue( tmp );
			}
			break;

		case TYPE_SUBVEC_REF:
		case TYPE_SUBVEC_CONST:
			switch ( value->VecRefPtr()->Type() )
				{
#define COPY_REF(tag,accessor)						\
	case tag:							\
		copy = new IValue( value->accessor ); 			\
		break;

				COPY_REF(TYPE_BOOL,BoolRef())
				COPY_REF(TYPE_BYTE,ByteRef())
				COPY_REF(TYPE_SHORT,ShortRef())
				COPY_REF(TYPE_INT,IntRef())
				COPY_REF(TYPE_FLOAT,FloatRef())
				COPY_REF(TYPE_DOUBLE,DoubleRef())
				COPY_REF(TYPE_COMPLEX,ComplexRef())
				COPY_REF(TYPE_DCOMPLEX,DcomplexRef())
				COPY_REF(TYPE_STRING,StringRef())

				default:
					fatal->Report(
						"bad type in copy_value()" );
				}
			break;

		default:
			fatal->Report( "bad type in copy_value()" );
		}

	copy->CopyAttributes( value );
	return copy;
	}

#define DEFINE_XXX_REL_OP_COMPUTE(name,type,coerce_func)		\
IValue* name( const IValue* lhs, const IValue* rhs, int lhs_len, RelExpr* expr )\
	{								\
	int lhs_copy, rhs_copy;					\
	type* lhs_array = lhs->coerce_func( lhs_copy, lhs_len );	\
	type* rhs_array = rhs->coerce_func( rhs_copy, rhs->Length() );	\
	glish_bool* result = new glish_bool[lhs_len];			\
									\
	int rhs_incr = rhs->Length() == 1 ? 0 : 1;			\
									\
	expr->Compute( lhs_array, rhs_array, result, lhs_len, rhs_incr );\
									\
	if ( lhs_copy )							\
		delete lhs_array;					\
									\
	if ( rhs_copy )							\
		delete rhs_array;					\
									\
	IValue* answer = new IValue( result, lhs_len );			\
	answer->CopyAttributes( lhs->AttributePtr() ? lhs : rhs );	\
	return answer;							\
	}


DEFINE_XXX_REL_OP_COMPUTE(bool_rel_op_compute,glish_bool,CoerceToBoolArray)
DEFINE_XXX_REL_OP_COMPUTE(byte_rel_op_compute,byte,CoerceToByteArray)
DEFINE_XXX_REL_OP_COMPUTE(short_rel_op_compute,short,CoerceToShortArray)
DEFINE_XXX_REL_OP_COMPUTE(int_rel_op_compute,int,CoerceToIntArray)
DEFINE_XXX_REL_OP_COMPUTE(float_rel_op_compute,float,CoerceToFloatArray)
DEFINE_XXX_REL_OP_COMPUTE(double_rel_op_compute,double,CoerceToDoubleArray)
DEFINE_XXX_REL_OP_COMPUTE(complex_rel_op_compute,complex,CoerceToComplexArray)
DEFINE_XXX_REL_OP_COMPUTE(dcomplex_rel_op_compute,dcomplex,
	CoerceToDcomplexArray)
DEFINE_XXX_REL_OP_COMPUTE(string_rel_op_compute,charptr,CoerceToStringArray)
