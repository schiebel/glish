// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997,1998 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include <string.h>
#include <iostream.h>
#include <stdlib.h>
#include <ctype.h>

#include "Glish/Value.h"
#include "Glish/Client.h"

#include "glish_event.h"
#include "Reporter.h"

#define AGENT_MEMBER_NAME "*agent*"

extern int glish_collecting_garbage;

int num_Values_created = 0;
int num_Values_deleted = 0;

int glish_dummy_int =  0;

const char* type_names[NUM_GLISH_TYPES] =
	{
	"error", "ref", "subref",
	"boolean", "byte", "short", "integer",
	"float", "double", "string", "agent", "function", "record",
	"complex", "dcomplex", "fail", "regex", "file"
	};

const Value* false_value = 0;

#define INIT_VALUE_ACTION		\
	attributes = 0;			\
	++num_Values_created;

Value::Value( )
	{
	DIAG4( (void*) this, "Value(", " ",")" )
	INIT_VALUE_ACTION
	kernel.SetFail( create_record_dict() );
	}

Value::Value( glish_type )
	{
	INIT_VALUE_ACTION
	}

Value::Value( const char *message, const char *xfile, int lineNum )
	{
	DIAG4( (void*) this, "Value(", " ",")" )
	INIT_VALUE_ACTION
	kernel.SetFail( create_record_dict() );

	recordptr rptr = kernel.constRecord();
	attributeptr attr = ModAttributePtr();
	if ( xfile && xfile[0] )
		{
		rptr->Insert( strdup("file"), create_value( xfile ) );
		Unref( (Value*) attr->Insert( strdup("file"), create_value( xfile ) ) );
		if ( lineNum > 0 )
			{
			rptr->Insert( strdup("line"), create_value( lineNum ) );
			Unref( (Value*) attr->Insert( strdup("line"), create_value( lineNum ) ) );
			}
		}

	if ( message )
		{
		rptr->Insert( strdup("message"), create_value( message ) );
		Unref( (Value*) attr->Insert( strdup("message"), create_value( message ) ) );
		}
	}

Value::Value( const Value *val, const char *, int )
	{
	DIAG2( (void*) this, "Value( const Value *, const char *, int )" )
	INIT_VALUE_ACTION
	kernel = val->kernel;
	recordptr rptr = kernel.modRecord();
	Value *v = (*rptr)["file"];
	if ( v )
		{
		charptr *sptr = v->StringPtr(0);
		charptr *sary = (charptr*) alloc_memory( sizeof(charptr) * (v->Length() + 1) );
		int x = 0;
		for ( ; x < v->Length(); ++x )
			sary[x] = strdup(sptr[x]);
		sary[x] = (charptr) alloc_memory( strlen((*glish_files)[file]) + 1);
		strcpy( (char*) sary[x], (*glish_files)[file] );
		rptr->Insert("file",create_value( sary, v->Length() + 1 ));
		Unref(v);
		}
	v = (*rptr)["line"];
	if ( v )
		{
		int *iptr = v->IntPtr(0);
		int *iary = (int*) alloc_memory( sizeof(int) * (v->Length() + 1) );
		int x = 0;
		for ( ; x < v->Length(); ++x )
			iary[x] = iptr[x];
		iary[x] = line;
		rptr->Insert("line",create_value( iary, v->Length() + 1 ));
		}
	}

void Value::SetFailMessage( Value *nv )
	{
	if ( Type() != TYPE_FAIL )
		fatal->Report( "Value::SetFailValue called for non fail value" );

	recordptr rptr = kernel.constRecord();
	Unref( (Value*) rptr->Insert( strdup("message"), nv ) );
	attributeptr attr = ModAttributePtr();
	Unref( (Value*) attr->Insert( strdup("message"), copy_value(nv) ) );
	}

void Value::SetFail( recordptr rec )
	{
	if ( Type() != TYPE_FAIL )
		fatal->Report( "Value::SetFail called for non fail value" );
	kernel.SetFail( rec );
	}
	  
