// $Header$

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"

#include <string.h>
#include <stream.h>
#include <stdlib.h>

#include "Sds/sdsgen.h"
#include "Glish/Value.h"

#include "glish_event.h"
#include "Reporter.h"
#include "config.h"

int num_Values_created = 0;
int num_Values_deleted = 0;

const char* type_names[NUM_GLISH_TYPES] =
	{
	"error", "ref", "subref",
	"boolean", "byte", "short", "integer",
	"float", "double", "string", "agent", "function", "record",
	"complex", "dcomplex", "opaque",
	};

const Value* false_value;

class SDS_ValueManager : public GlishObject {
    public:
	SDS_ValueManager( int sds_index )	{ sds = sds_index; }
	~SDS_ValueManager()
		{ 
		sds_destroy( sds );
		sds_discard( sds );
		}

    protected:
	int sds;
	};


class DelObj : public GlishObject {
public:
	DelObj( GlishObject* arg_obj )	{ obj = arg_obj; ptr = 0; }
	DelObj( void* arg_ptr )		{ obj = 0; ptr = arg_ptr; }

	~DelObj();

protected:
	GlishObject* obj;
	void* ptr;
};

DelObj::~DelObj()
	{
	Unref( obj );
	delete ptr;
	}


#define DEFINE_SINGLETON_CONSTRUCTOR(constructor_type)			\
Value::Value( constructor_type value )					\
	{								\
	DIAG4( (void*) this, "Value(", #constructor_type,")" )		\
	InitValue();							\
	kernel.SetArray( &value, 1, 1 );				\
	}

#define DEFINE_ARRAY_CONSTRUCTOR(constructor_type)			\
Value::Value( constructor_type value[], int len, array_storage_type s ) \
	{								\
	DIAG4( (void*) this, "Value(", #constructor_type, "[] )" )	\
	InitValue();							\
	kernel.SetArray( value, len, s == COPY_ARRAY || s == PRESERVE_ARRAY ); \
	}

#define DEFINE_ARRAY_REF_CONSTRUCTOR(constructor_type)			\
Value::Value( constructor_type& value_ref )				\
	{								\
	DIAG4( (void*) this, "Value(", #constructor_type, "& )" )	\
	InitValue();							\
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
	InitValue();
	kernel.SetRecord( value );
	}


Value::Value( SDS_Index& value )
	{
	DIAG2( (void*) this, "Value( SDS_Index & )" )
	InitValue();
	SetValue( value );
	}


Value::Value( Value* ref_value, value_type val_type )
	{
	DIAG2( (void*) this, "Value( Value*, value_type )" )
	InitValue();

	if ( val_type != VAL_CONST && val_type != VAL_REF )
		fatal->Report( "bad value_type in Value::Value" );

	ref_value = ref_value->Deref();

	Ref( ref_value );
	kernel.SetValue(ref_value);

	if ( val_type == VAL_CONST )
		kernel.MakeConst();

	attributes = ref_value->CopyAttributePtr();
	}

Value::Value( Value* ref_value, int index[], int num_elements,
		value_type val_type )
	{
	DIAG2( (void*) this, "Value( Value*, int[], int, value_type )" )
	InitValue();
	SetValue( ref_value, index, num_elements, val_type );
	attributes = ref_value->CopyAttributePtr();
	}


void Value::TakeValue( Value* new_value )
	{
	new_value = new_value->Deref();

	if ( new_value == this )
		{
		error->Report( "reference loop created" );
		return;
		}

	DeleteValue();
	kernel = new_value->kernel;

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


void Value::SetValue( SDS_Index& value )
	{
	kernel.SetOpaque( int_to_void(value.Index()) );
	}


void Value::SetValue( Value* ref_value, int index[], int num_elements,
			value_type val_type )
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
			kernel.SetVecRef( new VecRef( ref_value, index,
						num_elements, max_index ) );
			break;

		default:
			fatal->Report( "bad Value in Value::Value" );
		}

	if ( val_type == VAL_CONST )
		kernel.MakeConst( );

	}


void Value::InitValue()
	{
	description = 0;
	value_manager = 0;
	attributes = 0;
	++num_Values_created;
	}


void Value::DeleteValue()
	{
	if ( IsRef() )
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
	Unref( attributes );
	attributes = 0;
	}


void Value::DeleteAttribute( const Value* index )
	{
	char* index_string = index->StringVal();
	DeleteAttribute( index_string );
	delete index_string;
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
		case TYPE_OPAQUE:
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
val_type Value::name( int n ) const					\
	{								\
	if ( IsRef() )							\
		return Deref()->name( n );				\
									\
	if ( kernel.Length() < 1 )					\
		{							\
		error->Report( "empty array converted to ", type_name );\
		return zero;						\
		}							\
									\
	if ( n < 1 || n > kernel.Length() )				\
		{							\
		error->Report( "in conversion to ", type_name, " index (=", n,\
				") out of bounds, length =", kernel.Length() );	\
		return zero;						\
		}							\
									\
	switch ( Type() )						\
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
				text_func( StringPtr(0)[n - 1], successful ) );\
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
		case TYPE_OPAQUE:					\
			error->Report( "bad type", type_names[Type()],	\
				"converted to ", type_name, ":", this );\
			return zero;					\
									\
		case TYPE_SUBVEC_REF:					\
			{						\
			VecRef* ref = VecRefPtr();			\
			int err;					\
			int off = ref->TranslateIndex( n-1, &err );	\
			if ( err )					\
				{					\
				error->Report( "bad sub-vector subscript" );\
				return zero;				\
			}						\
			return ref->Val()->name( off );			\
			}						\
									\
		default:						\
			fatal->Report( "bad type in Value::XXX_VAL()" );\
			return zero;					\
		}							\
	}

XXX_VAL(BoolVal, glish_bool, .r || ptr.i, ? glish_true : glish_false, text_to_integer, "bool", glish_false)
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
	unsigned int limit = 0;
	int tmp = 0;
	const Value *val;
	const Value *precv;
	const Value *printv;
	static char prec[64];
	if ( attr && (val = (*attr)["print"]) && val->Type() == TYPE_RECORD &&
			val->HasRecordElement( "precision" ) &&
			(precv=val->ExistingRecordElement( "precision" )) &&
			precv != false_value && precv->IsNumeric() &&
			(tmp = precv->IntVal()) > 0 )
		limit = tmp;
	else if ( (val = lookup_sequencer_value( "system" )) && 
			val->Type() == TYPE_RECORD &&
			val->HasRecordElement( "print" ) &&
			(printv = val->ExistingRecordElement( "print" )) &&
			printv != false_value &&
			printv->Type() == TYPE_RECORD &&
			printv->HasRecordElement( "precision" ) &&
			(precv = printv->ExistingRecordElement("precision")) &&
			precv != false_value && precv->IsNumeric() &&
			(tmp = precv->IntVal()) > 0)
		limit = tmp;

	if ( limit )
		sprintf(prec,"%%.%df",limit);

	return limit ? prec : default_fmt;
	}

