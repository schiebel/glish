// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include <string.h>
#include <iostream.h>
#include <stdlib.h>
#include <ctype.h>

#include "Glish/Value.h"

#include "glish_event.h"
#include "Reporter.h"

extern int glish_collecting_garbage;

int num_Values_created = 0;
int num_Values_deleted = 0;

const char* type_names[NUM_GLISH_TYPES] =
	{
	"error", "ref", "subref",
	"boolean", "byte", "short", "integer",
	"float", "double", "string", "agent", "function", "record",
	"complex", "dcomplex", "fail",
	};

const Value* false_value = 0;

#define INIT_VALUE_ACTION		\
	description = 0;		\
	value_manager = 0;		\
	attributes = 0;			\
	++num_Values_created;

class DelObj : public GlishObject {
public:
	DelObj( GlishObject* arg_obj )	{ obj = arg_obj; ptr = 0; cptr = 0; }
	DelObj( void* arg_ptr )		{ obj = 0; ptr = arg_ptr; cptr = 0; }
	DelObj( char* arg_ptr )		{ obj = 0; ptr = 0; cptr = arg_ptr; }

	~DelObj();

protected:
	GlishObject* obj;
	void* ptr;
	char* cptr;
};

DelObj::~DelObj()
	{
	Unref( obj );
	if ( ptr ) delete ptr;
	if ( cptr ) free_memory( cptr );
	}


Value::Value( )
	{
	DIAG4( (void*) this, "Value(", " ",")" )
	INIT_VALUE_ACTION
	kernel.SetFail( );
	}

Value::Value( glish_type )
	{
	INIT_VALUE_ACTION
	}