#define DEFINE_SINGLETON_CONSTRUCTOR(constructor_type)			\
Value::Value( constructor_type value )					\
	{								\
	DIAG4( (void*) this, "Value(", #constructor_type,")" )		\
	INIT_VALUE_ACTION						\
	kernel.SetArray( &value, 1, 1 );				\
	}

#define DEFINE_ARRAY_CONSTRUCTOR(constructor_type)			\
Value::Value( constructor_type value[], int len, array_storage_type s ) \
	{								\
	DIAG4( (void*) this, "Value(", #constructor_type, "[] )" )	\
	INIT_VALUE_ACTION						\
	kernel.SetArray( value, len, s == COPY_ARRAY || s == PRESERVE_ARRAY ); \
	}

#define DEFINE_ARRAY_REF_CONSTRUCTOR(constructor_type)			\
Value::Value( constructor_type& value_ref )				\
	{								\
	DIAG4( (void*) this, "Value(", #constructor_type, "& )" )	\
	INIT_VALUE_ACTION						\
	SetValue( value_ref );						\
	}

#define DEFINE_CONSTRUCTORS(type,reftype)				\
	DEFINE_SINGLETON_CONSTRUCTOR(type)				\
	DEFINE_ARRAY_CONSTRUCTOR(type)					\
	DEFINE_ARRAY_REF_CONSTRUCTOR(reftype)

DEFINE_CONSTRUCTORS(glish_bool,glish_boolref)
DEFINE_CONSTRUCTORS(byte,byteref)
DEFINE_CONSTRUCTORS(short,shortref)
DEFINE_CONSTRUCTORS(int,intref)
DEFINE_CONSTRUCTORS(float,floatref)
DEFINE_CONSTRUCTORS(double,doubleref)
DEFINE_CONSTRUCTORS(complex,complexref)
DEFINE_CONSTRUCTORS(dcomplex,dcomplexref)
DEFINE_CONSTRUCTORS(charptr,charptrref)

Value::Value( recordptr value )
	{
	DIAG2( (void*) this, "Value( recordptr )" )
	INIT_VALUE_ACTION
	kernel.SetRecord( value );
	}


Value::Value( Value* ref_value, value_type val_type )
	{
	DIAG2( (void*) this, "Value( Value*, value_type )" )
	INIT_VALUE_ACTION

	int is_const = ref_value->IsConst() | ref_value->IsRefConst();
	if ( val_type != VAL_CONST && val_type != VAL_REF )
		fatal->Report( "bad value_type in Value::Value" );

	ref_value = ref_value->Deref();
	is_const |= ref_value->IsConst() | ref_value->VecRefDeref()->IsConst() |
			ref_value->IsRefConst() | ref_value->VecRefDeref()->IsRefConst();

	kernel.SetValue(ref_value);

	if ( val_type == VAL_CONST )
		kernel.MakeConst();
	else if ( is_const )
		kernel.MakeModConst();
	

	attributes = ref_value->CopyAttributePtr();
	}

Value::Value( Value* ref_value, int index[], int num_elements,
		value_type val_type, int take_index )
	{
	DIAG2( (void*) this, "Value( Value*, int[], int, value_type )" )
	INIT_VALUE_ACTION
	SetValue( ref_value, index, num_elements, val_type, take_index );
	attributes = ref_value->CopyAttributePtr();
	}


void Value::TakeValue( Value* new_value, Str &err )
	{
	new_value = new_value->Deref();

	if ( new_value == this )
		{
		err = strFail( "reference loop created" );
		return;
		}

	DeleteValue();
	kernel = new_value->kernel;
	AssignAttributes(new_value->TakeAttributes());

	Unref( new_value );
	}


Value::~Value()
	{
	DeleteValue();
	++num_Values_deleted;
	}


#define DEFINE_REF_SET_VALUE(reftype)					\
void Value::SetValue( reftype& value_ref )				\
	{								\
	kernel.SetArray( value_ref.DupVec(), value_ref.Length() );	\
	}

DEFINE_REF_SET_VALUE(glish_boolref)
DEFINE_REF_SET_VALUE(byteref)
DEFINE_REF_SET_VALUE(shortref)
DEFINE_REF_SET_VALUE(intref)
DEFINE_REF_SET_VALUE(floatref)
DEFINE_REF_SET_VALUE(doubleref)
DEFINE_REF_SET_VALUE(complexref)
DEFINE_REF_SET_VALUE(dcomplexref)
DEFINE_REF_SET_VALUE(charptrref)


void Value::SetValue( Value* ref_value, int index[], int num_elements,
			value_type val_type, int take_index )
	{
	if ( val_type != VAL_CONST && val_type != VAL_REF )
		fatal->Report( "bad value_type in Value::Value" );

	if ( ref_value->IsConst() && val_type == VAL_REF )
		warn->Report(
			"\"ref\" reference created from \"const\" reference" );

	ref_value = ref_value->Deref();

	int max_index;
	if ( ! IndexRange( index, num_elements, max_index ) )
		fatal->Report( "bad index in Value::Value" );

	if ( max_index > ref_value->Length() )
		if ( ! ref_value->Grow( max_index ) )
			return;

	switch ( ref_value->Type() )
		{
		case TYPE_BOOL:
		case TYPE_BYTE:
		case TYPE_SHORT:
		case TYPE_INT:
		case TYPE_FLOAT:
		case TYPE_DOUBLE:
		case TYPE_COMPLEX:
		case TYPE_DCOMPLEX:
		case TYPE_STRING:
		case TYPE_SUBVEC_REF:
			{
			VecRef *vr = new VecRef( ref_value, index,
						num_elements, max_index, take_index );
			kernel.SetVecRef( vr );
			Unref(vr);
			}
			break;

		default:
			fatal->Report( "bad Value in Value::Value" );
		}

	if ( val_type == VAL_CONST )
		kernel.MakeConst( );

	}


void Value::DeleteValue()
	{
	DeleteAttributes();
	}

void Value::DeleteAttributes()
	{
	if ( ! glish_collecting_garbage )
		Unref( attributes );
	attributes = 0;
	}


void Value::DeleteAttribute( const Value* index )
	{
	char* index_string = index->StringVal();
	DeleteAttribute( index_string );
	free_memory( index_string );
	}

void Value::DeleteAttribute( const char field[] )
	{
	attributeptr attr = ModAttributePtr();
	if ( attr )
		delete attr->Remove( field );
	}

int Value::IsNumeric() const
	{
	switch ( Type() )
		{
		case TYPE_BOOL:
		case TYPE_BYTE:
		case TYPE_SHORT:
		case TYPE_INT:
		case TYPE_FLOAT:
		case TYPE_DOUBLE:
		case TYPE_COMPLEX:
		case TYPE_DCOMPLEX:
			return 1;

		case TYPE_REF:
		case TYPE_STRING:
		case TYPE_AGENT:
		case TYPE_FUNC:
		case TYPE_REGEX:
		case TYPE_FILE:
		case TYPE_RECORD:
		case TYPE_FAIL:
			return 0;

		case TYPE_SUBVEC_REF:
			return VecRefPtr()->Val()->IsNumeric();

		case TYPE_ERROR:
		default:
			fatal->Report( "bad type in Value::IsNumeric()" );

			return 0;	// for overly clever compilers
		}
	}


int Value::IsAgentRecord() const
	{
	if ( Type() == TYPE_RECORD )
		{
		Value *v = (*RecordPtr(0))[AGENT_MEMBER_NAME];
		if ( ! v ) return 0;
		v = v->Deref();
		if ( v->Type() == TYPE_INT && v->Length() == ProxyId::len() )
			return 1;
		else
			return 0;
		}
	else
		return 0;
	}

#define DEFINE_CONST_ACCESSOR(name,tag,type,MOD,CONST)			\
type Value::name( int modify ) const					\
	{								\
	if ( IsVecRef() ) 						\
		return ((const Value*) VecRefPtr()->Val())->name( modify ); \
	else if ( Type() != tag )					\
		fatal->Report( "bad use of const accessor" );		\
	return (type) (modify ? kernel.MOD() : kernel.CONST()); 	\
	}

DEFINE_CONST_ACCESSOR(BoolPtr,TYPE_BOOL,glish_bool*,modArray,constArray)
DEFINE_CONST_ACCESSOR(BytePtr,TYPE_BYTE,byte*,modArray,constArray)
DEFINE_CONST_ACCESSOR(ShortPtr,TYPE_SHORT,short*,modArray,constArray)
DEFINE_CONST_ACCESSOR(IntPtr,TYPE_INT,int*,modArray,constArray)
DEFINE_CONST_ACCESSOR(FloatPtr,TYPE_FLOAT,float*,modArray,constArray)
DEFINE_CONST_ACCESSOR(DoublePtr,TYPE_DOUBLE,double*,modArray,constArray)
DEFINE_CONST_ACCESSOR(ComplexPtr,TYPE_COMPLEX,complex*,modArray,constArray)
DEFINE_CONST_ACCESSOR(DcomplexPtr,TYPE_DCOMPLEX,dcomplex*,modArray,constArray)
DEFINE_CONST_ACCESSOR(StringPtr,TYPE_STRING,charptr*,modArray,constArray)
DEFINE_CONST_ACCESSOR(RecordPtr,TYPE_RECORD,recordptr,modRecord,constRecord)
DEFINE_CONST_ACCESSOR(FailPtr,TYPE_FAIL,recordptr,modRecord,constRecord)


#define DEFINE_ACCESSOR(name,tag,type,MOD,CONST)			\
type Value::name( int modify )						\
	{								\
	if ( IsVecRef() ) 						\
		return VecRefPtr()->Val()->name();			\
	if ( Type() != tag )						\
		Polymorph( tag );					\
	return (type) (modify ? kernel.MOD() : kernel.CONST());		\
	}

DEFINE_ACCESSOR(BoolPtr,TYPE_BOOL,glish_bool*,modArray,constArray)
DEFINE_ACCESSOR(BytePtr,TYPE_BYTE,byte*,modArray,constArray)
DEFINE_ACCESSOR(ShortPtr,TYPE_SHORT,short*,modArray,constArray)
DEFINE_ACCESSOR(IntPtr,TYPE_INT,int*,modArray,constArray)
DEFINE_ACCESSOR(FloatPtr,TYPE_FLOAT,float*,modArray,constArray)
DEFINE_ACCESSOR(DoublePtr,TYPE_DOUBLE,double*,modArray,constArray)
DEFINE_ACCESSOR(ComplexPtr,TYPE_COMPLEX,complex*,modArray,constArray)
DEFINE_ACCESSOR(DcomplexPtr,TYPE_DCOMPLEX,dcomplex*,modArray,constArray)
DEFINE_ACCESSOR(StringPtr,TYPE_STRING,charptr*,modArray,constArray)
DEFINE_ACCESSOR(RecordPtr,TYPE_RECORD,recordptr,modRecord,constRecord)
DEFINE_ACCESSOR(FailPtr,TYPE_FAIL,recordptr,modRecord,constRecord)

#define DEFINE_CONST_REF_ACCESSOR(name,tag,type)			\
type& Value::name() const						\
	{								\
	if ( ! IsVecRef() )						\
		fatal->Report( "bad use of subarray reference accessor" );\
	if ( VecRefPtr()->Type() != tag )				\
		fatal->Report( "bad use of subarray reference accessor" );\
	return *(VecRefPtr()->name());					\
	}

DEFINE_CONST_REF_ACCESSOR(BoolRef,TYPE_BOOL,glish_boolref)
DEFINE_CONST_REF_ACCESSOR(ByteRef,TYPE_BYTE,byteref)
DEFINE_CONST_REF_ACCESSOR(ShortRef,TYPE_SHORT,shortref)
DEFINE_CONST_REF_ACCESSOR(IntRef,TYPE_INT,intref)
DEFINE_CONST_REF_ACCESSOR(FloatRef,TYPE_FLOAT,floatref)
DEFINE_CONST_REF_ACCESSOR(DoubleRef,TYPE_DOUBLE,doubleref)
DEFINE_CONST_REF_ACCESSOR(ComplexRef,TYPE_COMPLEX,complexref)
DEFINE_CONST_REF_ACCESSOR(DcomplexRef,TYPE_DCOMPLEX,dcomplexref)
DEFINE_CONST_REF_ACCESSOR(StringRef,TYPE_STRING,charptrref)

#define DEFINE_REF_ACCESSOR(name,tag,type)				\
type& Value::name()							\
	{								\
	if ( ! IsVecRef() )						\
		fatal->Report( "bad use of subarray reference accessor" );\
	if ( VecRefPtr()->Type() != tag )				\
		Polymorph( tag );					\
	return *(VecRefPtr()->name());					\
	}

DEFINE_REF_ACCESSOR(BoolRef,TYPE_BOOL,glish_boolref)
DEFINE_REF_ACCESSOR(ByteRef,TYPE_BYTE,byteref)
DEFINE_REF_ACCESSOR(ShortRef,TYPE_SHORT,shortref)
DEFINE_REF_ACCESSOR(IntRef,TYPE_INT,intref)
DEFINE_REF_ACCESSOR(FloatRef,TYPE_FLOAT,floatref)
DEFINE_REF_ACCESSOR(DoubleRef,TYPE_DOUBLE,doubleref)
DEFINE_REF_ACCESSOR(ComplexRef,TYPE_COMPLEX,complexref)
DEFINE_REF_ACCESSOR(DcomplexRef,TYPE_DCOMPLEX,dcomplexref)
DEFINE_REF_ACCESSOR(StringRef,TYPE_STRING,charptrref)


#define XXX_VAL(name, val_type, rhs_elm, conv, text_func, type_name, zero) \
val_type Value::name( int n, Str &err ) const				\
	{								\
	glish_type type = Type();					\
									\
	if ( type == TYPE_FAIL)						\
		return zero;						\
									\
	if ( IsRef() )							\
		return Deref()->name( n );				\
									\
	if ( kernel.Length() < 1 )					\
		{							\
		err = strFail( "empty array converted to ", type_name );\
		return zero;						\
		}							\
									\
	if ( n < 1 || n > kernel.Length() )				\
		{							\
		err = strFail( "in conversion to ", type_name,		\
			" index (=", n, ") out of bounds, length =",	\
			kernel.Length() );				\
		return zero;						\
		}							\
									\
	switch ( type )							\
		{							\
		case TYPE_BOOL:						\
			return val_type( BoolPtr(0)[n - 1] ? 1 : 0 );	\
									\
		case TYPE_BYTE:						\
			return val_type( BytePtr(0)[n - 1] conv );	\
									\
		case TYPE_SHORT:					\
			return val_type( ShortPtr(0)[n - 1] conv );	\
									\
		case TYPE_INT:						\
			return val_type( IntPtr(0)[n - 1] conv );	\
									\
		case TYPE_FLOAT:					\
			return val_type( FloatPtr(0)[n - 1] conv );	\
									\
		case TYPE_DOUBLE:					\
			return val_type( DoublePtr(0)[n - 1] conv );	\
									\
		case TYPE_COMPLEX:					\
			{						\
			complex &ptr = ComplexPtr(0)[n - 1];		\
			return val_type( ptr rhs_elm conv );		\
			}						\
		case TYPE_DCOMPLEX:					\
			{						\
			dcomplex &ptr = DcomplexPtr(0)[n - 1];		\
			return val_type( ptr rhs_elm conv );		\
			}						\
		case TYPE_STRING:					\
			{						\
			int successful;					\
			val_type result = val_type(			\
				text_func( StringPtr(0)[n - 1], successful ) ); \
									\
			if ( ! successful )				\
				warn->Report( "string \"", this,	\
					"\" converted to ", type_name );\
			return result;					\
			}						\
									\
		case TYPE_AGENT:					\
		case TYPE_FUNC:						\
		case TYPE_REGEX:					\
		case TYPE_FILE:						\
		case TYPE_RECORD:					\
			err = strFail( "bad type", type_names[type],	\
				"converted to ", type_name, ":", this );\
			return zero;					\
									\
		case TYPE_SUBVEC_REF:					\
			{						\
			VecRef* ref = VecRefPtr();			\
			int error;					\
			int off = ref->TranslateIndex( n-1, &error );	\
			if ( error )					\
				{					\
				err = strFail( "bad sub-vector subscript" ); \
				return zero;				\
				}					\
			return ref->Val()->name( off+1 );		\
			}						\
									\
		default:						\
			fatal->Report( "bad type in Value::XXX_VAL()" );\
			return zero;					\
		}							\
	}

inline glish_bool text_to_bool( const char *str, int &successful )
	{ successful = 1; return *str ? glish_true : glish_false; }

XXX_VAL(BoolVal, glish_bool, .r || ptr.i, ? glish_true : glish_false, text_to_bool, "bool", glish_false)
XXX_VAL(ByteVal, byte, .r,, text_to_integer, "byte", 0)
XXX_VAL(ShortVal, short, .r,, text_to_integer, "short", 0)
XXX_VAL(IntVal, int, .r,, text_to_integer, "integer", 0)
XXX_VAL(FloatVal, float, .r,, text_to_double, "float", 0.0)
XXX_VAL(DoubleVal, double, .r,, text_to_double, "double", 0.0)
XXX_VAL(ComplexVal, complex,,, text_to_dcomplex, "complex", complex(0.0, 0.0))
XXX_VAL(DcomplexVal, dcomplex,,, text_to_dcomplex, "dcomplex",
	dcomplex(0.0, 0.0))

const char *print_decimal_prec( const attributeptr attr, const char *default_fmt )
	{
	int limit = -1;
	int tmp = 0;
	const Value *val;
	const Value *precv;
	static char prec[64];
	if ( attr && (val = (*attr)["print"]) && val->Type() == TYPE_RECORD &&
			val->HasRecordElement( "precision" ) &&
			(precv=val->ExistingRecordElement( "precision" )) &&
			precv != false_value && precv->IsNumeric() &&
			(tmp = precv->IntVal()) >= 0 )
		limit = tmp;
	else 
		limit = lookup_print_precision( );

	if ( limit >= 0 )
		{
		sprintf(prec,"%%.%dg",limit);
		return prec;
		}

	return default_fmt;
	}

unsigned int Value::PrintLimit( ) const
	{
	unsigned int limit = 0;
	int tmp = 0;
	const attributeptr attr = AttributePtr();
	const Value *val;
	const Value *limitv;
	if ( attr && (val = (*attr)["print"]) && val->Type() == TYPE_RECORD &&
	     		val->HasRecordElement( "limit" ) && 
			(limitv  = val->ExistingRecordElement( "limit" )) &&
			limitv != false_value && limitv->IsNumeric() &&
			(tmp = limitv->IntVal()) > 0 )
		limit = tmp;
	else 
		limit = lookup_print_limit();

	return limit;
	}

char* Value::RecordStringVal( char sep, int max_elements, 
			int use_attr, int evalable, Str &err ) const
	{
	static value_list been_there;

	if ( VecRefDeref()->Type() != TYPE_RECORD )
		fatal->Report( "non-record type in Value::RecordStringVal()" );

	recordptr rptr = RecordPtr(0);
	int len = rptr->Length();

	if ( len == 0 )
		return strdup( "[=]" );

	if ( been_there.is_member( (Value*) VecRefDeref() ) )
		{
		const char *key;
		rptr->NthEntry( 0, key );
		char *ret = (char*) alloc_memory( sizeof(char)*(strlen(key)+7) );
		strcpy(ret,"[");
		strcat(ret,key);
		strcat(ret,"=***]");
		return ret;
		}
	else
		been_there.append( (Value*) VecRefDeref() );


	const char** key_strs = (const char**) alloc_memory( sizeof(char*)*len );
	char** element_strs = (char**) alloc_memory( sizeof(char*)*len );
	int total_len = 0;

	for ( int i = 0; i < len && ( ! max_elements || i < max_elements ); ++i )
		{
		Value* nth_val = rptr->NthEntry( i, key_strs[i] );

		if ( ! nth_val )
			fatal->Report(
				"bad record in Value::RecordStringVal()" );

		element_strs[i] = nth_val->StringVal( sep, max_elements, use_attr, evalable, 0, err );
		total_len += strlen( element_strs[i] ) + strlen( key_strs[i] );
		}

	// We generate a result of the form [key1=val1, key2=val2, ...],
	// so in addition to room for the keys and values we need 3 extra
	// characters per element (for the '=', ',', and ' '), 2 more for
	// the []'s (we could steal these from the last element since it
	// doesn't have a ", " at the end of it, but that seems a bit
	// evil), and 1 more for the end-of-string.
	char* result = (char*) alloc_memory( sizeof(char)*(total_len + 3 * len + 10) );

	strcpy( result, "[" );

	for ( LOOPDECL i = 0; i < len && ( ! max_elements || i < max_elements ); ++i )
		{
		sprintf( &result[strlen( result )], "%s=%s, ",
			 key_strs[i], element_strs[i] );
		free_memory( element_strs[i] );
		}

	// Now add the final ']', taking care to wipe out the trailing
	// ", ".
	if ( max_elements && len > max_elements )
		strcpy( &result[strlen( result ) - 2], " ... ]" );
	else
		strcpy( &result[strlen( result ) - 2], "]" );

	been_there.remove( (Value*) VecRefDeref() );
	free_memory( key_strs );
	free_memory( element_strs );

	return result;
	}

Value* Value::Deref()
	{
	if ( IsRef() )
		return RefPtr()->Deref();
	else
		return this;
	}

const Value* Value::Deref() const
	{
	if ( IsRef() )
		return ((const Value*) RefPtr())->Deref();
	else
		return this;
	}

Value* Value::VecRefDeref()
	{
	if ( IsVecRef() )
		return VecRefPtr()->Val()->VecRefDeref();
	else if ( IsRef() )
		return RefPtr()->VecRefDeref();
	else
		return this;
	}

const Value* Value::VecRefDeref() const
	{
	if ( IsVecRef() )
		return ((const Value*) VecRefPtr()->Val())->VecRefDeref();
	else if ( IsRef() )
		return ((const Value*) RefPtr())->VecRefDeref();
	else
		return this;
	}


#define COERCE_HDR(name, ctype, gtype, type_name, accessor)		\
	int length = kernel.Length();					\
									\
	if ( IsRef() )							\
		return Deref()->name( is_copy, size, result );		\
									\
	if ( ! IsNumeric() )						\
		fatal->Report( "non-numeric type in coercion of", this,	\
				"to ", type_name );			\
									\
	if ( ! result && length == size && Type() == gtype )		\
		{							\
		is_copy = 0;						\
		return accessor(0);					\
		}							\
									\
	is_copy = 1;							\
	if ( ! result )							\
		result = (ctype*) alloc_memory( sizeof(ctype)*size );	\
									\
	int incr = (length == 1 ? 0 : 1);				\
	int i, j;

glish_bool* Value::CoerceToBoolArray( int& is_copy, int size,
				glish_bool* result ) const
	{
	COERCE_HDR(CoerceToBoolArray, glish_bool, TYPE_BOOL, "bool", BoolPtr)

	switch ( Type() )
		{
#define BOOL_COERCE_BOOL_ACTION(OFFSET,XLATE)				\
		case TYPE_BOOL:						\
		        {						\
			glish_bool* bool_ptr = BoolPtr(0);		\
			for ( i = 0, j = 0; i < size; ++i, j += incr )	\
				{					\
				XLATE					\
				result[i] = bool_ptr[ OFFSET ];		\
				}					\
			break;						\
			}

#define BOOL_COERCE_ACTION(tag,type,rhs_elm,accessor,OFFSET,XLATE)	\
	case tag:							\
		{							\
		type* ptr = accessor(0);				\
		for ( i = 0, j = 0; i < size; ++i, j += incr )		\
			{						\
			XLATE						\
			result[i] = (ptr[ OFFSET ] rhs_elm ? glish_true : glish_false);\
			}						\
		break;							\
		}

BOOL_COERCE_BOOL_ACTION(j,)
BOOL_COERCE_ACTION(TYPE_BYTE,byte,,BytePtr,j,)
BOOL_COERCE_ACTION(TYPE_SHORT,short,,ShortPtr,j,)
BOOL_COERCE_ACTION(TYPE_INT,int,,IntPtr,j,)
BOOL_COERCE_ACTION(TYPE_FLOAT,float,,FloatPtr,j,)
BOOL_COERCE_ACTION(TYPE_DOUBLE,double,,DoublePtr,j,)
BOOL_COERCE_ACTION(TYPE_COMPLEX,complex,.r || ptr[j].i,ComplexPtr,j,)
BOOL_COERCE_ACTION(TYPE_DCOMPLEX,dcomplex,.r || ptr[j].i,DcomplexPtr,j,)

		case TYPE_SUBVEC_REF:
			{
			VecRef *ref = VecRefPtr();
			switch ( ref->Type() )
				{

#define COERCE_ACTION_XLATE						\
	int err;							\
	int off = ref->TranslateIndex( j, &err );			\
	if ( err )							\
		{							\
		error->Report( "index (=",j,				\
			") is out of range. Sub-vector reference may be invalid" );\
		return 0;						\
		}

BOOL_COERCE_BOOL_ACTION(off,COERCE_ACTION_XLATE)
BOOL_COERCE_ACTION(TYPE_INT,int,,IntPtr,off,COERCE_ACTION_XLATE)
BOOL_COERCE_ACTION(TYPE_FLOAT,float,,FloatPtr,off,COERCE_ACTION_XLATE)
BOOL_COERCE_ACTION(TYPE_DOUBLE,double,,DoublePtr,off,COERCE_ACTION_XLATE)
BOOL_COERCE_ACTION(TYPE_COMPLEX,complex,.r || ptr[off].i,ComplexPtr,off,COERCE_ACTION_XLATE)
BOOL_COERCE_ACTION(TYPE_DCOMPLEX,dcomplex,.r || ptr[off].i,DcomplexPtr,off,COERCE_ACTION_XLATE)
		default:
			error->Report(
				"bad type in Value::CoerceToBoolArray()" );
			return 0;
		}
			}
			break;


		default:
			error->Report(
				"bad type in Value::CoerceToBoolArray()" );
			return 0;
		}

	return result;
	}

 
#define CAST_ALPHA(from,to) glish_##from##_to_##to
#define CAST_(from,to) to

#define COERCE_ACTION_ALPHA(tag,rhs_type,rhs_elm,lhs_type,accessor,OFFSET,XLATE)\
	case tag:							\
		{							\
		rhs_type* rhs_ptr = accessor(0);			\
		glish_ary_##rhs_type##_to_##lhs_type( result, rhs_ptr, size, incr ); \
		break;							\
		}

#define COERCE_ACTION_(tag,rhs_type,rhs_elm,lhs_type,accessor,OFFSET,XLATE)\
	case tag:							\
		{							\
		rhs_type* rhs_ptr = accessor(0);			\
		for ( i = 0, j = 0; i < size; ++i, j += incr )		\
			{						\
			XLATE						\
			result[i] =					\
			lhs_type(rhs_ptr[OFFSET] rhs_elm);		\
			}						\
		break;							\
		}

#define COERCE_ACTIONS(type,error_msg,MOD)			\
COERCE_ACTION_(TYPE_BOOL,glish_bool,,type,BoolPtr,j,)		\
COERCE_ACTION_(TYPE_BYTE,byte,,type,BytePtr,j,)			\
COERCE_ACTION_(TYPE_SHORT,short,,type,ShortPtr,j,)		\
COERCE_ACTION_(TYPE_INT,int,,type,IntPtr,j,)			\
COERCE_ACTION##MOD(TYPE_FLOAT,float,,type,FloatPtr,j,)		\
COERCE_ACTION##MOD(TYPE_DOUBLE,double,,type,DoublePtr,j,)	\
COERCE_ACTION_(TYPE_COMPLEX,complex,.r,type,ComplexPtr,j,)	\
COERCE_ACTION_(TYPE_DCOMPLEX,dcomplex,.r,type,DcomplexPtr,j,)\
								\
		case TYPE_SUBVEC_REF:				\
			{					\
			VecRef *ref = VecRefPtr();		\
			switch ( ref->Type() )			\
				{				\
								\
COERCE_ACTION_(TYPE_BOOL,glish_bool,,type,BoolPtr,off,COERCE_ACTION_XLATE)\
COERCE_ACTION_(TYPE_BYTE,byte,,type,BytePtr,off,COERCE_ACTION_XLATE)\
COERCE_ACTION_(TYPE_SHORT,short,,type,ShortPtr,off,COERCE_ACTION_XLATE)\
COERCE_ACTION_(TYPE_INT,int,,type,IntPtr,off,COERCE_ACTION_XLATE)	\
COERCE_ACTION_(TYPE_FLOAT,float,,CAST##MOD(float,type),FloatPtr,off,COERCE_ACTION_XLATE)\
COERCE_ACTION_(TYPE_DOUBLE,double,,CAST##MOD(double,type),DoublePtr,off,COERCE_ACTION_XLATE)\
COERCE_ACTION_(TYPE_COMPLEX,complex,.r,type,ComplexPtr,off,COERCE_ACTION_XLATE)\
COERCE_ACTION_(TYPE_DCOMPLEX,dcomplex,.r,type,DcomplexPtr,off,COERCE_ACTION_XLATE)\
									\
				default:				\
					error->Report(			\
					"bad type in Value::",error_msg);\
					return 0;			\
				}					\
			}						\
			break;


byte* Value::CoerceToByteArray( int& is_copy, int size, byte* result ) const
	{
	COERCE_HDR(CoerceToByteArray, byte, TYPE_BYTE, "byte", BytePtr)

	switch ( Type() )
		{
#if defined(__alpha) || defined(__alpha__)
		COERCE_ACTIONS(byte,"CoerceToByteArray()",_ALPHA)
#else
		COERCE_ACTIONS(byte,"CoerceToByteArray()",_)
#endif
		default:
			error->Report(
				"bad type in Value::CoerceToByteArray()" );
			return 0;
		}

	return result;
	}

short* Value::CoerceToShortArray( int& is_copy, int size, short* result ) const
	{
	COERCE_HDR(CoerceToShortArray, short, TYPE_SHORT, "short", ShortPtr)

	switch ( Type() )
		{
#if defined(__alpha) || defined(__alpha__)
		COERCE_ACTIONS(short,"CoerceToShortArray()",_ALPHA)
#else
		COERCE_ACTIONS(short,"CoerceToShortArray()",_)
#endif

		default:
			error->Report(
				"bad type in Value::CoerceToShortArray()" );
			return 0;
		}

	return result;
	}

int* Value::CoerceToIntArray( int& is_copy, int size, int* result ) const
	{
	COERCE_HDR(CoerceToIntArray, int, TYPE_INT, "integer", IntPtr)

	switch ( Type() )
		{
#if defined(__alpha) || defined(__alpha__)
		COERCE_ACTIONS(int,"CoerceToIntArray()",_ALPHA)
#else
		COERCE_ACTIONS(int,"CoerceToIntArray()",_)
#endif
		default:
			error->Report(
				"bad type in Value::CoerceToIntArray()" );
			return 0;
		}

	return result;
	}


float* Value::CoerceToFloatArray( int& is_copy, int size, float* result ) const
	{
	COERCE_HDR(CoerceToFloatArray, float, TYPE_FLOAT, "float", FloatPtr)

	switch ( Type() )
		{
		COERCE_ACTIONS(float,"CoerceToFloatArray()",_)
		default:
			error->Report(
				"bad type in Value::CoerceToFloatArray()" );
			return 0;
		}

	return result;
	}


double* Value::CoerceToDoubleArray( int& is_copy, int size, double* result ) const
	{
	COERCE_HDR(CoerceToDoubleArray, double, TYPE_DOUBLE, "double", DoublePtr)

	switch ( Type() )
		{
		COERCE_ACTIONS(double,"CoerceToDoubleArray()",_)
		default:
			error->Report(
			    "bad type in Value::CoerceToDoubleArray()" );
			return 0;
		}

	return result;
	}


// Coercion builtin->complex.
#define COMPLEX_BIN_COERCE_ACTION(tag,rhs_type,lhs_type,accessor,OFFSET,XLATE)\
	case tag:							\
		{							\
		rhs_type* rhs_ptr = accessor(0);			\
		for ( i = 0, j = 0; i < size; ++i, j += incr )		\
			{						\
			XLATE						\
			result[i].r =					\
				lhs_type(rhs_ptr[OFFSET]);		\
			result[i].i = lhs_type(0);			\
			}						\
		break;							\
		}

// Coercion complex->complex.
#define COMPLEX_CPX_COERCE_ACTION(tag,rhs_type,lhs_type,accessor,OFFSET,XLATE)\
	case tag:							\
		{							\
		rhs_type* rhs_ptr = accessor(0);			\
		for ( i = 0, j = 0; i < size; ++i, j += incr )		\
			{						\
			XLATE						\
			result[i].r = lhs_type(rhs_ptr[OFFSET].r);	\
			result[i].i = lhs_type(rhs_ptr[OFFSET].i);	\
			}						\
		break;							\
		}

#define COERCE_COMPLEX_ACTIONS(type,error_msg)				\
COMPLEX_BIN_COERCE_ACTION(TYPE_BOOL,glish_bool,type,BoolPtr,j,)		\
COMPLEX_BIN_COERCE_ACTION(TYPE_BYTE,byte,type,BytePtr,j,)		\
COMPLEX_BIN_COERCE_ACTION(TYPE_SHORT,short,type,ShortPtr,j,)		\
COMPLEX_BIN_COERCE_ACTION(TYPE_INT,int,type,IntPtr,j,)			\
COMPLEX_BIN_COERCE_ACTION(TYPE_FLOAT,float,type,FloatPtr,j,)		\
COMPLEX_BIN_COERCE_ACTION(TYPE_DOUBLE,double,type,DoublePtr,j,)		\
COMPLEX_CPX_COERCE_ACTION(TYPE_COMPLEX,complex,type,ComplexPtr,j,)	\
COMPLEX_CPX_COERCE_ACTION(TYPE_DCOMPLEX,dcomplex,type,DcomplexPtr,j,)	\
									\
		case TYPE_SUBVEC_REF:					\
			{						\
			VecRef *ref = VecRefPtr();			\
			switch ( ref->Type() )				\
				{					\
									\
COMPLEX_BIN_COERCE_ACTION(TYPE_BOOL,glish_bool,type,BoolPtr,off,COERCE_ACTION_XLATE)\
COMPLEX_BIN_COERCE_ACTION(TYPE_BYTE,byte,type,BytePtr,off,COERCE_ACTION_XLATE)\
COMPLEX_BIN_COERCE_ACTION(TYPE_SHORT,short,type,ShortPtr,off,COERCE_ACTION_XLATE)\
COMPLEX_BIN_COERCE_ACTION(TYPE_INT,int,type,IntPtr,off,COERCE_ACTION_XLATE)\
COMPLEX_BIN_COERCE_ACTION(TYPE_FLOAT,float,type,FloatPtr,off,COERCE_ACTION_XLATE)\
COMPLEX_BIN_COERCE_ACTION(TYPE_DOUBLE,double,type,DoublePtr,off,COERCE_ACTION_XLATE)\
COMPLEX_CPX_COERCE_ACTION(TYPE_COMPLEX,complex,type,ComplexPtr,off,COERCE_ACTION_XLATE)\
COMPLEX_CPX_COERCE_ACTION(TYPE_DCOMPLEX,dcomplex,type,DcomplexPtr,off,COERCE_ACTION_XLATE)\
									\
				default:				\
					error->Report(			\
					"bad type in Value::",error_msg );\
					return 0;			\
				}					\
			}						\
			break;

complex* Value::CoerceToComplexArray( int& is_copy, int size,
				complex* result ) const
	{
	COERCE_HDR(CoerceToComplexArray, complex, TYPE_COMPLEX,
			"complex", ComplexPtr)

	switch ( Type() )
		{
		COERCE_COMPLEX_ACTIONS(float,"CoerceToComplexArray()")
		default:
			error->Report(
				"bad type in Value::CoerceToComplexArray()" );
			return 0;
		}

	return result;
	}


dcomplex* Value::CoerceToDcomplexArray( int& is_copy, int size,
				dcomplex* result ) const
	{
	COERCE_HDR(CoerceToDcomplexArray, dcomplex, TYPE_DCOMPLEX,
			"dcomplex", DcomplexPtr)

	switch ( Type() )
		{
		COERCE_COMPLEX_ACTIONS(double,"CoerceToDcomplexArray()")
		default:
			error->Report(
			    "bad type in Value::CoerceToDcomplexArray()" );
			return 0;
		}

	return result;
	}


charptr* Value::CoerceToStringArray( int& is_copy, int size, charptr* result ) const
	{
	if ( IsRef() )
		return Deref()->CoerceToStringArray(is_copy,size,result );

	if ( VecRefDeref()->Type() != TYPE_STRING )
		{
		// As with records (see Value::Polymprph()), we allow boolean
		// values of length 1 to be converted to strings; assuming that
		// they are uninitialized variables. This if-clause permits this
		// conversion.
		if ( size > 1 )
			warn->Report( "array values lost due to conversion to string type" );
		is_copy = 1;
		char **ary = (char**) alloc_memory(sizeof(char*)*size);
		for ( int x=0; x < size; ++x )
			{
			ary[x] = (char*) alloc_memory(1);
			ary[x][0] = '\0';
			}
		return (charptr*)ary;
		}

	if ( ! result && Length() == size && ! IsVecRef() )
		{
		is_copy = 0;
		return StringPtr(0);
		}

	is_copy = 1;
	if ( ! result )
		result = (charptr*) alloc_memory( sizeof(charptr)*size );

	int incr = (Length() == 1 ? 0 : 1);

	int i, j;
	charptr* string_ptr = StringPtr();
	if ( IsVecRef() )
		{
		VecRef* ref = VecRefPtr();
	for ( i = 0, j = 0; i < size; ++i, j += incr )
		    	{
			int err;
			int off  = ref->TranslateIndex( j, &err );
			if ( err )
				{
				error->Report( "index (=",j,
					       ") is out of range. Sub-vector reference may be invalid" );
				return 0;
				}
			result[i] = string_ptr[off];
			}
		}
	else
		{
		for ( i = 0, j = 0; i < size; ++i, j += incr )
			result[i] = string_ptr[j];
		}

	return result;
	}

Value* Value::RecordRef( const Value* index ) const
	{
	if ( Type() != TYPE_RECORD )
		return Fail( this, "is not a record" );

	if ( index->Type() != TYPE_STRING )
		return Fail( "non-string index in record reference:", index );

	if ( index->Length() == 1 )
		// Don't create a new record, just return the given element.
		return copy_value( ExistingRecordElement( index ) );

	recordptr new_record = create_record_dict();
	charptr* indices = index->StringPtr();
	int n = index->Length();

	for ( int i = 0; i < n; ++i )
		{
		char* key = strdup( indices[i] );
		new_record->Insert( key,
				copy_value( ExistingRecordElement( key ) ) );
		}

	return create_value( new_record );
	}


const Value* Value::ExistingRecordElement( const Value* index ) const
	{
	char* index_string = index->StringVal();
	const Value* result = ExistingRecordElement( index_string );
	free_memory( index_string );

	return result;
	}

const Value* Value::ExistingRecordElement( const char* field ) const
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		{
		warn->Report( "operand to .", field, " is not a record" );
		return false_value;
		}

	Value* member = (*RecordPtr(0))[field];

	if ( ! member )
		{
		warn->Report( ".", field, " is not a field in", this );
		return false_value;
		}
	else
		return member;
	}


Value* Value::GetOrCreateRecordElement( const Value* index )
	{
	char* index_string = index->StringVal();
	Value* result = GetOrCreateRecordElement( index_string );
	free_memory( index_string );

	return result;
	}

Value* Value::GetOrCreateRecordElement( const char* field )
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		{
		warn->Report( "operand to .", field, " is not a record" );
		return error_value();
		}

	Value* member = (*RecordPtr())[field];

	if ( ! member )
		{
		member = create_value( glish_false );
		RecordPtr()->Insert( strdup( field ), member );
		}

	return member;
	}

const Value* Value::HasRecordElement( const char* field ) const
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		fatal->Report( "non-record in Value::HasRecordElement" );

	return (*RecordPtr(0))[field];
	}


Value* Value::Field( const Value* index )
	{
	char* index_string = index->StringVal();
	Value* result = Field( index_string );
	free_memory( index_string );

	return result;
	}

Value* Value::Field( const char* field )
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		return 0;

	Value* member = (*RecordPtr())[field];

	if ( ! member )
		return 0;

	return member;
	}

Value* Value::Field( const char* field, glish_type t )
	{
	Value* result = Field( field );

	if ( result )
		result->Polymorph( t );

	return result;
	}

Value* Value::NthField( int n )
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		return 0;

	Value* member = RecordPtr()->NthEntry( n - 1 );

	if ( ! member )
		return 0;

	return member;
	}