char* Value::StringVal( char sep, unsigned int max_elements,
			int useAttributes ) const
	{
	glish_type type = Type();
	int length = kernel.Length();

	if ( IsRef() )
		return Deref()->StringVal( sep, max_elements, useAttributes );
	if ( type == TYPE_RECORD )
		return RecordStringVal();
	if ( type == TYPE_AGENT )
		return strdup( "<agent>" );
	if ( type == TYPE_FUNC )
		return strdup( "<function>" );
	if ( type == TYPE_OPAQUE )
		return strdup( "<opaque>" );
	if ( length == 0 )
		return strdup( "" );

	unsigned int buf_size;

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

	char* buf = new char[buf_size];
	if ( ! buf )
		fatal->Report( "out of memory in Value::StringVal()" );

	char* buf_ptr = buf;

	if ( type != TYPE_STRING && length > 1 )
		{
		// Insert []'s around value.
		*buf_ptr++ = '[';
		}

	glish_bool* bool_ptr;
	byte* byte_ptr;
	short* short_ptr;
	int* int_ptr;
	float* float_ptr;
	double* double_ptr;
	complex* complex_ptr;
	dcomplex* dcomplex_ptr;
	charptr* string_ptr;
	const char *flt_prec;

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
			int err;					\
			int index = ref->TranslateIndex( indx, &err );	\
			if ( err )					\
				{					\
				error->Report( "invalid sub-vector" );	\
				delete alloced;				\
				return strdup( "error" );		\
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
	const Value* shape_val;
	int shape_len;

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
	static int* column_width = new int[column_width_len];

	// Arrays for iterating through the matrix.
	static int indices_len = 32;
	static int* indices = new int[indices_len];
	static int* factor = new int[indices_len];

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
		delete buf;
		if ( shape_is_copy )
			delete shape;
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
		delete shape;

	append_buf( buf, buf_ptr, buf_size, "]" );

	return buf;
	}

unsigned int Value::PrintLimit( ) const
	{
	unsigned int limit = 0;
	int tmp = 0;
	const attributeptr attr = AttributePtr();
	const Value *val;
	const Value *printv;
	const Value *limitv;
	if ( attr && (val = (*attr)["print"]) && val->Type() == TYPE_RECORD &&
	     		val->HasRecordElement( "limit" ) && 
			(limitv  = val->ExistingRecordElement( "limit" )) &&
			limitv != false_value && limitv->IsNumeric() &&
			(tmp = limitv->IntVal()) > 0 )
		limit = tmp;
	else if ( (val = lookup_sequencer_value( "system" )) &&
			val->Type() == TYPE_RECORD &&
			val->HasRecordElement( "print" ) && 
			(printv = val->ExistingRecordElement( "print" )) &&
			printv != false_value &&
			printv->Type() == TYPE_RECORD &&
			printv->HasRecordElement( "limit" ) &&
			(limitv = printv->ExistingRecordElement("limit")) &&
			limitv != false_value && limitv->IsNumeric() &&
			(tmp = limitv->IntVal()) > 0 )
		limit = tmp;

	return limit;
	}

char* Value::RecordStringVal() const
	{
	if ( VecRefDeref()->Type() != TYPE_RECORD )
		fatal->Report( "non-record type in Value::RecordStringVal()" );

	recordptr rptr = RecordPtr(0);
	int len = rptr->Length();

	if ( len == 0 )
		return strdup( "[=]" );

	const char** key_strs = new const char*[len];
	char** element_strs = new char*[len];
	int total_len = 0;

	for ( int i = 0; i < len; ++i )
		{
		Value* nth_val = rptr->NthEntry( i, key_strs[i] );

		if ( ! nth_val )
			fatal->Report(
				"bad record in Value::RecordStringVal()" );

		element_strs[i] = nth_val->StringVal();
		total_len += strlen( element_strs[i] ) + strlen( key_strs[i] );
		}

	// We generate a result of the form [key1=val1, key2=val2, ...],
	// so in addition to room for the keys and values we need 3 extra
	// characters per element (for the '=', ',', and ' '), 2 more for
	// the []'s (we could steal these from the last element since it
	// doesn't have a ", " at the end of it, but that seems a bit
	// evil), and 1 more for the end-of-string.
	char* result = new char[total_len + 3 * len + 3];

	strcpy( result, "[" );

	for ( LOOPDECL i = 0; i < len; ++i )
		{
		sprintf( &result[strlen( result )], "%s=%s, ",
			 key_strs[i], element_strs[i] );
		delete element_strs[i];
		}

	// Now add the final ']', taking care to wipe out the trailing
	// ", ".
	strcpy( &result[strlen( result ) - 2], "]" );

	return result;
	}

int Value::SDS_IndexVal() const
	{
	if ( Type() != TYPE_OPAQUE )
		{
		error->Report( this, " is not an opaque value" );
		return SDS_NO_SUCH_SDS;
		}

	return void_to_int( kernel.GetOpaque() );
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
	int length = kernel.Length();				\
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
		result = new ctype[size];				\
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


#define COERCE_ACTION(tag,rhs_type,rhs_elm,lhs_type,accessor,OFFSET,XLATE)\
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

#define COERCE_ACTIONS(type,error_msg)				\
COERCE_ACTION(TYPE_BOOL,glish_bool,,type,BoolPtr,j,)		\
COERCE_ACTION(TYPE_BYTE,byte,,type,BytePtr,j,)			\
COERCE_ACTION(TYPE_SHORT,short,,type,ShortPtr,j,)		\
COERCE_ACTION(TYPE_INT,int,,type,IntPtr,j,)			\
COERCE_ACTION(TYPE_FLOAT,float,,type,FloatPtr,j,)		\
COERCE_ACTION(TYPE_DOUBLE,double,,type,DoublePtr,j,)		\
COERCE_ACTION(TYPE_COMPLEX,complex,.r,type,ComplexPtr,j,)	\
COERCE_ACTION(TYPE_DCOMPLEX,dcomplex,.r,type,DcomplexPtr,j,)	\
								\
		case TYPE_SUBVEC_REF:				\
			{					\
			VecRef *ref = VecRefPtr();		\
			switch ( ref->Type() )			\
				{				\
								\
COERCE_ACTION(TYPE_BOOL,glish_bool,,type,BoolPtr,off,COERCE_ACTION_XLATE)\
COERCE_ACTION(TYPE_BYTE,byte,,type,BytePtr,off,COERCE_ACTION_XLATE)	\
COERCE_ACTION(TYPE_SHORT,short,,type,ShortPtr,off,COERCE_ACTION_XLATE)\
COERCE_ACTION(TYPE_INT,int,,type,IntPtr,off,COERCE_ACTION_XLATE)	\
COERCE_ACTION(TYPE_FLOAT,float,,type,FloatPtr,off,COERCE_ACTION_XLATE)\
COERCE_ACTION(TYPE_DOUBLE,double,,type,DoublePtr,off,COERCE_ACTION_XLATE)\
COERCE_ACTION(TYPE_COMPLEX,complex,.r,type,ComplexPtr,off,COERCE_ACTION_XLATE)\
COERCE_ACTION(TYPE_DCOMPLEX,dcomplex,.r,type,DcomplexPtr,off,COERCE_ACTION_XLATE)\
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
		COERCE_ACTIONS(byte,"CoerceToByteArray()")
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
		COERCE_ACTIONS(short,"CoerceToShortArray()")

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
		COERCE_ACTIONS(int,"CoerceToIntArray()")
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
		COERCE_ACTIONS(float,"CoerceToFloatArray()")
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
		COERCE_ACTIONS(double,"CoerceToDoubleArray()")
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
		result = new charptr[size];

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
			delete indices;

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
		delete shape;						\
	delete factor;

#define SUBOP_CLEANUP_2(length)						\
	{								\
	SUBOP_CLEANUP_1							\
	for ( int x = 0; x < length; ++x )				\
		if ( index_is_copy[x] )					\
			delete index[x];				\
									\
	delete index;							\
	delete index_is_copy;						\
	delete cur;							\
	}

#define SUBOP_ABORT(length,retval)				\
	SUBOP_CLEANUP_2(length)					\
	delete len;						\
	return retval;

	int length = kernel.Length();

	if ( ! IsNumeric() && VecRefDeref()->Type() != TYPE_STRING )
		{
		error->Report( "invalid type in n-D array operation:",
				this );
		return error_value();
		}

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
		{
		error->Report( "invalid number of indexes for:", this );
		return error_value();
		}

	int shape_is_copy;
	int* shape = shape_val->CoerceToIntArray( shape_is_copy, shape_len );
	Value* op_val = (*ptr)["op[]"];

	int* factor = new int[shape_len];
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
				return error_value();
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
					return error_value();
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
		return error_value();
		}

	if ( max_len == 1 ) 
		{
		SUBOP_CLEANUP_1
		++offset;
		// Should separate ArrayRef to get a single value??
		return ArrayRef( &offset, 1 );
		}

	int* index_is_copy = new int[shape_len];
	int** index = new int*[shape_len];
	int* cur = new int[shape_len];
	int* len = new int[shape_len];
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
			index[i] = new int[len[i]];
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

				if ( len[i] > 1 )
					error->Report( "index #", i+1, ",",
							j+1, " into ", this, 
							"is out of range.");
				else
					error->Report( "index #", i+1, "into",
						this, "is out of range.");

				SUBOP_ABORT(i, error_value())
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
		type* ret = new type[vecsize];					\
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
			delete len;						\
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
		delete ret;				\
		SUBOP_CLEANUP_2(shape_len)		\
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
	theLen, off,strdup,SUBSCRIPT_OP_ACTION_XLATE(for(int X=0;X<v;X++) delete (char *) ret[X];))

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
		delete shape;			\
	if ( ishape_is_copy )			\
		delete ishape;			\
	if ( indx_is_copy )			\
		delete indx;			\
	delete factor;

#define PICK_INITIALIZE(error_return,SHORT)				\
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
		if ( ishape_val )					\
			error->Report("error in the array \"::shape\": ",\
				ishape_val );				\
		else							\
			error->Report( "no \"::shape\" for ", index,	\
				" but the array has \"::shape\"" );	\
		return error_return;					\
		}							\
	if ( ! shape_len )						\
		{							\
		if ( shape_val )					\
			error->Report("error in the array \"::shape\": ",\
				shape_val );				\
		else							\
			error->Report( "no \"::shape\" for ", this,	\
				" but the index has \"::shape\"" );	\
		return error_return;					\
		}							\
									\
	if ( ishape_len > 2 )						\
		{							\
		error->Report("invalid index of dimension (=", ishape_len, \
				") greater than 2");			\
		return error_return;					\
		}							\
									\
	int shape_is_copy = 0;						\
	int ishape_is_copy = 0;						\
	int indx_is_copy = 0;						\
	int* shape = shape_val->CoerceToIntArray( shape_is_copy, shape_len );\
	int* ishape =							\
		ishape_val->CoerceToIntArray( ishape_is_copy, ishape_len );\
	int ilen = index->Length();					\
	int len = Length();						\
	int* factor = new int[shape_len];				\
	int offset = 1;							\
	int* indx = index->CoerceToIntArray( indx_is_copy, ilen );	\
	Value* result = 0;						\
									\
	if ( ishape[1] != shape_len )					\
		{							\
		PICK_CLEANUP						\
		error->Report( "wrong number of columns in index (=",	\
			ishape[1], ") expected ", shape_len );		\
		return error_return;					\
		}							\
	if ( ilen < ishape[0] * ishape[1] )				\
		{							\
		PICK_CLEANUP						\
			error->Report( "Index \"::shape\"/length mismatch" );\
		return error_return;					\
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
		error->Report("Array \"::shape\"/length mismatch");	\
		return error_return;					\
		}

	PICK_INITIALIZE(error_value(),return this->operator[]( index );)

	switch ( Type() )
		{
#define PICK_ACTION_CLEANUP for(int X=0;X<i;X++) delete (char *) ret[X];
#define PICK_ACTION(tag,type,accessor,OFFSET,COPY_FUNC,XLATE,CLEANUP)	\
	case tag:							\
		{							\
		type* ptr = accessor();					\
		type* ret = new type[ishape[0]];			\
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
					delete ret;			\
					error->Report( "index number ", j,\
					" (=", cur, ") is out of range" );\
					return error_value();		\
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
		delete ret;						\
		error->Report( "index number ", j, " (=",cur,		\
			") is out of range. Sub-vector reference may be invalid" );\
		return error_value();					\
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
		{
		error->Report( "non-numeric type in subreference operation:",
				this );
		return error_value();
		}

	PICK_INITIALIZE(error_value(),return this->operator[]( index );)

	int* ret = new int[ishape[0]];
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
				delete ret;
				error->Report( "index number ", j, " (=",cur,
						") is out of range" );
				return error_value();
				}
			offset += factor[j] * (cur-1);
			}
		ret[i] = offset + 1;
		}

	result = create_value((Value*)this,ret,ishape[0],VAL_REF);

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

	PICK_INITIALIZE(,PICKASSIGN_SHORT)

	switch ( Type() )
		{
#define PICKASSIGN_ACTION(tag,type,to_accessor,from_accessor,COPY_FUNC,XLATE)\
	case tag:							\
		{							\
		int cur = 0;						\
		int* offset_vec = new int[ishape[0]];			\
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
					delete offset_vec;		\
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
		delete offset_vec;					\
		if ( is_copy )						\
			delete vec;					\
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
		delete offset_vec;					\
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
			{
			error->Report( "record index (=", index->IntVal(),
				") out of range (> ", Length(), ")" );
			return error_value();
			}

		return ret;
		}

	int indices_are_copy;
	int num_indices;
	int* indices = GenerateIndices( index, num_indices, indices_are_copy );

	if ( indices )
		{
		Value* result = TrueArrayRef( indices, num_indices );

		if ( indices_are_copy )
			delete indices;

		return result;
		}

	else
		return error_value();
	}