Value::Value( const char *message, const char *file, int lineNum )
	{
	DIAG4( (void*) this, "Value(", " ",")" )
	INIT_VALUE_ACTION
	kernel.SetFail( );

	if ( file && file[0] )
		{
		AssignAttribute( "file", create_value( file ));
		if ( lineNum > 0 )
			AssignAttribute( "line", create_value( lineNum ));
		}

	if ( message )
		AssignAttribute( "message", create_value( message ));
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

	Ref( ref_value );
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
	if ( IsRef() && ! glish_collecting_garbage )
		Unref( RefPtr() );

	if ( value_manager )
		{
		Unref( value_manager );

		// It's important to get rid of our value_manager
		// pointer here; a call to DeleteValue does not
		// necessarily mean we're throwing away the entire
		// Value object.  (For example, we may be called
		// by SetType, called in turn by Polymorph.)  Thus
		// as we're done with this value_manager, mark it
		// as so.
		value_manager = 0;
		}

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
			return ref->Val()->name( off );			\
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


static void append_buf( char* &buf, char* &buf_ptr, unsigned int& buf_size,
		const char* a = 0, const char* b = 0, const char* c = 0 )
	{
	a = a ? a : "";
	b = b ? b : "";
	c = c ? c : "";

	int buf_remaining = &buf[buf_size] - buf_ptr;
	int size_of_addition = strlen( a ) + strlen( b ) + strlen( c );

	while ( size_of_addition >= buf_remaining - 5 /* slop */ )
		{ // Need to grow the buffer.
		int buf_ptr_offset = buf_ptr - buf;

		buf_size *= 2;
		buf = (char*) realloc_memory( (void*) buf, buf_size );
		if ( ! buf )
			fatal->Report( "out of memory in append_buf()" );

		buf_ptr = buf + buf_ptr_offset;
		buf_remaining = &buf[buf_size] - buf_ptr;
		}

	*buf_ptr = '\0';
	strcat( buf_ptr, a );
	strcat( buf_ptr, b );
	strcat( buf_ptr, c );

	buf_ptr += size_of_addition;
	}

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

static char *format_error_message( const Value *, char, unsigned int, int );
char* Value::StringVal( char sep, unsigned int max_elements,
			int useAttributes, Str &err ) const
	{
	glish_type type = Type();
	int length = kernel.Length();

	if ( IsRef() )
		return Deref()->StringVal( sep, max_elements, useAttributes );
	if ( type == TYPE_RECORD )
		return RecordStringVal( sep, max_elements, useAttributes, err );
	if ( type == TYPE_AGENT )
		return strdup( "<agent>" );
	if ( type == TYPE_FUNC )
		return strdup( "<function>" );
	if ( type == TYPE_FAIL )
		return format_error_message( this, sep, max_elements, useAttributes );
	if ( length == 0 )
		return strdup( "" );

	unsigned int buf_size = 0;

	// Make a guess as to a probable good size for buf.
	if ( type == TYPE_STRING )
		{
		buf_size = strlen( StringPtr(0)[0] ) * (length + 1);
		if ( buf_size == 0 )
			buf_size = 8;
		}

	else if ( type == TYPE_COMPLEX  || type == TYPE_DCOMPLEX )
		buf_size = length * 16 * 2 + 1;

	else if ( type == TYPE_FLOAT  || type == TYPE_DOUBLE )
		buf_size = length * 16;

	else
		buf_size = length * 8;

	char* buf = (char*) alloc_memory( sizeof(char)*buf_size );
	if ( ! buf )
		fatal->Report( "out of memory in Value::StringVal()" );

	char* buf_ptr = buf;

	if ( type != TYPE_STRING && length > 1 )
		{
		// Insert []'s around value.
		*buf_ptr++ = '[';
		}

	glish_bool* bool_ptr = 0;
	byte* byte_ptr = 0;
	short* short_ptr = 0;
	int* int_ptr = 0;
	float* float_ptr = 0;
	double* double_ptr = 0;
	complex* complex_ptr = 0;
	dcomplex* dcomplex_ptr = 0;
	charptr* string_ptr = 0;
	const char *flt_prec = 0;

	switch ( VecRefDeref()->Type() )
		{
#define ASSIGN_PTR(tag,ptr_name,accessor,extra)				\
	case tag:							\
		ptr_name = accessor(0);					\
		extra							\
		break;

		ASSIGN_PTR(TYPE_BOOL,bool_ptr,BoolPtr,)
		ASSIGN_PTR(TYPE_INT,int_ptr,IntPtr,)
		ASSIGN_PTR(TYPE_BYTE,byte_ptr,BytePtr,)
		ASSIGN_PTR(TYPE_SHORT,short_ptr,ShortPtr,)
		ASSIGN_PTR(TYPE_FLOAT,float_ptr,FloatPtr,flt_prec = print_decimal_prec( AttributePtr() );)
		ASSIGN_PTR(TYPE_DOUBLE,double_ptr,DoublePtr,flt_prec = print_decimal_prec( AttributePtr() );)
		ASSIGN_PTR(TYPE_COMPLEX,complex_ptr,ComplexPtr,flt_prec = print_decimal_prec( AttributePtr() );)
		ASSIGN_PTR(TYPE_DCOMPLEX,dcomplex_ptr,DcomplexPtr,flt_prec = print_decimal_prec( AttributePtr() );)
		ASSIGN_PTR(TYPE_STRING,string_ptr,StringPtr,)

		default:
			fatal->Report( "bad type in Value::StringVal()" );
		}


// Macro to generate the text corresponding to a single element of a given type.
#define PLACE_ELEMENT_ACTION(buffer,str_buffer,indx,FLOAT_PRECISION)	\
	case TYPE_BOOL:							\
		strcpy( buffer, bool_ptr[indx] ? "T" : "F" );		\
		break;							\
									\
	case TYPE_BYTE:							\
		sprintf( buffer, "%d", byte_ptr[indx] );		\
		break;							\
									\
	case TYPE_SHORT:						\
		sprintf( buffer, "%d", short_ptr[indx] );		\
		break;							\
									\
	case TYPE_INT:							\
		sprintf( buffer, "%d", int_ptr[indx] );			\
		break;							\
									\
	case TYPE_FLOAT:						\
		sprintf( buffer, FLOAT_PRECISION, float_ptr[indx] );	\
		break;							\
									\
	case TYPE_DOUBLE:						\
		sprintf( buffer, FLOAT_PRECISION, double_ptr[indx] );	\
		break;							\
									\
	case TYPE_COMPLEX:						\
		{							\
		char t[64];						\
		sprintf( t, FLOAT_PRECISION, complex_ptr[indx].r );	\
		strcpy( buffer, t);					\
		if ( complex_ptr[indx].i >= 0.0 ) 			\
			strcat( buffer, "+" );				\
		sprintf( t, FLOAT_PRECISION, complex_ptr[indx].i );	\
		strcat( t, "i");					\
		strcat( buffer, t);					\
		}							\
		break;							\
									\
	case TYPE_DCOMPLEX:						\
		{							\
		char t[64];						\
		sprintf( t, FLOAT_PRECISION, dcomplex_ptr[indx].r );	\
		strcpy( buffer, t);					\
		if ( dcomplex_ptr[indx].i >= 0.0 ) 			\
			strcat( buffer, "+" );				\
		sprintf( t, FLOAT_PRECISION, dcomplex_ptr[indx].i );	\
		strcat( t, "i");					\
		strcat( buffer, t);					\
		}							\
		break;							\
									\
	case TYPE_STRING:						\
		str_buffer = string_ptr[ indx ];			\
		break;


// Generate text for an element, translating subref indices if needed.
#define PLACE_ELEMENT(buffer,str_buffer,indx,alloced)			\
	switch ( type )							\
		{							\
		PLACE_ELEMENT_ACTION(buffer,str_buffer,indx,flt_prec)	\
									\
		case TYPE_SUBVEC_REF:					\
			{						\
			VecRef* ref = VecRefPtr();			\
			int erri = 0;					\
			int index = ref->TranslateIndex( indx, &erri );	\
			if ( erri )					\
				{					\
				err = strFail( "invalid sub-vector" );	\
				free_memory( alloced );			\
				return strdup( " " );			\
				}					\
			switch ( ref->Type() )				\
				{					\
				PLACE_ELEMENT_ACTION(buffer,str_buffer,index,flt_prec)\
									\
				default:				\
					fatal->Report(			\
				    "bad type in Value::StringVal()" ); \
				} 					\
			}						\
			break;						\
									\
		default:						\
			fatal->Report(					\
			    "bad type in Value::StringVal()" );		\
		}


	char numeric_buf[256];

	const attributeptr attr = AttributePtr();
	const Value* shape_val = 0;
	int shape_len = 0;

	if ( ! useAttributes || ! attr || ! (shape_val = (*attr)["shape"]) || 
	     ! shape_val->IsNumeric() ||
	     (shape_len = shape_val->Length()) <= 1 )
		{ // not an n-D array.
		for ( int i = 0; i < length && ( ! max_elements ||
		      i < max_elements) ; ++i )
			{
			const char* addition = numeric_buf;

			PLACE_ELEMENT(numeric_buf, addition, i, buf);
			append_buf( buf, buf_ptr, buf_size, addition );

			if ( i < length - 1 )
				// More to come.
				*buf_ptr++ = sep;
			}

		if ( max_elements && length > max_elements )
			append_buf( buf, buf_ptr, buf_size, "... " );
		if ( type != TYPE_STRING && length > 1 )
			{
			// Insert []'s around value.
			append_buf( buf, buf_ptr, buf_size, "] " );
			*buf_ptr = '\0';
			}

		return buf;
		}

	// Okay, from this point on it's an n-D array.
	static char indent[] = "    ";

	// Later the pivots for outputting by planes can be made variable
	int r = 0;
	int c = 1;

	// How many element have we output...
	int element_count = 0;

	int shape_is_copy = 0;
	int* shape = shape_val->CoerceToIntArray( shape_is_copy, shape_len );

	// Store for setting up a plane in advance to get the proper
	// spacing for the columns.  Note that these and the arrays
	// created just below are static, so we don't free them on exit.
	static int column_width_len = 64;
	static int* column_width = (int*) alloc_memory( sizeof(int)*column_width_len );

	// Arrays for iterating through the matrix.
	static int indices_len = 32;
	static int* indices = (int*) alloc_memory( sizeof(int)*indices_len );
	static int* factor = (int*) alloc_memory( sizeof(int)*indices_len );

	// Resize arrays as necessary.
	while ( shape[c] > column_width_len )
		{
		column_width_len *= 2;
		column_width = (int*) realloc_memory( (void*) column_width,
					column_width_len * sizeof(int) );
		if ( ! column_width )
			fatal->Report( "out of memory in Value::StringVal()" );
		}

	while ( shape_len > indices_len )
		{
		indices_len *= 2;
		indices = (int*) realloc_memory( (void*) indices,
						indices_len * sizeof(int) );
		factor = (int*) realloc_memory( (void*) factor,
					indices_len * sizeof(int) );
		if ( ! indices || ! factor )
			fatal->Report( "out of memory in Value::StringVal()" );
		}

	// Calculate the size and the offset for the columns.
	int size = 1;
	int offset = 0;
	for ( int i = 0; i < shape_len; ++i )
		{
		indices[i] = 0;
		factor[i] = size;
		size *= shape[i];
		}

	// Check to see if the vector length and the shape jive.
	if ( size > length ) 
		{
		warn->Report( "\"::shape\"/length mismatch" );
		free_memory( buf );
		if ( shape_is_copy )
			free_memory( shape );
		return StringVal( sep );
		}

	int max_free = shape_len-1;

	if ( shape_len > 2 )
		for ( max_free = shape_len-1; max_free > 0; --max_free )
			if ( max_free != r && max_free != c )
				break;

	while ( indices[max_free] < shape[max_free] && ( ! max_elements ||
			element_count < max_elements) )
		{
		// Output the plane label
		for ( LOOPDECL i = 0; i < shape_len; ++i )
			{
			if ( i == r )
				sprintf( numeric_buf, "1:%d", shape[r] );
			else if ( i != c )
				sprintf( numeric_buf, "%d", indices[i] + 1 );
			else
				numeric_buf[0] = '\0';

			if ( i < shape_len - 1 )
				strcat( numeric_buf, "," );
			else
				strcat( numeric_buf, "]\n" );

			append_buf( buf, buf_ptr, buf_size, i==0 ? "[" : 0,
					numeric_buf );
			}

		// Calculate column widths.
		for ( indices[r] = 0; indices[r] < shape[r]; ++indices[r] )
			for ( indices[c] = 0; indices[c] < shape[c] - 1;
			      ++indices[c] )
				{
				offset = 0;
				for ( LOOPDECL i = 0; i < shape_len; ++i )
					offset += factor[i] * indices[i];

				char store[256];
				const char* addition = store;

				PLACE_ELEMENT(store,addition,offset,buf)

				int add_len = strlen( addition );
				if ( indices[r] == 0 || 
				     add_len > column_width[indices[c]] )
					column_width[indices[c]] = add_len;
				}

		// Output plane.
		for ( indices[r] = 0; indices[r] < shape[r] && ( !max_elements ||
			element_count < max_elements) ; ++indices[r] )
			{
			for ( indices[c] = 0; indices[c] < shape[c] && ( !max_elements ||
				element_count < max_elements); ++indices[c] )
				{
				offset = 0;
				for ( LOOPDECL i = 0; i < shape_len; ++i )
					offset += factor[i] * indices[i];

				const char* addition = numeric_buf;
				PLACE_ELEMENT(numeric_buf,addition,offset,buf);

				element_count++;
				char affix[256];
				if ( max_elements && element_count >= max_elements )

					strcpy(affix, " ... ");

				else if ( indices[c] < shape[c] - 1 )
					{
					int n = column_width[indices[c]] -
						strlen( addition ) + 1;

					LOOPDECL i = 0;
					for ( ; i < n; ++i )
						affix[i] = ' ';
					affix[i] = '\0';
					}

				else if ( offset != size - 1 )
					{
					affix[0] = '\n';
					affix[1] = '\0';
					}
				else
					affix[0] = '\0';

				append_buf( buf, buf_ptr, buf_size,
						indices[c] == 0 ? indent : 0,
						addition, affix );
				}
			}

		// Increment counters.
		for ( LOOPDECL i = 0; i <= max_free; ++i )
			{
			if ( i == r || i == c )
				continue;
			else if ( ++indices[i] < shape[i] )
				break;
			else if ( i != max_free )
				indices[i] = 0;
			}
		}

	if ( shape_is_copy )
		free_memory( shape );

	append_buf( buf, buf_ptr, buf_size, "]" );

	return buf;
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

char* Value::RecordStringVal( char sep, unsigned int max_elements, 
			int use_attr, Str &err ) const
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

		element_strs[i] = nth_val->StringVal( sep, max_elements, use_attr, err );
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
		COERCE_COMPLEX_ACTIONS(float,"CoerceToDcomplexArray()")
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
		error->Report( "non-string type in coercion of", this,
				"to string" );
		return 0;
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

Value* Value::operator []( const Value* index ) const
	{
	if ( index->Type() == TYPE_STRING )
		return RecordRef( index );

	int indices_are_copy;
	int num_indices;
	int* indices = GenerateIndices( index, num_indices, indices_are_copy );

	if ( indices )
		{
		Value* result = ArrayRef( indices, num_indices );

		if ( indices_are_copy )
			free_memory( indices );

		return result;
		}

	else
		return error_value();
	}


Value* Value::operator []( const_value_list* args_val ) const
	{

// These are a bunch of macros for cleaning up the dynamic memory used
// by this routine (and Value::SubRef) prior to exit.
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

	int length = kernel.Length();

	if ( ! IsNumeric() && VecRefDeref()->Type() != TYPE_STRING )
		return Fail( "invalid type in n-D array operation:", this );

	// Collect attributes.
	int args_len = args_val->length();
	const attributeptr ptr = AttributePtr();
	const Value* shape_val = ptr ? (*ptr)["shape"] : 0;
	if ( ! shape_val || ! shape_val->IsNumeric() )
		{
		warn->Report( "invalid or non-existant \"shape\" attribute" );

		if ( args_len >= 1 )
			return operator[]( (*args_val)[0] );
		else
			return copy_value( this );
		}

	int shape_len = shape_val->Length();
	if ( shape_len != args_len )
		return Fail( "invalid number of indexes for:", this );

	int shape_is_copy;
	int* shape = shape_val->CoerceToIntArray( shape_is_copy, shape_len );
	Value* op_val = (*ptr)["op[]"];

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
				SUBOP_CLEANUP_1
				return Fail( "index #", i+1, "into", this,
						"is not numeric");
				}

			if ( arg->Length() > max_len )
				max_len = arg->Length();

			if ( max_len == 1 )
				{
				int ind = arg->IntVal();
				if ( ind < 1 || ind > shape[i] )
					{
					SUBOP_CLEANUP_1
					return Fail( "index #", i+1, "into",
						this, "is out of range");
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
		SUBOP_CLEANUP_1
		return Fail( "\"::shape\"/length mismatch" );
		}

	if ( max_len == 1 ) 
		{
		SUBOP_CLEANUP_1
		++offset;
		// Should separate ArrayRef to get a single value??
		return ArrayRef( &offset, 1 );
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
					return Fail( "index #", i+1, ",",
							j+1, " into ", this, 
							"is out of range.");
				else
					return Fail( "index #", i+1, "into",
						this, "is out of range.");

				}
			}
		}

	// Loop through filling resultant vector.
	Value* result;
	switch ( Type() )
		{
#define SUBSCRIPT_OP_ACTION(tag,type,accessor,LEN,OFFSET,copy_func,ERROR)	\
	case tag:								\
		{								\
		type* vec = accessor;						\
		type* ret = (type*) alloc_memory( sizeof(type)*vecsize );	\
										\
		for ( int v = 0; v < vecsize; ++v )				\
			{							\
			/**** Calculate offset ****/				\
			offset = 0;						\
			for ( LOOPDECL i = 0; i < shape_len; ++i )		\
				offset += factor[i] *				\
						(index[i][cur[i]]-1);		\
			/**** Set Value ****/					\
			ERROR							\
			ret[v] = copy_func( vec[OFFSET] );			\
			/****  Advance counters ****/				\
			for ( LOOPDECL i = 0; i < shape_len; ++i )		\
				if ( ++cur[i] < len[i] )			\
					break;					\
				else						\
					cur[i] = 0;				\
			}							\
										\
		result = create_value( ret, vecsize );				\
		if ( ! is_element )						\
			{							\
			int z = 0;						\
			for ( int x = 0; x < shape_len; ++x )			\
				if ( len[x] > 1 )				\
					len[z++] = len[x];			\
										\
			result->AssignAttribute( "shape",			\
						create_value( len, z ) );	\
			if ( op_val )						\
				result->AssignAttribute( "op[]", op_val );	\
			}							\
		else								\
			free_memory( len );					\
		}								\
		break;

SUBSCRIPT_OP_ACTION(TYPE_BOOL,glish_bool,BoolPtr(),length,offset,,)
SUBSCRIPT_OP_ACTION(TYPE_BYTE,byte,BytePtr(),length,offset,,)
SUBSCRIPT_OP_ACTION(TYPE_SHORT,short,ShortPtr(),length,offset,,)
SUBSCRIPT_OP_ACTION(TYPE_INT,int,IntPtr(),length,offset,,)
SUBSCRIPT_OP_ACTION(TYPE_FLOAT,float,FloatPtr(),length,offset,,)
SUBSCRIPT_OP_ACTION(TYPE_DOUBLE,double,DoublePtr(),length,offset,,)
SUBSCRIPT_OP_ACTION(TYPE_COMPLEX,complex,ComplexPtr(),length,offset,,)
SUBSCRIPT_OP_ACTION(TYPE_DCOMPLEX,dcomplex,DcomplexPtr(),length,offset,,)
SUBSCRIPT_OP_ACTION(TYPE_STRING,charptr,StringPtr(),length,offset,strdup,)

		case TYPE_SUBVEC_REF:
			{
			VecRef* ref = VecRefPtr();
			Value* theVal = ref->Val();
			int theLen = theVal->Length();

			switch ( theVal->Type() )
				{

#define SUBSCRIPT_OP_ACTION_XLATE(EXTRA_ERROR)		\
	int err;					\
	int off = ref->TranslateIndex( offset, &err );	\
	if ( err )					\
		{					\
		EXTRA_ERROR				\
		free_memory( ret );			\
		SUBOP_CLEANUP(shape_len)		\
		return error_value();			\
		}

SUBSCRIPT_OP_ACTION(TYPE_BOOL, glish_bool, theVal->BoolPtr(),
	theLen, off,,SUBSCRIPT_OP_ACTION_XLATE(;))
SUBSCRIPT_OP_ACTION(TYPE_BYTE, byte, theVal->BytePtr(),
	theLen, off,,SUBSCRIPT_OP_ACTION_XLATE(;))
SUBSCRIPT_OP_ACTION(TYPE_SHORT, short, theVal->ShortPtr(),
	theLen, off,,SUBSCRIPT_OP_ACTION_XLATE(;))
SUBSCRIPT_OP_ACTION(TYPE_INT, int, theVal->IntPtr(),
	theLen, off,,SUBSCRIPT_OP_ACTION_XLATE(;))
SUBSCRIPT_OP_ACTION(TYPE_FLOAT, float, theVal->FloatPtr(),
	theLen, off,,SUBSCRIPT_OP_ACTION_XLATE(;))
SUBSCRIPT_OP_ACTION(TYPE_DOUBLE, double, theVal->DoublePtr(),
	theLen, off,,SUBSCRIPT_OP_ACTION_XLATE(;))
SUBSCRIPT_OP_ACTION(TYPE_COMPLEX, complex, theVal->ComplexPtr(),
	theLen, off,,SUBSCRIPT_OP_ACTION_XLATE(;))
SUBSCRIPT_OP_ACTION(TYPE_DCOMPLEX, dcomplex, theVal->DcomplexPtr(),
	theLen, off,,SUBSCRIPT_OP_ACTION_XLATE(;))
SUBSCRIPT_OP_ACTION(TYPE_STRING, charptr, theVal->StringPtr(),
	theLen, off,strdup,SUBSCRIPT_OP_ACTION_XLATE(for(int X=0;X<v;X++) free_memory((void*)ret[X]);))

				default:
					fatal->Report(
				"bad subref type in Value::operator[]" );
				}
			}
			break;

		default:
			fatal->Report( "bad type in Value::operator[]" );
		}

	SUBOP_CLEANUP_2(shape_len)
	return result;
	}