const Value* Value::NthField( int n ) const
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		return 0;

	Value* member = RecordPtr(0)->NthEntry( n - 1 );

	if ( ! member )
		return 0;

	return member;
	}

const char* Value::NthFieldName( int n ) const
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		return 0;

	const char* key;
	Value* member = RecordPtr(0)->NthEntry( n - 1, key );

	if ( ! member )
		return 0;

	return key;
	}

char* Value::NewFieldName( int alloc )
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		return 0;

	static int counter = 0;
	static char buf[128];

	do
		sprintf( buf, "*%d", ++counter );
	while ( Field( buf ) );

	return alloc ? strdup( buf ) : buf;
	}


#define DEFINE_FIELD_VAL(tag,type,valfunc)				\
int Value::FieldVal( const char* field, type& val, int num, Str &err )	\
	{								\
	Value* result = Field( field, tag );				\
	if ( ! result )							\
		{							\
		err = strFail( "field (", field, ") not found" );	\
		return 0;						\
		}							\
	val = result->valfunc( num, err );				\
	return 1;							\
	}

DEFINE_FIELD_VAL(TYPE_BOOL, glish_bool, BoolVal)
DEFINE_FIELD_VAL(TYPE_BYTE, byte, ByteVal)
DEFINE_FIELD_VAL(TYPE_SHORT, short, ShortVal)
DEFINE_FIELD_VAL(TYPE_INT, int, IntVal)
DEFINE_FIELD_VAL(TYPE_FLOAT, float, FloatVal)
DEFINE_FIELD_VAL(TYPE_DOUBLE, double, DoubleVal)
DEFINE_FIELD_VAL(TYPE_COMPLEX, complex, ComplexVal)
DEFINE_FIELD_VAL(TYPE_DCOMPLEX, dcomplex, DcomplexVal)