Value* Value::SubRef( const_value_list *args_val )
	{
	if ( ! IsNumeric() && VecRefDeref()->Type() != TYPE_STRING )
		{
		error->Report( "invalid type in subreference operation:",
				this );
		return error_value();
		}

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
			{
			error->Report( "invalid missing argument" );
			return error_value();
			}
		}

	int shape_len = shape_val->Length();
	int args_len = (*args_val).length();
	if ( shape_len != args_len )
		{
		error->Report( "invalid number of indexes for:", this );
		return error_value();
		}

	int shape_is_copy;
	int* shape = shape_val->CoerceToIntArray( shape_is_copy, shape_len );
	Value* op_val = (*ptr)["op[]"];

	int* factor = new int[shape_len];
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
				return error_value();
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
					return error_value();
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
		return error_value();
		}

	if ( max_len == 1 ) 
		{
		SUBOP_CLEANUP_1
		++offset;
		return create_value( (Value*) this, &offset, 1, VAL_REF );
		}

	int* index_is_copy = new int[shape_len];
	int** index = new int*[shape_len];
	int* cur = new int[shape_len];
	int* len = new int[shape_len];
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
			index[i] = new int[len[i]];
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

				if ( len[i] > 1 )
					error->Report( "index #", i+1, ",",
							j+1, " into ", this, 
							"is out of range.");
				else
					error->Report( "index #", i+1, "into",
						this, "is out of range.");

				SUBOP_ABORT(i, error_value())
				}
			}
		}

	// Loop through filling resultant vector.

	int* ret = new int[vecsize];
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

	Value* result = create_value( (Value*) this, ret, vecsize, VAL_REF );
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
		delete len;

	SUBOP_CLEANUP_2(shape_len)
	return result;
	}