Value* Value::Pick( const Value *index ) const
	{
#define PICK_CLEANUP				\
	if ( shape_is_copy )			\
		free_memory( shape );		\
	if ( ishape_is_copy )			\
		free_memory( ishape );		\
	if ( indx_is_copy )			\
		free_memory( indx );		\
	free_memory( factor );

#define PICK_INITIALIZE(ERR_RET,SHORT)					\
	const attributeptr attr = AttributePtr();			\
	const attributeptr iattr = index->AttributePtr();		\
	const Value* shape_val = 0;					\
	const Value* ishape_val = 0;					\
	int shape_len = 0;						\
	int ishape_len = 0;						\
									\
	if ( attr && (shape_val = (*attr)["shape"]) &&			\
	     shape_val->IsNumeric() )					\
		shape_len = shape_val->Length();			\
	if ( iattr && (ishape_val = (*iattr)["shape"]) &&		\
	     ishape_val->IsNumeric() )					\
		ishape_len = ishape_val->Length();			\
									\
	/* Neither has a shape so pick from the vector. */		\
	if ( ishape_len <= 1 && shape_len <= 1 )			\
		{							\
		SHORT							\
		}							\
									\
	if ( ! ishape_len )						\
		{							\
		Value *err = 0;						\
		if ( ishape_val )					\
			ERR_RET(("error in the array \"::shape\": ",	\
				ishape_val))				\
		else							\
			ERR_RET(("no \"::shape\" for ", index,		\
				" but the array has \"::shape\""))	\
		}							\
	if ( ! shape_len )						\
		{							\
		if ( shape_val )					\
			ERR_RET(("error in the array \"::shape\": ",	\
				shape_val))				\
		else							\
			ERR_RET(("no \"::shape\" for ", this,		\
				" but the index has \"::shape\""))	\
		}							\
									\
	if ( ishape_len > 2 )						\
		ERR_RET(("invalid index of dimension (=", ishape_len, \
				") greater than 2"))			\
									\
	int shape_is_copy = 0;						\
	int ishape_is_copy = 0;						\
	int indx_is_copy = 0;						\
	int* shape = shape_val->CoerceToIntArray( shape_is_copy, shape_len );\
	int* ishape =							\
		ishape_val->CoerceToIntArray( ishape_is_copy, ishape_len );\
	int ilen = index->Length();					\
	int len = Length();						\
	int* factor = (int*) alloc_memory( sizeof(int)*shape_len );	\
	int offset = 1;							\
	int* indx = index->CoerceToIntArray( indx_is_copy, ilen );	\
	Value* result = 0;						\
									\
	if ( ishape[1] != shape_len )					\
		{							\
		PICK_CLEANUP						\
		ERR_RET(("wrong number of columns in index (=",		\
			ishape[1], ") expected ", shape_len))		\
		}							\
	if ( ilen < ishape[0] * ishape[1] )				\
		{							\
		PICK_CLEANUP						\
		ERR_RET(("Index \"::shape\"/length mismatch"))		\
		}							\
	for ( int i = 0; i < shape_len; ++i )				\
		{							\
		factor[i] = offset;					\
		offset *= shape[i];					\
		}							\
									\
	if ( len < offset )						\
		{							\
		PICK_CLEANUP						\
		ERR_RET(("Array \"::shape\"/length mismatch"))		\
		}

#define PICK_FAIL_IVAL(x) return Fail x;
#define PICK_FAIL_VOID(x) { error->Report x; return; }

	PICK_INITIALIZE( PICK_FAIL_IVAL, return this->operator[]( index );)

	switch ( Type() )
		{
#define PICK_ACTION_CLEANUP for(int X=0;X<i;X++) free_memory((void*)ret[X]);
#define PICK_ACTION(tag,type,accessor,OFFSET,COPY_FUNC,XLATE,CLEANUP)	\
	case tag:							\
		{							\
		type* ptr = accessor();					\
		type* ret = (type*) alloc_memory( sizeof(type)*ishape[0] );\
		int cur = 0;						\
		for ( LOOPDECL i = 0; i < ishape[0]; ++i )		\
			{						\
			offset = 0;					\
			int j = 0;					\
			for ( ; j < ishape[1]; ++j )			\
				{					\
				cur = indx[i + j * ishape[0]];		\
				if ( cur < 1 || cur > shape[j] )	\
					{				\
					PICK_CLEANUP			\
					CLEANUP				\
					free_memory( ret );		\
					return Fail( "index number ", j,\
					" (=", cur, ") is out of range" );\
					}				\
				offset += factor[j] * (cur-1);		\
				}					\
			XLATE						\
			ret[i] = COPY_FUNC( ptr[ OFFSET ] );		\
			}						\
		result = create_value( ret, ishape[0] );		\
		}							\
		break;

		PICK_ACTION(TYPE_BOOL,glish_bool,BoolPtr,offset,,,)
		PICK_ACTION(TYPE_BYTE,byte,BytePtr,offset,,,)
		PICK_ACTION(TYPE_SHORT,short,ShortPtr,offset,,,)
		PICK_ACTION(TYPE_INT,int,IntPtr,offset,,,)
		PICK_ACTION(TYPE_FLOAT,float,FloatPtr,offset,,,)
		PICK_ACTION(TYPE_DOUBLE,double,DoublePtr,offset,,,)
		PICK_ACTION(TYPE_COMPLEX,complex,ComplexPtr,offset,,,)
		PICK_ACTION(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,offset,,,)
		PICK_ACTION(TYPE_STRING,charptr,StringPtr,offset,strdup,,
			PICK_ACTION_CLEANUP)

		case TYPE_SUBVEC_REF:
			{
			VecRef* ref = VecRefPtr();
			Value* theVal = ref->Val();
			int theLen = theVal->Length();

			switch ( theVal->Type() )
				{

#define PICK_ACTION_XLATE(CLEANUP)					\
	int err;							\
	int off = ref->TranslateIndex( offset, &err );			\
	if ( err )							\
		{							\
		CLEANUP							\
		free_memory( ret );					\
		return Fail( "index number ", j, " (=",cur,		\
			") is out of range. Sub-vector reference may be invalid" );\
		}

PICK_ACTION(TYPE_BOOL,glish_bool,theVal->BoolPtr,off,,PICK_ACTION_XLATE(;),)
PICK_ACTION(TYPE_BYTE,byte,theVal->BytePtr,off,,PICK_ACTION_XLATE(;),)
PICK_ACTION(TYPE_SHORT,short,theVal->ShortPtr,off,,PICK_ACTION_XLATE(;),)
PICK_ACTION(TYPE_INT,int,theVal->IntPtr,off,,PICK_ACTION_XLATE(;),)
PICK_ACTION(TYPE_FLOAT,float,theVal->FloatPtr,off,,PICK_ACTION_XLATE(;),)
PICK_ACTION(TYPE_DOUBLE,double,theVal->DoublePtr,off,,PICK_ACTION_XLATE(;),)
PICK_ACTION(TYPE_COMPLEX,complex,theVal->ComplexPtr,off,,PICK_ACTION_XLATE(;),)
PICK_ACTION(TYPE_DCOMPLEX,dcomplex,theVal->DcomplexPtr,off,,PICK_ACTION_XLATE(;),)
PICK_ACTION(TYPE_STRING,charptr,theVal->StringPtr,off,strdup,
	PICK_ACTION_XLATE(PICK_ACTION_CLEANUP),PICK_ACTION_CLEANUP)

				default:
					fatal->Report(
					"bad subref type in Value::Pick" );
				}
			}
			break;

		default:
			fatal->Report( "bad subref type in Value::Pick" );
		}

	PICK_CLEANUP
	return result;
	}