int Value::FieldVal( const char* field, char*& val )
	{
	Value* result = Field( field, TYPE_STRING );
	if ( ! result )
		return 0;

	val = result->StringVal();
	return 1;
	}


#define DEFINE_FIELD_PTR(name,tag,type,accessor)			\
type Value::name( const char* field, int& len, int modify )		\
	{								\
	Value* result = Field( field, tag );				\
	if ( ! result )							\
		return 0;						\
									\
	len = result->Length();						\
	return modify ? result->accessor() : result->accessor(0);	\
	}

DEFINE_FIELD_PTR(FieldBoolPtr,TYPE_BOOL,glish_bool*,BoolPtr)
DEFINE_FIELD_PTR(FieldBytePtr,TYPE_BYTE,byte*,BytePtr)
DEFINE_FIELD_PTR(FieldShortPtr,TYPE_SHORT,short*,ShortPtr)
DEFINE_FIELD_PTR(FieldIntPtr,TYPE_INT,int*,IntPtr)
DEFINE_FIELD_PTR(FieldFloatPtr,TYPE_FLOAT,float*,FloatPtr)
DEFINE_FIELD_PTR(FieldDoublePtr,TYPE_DOUBLE,double*,DoublePtr)
DEFINE_FIELD_PTR(FieldComplexPtr,TYPE_COMPLEX,complex*,ComplexPtr)
DEFINE_FIELD_PTR(FieldDcomplexPtr,TYPE_DCOMPLEX,dcomplex*,DcomplexPtr)
DEFINE_FIELD_PTR(FieldStringPtr,TYPE_STRING,charptr*,StringPtr)