Value* Value::RecordRef( const Value* index ) const
	{
	if ( Type() != TYPE_RECORD )
		{
		error->Report( this, "is not a record" );
		return error_value();
		}

	if ( index->Type() != TYPE_STRING )
		{
		error->Report( "non-string index in record reference:", index );
		return error_value();
		}

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
	delete index_string;

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
	delete index_string;

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
	delete index_string;

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
int Value::FieldVal( const char* field, type& val, int num )		\
	{								\
	Value* result = Field( field, tag );				\
	if ( ! result )							\
		return 0;						\
									\
	val = result->valfunc( num );					\
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

		indices = new int[num_indices];
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
			{
			error->Report( "index (=", indices[i],
				") out of range, array length =", kernel.Length() );
			return error_value();
			}

	switch ( Type() )
		{

#define ARRAY_REF_ACTION(tag,type,accessor,copy_func,OFFSET,XLATE)	\
	case tag:							\
		{							\
		type* source_ptr = accessor(0);				\
		type* new_values = new type[num_indices];		\
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
		delete new_values;			\
		error->Report("invalid index (=",indices[i],"), sub-vector reference may be bad");\
		return error_value();			\
		}

ARRAY_REF_ACTION(TYPE_BOOL,glish_bool,BoolPtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_BYTE,byte,BytePtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_SHORT,short,ShortPtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_INT,int,IntPtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_FLOAT,float,FloatPtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_DOUBLE,double,DoublePtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_COMPLEX,complex,ComplexPtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,,off,ARRAY_REF_ACTION_XLATE(;))
ARRAY_REF_ACTION(TYPE_STRING,charptr,StringPtr,strdup,off,ARRAY_REF_ACTION_XLATE(for(int X=0; X<i; X++) delete (char *) new_values[X];))

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

Value* Value::TrueArrayRef( int* indices, int num_indices ) const
	{
	if ( IsRef() )
		return Deref()->TrueArrayRef( indices, num_indices );

	if ( VecRefDeref()->Type() == TYPE_RECORD )
		return RecordSlice( indices, num_indices );

	for ( int i = 0; i < num_indices; ++i )
		if ( indices[i] < 1 || indices[i] > kernel.Length() )
			{
			error->Report( "index (=", indices[i],
				") out of range, array length =", kernel.Length() );
			return error_value();
			}

	return create_value( (Value*) this, indices, num_indices, VAL_REF );
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
		{
		error->Report( "record index (=", max_index,
			") out of range (> ", rptr->Length(), ")" );
		return error_value();
		}

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
			delete indices;
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
			delete rhs_array;				\
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
	CoerceToStringArray, strdup, delete (char*) (lhs[indices[i]-1]);)

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
	CoerceToStringArray, strdup, delete (char*) (lhs[indices[i]-1]);)

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
			delete rhs_array;				\
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
	CoerceToStringArray,strdup, delete (char*) (lhs[i]);)

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
	CoerceToStringArray, strdup, delete (char*) (lhs[i]);)

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

	int* factor = new int[shape_len];
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