Value* Value::PickRef( const Value *index )
	{
	if ( ! IsNumeric() && Type() != TYPE_STRING )
		return Fail( "non-numeric type in subreference operation:",
				this );

	PICK_INITIALIZE(PICK_FAIL_IVAL, return this->operator[]( index );)

	int* ret = (int*) alloc_memory( sizeof(int)*ishape[0] );
	int cur = 0;
	for ( LOOPDECL i = 0; i < ishape[0]; ++i )
		{
		offset = 0;
		for ( int j = 0; j < ishape[1]; ++j )
			{
			cur = indx[i + j * ishape[0]];
			if ( cur < 1 || cur > shape[j] )
				{
				PICK_CLEANUP
				free_memory( ret );
				return Fail( "index number ", j, " (=",cur,
						") is out of range" );
				}
			offset += factor[j] * (cur-1);
			}
		ret[i] = offset + 1;
		}

	result = create_value((Value*)this,ret,ishape[0],VAL_REF,1);

	const attributeptr cap = result->AttributePtr();
	if ( (*cap)["shape"] )
		{
		if ( cap->Length() == 1 )
			result->DeleteAttributes();
		else
			{
			attributeptr ap = result->ModAttributePtr();
			delete ap->Remove( "shape" );
			}
		}

	PICK_CLEANUP
	return result;
	}