#define DEFINE_SET_FIELD_SCALAR(type)					\
void Value::SetField( const char* field, type val )			\
	{								\
	Value* field_elem = create_value( val );			\
	AssignRecordElement( field, field_elem );			\
	Unref( field_elem );						\
	}

#define DEFINE_SET_FIELD_ARRAY(type)					\
void Value::SetField( const char* field, type val[], int num_elements,	\
			array_storage_type arg_storage )		\
	{								\
	Value* field_elem = create_value( val, num_elements, arg_storage );\
	AssignRecordElement( field, field_elem );			\
	Unref( field_elem );						\
	}

#define DEFINE_SET_FIELD(type)						\
	DEFINE_SET_FIELD_SCALAR(type)					\
	DEFINE_SET_FIELD_ARRAY(type)

DEFINE_SET_FIELD(glish_bool)
DEFINE_SET_FIELD(byte)
DEFINE_SET_FIELD(short)
DEFINE_SET_FIELD(int)
DEFINE_SET_FIELD(float)
DEFINE_SET_FIELD(double)
DEFINE_SET_FIELD(complex)
DEFINE_SET_FIELD(dcomplex)
DEFINE_SET_FIELD(const char*)


int* Value::GenerateIndices( const Value* index, int& num_indices,
				int& indices_are_copy, int check_size ) const
	{
	if ( ! index->IsNumeric() )
		{
		error->Report( "non-numeric array index:", index );
		return 0;
		}

	num_indices = index->Length();

	int* indices;

	if ( index->Type() == TYPE_BOOL )
		{
		int index_len = num_indices;
		if ( check_size && index_len != kernel.Length() )
			{
			error->Report( "boolean array index has", index_len,
					"elements, array has", kernel.Length() );
			return 0;
			}

		// First figure out how many elements we're going to be copying.
		glish_bool* vals = index->BoolPtr(0);
		num_indices = 0;

		for ( int i = 0; i < index_len; ++i )
			if ( vals[i] )
				++num_indices;

		indices = (int*) alloc_memory( sizeof(int)*num_indices );
		indices_are_copy = 1;

		num_indices = 0;
		for ( LOOPDECL i = 0; i < index_len; ++i )
			if ( vals[i] )
				indices[num_indices++] = i + 1;
		}

	else
		indices = index->CoerceToIntArray( indices_are_copy,
							num_indices );

	return indices;
	}


Value* Value::RecordSlice( int* indices, int num_indices ) const
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		fatal->Report( "non-record type in Value::RecordSlice()" );

	int max_index = 0;
	if ( ! IndexRange( indices, num_indices, max_index ) )
		return error_value();

	recordptr rptr = RecordPtr(0);

	if ( max_index > rptr->Length() )
		return Fail( "record index (=", max_index,
			") out of range (> ", rptr->Length(), ")" );

	if ( num_indices == 1 )
		return copy_value( NthField( indices[0] ) );

	recordptr new_record = create_record_dict();

	for ( int i = 0; i < num_indices; ++i )
		{
		const char* key;
		Value* new_member = rptr->NthEntry( indices[i] - 1, key );

		if ( ! new_member )
			fatal->Report( "no member corresponding to key = \"",
					key, "\" in Value::RecordSlice" );

		new_record->Insert( strdup( key ), copy_value( new_member ) );
		}

	return create_value( new_record );
	}


void Value::AssignElements( const Value* index, Value* value )
	{
	if ( index->Type() == TYPE_STRING )
		AssignRecordElements( index, value );

	else
		{
		int indices_are_copy;
		int num_indices;
		int* indices = GenerateIndices( index, num_indices,
							indices_are_copy );

		if ( ! indices )
			return;

		if ( VecRefDeref()->Type() == TYPE_RECORD )
			AssignRecordSlice( value, indices, num_indices );

		else
			AssignArrayElements( value, indices, num_indices );

		if ( indices_are_copy )
			free_memory( indices );
		}

	Unref( value );
	}

void Value::AssignElements( Value* value )
	{
	if ( VecRefDeref()->Type() == TYPE_RECORD )
		AssignRecordElements( value );
	else
		AssignArrayElements( value );

	Unref( value );
	}

void Value::AssignElements( const_value_list* args_val, Value* value )
	{
	if ( VecRefDeref()->Type() == TYPE_RECORD )
		error->Report("bad type in Value::AssignElements,", __LINE__);
	else
		AssignArrayElements( args_val, value );

	Unref( value );
	}

void Value::AssignRecordElements( const Value* index, Value* value )
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		{
		error->Report( this, " is not a record" );
		return;
		}

	if ( index->Length() == 1 )
		{
		AssignRecordElement( index->StringPtr(0)[0], value );
		return;
		}

	if ( value->VecRefDeref()->Type() != TYPE_RECORD )
		{
		error->Report( "assignment of non-record type to subrecord" );
		return;
		}

	recordptr rhs_rptr = value->RecordPtr(0);
	charptr* indices = index->StringPtr(0);

	if ( index->Length() != rhs_rptr->Length() )
		{
		error->Report( "in record assignment: # record indices (",
				index->Length(),
				") differs from # right-hand elements (",
				rhs_rptr->Length(), ")" );
		return;
		}

	int n = rhs_rptr->Length();
	for ( int i = 0; i < n; ++i )
		AssignRecordElement( indices[i], rhs_rptr->NthEntry( i ) );
	}

void Value::AssignRecordElements( Value* value )
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		{
		error->Report( this, " is not a record" );
		return;
		}

	if ( value->VecRefDeref()->Type() != TYPE_RECORD )
		{
		error->Report( "assignment of non-record type to subrecord" );
		return;
		}

	recordptr rhs_rptr = value->Deref()->RecordPtr(0);
	const char* key;
	Value* val;

	int n = rhs_rptr->Length();
	for ( int i = 0; i < n; ++i )
		{
		val = rhs_rptr->NthEntry( i, key );
		AssignRecordElement( key, val );
		}
	}

void Value::AssignRecordElement( const char* index, Value* value )
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		fatal->Report( this, " is not a record" );

	recordptr rptr = RecordPtr();
	Value* member = (*rptr)[index];

	if ( member && member->IsConst() )
		{
		error->Report( "'const' values cannot be modified." );
		return;
		}

	if ( ! member && IsFieldConst() )
		{
		error->Report( "fields cannot be added to a 'const' record." );
		return;
		}

	Ref( value );	// So AssignElements() doesn't throw it away.

	if ( member )
		// We'll be replacing this member in the record dictionary.
		Unref( member );

	else
		// We'll be creating a new element in the dictionary.
		index = strdup( index );

	rptr->Insert( index, value );
	}


void Value::AssignRecordSlice( Value* value, int* indices, int num_indices )
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		fatal->Report(
			"non-record type in Value::AssignRecordSlice()" );

	recordptr rptr = RecordPtr();

	int max_index = 0;
	if ( ! IndexRange( indices, num_indices, max_index ) )
		return;

	if ( num_indices == 1 )
		{ // Just assigning to one field.
		int n = indices[0];

		if ( n <= rptr->Length() )
			AssignRecordElement( NthFieldName( n ), value );

		else if ( n > rptr->Length() + 1 )
			error->Report( "record index (=", n,
				") out of range (> ", rptr->Length() + 1, ")" );

		else
			// Create a new field.
			AssignRecordElement( NewFieldName(0), value );

		return;
		}

	// Assigning multiple elements.
	if ( value->VecRefDeref()->Type() != TYPE_RECORD )
		{
		error->Report(
			"non-record type assigned to record slice" );
		return;
		}

	recordptr rhs_rptr = value->RecordPtr(0);

	if ( rhs_rptr->Length() != num_indices )
		{
		error->Report( "length mismatch,", num_indices,
				" indices given but RHS has ",
				rhs_rptr->Length(), " elements" );
		return;
		}

	for ( int i = 0; i < num_indices; ++i )
		{
		int n = indices[i];
		Value* val = rhs_rptr->NthEntry( i );

		if ( n <= rptr->Length() )
			AssignRecordElement( NthFieldName( n ), val );

		else if ( n > rptr->Length() + 1 )
			error->Report( "record index (=", n,
				") out of range (> ", rptr->Length() + 1, ")" );

		else
			AssignRecordElement( NewFieldName(0), val );
		}
	}

void Value::AssignArrayElements( Value* value, int* indices, int num_indices )
	{
	if ( IsRef() )
		{
		Deref()->AssignArrayElements( value, indices, num_indices );
		return;
		}

	glish_type max_type;
	if ( ! compatible_types( this, value, max_type ) )
		return;

	Polymorph( max_type );
	value->Polymorph( max_type );

	int rhs_len = value->Length();

	if ( rhs_len == 1 )
		// Scalar
		rhs_len = num_indices;

	if ( rhs_len != num_indices )
		{
		error->Report( "in array assignment: # indices (",
				num_indices, ") doesn't match # values (",
				rhs_len, ")" );
		}
	else
		AssignArrayElements( indices, num_indices, value, rhs_len );
	}

void Value::AssignArrayElements( int* indices, int num_indices, Value* value,
				int rhs_len )
	{
	if ( IsRef() )
		{
		Deref()->AssignArrayElements(indices,num_indices,value,rhs_len);
		return;
		}

	int max_index, min_index;
	if ( ! IndexRange( indices, num_indices, max_index, min_index ) )
		return;

	int orig_len = Length();
	if ( max_index > Length() )
		if ( ! Grow( (unsigned int) max_index ) )
			return;

	if ( Type() == TYPE_STRING && min_index > orig_len )
		{
		char **ary = (char**) StringPtr();
		for ( int x=orig_len; x < min_index; ++x )
			{
			ary[x] = (char*) alloc_memory(1);
			ary[x][0] = '\0';
			}
		}

	switch ( Type() )
		{
#define ASSIGN_ARRAY_ELEMENTS_ACTION(tag,lhs_type,rhs_type,accessor,coerce_func,copy_func,delete_old_value)		\
	case tag:							\
		{							\
		int rhs_copy;						\
		rhs_type rhs_array = value->coerce_func( rhs_copy,	\
							rhs_len );	\
		lhs_type lhs = accessor();				\
		for ( int i = 0; i < num_indices; ++i )			\
			{						\
			delete_old_value				\
			lhs[indices[i]-1] = copy_func(rhs_array[i]);	\
			}						\
									\
		if ( rhs_copy )						\
			free_memory( rhs_array );			\
									\
		break;							\
		}

ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_BOOL,glish_bool*,glish_bool*,BoolPtr,
	CoerceToBoolArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_BYTE,byte*,byte*,BytePtr,
	CoerceToByteArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_SHORT,short*,short*,ShortPtr,
	CoerceToShortArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_INT,int*,int*,IntPtr,
	CoerceToIntArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_FLOAT,float*,float*,FloatPtr,
	CoerceToFloatArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_DOUBLE,double*,double*,DoublePtr,
	CoerceToDoubleArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_COMPLEX,complex*,complex*,ComplexPtr,
	CoerceToComplexArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_DCOMPLEX,dcomplex*,dcomplex*,DcomplexPtr,
	CoerceToDcomplexArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_STRING,charptr*,charptr*,StringPtr,
	CoerceToStringArray, strdup, free_memory( (void*) lhs[indices[i]-1] );)

		case TYPE_SUBVEC_REF:
			switch ( VecRefPtr()->Type() )
				{
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_BOOL,glish_boolref&,glish_bool*,BoolRef,
	CoerceToBoolArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_BYTE,byteref&,byte*,ByteRef,
	CoerceToByteArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_SHORT,shortref&,short*,ShortRef,
	CoerceToShortArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_INT,intref&,int*,IntRef,
	CoerceToIntArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_FLOAT,floatref&,float*,FloatRef,
	CoerceToFloatArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_DOUBLE,doubleref&,double*,DoubleRef,
	CoerceToDoubleArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_COMPLEX,complexref&,complex*,ComplexRef,
	CoerceToComplexArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_DCOMPLEX,dcomplexref&,dcomplex*,DcomplexRef,
	CoerceToDcomplexArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_STRING,charptrref&,charptr*,StringRef,
	CoerceToStringArray, strdup, free_memory( (void*) lhs[indices[i]-1] );)

				default:
					fatal->Report(
			"bad subvec type in Value::AssignArrayElements()" );
				}
			break;

		default:
			fatal->Report(
				"bad type in Value::AssignArrayElements()" );
		}
	}