#define ASSIGN_ARY_ELEMENTS_ACTION_A_XLATE					\
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

	int* index_is_copy = new int[shape_len];
	int** index = new int*[shape_len];
	int* cur = new int[shape_len];
	int* len = new int[shape_len];
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
			index[i] = new int[len[i]];
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

				if ( len[i] > 1 )
					error->Report( "index #", i+1, ",",
							j+1, " into ", this, 
							"is out of range.");
				else
					error->Report( "index #", i+1, "into",
						this, "is out of range.");

				SUBOP_ABORT(i,)
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
			delete vec;					\
									\
		delete len;						\
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

#define ASSIGN_ARY_ELEMENTS_ACTION_XLATE					\
	int err;							\
	int off = ref->TranslateIndex( offset, &err );			\
	if ( err )							\
		{							\
		if ( is_copy )						\
			delete vec;					\
		delete len;						\
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

	glish_bool* result = new glish_bool[length];

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
		delete result;						\
		return;							\
		}

NOT_ACTION(TYPE_INT,int,,IntPtr,off,NOT_ACTION_XLATE)
NOT_ACTION(TYPE_FLOAT,float,,FloatPtr,off,NOT_ACTION_XLATE)
NOT_ACTION(TYPE_DOUBLE,double,,DoublePtr,off,NOT_ACTION_XLATE)
NOT_ACTION(TYPE_COMPLEX,complex,.r || ptr[off].i,ComplexPtr,off,NOT_ACTION_XLATE)
NOT_ACTION(TYPE_DCOMPLEX,dcomplex,.r || ptr[off].i,DcomplexPtr,off,NOT_ACTION_XLATE)
				default:
					error->Report( "bad type in Value::Not()" );
					delete result;
					return;
				}
			}
			break;


		default:
			error->Report( "bad type in Value::Not()" );
			delete result;
			return;
		}

	kernel.SetArray( result, length );
	}