void Value::PickAssign( const Value* index, Value* value )
	{
#define PICKASSIGN_SHORT		\
	AssignElements( index, value );	\
	return;

	PICK_INITIALIZE(PICK_FAIL_VOID, PICKASSIGN_SHORT)

	switch ( Type() )
		{
#define PICKASSIGN_ACTION(tag,type,to_accessor,from_accessor,COPY_FUNC,XLATE)\
	case tag:							\
		{							\
		int cur = 0;						\
		int* offset_vec = (int*) alloc_memory( sizeof(int)*ishape[0] );\
		for ( LOOPDECL i = 0; i < ishape[0]; ++i )		\
			{						\
			offset_vec[i] = 0;				\
			int j = 0;					\
			for ( ; j < ishape[1]; ++j )			\
				{					\
				cur = indx[i + j * ishape[0]];		\
				if ( cur < 1 || cur > shape[j] )	\
					{				\
					PICK_CLEANUP			\
					free_memory( offset_vec );	\
					error->Report("index number ", i,\
							" (=", cur,	\
							") is out of range");\
					return;				\
					}				\
				offset_vec[i] += factor[j] * (cur-1);	\
				}					\
			XLATE						\
			}						\
									\
		int is_copy;						\
		type* vec = value->from_accessor( is_copy, ishape[0] );	\
		type* ret = to_accessor();				\
		for ( LOOPDECL i = 0; i < ishape[0]; ++i )		\
			ret[ offset_vec[i] ] = COPY_FUNC( vec[i] );	\
		free_memory( offset_vec );				\
		if ( is_copy )						\
			free_memory( vec );				\
		}							\
		break;

PICKASSIGN_ACTION(TYPE_BOOL,glish_bool,BoolPtr,CoerceToBoolArray,,)
PICKASSIGN_ACTION(TYPE_BYTE,byte,BytePtr,CoerceToByteArray,,)
PICKASSIGN_ACTION(TYPE_SHORT,short,ShortPtr,CoerceToShortArray,,)
PICKASSIGN_ACTION(TYPE_INT,int,IntPtr,CoerceToIntArray,,)
PICKASSIGN_ACTION(TYPE_FLOAT,float,FloatPtr,CoerceToFloatArray,,)
PICKASSIGN_ACTION(TYPE_DOUBLE,double,DoublePtr,CoerceToDoubleArray,,)
PICKASSIGN_ACTION(TYPE_COMPLEX,complex,ComplexPtr,CoerceToComplexArray,,)
PICKASSIGN_ACTION(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,CoerceToDcomplexArray,,)
PICKASSIGN_ACTION(TYPE_STRING,charptr,StringPtr,CoerceToStringArray,strdup,)

		case TYPE_SUBVEC_REF:
			{
			VecRef* ref = VecRefPtr();
			Value* theVal = ref->Val();

			switch ( theVal->Type() )
				{

#define PICKASSIGN_ACTION_XLATE						\
	int err;							\
	offset_vec[i] = ref->TranslateIndex( offset_vec[i], &err );	\
	if ( err )							\
		{							\
		PICK_CLEANUP						\
		free_memory( offset_vec );				\
		error->Report( "index number ", j, " (=",cur,		\
			") is out of range. Sub-vector reference may be invalid" );\
		return;							\
		}


PICKASSIGN_ACTION(TYPE_BOOL,glish_bool,BoolPtr,CoerceToBoolArray,,
	PICKASSIGN_ACTION_XLATE)
PICKASSIGN_ACTION(TYPE_BYTE,byte,BytePtr,CoerceToByteArray,,
	PICKASSIGN_ACTION_XLATE)
PICKASSIGN_ACTION(TYPE_SHORT,short,ShortPtr,CoerceToShortArray,,
	PICKASSIGN_ACTION_XLATE)
PICKASSIGN_ACTION(TYPE_INT,int,IntPtr,CoerceToIntArray,,
	PICKASSIGN_ACTION_XLATE)
PICKASSIGN_ACTION(TYPE_FLOAT,float,FloatPtr,CoerceToFloatArray,,
	PICKASSIGN_ACTION_XLATE)
PICKASSIGN_ACTION(TYPE_DOUBLE,double,DoublePtr,CoerceToDoubleArray,,
	PICKASSIGN_ACTION_XLATE)
PICKASSIGN_ACTION(TYPE_COMPLEX,complex,ComplexPtr,CoerceToComplexArray,,
	PICKASSIGN_ACTION_XLATE)
PICKASSIGN_ACTION(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,CoerceToDcomplexArray,,
	PICKASSIGN_ACTION_XLATE)
PICKASSIGN_ACTION(TYPE_STRING,charptr,StringPtr,CoerceToStringArray,strdup,
	PICKASSIGN_ACTION_XLATE)

				default:
					fatal->Report(
					"bad type in Value::PickAssign" );
				}
			}
			break;

		default:
			fatal->Report( "bad type in Value::PickAssign" );
		}

	PICK_CLEANUP
	return;
	}