void Value::AssignArrayElements( Value* value )
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
		type_rhs rhs_array = value->Deref()->coerce_func(	\
						rhs_copy, max_index );	\
		type_lhs lhs = accessor();				\
		for ( int i = 0; i < max_index; ++i )			\
			{						\
			delete_old_value				\
			lhs[i] = copy_func( rhs_array[i] );		\
			}						\
									\
		if ( rhs_copy )						\
			free_memory( rhs_array );			\
									\
		break;							\
		}

ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_BOOL,glish_bool*,glish_bool*,BoolPtr,
	CoerceToBoolArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_BYTE,byte*,byte*,BytePtr,
	CoerceToByteArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_SHORT,short*,short*,ShortPtr,
	CoerceToShortArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_INT,int*,int*,IntPtr,
	CoerceToIntArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_FLOAT,float*,float*,FloatPtr,
	CoerceToFloatArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_DOUBLE,double*,double*,DoublePtr,
	CoerceToDoubleArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_COMPLEX,complex*,complex*,
	ComplexPtr,CoerceToComplexArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_DCOMPLEX,dcomplex*,dcomplex*,
	DcomplexPtr,CoerceToDcomplexArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_STRING,charptr*,charptr*,StringPtr,
	CoerceToStringArray,strdup, free_memory( (void*) lhs[i] );)

		case TYPE_SUBVEC_REF:
			switch ( VecRefPtr()->Type() )
				{
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_BOOL,glish_boolref&,glish_bool*,
	BoolRef, CoerceToBoolArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_BYTE,byteref&,byte*,ByteRef,
	CoerceToByteArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_SHORT,shortref&,short*,ShortRef,
	CoerceToShortArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_INT,intref&,int*,IntRef,
	CoerceToIntArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_FLOAT,floatref&,float*,FloatRef,
	CoerceToFloatArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_DOUBLE,doubleref&,double*,DoubleRef,
	CoerceToDoubleArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_COMPLEX,complexref&,complex*,
	ComplexRef,CoerceToComplexArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_DCOMPLEX,dcomplexref&,dcomplex*,
	DcomplexRef, CoerceToDcomplexArray,,)
ASSIGN_ARRAY_VALUE_ELEMENTS_ACTION(TYPE_STRING,charptrref&,charptr*,StringRef,
	CoerceToStringArray, strdup, free_memory( (void*) lhs[i] );)

				default:
					fatal->Report(
		"bad sub-array reference in Value::AssignArrayElements()" );
				}
			break;
		default:
			fatal->Report(
				"bad type in Value::AssignArrayElements()" );
		}
	}


#define SUBOP_CLEANUP_1							\
	if ( shape_is_copy )						\
		free_memory( shape );					\
	free_memory( factor );

#define SUBOP_CLEANUP_2(length)						\
	{								\
	SUBOP_CLEANUP_1							\
	for ( int x = 0; x < length; ++x )				\
		if ( index_is_copy[x] )					\
			free_memory( index[x] );			\
									\
	free_memory( index );						\
	free_memory( index_is_copy );					\
	free_memory( cur );						\
	}

#define SUBOP_CLEANUP(length)						\
	SUBOP_CLEANUP_2(length)						\
	free_memory( len );

void Value::AssignArrayElements( const_value_list* args_val, Value* value )
	{
	if ( ! IsNumeric() && VecRefDeref()->Type() != TYPE_STRING )
		{
		error->Report( "invalid type in n-D assignment:", this );
		return;
		}

	// Collect attributes.
	int args_len = (*args_val).length();
	const attributeptr ptr = AttributePtr();
	const Value* shape_val = ptr ? (*ptr)["shape"] : 0;
	if ( ! shape_val || ! shape_val->IsNumeric() )
		{
		warn->Report( "invalid or non-existant \"shape\" attribute" );

		Ref(value);		// Our caller && AssignElements() will unref

		if ( args_len >= 1 )
			AssignElements( (*args_val)[0], value );
		else
			AssignElements( value );
		return;
		}

	int shape_len = shape_val->Length();
	if ( shape_len != args_len )
		{
		error->Report( "invalid number of indexes for:", this );
		return;
		}

	int shape_is_copy;
	int* shape = shape_val->CoerceToIntArray( shape_is_copy, shape_len );

	int* factor = (int*) alloc_memory( sizeof(int)*shape_len );
	int cur_factor = 1;
	int offset = 0;
	int max_len = 0;
	for ( int i = 0; i < args_len; ++i )
		{
		const Value* arg = (*args_val)[i];

		if ( arg )
			{
			if ( ! arg->IsNumeric() )
				{
				error->Report( "index #", i+1, "into", this,
						"is not numeric");

				SUBOP_CLEANUP_1
				return;
				}

			if ( arg->Length() > max_len )
				max_len = arg->Length();

			if ( max_len == 1 )
				{
				int ind = arg->IntVal();
				if ( ind < 1 || ind > shape[i] )
					{
					error->Report( "index #", i+1, "into",
						this, "is out of range");
					SUBOP_CLEANUP_1
					return;
					}

				offset += cur_factor * (ind - 1);
				}
			}

		else
			{ // Missing subscript.
			if ( shape[i] > max_len )
				max_len = shape[i];

			if ( max_len == 1 )
				offset += cur_factor * (shape[i] - 1);
			}

		factor[i] = cur_factor;
		cur_factor *= shape[i];
		}

	// Check to see if we're valid.
	if ( cur_factor > Length() )
		{
		error->Report( "\"::shape\"/length mismatch" );
		SUBOP_CLEANUP_1
		return;
		}
	
	glish_type max_type;
	if ( ! compatible_types( this, value, max_type ) )
		{
		error->Report( "non-compatible types for assignment" );
		SUBOP_CLEANUP_1
		return;
		}

	Polymorph( max_type );
	value->Polymorph( max_type );

	if ( max_len == 1 ) 
		{
		SUBOP_CLEANUP_1
		switch ( Type() )
			{
#define ASSIGN_ARY_ELEMENTS_ACTION_A(tag,type,to_accessor,from_accessor,OFFSET,XLATE)\
	case tag:							\
		{							\
		type* ret = to_accessor();				\
		XLATE							\
		ret[ OFFSET ] = value->from_accessor();			\
		}							\
		break;

	ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_BOOL,glish_bool,BoolPtr,BoolVal,offset,)
	ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_BYTE,byte,BytePtr,ByteVal,offset,)
	ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_SHORT,short,ShortPtr,ShortVal,offset,)
	ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_INT,int,IntPtr,IntVal,offset,)
	ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_FLOAT,float,FloatPtr,FloatVal,offset,)
	ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_DOUBLE,double,DoublePtr,DoubleVal,offset,)
	ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_COMPLEX,complex,ComplexPtr,ComplexVal,offset,)
	ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,DcomplexVal,offset,)
	ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_STRING,charptr,StringPtr,StringVal,offset,)

			case TYPE_SUBVEC_REF:
				{
				VecRef* ref = VecRefPtr();
				switch ( ref->Val()->Type() )
					{


#define ASSIGN_ARY_ELEMENTS_ACTION_A_XLATE				\
	int err;							\
	int off = ref->TranslateIndex( offset, &err );			\
	if ( err )							\
		{							\
		error->Report("index ",offset,				\
			" out of range. Sub-vector reference may be invalid" );\
		return;							\
		}

ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_BOOL, glish_bool, BoolPtr, BoolVal,
	off,ASSIGN_ARY_ELEMENTS_ACTION_A_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_BYTE, byte, BytePtr, ByteVal,
	off,ASSIGN_ARY_ELEMENTS_ACTION_A_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_SHORT, short, ShortPtr, ShortVal,
	off,ASSIGN_ARY_ELEMENTS_ACTION_A_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_INT, int, IntPtr, IntVal,
	off,ASSIGN_ARY_ELEMENTS_ACTION_A_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_FLOAT, float, FloatPtr, FloatVal,
	off,ASSIGN_ARY_ELEMENTS_ACTION_A_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_DOUBLE, double, DoublePtr, DoubleVal,
	off,ASSIGN_ARY_ELEMENTS_ACTION_A_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_COMPLEX, complex, ComplexPtr, ComplexVal,
	off,ASSIGN_ARY_ELEMENTS_ACTION_A_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_DCOMPLEX, dcomplex, DcomplexPtr, DcomplexVal,
	off,ASSIGN_ARY_ELEMENTS_ACTION_A_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION_A(TYPE_STRING, charptr, StringPtr, StringVal,
	off,ASSIGN_ARY_ELEMENTS_ACTION_A_XLATE)

				default:
					fatal->Report(
				"bad subvec type in Value::AssignArrayElements" );
					}
				}
				break;

			default:			
				fatal->Report(
					"bad type in Value::AssignArrayElements" );
			}
		return;
		}

	int* index_is_copy = (int*) alloc_memory( sizeof(int)*shape_len );
	int** index = (int**) alloc_memory( sizeof(int*)*shape_len );
	int* cur = (int*) alloc_memory( sizeof(int)*shape_len );
	int* len = (int*) alloc_memory( sizeof(int)*shape_len );
	int vecsize = 1;
	int is_element = 1;
	int spoof_dimension = 0;
	for ( LOOPDECL i = 0; i < args_len; ++i )
		{
		const Value* arg = (*args_val)[i];
		if ( arg )
			{
			index[i] = GenerateIndices( arg, len[i],
						index_is_copy[i], 0 );
			spoof_dimension = 0;
			}

		else
			{ // Spoof entire dimension.
			len[i] = shape[i];
			index[i] = (int*) alloc_memory( sizeof(int)*len[i] );
			for ( int j = 0; j < len[i]; j++ )
				index[i][j] = j+1;
			index_is_copy[i] = 1;
			spoof_dimension = 1;
			}

		if ( is_element && len[i] > 1 )
			is_element = 0;

		vecsize *= len[i];
		cur[i] = 0;

		if ( ! spoof_dimension )
			{
			for ( int j = 0; j < len[i]; ++j )
				{
				if ( index[i][j] >= 1 &&
				     index[i][j] <= shape[i] )
					continue;

				SUBOP_CLEANUP(i)
				if ( len[i] > 1 )
					error->Report( "index #", i+1, ",",
							j+1, " into ", this, 
							"is out of range.");
				else
					error->Report( "index #", i+1, "into",
						this, "is out of range.");
				}
			}
		}

	// Loop through filling resultant vector.

	switch ( Type() )
		{
#define ASSIGN_ARY_ELEMENTS_ACTION(tag,type,to_accessor,from_accessor,OFFSET,copy_func,XLATE)	\
	case tag:							\
		{							\
		int is_copy;						\
		type* vec = value->from_accessor( is_copy, vecsize );	\
		type* ret = to_accessor();				\
									\
		for ( int v = 0; v < vecsize; ++v )			\
			{						\
			/**** Calculate offset ****/			\
			offset = 0;					\
			for ( LOOPDECL i = 0; i < shape_len; ++i )	\
				offset += factor[i] *			\
						(index[i][cur[i]]-1);	\
			/**** Set Value ****/				\
			XLATE						\
			ret[ OFFSET ] = copy_func( vec[v] );		\
			/****  Advance counters ****/			\
			for ( LOOPDECL i = 0; i < shape_len; ++i )	\
				if ( ++cur[i] < len[i] )		\
					break;				\
				else					\
					cur[i] = 0;			\
			}						\
									\
		if ( is_copy )						\
			free_memory( vec );				\
									\
		free_memory( len );					\
		}							\
		break;

ASSIGN_ARY_ELEMENTS_ACTION(TYPE_BOOL,glish_bool,BoolPtr,CoerceToBoolArray,offset,,)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_BYTE,byte,BytePtr,CoerceToByteArray,offset,,)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_SHORT,short,ShortPtr,CoerceToShortArray,offset,,)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_INT,int,IntPtr,CoerceToIntArray,offset,,)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_FLOAT,float,FloatPtr,CoerceToFloatArray,offset,,)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_DOUBLE,double,DoublePtr,CoerceToDoubleArray,offset,,)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_COMPLEX,complex,ComplexPtr,CoerceToComplexArray,offset,,)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,CoerceToDcomplexArray,offset,,)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_STRING,charptr,StringPtr,CoerceToStringArray,offset,strdup,)

		case TYPE_SUBVEC_REF:
			{
			VecRef* ref = VecRefPtr();
			Value* theVal = ref->Val();

			switch ( theVal->Type() )
				{

#define ASSIGN_ARY_ELEMENTS_ACTION_XLATE				\
	int err;							\
	int off = ref->TranslateIndex( offset, &err );			\
	if ( err )							\
		{							\
		if ( is_copy )						\
			free_memory( vec );				\
		free_memory( len );					\
		SUBOP_CLEANUP_2(shape_len)				\
		error->Report("invalid index (=",offset+1,"), sub-vector reference may be bad");\
		return;							\
		}

ASSIGN_ARY_ELEMENTS_ACTION(TYPE_BOOL, glish_bool, BoolPtr, CoerceToBoolArray,off,,
	ASSIGN_ARY_ELEMENTS_ACTION_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_BYTE, byte, BytePtr, CoerceToByteArray,off,,
	ASSIGN_ARY_ELEMENTS_ACTION_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_SHORT, short, ShortPtr, CoerceToShortArray,off,,
	ASSIGN_ARY_ELEMENTS_ACTION_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_INT, int, IntPtr, CoerceToIntArray,off,,
	ASSIGN_ARY_ELEMENTS_ACTION_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_FLOAT, float, FloatPtr, CoerceToFloatArray,off,,
	ASSIGN_ARY_ELEMENTS_ACTION_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_DOUBLE, double, DoublePtr, CoerceToDoubleArray,off,,
	ASSIGN_ARY_ELEMENTS_ACTION_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_COMPLEX, complex, ComplexPtr, CoerceToComplexArray,off,,
	ASSIGN_ARY_ELEMENTS_ACTION_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_DCOMPLEX, dcomplex, DcomplexPtr,
	CoerceToDcomplexArray, off,, ASSIGN_ARY_ELEMENTS_ACTION_XLATE)
ASSIGN_ARY_ELEMENTS_ACTION(TYPE_STRING, charptr, StringPtr,
	CoerceToStringArray, off, strdup, ASSIGN_ARY_ELEMENTS_ACTION_XLATE)

				default:
					fatal->Report(
				"bad subref type in Value::AssignArrayElements" );
				}
			}
			break;

		default:
			fatal->Report( "bad type in Value::AssignArrayElements" );
		}

	SUBOP_CLEANUP_2(shape_len)
	return;
	}