void Value::AddToSds( int sds, del_list* dlist, const char* name,
			struct record_header* rh, int level ) const
	{
	glish_type type = Type();

	if ( IsVecRef() )
		{
		Value* copy = copy_value( this );
		dlist->append( new DelObj( copy ) );
		copy->AddToSds( sds, dlist, name, rh, level );
		return;
		}

	if ( IsRef() )
		{
		Deref()->AddToSds( sds, dlist, name, rh, level );
		return;
		}

	const attributeptr attr = AttributePtr();
	if ( type == TYPE_RECORD || attr )
		{
		if ( level > 1 )
			{
			if ( attr )
				{
				if ( name )
					{
					sds_begin_sub_record( rh,
						(char*) name );
					name = "val*";
					}
				else
					{
					if ( type == TYPE_RECORD )
						sds_begin_sub_record( rh, "" );
					else
						sds_begin_sub_record( rh,
							"val*" );
					}
				}
			else
				sds_begin_sub_record( rh, (char*) name );
			}

		else if ( level == 1 )
			{
			if ( attr )
				{
				if ( name )
					{
					rh = sds_begin_record( (char*) name );
					name = "val*";
					}
				else
					{
					if ( type == TYPE_RECORD )
						rh = sds_begin_record( "" );
					else
						rh = sds_begin_record( "val*" );
					}
				}
			else
				rh = sds_begin_record( (char*) name );
			}

		// Else we're doing a top-level record, leave rh nil.
		}

	if ( ! name )
		name = attr ? "val*" : "";

	switch ( type )
		{
#define ADD_TO_SDS_ACTION(tag,type,accessor,SDS_type)			\
	case tag:							\
		{							\
		type* array = accessor(0);				\
									\
		if ( rh )						\
			sds_record_entry( rh, SDS_type, Length(),	\
					(char*) array, (char*) name );	\
		else							\
			(void) sds_declare_structure( sds, (char*) array,	\
						   (char*) name, Length(),\
						   SDS_type );		\
		break;							\
		}

ADD_TO_SDS_ACTION(TYPE_BOOL,glish_bool,BoolPtr,SDS_INT)
ADD_TO_SDS_ACTION(TYPE_BYTE,byte,BytePtr,SDS_BYTE)
ADD_TO_SDS_ACTION(TYPE_SHORT,short,ShortPtr,SDS_SHORT)
ADD_TO_SDS_ACTION(TYPE_INT,int,IntPtr,SDS_INT)
ADD_TO_SDS_ACTION(TYPE_FLOAT,float,FloatPtr,SDS_FLOAT)
ADD_TO_SDS_ACTION(TYPE_DOUBLE,double,DoublePtr,SDS_DOUBLE)
ADD_TO_SDS_ACTION(TYPE_COMPLEX,complex,ComplexPtr,SDS_COMPLEX)
ADD_TO_SDS_ACTION(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,SDS_DOUBLE_COMPLEX)

		case TYPE_STRING:
			{
			int total_size = 0;
			charptr* strs = StringPtr(0);
			int length = kernel.Length();

			// First figure out how much total storage is needed
			// for all of the strings.

			for ( int i = 0; i < length; ++i )
				total_size += strlen( strs[i] ) + 1;

			// Create a suitable contiguous heap for holding all
			// the strings.
			char* heap = new char[total_size];

			char* heap_ptr = heap;
			for ( LOOPDECL i = 0; i < length; ++i )
				{
				strcpy( heap_ptr, strs[i] );
				heap_ptr += strlen( strs[i] ) + 1;
				}

			if ( rh )
				sds_record_entry( rh, SDS_STRING, total_size,
							heap, (char*) name );
			else
				(void) sds_declare_structure( sds, heap,
					(char*) name, total_size, SDS_STRING );

			dlist->append( new DelObj( (void*) heap ) );

			break;
			}

		case TYPE_AGENT:
			warn->Report( "skipping agent value \"", name,
					"\" in creation of dataset" );
			break;

		case TYPE_FUNC:
			warn->Report( "skipping function value \"", name,
					"\" in creation of dataset" );
			break;

		case TYPE_RECORD:
			{
			recordptr rptr = RecordPtr(0);

			for ( int i = 0; i < rptr->Length(); ++i )
				{
				const char* key;
				Value* member = rptr->NthEntry( i, key );

				member->AddToSds( sds, dlist, key, rh,
							level + 1 );
				}

			break;
			}

		case TYPE_OPAQUE:
			fatal->Report( "opaque value in Value::AddToSDS()" );
			break;

		default:
			fatal->Report( "bad type in Value::AddToSds()" );
		}

	if ( attr ) 
		{
		IterCookie* c = attr->InitForIteration();

		const Value* member;
		const char* key;
		while ( (member = attr->NextEntry( key, c )) ) 
			{
			char* new_name = new char[strlen( key ) + 10];
			sprintf( new_name, "at*%s", key );
			member->AddToSds( sds, dlist, new_name, rh, level + 1 );
			delete new_name;
			}
		}

	if ( type == TYPE_RECORD || attr )
		{
		if ( level > 1 )
			sds_end_sub_record( rh );

		else if ( level == 1 )
			sds_end_and_declare( rh, sds );
		}
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
		type* new_val = new type[length];			\
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
		case TYPE_OPAQUE:
			error->Report( "cannot increase array of",
					type_names[Type()], "via assignment" );
			return 0;

		default:
			fatal->Report( "bad type in Value::Grow()" );
		}

	return 1;
	}