Value* Value::SubRef( const Value* index )
	{
	if ( VecRefDeref()->Type() == TYPE_RECORD )
		{
		if ( index->Type() == TYPE_STRING )
			return GetOrCreateRecordElement( index );

		Value* ret = NthField( index->IntVal() );
		if ( ! ret )
			return Fail( "record index (=", index->IntVal(),
				") out of range (> ", Length(), ")" );

		return ret;
		}

	int indices_are_copy;
	int num_indices;
	int* indices = GenerateIndices( index, num_indices, indices_are_copy );

	if ( indices )
		return TrueArrayRef( indices, num_indices, indices_are_copy );
	else
		return error_value();
	}

Value* Value::SubRef( const_value_list *args_val )
	{
	if ( ! IsNumeric() && VecRefDeref()->Type() != TYPE_STRING )
		return Fail( "invalid type in subreference operation:",
				this );

	// Collect attributes.
	const attributeptr ptr = AttributePtr();
	const Value* shape_val = ptr ? (*ptr)["shape"] : 0;
	if ( ! shape_val || ! shape_val->IsNumeric() )
		{
		warn->Report( "invalid or non-existant \"shape\" attribute" );

		const Value* arg = (*args_val)[0];
		if ( arg )
			return SubRef( arg );
		else
			return Fail( "invalid missing argument" );
		}

	int shape_len = shape_val->Length();
	int args_len = (*args_val).length();
	if ( shape_len != args_len )
		return Fail( "invalid number of indexes for:", this );

	int shape_is_copy;
	int* shape = shape_val->CoerceToIntArray( shape_is_copy, shape_len );
	Value* op_val = (*ptr)["op[]"];

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
				SUBOP_CLEANUP_1
				return Fail( "index #", i+1, "into", this,
						"is not numeric");
				}

			if ( arg->Length() > max_len )
				max_len = arg->Length();

			if ( max_len == 1 )
				{
				int ind = arg->IntVal();
				if ( ind < 1 || ind > shape[i] )
					{
					SUBOP_CLEANUP_1
					return Fail( "index #", i+1, "into",
						this, "is out of range");
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
		SUBOP_CLEANUP_1
		return Fail( "\"::shape\"/length mismatch" );
		}

	if ( max_len == 1 ) 
		{
		SUBOP_CLEANUP_1
		++offset;
		return create_value( (Value*) this, &offset, 1, VAL_REF );
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
					return Fail( "index #", i+1, ",",
							j+1, " into ", this, 
							"is out of range.");
				else
					return Fail( "index #", i+1, "into",
						this, "is out of range.");

				}
			}
		}

	// Loop through filling resultant vector.

	int* ret = (int*) alloc_memory( sizeof(int)*vecsize );
	for ( int v = 0; v < vecsize; ++v )
		{
		// Calculate offset.
		offset = 0;
		for ( LOOPDECL i = 0; i < shape_len; ++i )
			offset += factor[i] * (index[i][cur[i]] - 1);

		// Set value.
		ret[v] = offset + 1;

		// Advance counters.
		for ( LOOPDECL i = 0; i < shape_len; ++i )
			if ( ++cur[i] < len[i] )
				break;
			else
				cur[i] = 0;
		}

	Value* result = create_value( (Value*) this, ret, vecsize, VAL_REF, 1 );
	if ( ! is_element )
		{
		int z = 0;
		for ( int x = 0; x < shape_len; ++x )
			if ( len[x] > 1 )
				len[z++] = len[x];

		Value* len_v = create_value( len, z );
		result->AssignAttribute( "shape", len_v );

		if ( op_val )
			result->AssignAttribute( "op[]", op_val );
		}
	else
		free_memory( len );

	SUBOP_CLEANUP_2(shape_len)
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