int Value::IndexRange( int* indices, int num_indices, int& max_index, int& min_index ) const
	{
	max_index = 0;
	min_index = num_indices > 0 ? indices[num_indices-1] : 0;

	for ( int i = 0; i < num_indices; ++i )
		{
		if ( indices[i] < 1 )
			{
			error->Report( "index (=", indices[i],
				") out of range (< 1)" );
			return 0;
			}

		else
			{
			if ( indices[i] > max_index )
				max_index = indices[i];
			if ( indices[0] < min_index )
				min_index = indices[i];
			}
		}

	return 1;
	}

void Value::Negate()
	{
	if ( ! IsNumeric() )
		{
		error->Report( "negation of non-numeric value:", this );
		return;
		}

	int length = kernel.Length();

	switch ( Type() )
		{
#define NEGATE_ACTION(tag,type,accessor,func)				\
	case tag:							\
		{							\
		type* ptr = accessor();					\
		for ( int i = 0; i < length; ++i )			\
			ptr[i] = func(ptr[i]);				\
		break;							\
		}

#define COMPLEX_NEGATE_ACTION(tag,type,accessor,func)			\
	case tag:							\
		{							\
		type* ptr = accessor();					\
		for ( int i = 0; i < length; ++i )			\
			{						\
			ptr[i].r = func(ptr[i].r);			\
			ptr[i].i = func(ptr[i].i);			\
			}						\
		break;							\
		}

NEGATE_ACTION(TYPE_BOOL,glish_bool,BoolPtr,(glish_bool) ! (int))
NEGATE_ACTION(TYPE_BYTE,byte,BytePtr,-)
NEGATE_ACTION(TYPE_SHORT,short,ShortPtr,-)
NEGATE_ACTION(TYPE_INT,int,IntPtr,-)
NEGATE_ACTION(TYPE_FLOAT,float,FloatPtr,-)
NEGATE_ACTION(TYPE_DOUBLE,double,DoublePtr,-)
COMPLEX_NEGATE_ACTION(TYPE_COMPLEX,complex,ComplexPtr,-)
COMPLEX_NEGATE_ACTION(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,-)

		default:
			fatal->Report( "bad type in Value::Negate()" );
		}
	}

void Value::Not()
	{
	if ( Type() == TYPE_FAIL )
		{
		glish_bool *ary = (glish_bool*) alloc_memory( sizeof(glish_bool) );
		ary[0] = glish_true;
		kernel.SetArray( ary, 1 );
		return;
		}

	if ( ! IsNumeric() )
		{
		error->Report( "logical negation of non-numeric value:", this );
		return;
		}

	int length = kernel.Length();

	if ( Type() == TYPE_BOOL )
		{
		glish_bool* ptr = BoolPtr();
		for ( int i = 0; i < length; ++i )
			ptr[i] = glish_bool( ! int( ptr[i] ) );
		return;
		}

	glish_bool* result = (glish_bool*) alloc_memory( sizeof(glish_bool)*length );

	switch ( Type() )
		{
#define NOT_ACTION(tag,type,rhs_elm,accessor,OFFSET,XLATE)		\
	case tag:							\
		{							\
		type* ptr = accessor(0);				\
		for ( int i = 0; i < length; ++i )			\
			{						\
			XLATE						\
			result[i] = (ptr[ OFFSET ] rhs_elm ? glish_false : glish_true);\
			}						\
		break;							\
		}
NOT_ACTION(TYPE_BYTE,byte,,BytePtr,i,)
NOT_ACTION(TYPE_SHORT,short,,ShortPtr,i,)
NOT_ACTION(TYPE_INT,int,,IntPtr,i,)
NOT_ACTION(TYPE_FLOAT,float,,FloatPtr,i,)
NOT_ACTION(TYPE_DOUBLE,double,,DoublePtr,i,)
NOT_ACTION(TYPE_COMPLEX,complex,.r || ptr[i].i,ComplexPtr,i,)
NOT_ACTION(TYPE_DCOMPLEX,dcomplex,.r || ptr[i].i,DcomplexPtr,i,)

		case TYPE_SUBVEC_REF:
			{
			VecRef *ref = VecRefPtr();
			switch ( ref->Type() )
				{

#define NOT_ACTION_XLATE						\
	int err;							\
	int off = ref->TranslateIndex( i, &err );			\
	if ( err )							\
		{							\
		error->Report( "index (=",i,				\
			") is out of range. Sub-vector reference may be invalid" );\
		free_memory( result );					\
		return;							\
		}

NOT_ACTION(TYPE_INT,int,,IntPtr,off,NOT_ACTION_XLATE)
NOT_ACTION(TYPE_FLOAT,float,,FloatPtr,off,NOT_ACTION_XLATE)
NOT_ACTION(TYPE_DOUBLE,double,,DoublePtr,off,NOT_ACTION_XLATE)
NOT_ACTION(TYPE_COMPLEX,complex,.r || ptr[off].i,ComplexPtr,off,NOT_ACTION_XLATE)
NOT_ACTION(TYPE_DCOMPLEX,dcomplex,.r || ptr[off].i,DcomplexPtr,off,NOT_ACTION_XLATE)
				default:
					error->Report( "bad type in Value::Not()" );
					free_memory( result );
					return;
				}
			}
			break;


		default:
			error->Report( "bad type in Value::Not()" );
			free_memory( result );
			return;
		}

	kernel.SetArray( result, length );
	}

//
// If you change this function also change IValue::Polymorph
//
void Value::Polymorph( glish_type new_type )
	{
	glish_type type = Type();
	int length = kernel.Length();

	if ( type == new_type )
		return;

	if ( IsVecRef() )
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
		kernel.BoolToInt();
		return;
		}

	switch ( new_type )
		{
#define POLYMORPH_ACTION(tag,type,coerce_func)				\
	case tag:							\
		{							\
		int is_copy = 0;					\
		type* new_val = coerce_func( is_copy, length );		\
		if ( is_copy )						\
			kernel.SetArray( new_val, length );		\
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

		case TYPE_RECORD:
			if ( length > 1 )
				warn->Report(
			"array values lost due to conversion to record type" );

			kernel.SetRecord( create_record_dict() );

			break;

		default:
			fatal->Report( "bad type in Value::Polymorph()" );
		}
	}

void Value::VecRefPolymorph( glish_type new_type )
	{
	int length = kernel.Length();

	if ( IsVecRef() )
		{
		Polymorph( new_type );
		return;
		}

	switch ( new_type )
		{
#define VECREF_POLYMORPH_ACTION(tag,type,ref_type,ref_func,copy_func)	\
	case tag:							\
		{							\
		ref_type& old = ref_func();				\
		type* new_val = (type*) alloc_memory( sizeof(type)*length );\
		for ( int i = 0; i < length; ++i )			\
			new_val[i] = copy_func( old[i] );		\
		kernel.SetArray( new_val, length );			\
		break;							\
		}

VECREF_POLYMORPH_ACTION(TYPE_BOOL,glish_bool,glish_boolref,BoolRef,)
VECREF_POLYMORPH_ACTION(TYPE_BYTE,byte,byteref,ByteRef,)
VECREF_POLYMORPH_ACTION(TYPE_SHORT,short,shortref,ShortRef,)
VECREF_POLYMORPH_ACTION(TYPE_INT,int,intref,IntRef,)
VECREF_POLYMORPH_ACTION(TYPE_FLOAT,float,floatref,FloatRef,)
VECREF_POLYMORPH_ACTION(TYPE_DOUBLE,double,doubleref,DoubleRef,)
VECREF_POLYMORPH_ACTION(TYPE_COMPLEX,complex,complexref,ComplexRef,)
VECREF_POLYMORPH_ACTION(TYPE_DCOMPLEX,dcomplex,dcomplexref,DcomplexRef,)
VECREF_POLYMORPH_ACTION(TYPE_STRING,charptr,charptrref,StringRef,strdup)

		case TYPE_RECORD:
			if ( length > 1 )
				warn->Report(
			"array values lost due to conversion to record type" );

			kernel.SetRecord( create_record_dict() );
			break;

		default:
			fatal->Report( "bad type in Value::VecRefPolymorph()" );
		}
	}


Value* Value::AttributeRef( const Value* index ) const
	{
	return attributes ? attributes->RecordRef( index ) :
		create_value( glish_false );
	}

int Value::Grow( unsigned int new_size )
	{
	switch ( Type() )
		{
		case TYPE_BOOL:
		case TYPE_BYTE:
		case TYPE_SHORT:
		case TYPE_INT:
		case TYPE_FLOAT:
		case TYPE_DOUBLE:
		case TYPE_COMPLEX:
		case TYPE_DCOMPLEX:
		case TYPE_STRING:
			kernel.Grow( new_size );
			break;
		case TYPE_SUBVEC_REF:
		case TYPE_AGENT:
		case TYPE_FUNC:
		case TYPE_REGEX:
		case TYPE_FILE:
		case TYPE_RECORD:
			error->Report( "cannot increase array of",
					type_names[Type()], "via assignment" );
			return 0;

		default:
			fatal->Report( "bad type in Value::Grow()" );
		}

	return 1;
	}