void Value::DescribeSelf( ostream& s ) const
	{
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
		delete desc;
		}
	}

Value* empty_value()
	{
	int i = 0;
	return create_value( &i, 0, COPY_ARRAY );
	}

Value* error_value()
	{
	return create_value( glish_false );
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

	charptr* result = new charptr[result_len];

	heap_ptr = heap;
	for ( int i = 0; i < result_len; ++i )
		{
		result[i] = strdup( heap_ptr );
		heap_ptr += strlen( heap_ptr ) + 1;
		}

	return result;
	}


#define SDS_COPY_MODE COPY_ARRAY
#define SDS_DO_MGR_INIT 0

static Value* read_single_value_from_SDS( GlishObject* manager,
						struct sds_odesc* odesc )
	{
	void* addr = (void*) odesc->address;
	int length = (int) odesc->nelems;
	int sds_type = (int) odesc->elemcod;

	int do_value_manager = SDS_DO_MGR_INIT;

	Value* result;

	switch ( sds_type )
		{
		case SDS_BYTE:
			result = create_value( (byte *) addr, length, SDS_COPY_MODE );
			break;

		case SDS_SHORT:
			result = create_value( (short *) addr, length, SDS_COPY_MODE );
			break;

		case SDS_INT:
			result = create_value( (int *) addr, length, SDS_COPY_MODE );
			break;

		case SDS_FLOAT:
			result = create_value( (float *) addr, length, SDS_COPY_MODE );
			break;

		case SDS_DOUBLE:
			result = create_value( (double *) addr, length, SDS_COPY_MODE );
			break;

		case SDS_COMPLEX:
			result = create_value( (complex *) addr, length, SDS_COPY_MODE );
			break;

		case SDS_DOUBLE_COMPLEX:
			result = create_value( (dcomplex *) addr, length, SDS_COPY_MODE );
			break;

		case SDS_STRING:
			{
			int heap_len;
			charptr* heap = make_string_array( (char*) addr,
							length, heap_len );
			result = create_value( heap, heap_len );
			do_value_manager = 0;
			break;
			}

		default:
			{
			warn->Report(
			    "don't know how to deal with SDS type ", sds_type );
			result = error_value();
			do_value_manager = 0;
			break;
			}
		}

	if ( do_value_manager )
		{
		Ref( manager );
		result->SetValueManager( manager );
		}

	return result;
	}