char* Value::NewFieldName()
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		return 0;

	static int counter = 0;

	char buf[128];
	do
		sprintf( buf, "*%d", ++counter );
	while ( Field( buf ) );

	return strdup( buf );
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

Value* Value::ArrayRef( int* indices, int num_indices )
		const
	{

	if ( IsRef() )
		return Deref()->ArrayRef( indices, num_indices );

	if ( Type() == TYPE_RECORD )
		return RecordSlice( indices, num_indices );

	for ( int i = 0; i < num_indices; ++i )
		if ( indices[i] < 1 || indices[i] > kernel.Length() )
			return Fail( "index (=", indices[i],
				") out of range, array length =", kernel.Length() );

	switch ( Type() )
		{

#define ARRAY_REF_ACTION(tag,type,accessor,copy_func,OFFSET,XLATE)	\
	case tag:							\
		{							\
		type* source_ptr = accessor(0);				\
		type* new_values = (type*) alloc_memory(		\
					sizeof(type)*num_indices );	\
									\
		for ( LOOPDECL i = 0; i < num_indices; ++i )		\
			{						\
			XLATE						\
			new_values[i] = copy_func(source_ptr[OFFSET]);	\
			}						\
		return create_value( new_values, num_indices );		\
		}

ARRAY_REF_ACTION(TYPE_BOOL,glish_bool,BoolPtr,,indices[i]-1,)
ARRAY_REF_ACTION(TYPE_BYTE,byte,BytePtr,,indices[i]-1,)
ARRAY_REF_ACTION(TYPE_SHORT,short,ShortPtr,,indices[i]-1,)
ARRAY_REF_ACTION(TYPE_INT,int,IntPtr,,indices[i]-1,)
ARRAY_REF_ACTION(TYPE_FLOAT,float,FloatPtr,,indices[i]-1,)
ARRAY_REF_ACTION(TYPE_DOUBLE,double,DoublePtr,,indices[i]-1,)
ARRAY_REF_ACTION(TYPE_COMPLEX,complex,ComplexPtr,,indices[i]-1,)
ARRAY_REF_ACTION(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,,indices[i]-1,)
ARRAY_REF_ACTION(TYPE_STRING,charptr,StringPtr,strdup,indices[i]-1,)

		case TYPE_SUBVEC_REF:
			{
			VecRef *ref = VecRefPtr();
			switch ( ref->Type() )
				{
#define ARRAY_REF_ACTION_XLATE(EXTRA_ERROR)		\
	int err;					\
	int off = ref->TranslateIndex( indices[i]-1, &err );\
	if ( err )					\
		{					\
		EXTRA_ERROR				\
		free_memory( new_values );		\
		return Fail("invalid index (=",indices[i],"), sub-vector reference may be bad");\
		}

ARRAY_REF_ACTION(TYPE_BOOL,glish_bool,BoolPtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_BYTE,byte,BytePtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_SHORT,short,ShortPtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_INT,int,IntPtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_FLOAT,float,FloatPtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_DOUBLE,double,DoublePtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_COMPLEX,complex,ComplexPtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_STRING,charptr,StringPtr,strdup,off,ARRAY_REF_ACTION_XLATE(for(int X=0; X<i; X++) free_memory( (void*) new_values[X] );))

		default:
			fatal->Report( "bad type in Value::ArrayRef()" );
			return 0;
		}
	}
			break;

		default:
			fatal->Report( "bad type in Value::ArrayRef()" );
			return 0;
		}

	return 0;
	}