char *Value::GetNSDesc( int ) const
	{
	glish_type type = Type();
	if ( type == TYPE_AGENT )
		return strdup( "<agent>" );
	if ( type == TYPE_FUNC )
		return strdup( "<function>" );
	if ( type == TYPE_REGEX )
		return strdup( "<regex>" );
	if ( type == TYPE_FILE )
		return strdup( "<file>" );
	return 0;
	}


int Value::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.prefix() ) s << opt.prefix();
	if ( IsRef() )
		{
		if ( IsConst() )
			s << "const ";
		else
			s << "ref ";

		RefPtr()->Describe( s, ioOpt(opt.flags(),opt.sep()) );
		}
	else
		{
		char* desc = StringVal( opt.sep(), PrintLimit() , 1 );
		s << desc;
		free_memory( desc );
		}
	return 1;
	}

#ifdef MEMFREE
unsigned int Value::CountRefs( recordptr r ) const
	{
	static value_list been_there;

	if ( been_there.is_member( (Value*)this ) )
		return 0;

// 	glish_type type = Type();
// 	if ( type == TYPE_REF )
// 		{
// 		const Value *v = Deref();
// 		if ( v->Type() == TYPE_RECORD &&
// 		     v->RecordPtr(0) == r )
// 			return 1;
// 		}
// 	else if ( type == TYPE_RECORD )
// 		{
// 		been_there.append( (Value*)this );
// 		unsigned int count = RecordPtr(0) == r ? 1 : count_references( RecordPtr(0), r );
// 		been_there.remove( (Value*)this );
// 		return count;
// 		}

	if ( Type() == TYPE_RECORD )
		{
		been_there.append( (Value*)this );
		unsigned int count = RecordPtr(0) == r ? 1 : count_references( RecordPtr(0), r );
		been_there.remove( (Value*)this );
		return count;
		}

	return 0;
	}

int Value::CountRefs( Value *val ) const
	{
	static value_list been_there;

	if ( been_there.is_member( (Value*)this ) )
		return 0;

	glish_type type = Type();
	if ( type == TYPE_REF )
		{
		const Value *v = Deref();
		if ( v == val )
			return 1;
		else if ( v->Type() == TYPE_RECORD )
			return v->CountRefs(val);
		else
			return 0;
		}

	else if ( type == TYPE_RECORD )
		{
		recordptr r = RecordPtr(0);
		IterCookie* c = r->InitForIteration();
		Value* member;
		const char* key;

		int count = 0;
		while ( (member = r->NextEntry( key, c )) )
			{
			glish_type member_type = member->Type();
			if ( member_type == TYPE_REF || member_type == TYPE_RECORD )
				count += member->CountRefs(val);
			}

		return count;
		}

	return 0;
	}
#endif


int Value::Sizeof( ) const
	{
	return kernel.Sizeof( ) + sizeof( GlishObject ) + 4 + (attributes ? attributes->Sizeof() : 0);
	}
		  

int Value::Bytes( int addPerValue ) const
	{
	return kernel.Bytes( addPerValue ) + 
		(attributes ? attributes->Bytes( addPerValue ) : 0);
	}

int Value::ToMemBlock(char *memory, int offset) const
	{
	if ( IsVecRef() )
		{
		Value* copy = copy_value( this );
		offset = copy->ToMemBlock( memory, offset );
		Unref(copy);
		return offset;
		}

	if ( IsRef() )
		{
		const Value *v = Deref();
		offset = v->kernel.ToMemBlock( memory, offset, attributes ? 1 : 0 );
		}
	else
		offset = kernel.ToMemBlock( memory, offset, attributes ? 1 : 0 );

	if ( attributes )
		offset = attributes->kernel.ToMemBlock( memory, offset, 0 );

	return offset;
	}

Value *ValueFromMemBlock(char *memory, int &offset)
	{
	ValueKernel::header h;
	Value *v = 0;

	memcpy(&h,&memory[offset],sizeof(h));
	offset += sizeof(h);

	switch ( h.type )
		{
		case TYPE_RECORD:
			{
			recordptr rec = create_record_dict();

			for (int i = 0; i < h.len; i++)
				{
				int kl = strlen((char*) &memory[offset]);
				char *key = (char*) alloc_memory( sizeof(char)*(kl+1) );
				memcpy(key,&memory[offset],kl+1);
				offset += kl + 1;
				Value *member = ValueFromMemBlock(memory, offset);
				rec->Insert( key, member );
				}

			v = create_value( rec );
			}
			break;
		case TYPE_STRING:
			{
			char **s = (char**) alloc_memory( sizeof(char*)*h.len );
			for (int i=0; i < h.len; i++)
				{
				int l = strlen(&memory[offset]);
				s[i] = (char*) alloc_memory( sizeof(char)*(l+1) );
				memcpy(s[i],&memory[offset],l+1);
				offset += l+1;
				}

			v = create_value((charptr*)s, h.len);
			}
			break;
		default:
			{
			void *values = alloc_memory( sizeof(char)*h.len );
			memcpy(values,&memory[offset],h.len);
			offset += h.len;
			switch( h.type )
				{

#define VALUE_FROM_MEM_ACTION(tag,type)				\
	case tag:						\
		v = create_value( (type*) values, h.len / sizeof(type) ); \
		break;

VALUE_FROM_MEM_ACTION(TYPE_BOOL, glish_bool)
VALUE_FROM_MEM_ACTION(TYPE_BYTE, byte)
VALUE_FROM_MEM_ACTION(TYPE_SHORT, short)
VALUE_FROM_MEM_ACTION(TYPE_INT, int)
VALUE_FROM_MEM_ACTION(TYPE_FLOAT, float)
VALUE_FROM_MEM_ACTION(TYPE_DOUBLE, double)
VALUE_FROM_MEM_ACTION(TYPE_COMPLEX, complex)
VALUE_FROM_MEM_ACTION(TYPE_DCOMPLEX, dcomplex)

				default:
					fatal->Report( "Bad type (", (int) h.type, ") in ValueFromMemBlock( )" );
				}
			}
		}

	if ( h.have_attr )
		{
		Value *attr = ValueFromMemBlock(memory, offset);
		v->AssignAttributes( attr );
		}

	return v;
	}

Value *ValueFromMemBlock( char *memory )
	{
	int offset = 0;
	return ValueFromMemBlock( memory, offset );
	}


Value* empty_value(glish_type t)
	{
	int i = 0;
	Value *ret = create_value( &i, 0, COPY_ARRAY );
	if ( t != TYPE_INT ) ret->Polymorph( t );
	return ret;
	}

Value* empty_bool_value()
	{
	glish_bool b = glish_false;
	return create_value( &b, 0, COPY_ARRAY );
	}

Value* error_value( )
	{
	return create_value( );
	}

Value* error_value( const char *message )
	{
	return create_value( message, (const char*) 0, 0 );
	}

Value* error_value( const char *message, const char *file, int line )
	{
	return create_value( message, file, line );
	}

Value* create_record()
	{
	return create_value( create_record_dict() );
	}

int compatible_types( const Value* v1, const Value* v2, glish_type& max_type )
	{
	max_type = v1->VecRefDeref()->Type();
	glish_type t = v2->VecRefDeref()->Type();

	if ( v1->IsNumeric() )
		{
		if ( ! v2->IsNumeric() )
			{
			error->Report( "numeric and non-numeric types mixed" );
			return 0;
			}

		max_type = max_numeric_type( max_type, t );
		}

	else
		{
		if ( t != max_type )
			{
			error->Report( "types are incompatible" );
			return 0;
			}
		}

	return 1;
	}


void init_values()
	{
	if ( ! false_value )
		false_value = create_value( glish_false );
	}

void finalize_values()
	{
	delete (Value*) false_value;
	}

charptr *csplit( char* source, int &num_pieces, char* split_chars )
	{

	if ( strlen(split_chars) == 0 )
		{
		num_pieces = strlen(source);
		char **strings = (char**) alloc_memory( sizeof(char*)*num_pieces );
		char *ptr = source;
		for ( int i = 0; i < num_pieces ; i++ )
			{
			strings[i] = (char*) alloc_memory(sizeof(char)*2);
			strings[i][0] = *ptr++;
			strings[i][1] = '\0';
			}
		return (charptr*) strings;
		}

	// First see how many pieces the split will result in.
	num_pieces = 0;
	char* source_copy = strdup( source );
	charptr next_string = strtok( source_copy, split_chars );
	while ( next_string )
		{
		++num_pieces;
		next_string = strtok( 0, split_chars );
		}
	free_memory( source_copy );

	charptr* strings = (charptr*) alloc_memory( sizeof(charptr)*num_pieces );
	charptr* sptr = strings;
	next_string = strtok( source, split_chars );
	while ( next_string )
		{
		*(sptr++) = strdup( next_string );
		next_string = strtok( 0, split_chars );
		}

	return strings;
	}

Value *split( char* source, char* split_chars )
	{
	int i = 0;
	charptr *s = csplit( source, i, split_chars );
	return create_value( s, i );
	}

int text_to_integer( const char text[], int& successful )
	{
	char* text_ptr;
	double d = strtod( text, &text_ptr );
	int result = int( d );
	successful = text_ptr == &text[strlen( text )] && d == result;

	return result;
	}


double text_to_double( const char text[], int& successful )
	{
	char* text_ptr;
	double result = strtod( text, &text_ptr );
	successful = text_ptr == &text[strlen( text )];

	return result;
	}


// ### This should be looked at again, later.
dcomplex text_to_dcomplex( const char text[], int& successful )
	{
	dcomplex result( 0.0, 0.0 );

	char* text_ptr;
	double num = strtod( text, &text_ptr );

	if ( text == text_ptr )
		{
		successful = 0;
		return result;
		}

	while ( *text_ptr && isspace(*text_ptr) )
		++text_ptr;

	if ( *text_ptr == 'i' )
		{
		result.i = num;
		successful = 1;
		return result;
		}

	result.r = num;

	char* ptr = text_ptr;
	if ( !*ptr || *ptr != '-' && *ptr != '+' )
		{
		successful = 1;
		return result;
		}

	char sign = *ptr++;

	while ( isspace(*ptr) )
		++ptr;

	result.i = strtod( ptr, &text_ptr ) * (sign == '-' ? -1 : 1);

	if ( ptr == text_ptr )
		{
		successful = 0;
		return result;
		}

	while ( *text_ptr && isspace(*text_ptr) )
		++text_ptr;

	successful = *text_ptr == 'i';

	return result;
	}


glish_type max_numeric_type( glish_type t1, glish_type t2 )
	{
#define TEST_TYPE(type)			\
	if ( t1 == type || t2 == type )	\
		return type;

	TEST_TYPE(TYPE_DCOMPLEX)
	else TEST_TYPE(TYPE_COMPLEX)
	else TEST_TYPE(TYPE_DOUBLE)
	else TEST_TYPE(TYPE_FLOAT)
	else TEST_TYPE(TYPE_INT)
	else TEST_TYPE(TYPE_SHORT)
	else TEST_TYPE(TYPE_BYTE)
	else
		return TYPE_BOOL;
	}

Value *Fail( const RMessage& m0,
	       const RMessage& m1, const RMessage& m2,
	       const RMessage& m3, const RMessage& m4,
	       const RMessage& m5, const RMessage& m6,
	       const RMessage& m7, const RMessage& m8,
	       const RMessage& m9, const RMessage& m10,
	       const RMessage& m11, const RMessage& m12,
	       const RMessage& m13, const RMessage& m14,
	       const RMessage& m15, const RMessage& m16
	   )
	{
	return generate_error( m0,m1,m2,m3,m4,m5,m6,m7,m8,m9,
			       m10,m11,m12,m13,m14,m15,m16 );
	}

Value *Fail( )
	{
	return error_value( );
	}

#ifdef CLASS
#undef CLASS
#endif
#ifdef DOIVAL
#undef DOIVAL
#endif
#define CLASS Value
// This is also included by <IValue.cc>
#include "StringVal"