// Forward declaration since read_element() and traverse_record()
// are mutually recursive.
static Value* traverse_record( Value* record, Value*& attr, int sds,
				int index, GlishObject* manager,
				struct sds_odesc*& thing_list,
				int record_level, int& level );

static Value* read_element( int sds, int index, GlishObject* manager,
				struct sds_odesc*& thing_list,
				int& level )
	{
	struct sds_odesc* odesc = &thing_list[level];

	Value* result;
	if ( odesc->elemcod & SDS_INDLIST )
		{ // Subrecord.
		Value* a = 0;
		result = traverse_record( 0, a, sds, index, manager,
					thing_list, level + 1, level );
		}

	else
		{
		result = read_single_value_from_SDS( manager, odesc );
		level = sds_describe( sds, index, &thing_list );
		}

	return result;
	}

static Value* traverse_record( Value* record, Value*& attr,
				int sds, int index,
				GlishObject* manager,
				struct sds_odesc*& thing_list,
				int record_level, int& level )
	{
	level = sds_describe( sds, index, &thing_list );

	while ( level == record_level )
		{
		char* name = thing_list[level].name;
		if ( ! record )
			{
			if ( ! strcmp( "val*", name )  )
				{
				record = read_element( sds, index, manager,
							thing_list, level );
				continue;
				}

			if ( strncmp( "at*", name, 3 ) )
				record = create_record();
			}

		Value* elm = read_element( sds, index, manager, thing_list,
						level );
		if ( ! strncmp( "at*", name, 3 )  )
			{
			if ( ! attr )
				attr = create_record();
			attr->RecordPtr()->Insert( strdup( name + 3 ), elm );
			}
		else
			{
			record->AssignRecordElement( name, elm );
			Unref( elm );
			}
		}

	if ( attr && record && record->AttributePtr() != attr->RecordPtr() )
		record->AssignAttributes( attr );

	return record;
	}

Value* read_value_from_SDS( int sds, int is_opaque_sds )
	{
	SDS_ValueManager* manager = new SDS_ValueManager( sds );

	sds_cleanup( sds );
	Value* result;

	if ( is_opaque_sds )
		{
		SDS_Index sds_index(sds);
		result = create_value( sds_index );
		Ref( manager );
		result->SetValueManager( manager );
		}

	else
		{
		struct sds_odesc* thing_list;

		// Check the name of the object; if empty then this
		// is a single array object; otherwise treat it as
		// a record.

		int num_sds_objects = (int) sds_array_size( sds, 0 ) - 1;
		const char* object_name = sds_obind2name( sds, 1 );

		if ( num_sds_objects && ( ! object_name || ! *object_name ) )
			{ // Single array object.
			int level = sds_describe( sds, 1, &thing_list );
			result = read_single_value_from_SDS( manager,
						&thing_list[level] );
			}

		else
			{
			// Initialized to zero, filled in within 
			// "traverse_record".
			result = 0;
			Value* attr = 0;
			int level;
			for ( int i = 1; i <= num_sds_objects; ++i )
				result = traverse_record( result, attr, sds,
					i, manager, thing_list, 1, level );

			if ( ! result )
				result = create_record();

			// Special backward-compatibility check.  If
			// this record has a single "*record*" field,
			// then it was generated by an older version of
			// Glish, and we "dereference" that field.

			Value* real_result;
			if ( result->Length() == 1 &&
			     (real_result = result->Field( "*record*" )) )
				{
				Ref( real_result );
				Unref( result );
				result = real_result;
				}
			}
		}

	Unref( manager );

	sds_cleanup( sds );

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
	// First see how many pieces the split will result in.
	num_pieces = 0;
	char* source_copy = strdup( source );
	charptr next_string = strtok( source_copy, split_chars );
	while ( next_string )
		{
		++num_pieces;
		next_string = strtok( 0, split_chars );
		}
	delete source_copy;

	charptr* strings = new charptr[num_pieces];
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