Value* Value::TrueArrayRef( int* indices, int num_indices, int take_indices ) const
	{
	if ( IsRef() )
		return Deref()->TrueArrayRef( indices, num_indices );

	if ( VecRefDeref()->Type() == TYPE_RECORD )
		return RecordSlice( indices, num_indices );

	for ( int i = 0; i < num_indices; ++i )
		if ( indices[i] < 1 || indices[i] > kernel.Length() )
			{
			if ( take_indices )
				free_memory( indices );

			return Fail( "index (=", indices[i],
				") out of range, array length =", kernel.Length() );
			}

	return create_value( (Value*) this, indices, num_indices, VAL_REF, take_indices );
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

	recordptr lhs_rptr = RecordPtr();
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
			AssignRecordElement( NewFieldName(), value );

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
			AssignRecordElement( NewFieldName(), val );
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
	Value* op_val = (*ptr)["op[]"];

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
			int theLen = theVal->Length();

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

int Value::IndexRange( int* indices, int num_indices, int& max_index ) const
	{
	max_index = 0;

	for ( int i = 0; i < num_indices; ++i )
		{
		if ( indices[i] < 1 )
			{
			error->Report( "index (=", indices[i],
				") out of range (< 1)" );
			return 0;
			}

		else if ( indices[i] > max_index )
			max_index = indices[i];
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
		int is_copy;						\
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
	glish_type type = Type();
	int length = kernel.Length();

	if ( IsVecRef() )
		{
		Polymorph( new_type );
		return;
		}

	VecRef* ref = VecRefPtr();
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
		case TYPE_RECORD:
			error->Report( "cannot increase array of",
					type_names[Type()], "via assignment" );
			return 0;

		default:
			fatal->Report( "bad type in Value::Grow()" );
		}

	return 1;
	}


int Value::DescribeSelf( OStream& s, charptr prefix ) const
	{
	if ( prefix ) s << prefix;
	if ( IsRef() )
		{
		if ( IsConst() )
			s << "const ";
		else
			s << "ref ";

		RefPtr()->DescribeSelf( s );
		}
	else
		{
		char* desc = StringVal( ' ', PrintLimit() , 1 );
		s << desc;
		free_memory( desc );
		}
	return 1;
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


Value* empty_value()
	{
	int i = 0;
	return create_value( &i, 0, COPY_ARRAY );
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

static charptr* make_string_array( char* heap, int len, int& result_len )
	{
	result_len = 0;
	char* heap_ptr = heap;
	for ( ; heap_ptr < &heap[len]; ++heap_ptr )
		if ( *heap_ptr == '\0' )
			++result_len;

	charptr* result = (charptr*) alloc_memory( sizeof(charptr)*result_len );

	heap_ptr = heap;
	for ( int i = 0; i < result_len; ++i )
		{
		result[i] = strdup( heap_ptr );
		heap_ptr += strlen( heap_ptr ) + 1;
		}

	return result;
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

void delete_list( del_list* dlist )
	{
	if ( dlist )
		loop_over_list( *dlist, i )
			Unref( (*dlist)[i] );
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
		successful = 0;
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

static char *format_error_message( const Value *val, char sep, 
				   unsigned int max_elements,
				   int useAttributes )
	{
	if ( ! val || val->Type() != TYPE_FAIL )
		return 0;

	const attributeptr attr = val->AttributePtr();

	if ( ! attr )
		return strdup( "<fail>" );

	const Value *value1;
	int len = 0;
	char *intro = "<fail>";
	char *msg[9];
	int cnt = 7;

	msg[0] = msg[1] = msg[2] = msg[3] = msg[4] = msg[5] = msg[6] = msg[7] = msg[8] = 0;
	if ( (value1 = (*attr)["message"]) )
		{
		msg[0] = value1->StringVal(sep, max_elements, useAttributes );
		cnt += strlen(msg[0]);
		}

	if ( (value1 = (*attr)["file"]) && value1->Type() == TYPE_STRING )
		{
		msg[1] = value1->StringVal( sep, max_elements, useAttributes );
		cnt += strlen(msg[1]);
		}

	if ( (value1 = (*attr)["line"]) && value1->IsNumeric() )
		{
		int l = value1->IntVal();
		msg[2] = (char*) alloc_memory( sizeof(char)*48 );
		sprintf(msg[2],"Line: %d",l);
		cnt += strlen(msg[2]);
		}

	if ( (value1 = (*attr)["stack"]) &&
	     value1->Type() == TYPE_STRING &&
	     (len = value1->Length()) )
		{
		charptr *stack = value1->StringPtr(0);
		int l = strlen(stack[--len]);
		msg[3] = (char*) alloc_memory( sizeof(char)*(l+10) );
		strcpy(msg[3],"Stack:\t");
		strcat(msg[3],stack[len]);
		strcat(msg[3],"()");
		cnt += l + 13;
		for (int x=4; x < 8 && len; x++)
			{
			l = strlen(stack[--len]);
			msg[x] = (char*) alloc_memory( sizeof(char)*(l+4) );
			strcpy(msg[x],"\t");
			strcat(msg[x],stack[len]);
			strcat(msg[x],"()");
			cnt += l + 4;
			}
		if ( len )
			{
			msg[8] = "\t...";
			cnt += 5;
			}
		}

	// Add some for line feeds etc...
	char *result = (char*) alloc_memory( sizeof(char)*(cnt + 24) );

	strcpy(result, intro);
	if ( msg[0] ) { strcat(result, ": "); strcat(result, msg[0]); }
	if ( msg[1] ) { strcat(result, "\n\tFile: "); strcat(result, msg[1]); }
	if ( msg[2] )
		{
		if ( msg[1] ) strcat( result, ", ");
		else strcat( result, "\n\t");
		strcat( result, msg[2] );
		}
	for ( int x=3; x <= 8; x++ )
		if ( msg[x] ) { strcat(result, "\n\t"); strcat(result, msg[x]); }

	for ( int i=0; i < 8; i++ )
		if ( msg[i] ) free_memory( msg[i] );

	return result;
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
