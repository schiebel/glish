// $Header$

#include <string.h>
#include <stream.h>
#include <stdlib.h>	// for realloc(), memcpy()

#include "Sds/sdsgen.h"
#include "Glish/Value.h"

#include "glish_event.h"
#include "BinOpExpr.h"
#include "Func.h"
#include "Agent.h"	// so we can Unref( AgentVal() )
#include "Reporter.h"

#if defined(__GNUG__) || defined(SABER)
typedef void* malloc_t;	// needed for cfront compatibility
#endif


int num_Values_created = 0;
int num_Values_deleted = 0;

const char* type_names[NUM_GLISH_TYPES] =
	{
	"error", "ref", "const", "boolean", "integer", "float", "double",
	"string", "agent", "function", "record", "opaque",
	};


const Value* false_value;


#define AGENT_MEMBER_NAME "*agent*"


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


#define DEFINE_SINGLETON_CONSTRUCTOR(constructor_type)			\
Value::Value( constructor_type value )					\
	{								\
	type = TYPE_ERROR;						\
	description = 0;						\
	value_manager = 0;						\
	SetValue( &value, 1, COPY_ARRAY );				\
	++num_Values_created;						\
	}

#define DEFINE_ARRAY_CONSTRUCTOR(constructor_type)			\
Value::Value( constructor_type value[], int len, array_storage_type storage )\
	{								\
	type = TYPE_ERROR;						\
	description = 0;						\
	value_manager = 0;						\
	SetValue( value, len, storage );				\
	++num_Values_created;						\
	}

#define DEFINE_CONSTRUCTORS(type)					\
	DEFINE_SINGLETON_CONSTRUCTOR(type)				\
	DEFINE_ARRAY_CONSTRUCTOR(type)

DEFINE_CONSTRUCTORS(bool)
DEFINE_CONSTRUCTORS(int)
DEFINE_CONSTRUCTORS(float)
DEFINE_CONSTRUCTORS(double)
DEFINE_CONSTRUCTORS(charptr)
DEFINE_SINGLETON_CONSTRUCTOR(agentptr)
DEFINE_CONSTRUCTORS(funcptr)


Value::Value( recordptr value, Agent* agent )
	{
	type = TYPE_ERROR;
	description = 0;
	value_manager = 0;
	SetValue( value, agent );
	++num_Values_created;
	}


Value::Value( SDS_Index& value )
	{
	type = TYPE_ERROR;
	description = 0;
	value_manager = 0;
	SetValue( value );
	++num_Values_created;
	}


Value::Value( Value* ref_value, value_type val_type )
	{
	description = 0;
	value_manager = 0;
	storage = TAKE_OVER_ARRAY;

	if ( val_type == VAL_CONST )
		type = TYPE_CONST;

	else if ( val_type == VAL_REF )
		type = TYPE_REF;

	else
		fatal->Report( "bad value_type in Value::Value" );

	if ( ref_value->IsConst() && val_type == VAL_REF )
		warn->Report(
			"\"ref\" reference created from \"const\" reference" );

	ref_value = ref_value->Deref();

	Ref( ref_value );
	values = (void*) ref_value;

	++num_Values_created;
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

	int my_ref_count = ref_count;

	*this = *new_value;

	ref_count = my_ref_count;

	new_value->type = TYPE_ERROR;
	Unref( new_value );
	}


Value::~Value()
	{
	DeleteValue();
	++num_Values_deleted;
	}


#define DEFINE_SET_VALUE(type, glish_type)				\
void Value::SetValue( type array[], int len, array_storage_type arg_storage )\
	{								\
	SetType( glish_type );						\
	max_size = length = len;					\
	storage = arg_storage;						\
	values = storage == COPY_ARRAY ? copy_values( array, type ) : array;\
	}

DEFINE_SET_VALUE(bool,TYPE_BOOL)
DEFINE_SET_VALUE(int,TYPE_INT)
DEFINE_SET_VALUE(float,TYPE_FLOAT)
DEFINE_SET_VALUE(double,TYPE_DOUBLE)
DEFINE_SET_VALUE(agentptr,TYPE_AGENT)
DEFINE_SET_VALUE(funcptr,TYPE_FUNC)

void Value::SetValue( const char* array[], int len,
			array_storage_type arg_storage )
	{
	SetType( TYPE_STRING );

	max_size = length = len;
	storage = arg_storage;

	if ( storage == COPY_ARRAY )
		{
		values = (void *) new charptr[len];
		charptr* sptr = StringPtr();

		for ( int i = 0; i < len; ++i )
			sptr[i] = strdup( array[i] );
		}

	else
		values = array;
	}


void Value::SetValue( recordptr value, Agent* agent )
	{
	SetType( TYPE_RECORD );
	values = (void*) value;
	max_size = length = 1;
	storage = TAKE_OVER_ARRAY;

	if ( agent )
		RecordPtr()->Insert( strdup( AGENT_MEMBER_NAME ),
					new Value( agent ) );
	}


void Value::SetValue( SDS_Index& value )
	{
	SetType( TYPE_OPAQUE );
	values = (void*) value.Index();
	max_size = length = 1;
	storage = PRESERVE_ARRAY;
	}


void Value::SetType( glish_type new_type )
	{
	DeleteValue();
	type = new_type;
	}


void Value::DeleteValue()
	{
	switch ( type )
		{
		case TYPE_CONST:
		case TYPE_REF:
			Unref( RefPtr() );

			// So we don't also delete "values" ...
			type = TYPE_ERROR;
			break;

		case TYPE_STRING:
			if ( ! value_manager && storage != PRESERVE_ARRAY )
				{
				charptr* sptr = StringPtr();
				for ( int i = 0; i < length; ++i )
					delete (char*) sptr[i];
				}
			break;

		case TYPE_AGENT:
			Unref( AgentVal() );
			break;

		case TYPE_RECORD:
			{
			recordptr rptr = RecordPtr();

			IterCookie* c = rptr->InitForIteration();

			Value* member;
			const char* key;
			while ( (member = rptr->NextEntry( key, c )) )
				{
				delete (char*) key;
				Unref( member );
				}

			delete rptr;

			// So we don't delete "values" again ...
			type = TYPE_ERROR;
			break;
			}

		default:
			break;
		}

	if ( type != TYPE_ERROR )
		{
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

		else if ( storage != PRESERVE_ARRAY )
			delete values;
		}
	}


bool Value::IsNumeric() const
	{
	switch ( type )
		{
		case TYPE_BOOL:
		case TYPE_INT:
		case TYPE_FLOAT:
		case TYPE_DOUBLE:
			return true;

		case TYPE_CONST:
		case TYPE_REF:
		case TYPE_STRING:
		case TYPE_AGENT:
		case TYPE_FUNC:
		case TYPE_RECORD:
		case TYPE_OPAQUE:
			return false;

		case TYPE_ERROR:
		default:
			fatal->Report( "bad type in Value::IsNumeric()" );

			return false;	// for overly clever compilers
		}
	}


bool Value::IsAgentRecord() const
	{
	if ( type == TYPE_RECORD && (*RecordPtr())[AGENT_MEMBER_NAME] )
		return true;
	else
		return false;
	}


#define DEFINE_CONST_ACCESSOR(name,tag,type)				\
type Value::name() const						\
	{								\
	if ( Type() != tag )						\
		fatal->Report( "bad use of const accessor" );		\
	return (type) values;						\
	}

DEFINE_CONST_ACCESSOR(BoolPtr,TYPE_BOOL,bool*)
DEFINE_CONST_ACCESSOR(IntPtr,TYPE_INT,int*)
DEFINE_CONST_ACCESSOR(FloatPtr,TYPE_FLOAT,float*)
DEFINE_CONST_ACCESSOR(DoublePtr,TYPE_DOUBLE,double*)
DEFINE_CONST_ACCESSOR(StringPtr,TYPE_STRING,charptr*)
DEFINE_CONST_ACCESSOR(FuncPtr,TYPE_FUNC,funcptr*)
DEFINE_CONST_ACCESSOR(AgentPtr,TYPE_AGENT,agentptr*)
DEFINE_CONST_ACCESSOR(RecordPtr,TYPE_RECORD,recordptr)


#define DEFINE_ACCESSOR(name,tag,type)					\
type Value::name()							\
	{								\
	if ( Type() != tag )						\
		Polymorph( tag );					\
	return (type) values;						\
	}

DEFINE_ACCESSOR(BoolPtr,TYPE_BOOL,bool*)
DEFINE_ACCESSOR(IntPtr,TYPE_INT,int*)
DEFINE_ACCESSOR(FloatPtr,TYPE_FLOAT,float*)
DEFINE_ACCESSOR(DoublePtr,TYPE_DOUBLE,double*)
DEFINE_ACCESSOR(StringPtr,TYPE_STRING,charptr*)
DEFINE_ACCESSOR(FuncPtr,TYPE_FUNC,funcptr*)
DEFINE_ACCESSOR(AgentPtr,TYPE_AGENT,agentptr*)
DEFINE_ACCESSOR(RecordPtr,TYPE_RECORD,recordptr)


bool Value::BoolVal( int n ) const
	{
	if ( IsRef() )
		return Deref()->BoolVal( n );

	if ( length == 0 )
		{
		error->Report( "no data in array to interpret as boolean" );
		return false;
		}

	if ( n < 1 || n > length )
		{
		error->Report( "Value::BoolVal index (=", n,
				") out of bounds, length =", length );
		return false;
		}

	switch ( type )
		{
		case TYPE_BOOL:
			return BoolPtr()[n - 1];

		case TYPE_INT:
		case TYPE_FLOAT:
		case TYPE_DOUBLE:
			return IntVal( n ) ? true : false;

		case TYPE_STRING:
			error->Report( "cannot treat string as boolean" );
			return false;

		case TYPE_AGENT:
		case TYPE_FUNC:
		case TYPE_RECORD:
		case TYPE_OPAQUE:
			error->Report( "bad use of", type_names[Type()],
					"as boolean in:", this );
			return false;

		default:
			fatal->Report( "bad type in Value::BoolVal()" );
			return false;
		}
	}

int Value::IntVal( int n ) const
	{
	if ( IsRef() )
		return Deref()->IntVal( n );

	if ( length < 1 )
		{
		error->Report( "empty array converted to integer (= 0)" );
		return 0;
		}

	if ( n < 1 || n > length )
		{
		error->Report( "Value::IntVal index (=", n,
				") out of bounds, length =", length );
		return 0;
		}

	switch ( type )
		{
		case TYPE_BOOL:
			return BoolPtr()[n - 1] ? 1 : 0;

		case TYPE_INT:
			return IntPtr()[n - 1];

		case TYPE_FLOAT:
			return int( FloatPtr()[n - 1] );

		case TYPE_DOUBLE:
			return int( DoublePtr()[n - 1] );

		case TYPE_STRING:
			int result = 0;

			if ( ! text_to_integer( StringPtr()[n - 1], result ) )
				warn->Report( "string \"", this,
						"\" converted to integer" );
			return result;

		case TYPE_AGENT:
		case TYPE_FUNC:
		case TYPE_RECORD:
		case TYPE_OPAQUE:
			error->Report( "bad type", type_names[Type()],
					"converted to integer:", this );
			return 0;

		default:
			fatal->Report( "bad type in Value::IntVal()" );
			return 0;
		}
	}

double Value::DoubleVal( int n ) const
	{
	if ( IsRef() )
		return Deref()->DoubleVal( n );

	if ( length < 1 )
		{
		error->Report( "empty array converted to double (= 0.0)" );
		return 0.0;
		}

	if ( n < 1 || n > length )
		{
		error->Report( "Value::DoubleVal index (=", n,
				") out of bounds, length =", length );
		return 0.0;
		}

	switch ( type )
		{
		case TYPE_BOOL:
			return BoolPtr()[n - 1] ? 1.0 : 0.0;

		case TYPE_INT:
			return double( IntPtr()[n - 1] );

		case TYPE_FLOAT:
			return double( FloatPtr()[n - 1] );

		case TYPE_DOUBLE:
			return DoublePtr()[n - 1];

		case TYPE_STRING:
			double result = 0.0;

			if ( ! text_to_double( StringPtr()[n - 1], result ) )
				warn->Report( "string \"", this,
						"\" converted to double" );
			return result;

		case TYPE_AGENT:
		case TYPE_FUNC:
		case TYPE_RECORD:
		case TYPE_OPAQUE:
			error->Report( "bad type", type_names[Type()],
					"converted to double:", this );
			return 0.0;

		default:
			fatal->Report( "bad type in Value::DoubleVal()" );
			return 0.0;
		}
	}

char* Value::StringVal( char sep ) const
	{
	if ( IsRef() )
		return Deref()->StringVal();

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

	// Make a guess as to a probable good size for buf
	if ( type == TYPE_STRING )
		{
		buf_size = strlen( StringPtr()[0] ) * (length + 1);
		if ( buf_size == 0 )
			buf_size = 8;
		}

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

	bool* bool_ptr;
	int* int_ptr;
	float* float_ptr;
	double* double_ptr;
	charptr* string_ptr;

	switch ( type )
		{
#define ASSIGN_PTR(tag,ptr_name,source)					\
	case tag:							\
		ptr_name = source;					\
		break;

		ASSIGN_PTR(TYPE_BOOL,bool_ptr,BoolPtr())
		ASSIGN_PTR(TYPE_INT,int_ptr,IntPtr())
		ASSIGN_PTR(TYPE_FLOAT,float_ptr,FloatPtr())
		ASSIGN_PTR(TYPE_DOUBLE,double_ptr,DoublePtr())
		ASSIGN_PTR(TYPE_STRING,string_ptr,StringPtr())

		default:
			fatal->Report( "bad type in Value::StringVal()" );
		}

	for ( int i = 0; i < length; ++i )
		{
		char numeric_buf[128];
		const char* addition = numeric_buf;

		switch ( type )
			{
			case TYPE_BOOL:
				if ( bool_ptr[i] )
					strcpy( numeric_buf, "T" );
				else
					strcpy( numeric_buf, "F" );
				break;

			case TYPE_INT:
				sprintf( numeric_buf, "%d", int_ptr[i] );
				break;

			case TYPE_FLOAT:
				sprintf( numeric_buf, "%g", float_ptr[i] );
				break;

			case TYPE_DOUBLE:
				sprintf( numeric_buf, "%g", double_ptr[i] );
				break;

			case TYPE_STRING:
				addition = string_ptr[i];
				break;
				
			default:
				fatal->Report(
				    "bad type in Value::StringVal()" );
			}

		int buf_remaining = &buf[buf_size] - buf_ptr;
		int size_of_addition = strlen( addition );

		while ( size_of_addition >= buf_remaining - 5 /* slop */ )
			{ // Need to grow the buffer
			int buf_ptr_offset = buf_ptr - buf;

			buf_size *= 2;

			buf = (char*) realloc( (malloc_t) buf, buf_size );

			if ( ! buf )
				fatal->Report(
					"out of memory in Value::StringVal()" );

			buf_ptr = buf + buf_ptr_offset;
			buf_remaining = &buf[buf_size] - buf_ptr;
			}

		strcpy( buf_ptr, addition );
		buf_ptr += size_of_addition;

		if ( i < length - 1 )
			// More to come.
			*buf_ptr++ = sep;
		}

	if ( type != TYPE_STRING && length > 1 )
		{
		// Insert []'s around value.
		*buf_ptr++ = ']';
		*buf_ptr = '\0';
		}

	return buf;
	}

char* Value::RecordStringVal() const
	{
	if ( type != TYPE_RECORD )
		fatal->Report( "non-record type in Value::RecordStringVal()" );

	recordptr rptr = RecordPtr();
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

	for ( i = 0; i < len; ++i )
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

Agent* Value::AgentVal() const
	{
	if ( type == TYPE_AGENT )
		return AgentPtr()[0];

	if ( type == TYPE_RECORD )
		{
		Value* member = (*RecordPtr())[AGENT_MEMBER_NAME];

		if ( member )
			return member->AgentVal();
		}

	error->Report( this, " is not an agent value" );
	return 0;
	}

Func* Value::FuncVal() const
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

int Value::SDS_IndexVal() const
	{
	if ( type != TYPE_OPAQUE )
		{
		error->Report( this, " is not an opaque value" );
		return SDS_NO_SUCH_SDS;
		}

	return int(values);
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


bool* Value::CoerceToBoolArray( bool& is_copy, int size, bool* result ) const
	{
	if ( ! IsNumeric() )
		fatal->Report( "non-numeric type in coercion of", this,
				"to boolean" );

	if ( ! result && type == TYPE_BOOL && length == size )
		{
		is_copy = false;
		return BoolPtr();
		}

	is_copy = true;
	if ( ! result )
		result = new bool[size];

	int incr = (length == 1 ? 0 : 1);
	int i, j;

	switch ( type )
		{
		case TYPE_BOOL:
			bool* bool_ptr = BoolPtr();
			for ( i = 0, j = 0; i < size; ++i, j += incr )
				result[i] = bool_ptr[j];
			break;

#define BOOL_COERCE_ACTION(tag,type,accessor)				\
	case tag:							\
		{							\
		type* ptr = accessor;					\
		for ( i = 0, j = 0; i < size; ++i, j += incr )		\
			result[i] = (ptr[j] ? true : false);		\
		break;							\
		}

BOOL_COERCE_ACTION(TYPE_INT,int,IntPtr())
BOOL_COERCE_ACTION(TYPE_FLOAT,float,FloatPtr())
BOOL_COERCE_ACTION(TYPE_DOUBLE,double,DoublePtr())

		default:
			error->Report(
				"bad type in Value::CoerceToBoolArray()" );
			return 0;
		}

	return result;
	}


int* Value::CoerceToIntArray( bool& is_copy, int size, int* result ) const
	{
	if ( ! IsNumeric() )
		fatal->Report( "non-numeric type in coercion of", this,
				"to integer" );

	if ( ! result && length == size &&
	     (type == TYPE_INT || type == TYPE_BOOL) )
		{
		is_copy = false;
		return type == TYPE_INT ? IntPtr() : (int*) BoolPtr();
		}

	is_copy = true;
	if ( ! result )
		result = new int[size];

	int incr = (length == 1 ? 0 : 1);
	int i, j;

	switch ( type )
		{
#define COERCE_ACTION(tag,rhs_type,lhs_type,accessor)			\
	case tag:							\
		{							\
		rhs_type* rhs_ptr = accessor;				\
		for ( i = 0, j = 0; i < size; ++i, j += incr )		\
			result[i] = lhs_type(rhs_ptr[j]);		\
		break;							\
		}

COERCE_ACTION(TYPE_BOOL,bool,int,BoolPtr())
COERCE_ACTION(TYPE_INT,int,int,IntPtr())
COERCE_ACTION(TYPE_FLOAT,float,int,FloatPtr())
COERCE_ACTION(TYPE_DOUBLE,double,int,DoublePtr())

		default:
			error->Report(
				"bad type in Value::CoerceToIntArray()" );
			return 0;
		}

	return result;
	}


float* Value::CoerceToFloatArray( bool& is_copy, int size, float* result ) const
	{
	if ( ! IsNumeric() )
		fatal->Report( "non-numeric type in coercion of", this,
				"to float" );

	if ( ! result && type == TYPE_FLOAT && length == size )
		{
		is_copy = false;
		return FloatPtr();
		}

	is_copy = true;
	if ( ! result )
		result = new float[size];

	int incr = (length == 1 ? 0 : 1);
	int i, j;

	switch ( type )
		{
COERCE_ACTION(TYPE_BOOL,bool,float,BoolPtr())
COERCE_ACTION(TYPE_INT,int,float,IntPtr())
COERCE_ACTION(TYPE_FLOAT,float,float,FloatPtr())
COERCE_ACTION(TYPE_DOUBLE,double,float,DoublePtr())

		default:
			error->Report(
				"bad type in Value::CoerceToFloatArray()" );
			return 0;
		}

	return result;
	}


double* Value::CoerceToDoubleArray( bool& is_copy, int size, double* result )
    const
	{
	if ( ! IsNumeric() )
		fatal->Report( "non-numeric type in coercion of", this,
				"to double" );

	if ( ! result && type == TYPE_DOUBLE && length == size )
		{
		is_copy = false;
		return DoublePtr();
		}

	is_copy = true;
	if ( ! result )
		result = new double[size];

	int incr = (length == 1 ? 0 : 1);
	int i, j;

	switch ( type )
		{
COERCE_ACTION(TYPE_BOOL,bool,double,BoolPtr())
COERCE_ACTION(TYPE_INT,int,double,IntPtr())
COERCE_ACTION(TYPE_FLOAT,float,double,FloatPtr())
COERCE_ACTION(TYPE_DOUBLE,double,double,DoublePtr())

		default:
			error->Report(
			    "bad type in Value::CoerceToDoubleArray()" );
			return 0;
		}

	return result;
	}


charptr* Value::CoerceToStringArray( bool& is_copy, int size, charptr* result )
    const
	{
	if ( type != TYPE_STRING )
		{
		error->Report( "non-string type in coercion of", this,
				"to string" );
		return 0;
		}

	if ( ! result && length == size )
		{
		is_copy = false;
		return StringPtr();
		}

	is_copy = true;
	if ( ! result )
		result = new charptr[size];

	int incr = (length == 1 ? 0 : 1);

	int i, j;
	charptr* string_ptr = StringPtr();
	for ( i = 0, j = 0; i < size; ++i, j += incr )
		result[i] = string_ptr[j];

	return result;
	}

funcptr* Value::CoerceToFuncArray( bool& is_copy, int size, funcptr* result )
    const
	{
	if ( type != TYPE_FUNC )
		fatal->Report( "non-func type in CoerceToFuncArray()" );

	if ( size != length )
		fatal->Report( "size != length in CoerceToFuncArray()" );

	if ( result )
		fatal->Report( "prespecified result in CoerceToFuncArray()" );

	is_copy = false;
	return FuncPtr();
	}


Value* Value::operator []( const Value* index ) const
	{
	if ( index->Type() == TYPE_STRING )
		return RecordRef( index );

	bool indices_are_copy;
	int num_indices;
	int* indices = GenerateIndices( index, num_indices,
						indices_are_copy );

	if ( indices )
		{
		Value* result = ArrayRef( indices, num_indices );

		if ( indices_are_copy )
			delete indices;

		return result;
		}

	else
		return new Value( false );
	}


Value* Value::RecordRef( const Value* index ) const
	{
	if ( index->Type() != TYPE_STRING )
		{
		error->Report( "non-string index in record reference" );
		return new Value( false );
		}

	if ( index->Length() == 1 )
		// Don't create a new record, just return the given element.
		return copy_value( ExistingRecordElement( index ) );

	recordptr rptr = RecordPtr();
	recordptr new_record = create_record_dict();
	charptr* indices = index->StringPtr();

	for ( int i = 0; i < index->Length(); ++i )
		{
		char* key = strdup( indices[i] );
		new_record->Insert( key,
				copy_value( ExistingRecordElement( key ) ) );
		}

	return new Value( new_record );
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
	if ( type != TYPE_RECORD )
		{
		warn->Report( "operand to .", field, " is not a record" );
		return false_value;
		}

	Value* member = (*RecordPtr())[field];

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
	if ( type != TYPE_RECORD )
		{
		warn->Report( "operand to .", field, " is not a record" );
		return new Value( false );
		}

	Value* member = (*RecordPtr())[field];

	if ( ! member )
		{
		member = new Value( false );
		RecordPtr()->Insert( strdup( field ), member );
		}

	return member;
	}

Value* Value::HasRecordElement( const char* field ) const
	{
	if ( type != TYPE_RECORD )
		fatal->Report( "non-record in Value::HasRecordElement" );

	return (*RecordPtr())[field];
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
	if ( type != TYPE_RECORD )
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
	if ( type != TYPE_RECORD )
		return 0;

	Value* member = RecordPtr()->NthEntry( n - 1 );

	if ( ! member )
		return 0;

	return member;
	}

const Value* Value::NthField( int n ) const
	{
	return ((Value*) this)->NthField( n );
	}

const char* Value::NthFieldName( int n ) const
	{
	if ( type != TYPE_RECORD )
		return 0;

	const char* key;
	Value* member = RecordPtr()->NthEntry( n - 1, key );

	if ( ! member )
		return 0;

	return key;
	}

char* Value::NewFieldName()
	{
	if ( type != TYPE_RECORD )
		return 0;

	static int counter = 0;

	char buf[128];
	do
		sprintf( buf, "*%d", ++counter );
	while ( Field( buf ) );

	return strdup( buf );
	}


#define DEFINE_FIELD_VAL(tag,type,valfunc)				\
bool Value::FieldVal( const char* field, type& val, int num )		\
	{								\
	Value* result = Field( field, tag );				\
	if ( ! result )							\
		return false;						\
									\
	val = result->valfunc( num );					\
	return true;							\
	}

DEFINE_FIELD_VAL(TYPE_BOOL, bool, BoolVal)
DEFINE_FIELD_VAL(TYPE_INT, int, IntVal)
DEFINE_FIELD_VAL(TYPE_DOUBLE, double, DoubleVal)

bool Value::FieldVal( const char* field, charptr& val )
	{
	Value* result = Field( field, TYPE_STRING );
	if ( ! result )
		return false;

	val = result->StringVal();
	return true;
	}


#define DEFINE_FIELD_PTR(name,tag,type,accessor)			\
type Value::name( const char* field, int& len )				\
	{								\
	Value* result = Field( field, tag );				\
	if ( ! result )							\
		return 0;						\
									\
	len = result->Length();						\
	return result->accessor;					\
	}

DEFINE_FIELD_PTR(FieldBoolPtr,TYPE_BOOL,bool*,BoolPtr())
DEFINE_FIELD_PTR(FieldIntPtr,TYPE_INT,int*,IntPtr())
DEFINE_FIELD_PTR(FieldFloatPtr,TYPE_FLOAT,float*,FloatPtr())
DEFINE_FIELD_PTR(FieldDoublePtr,TYPE_DOUBLE,double*,DoublePtr())
DEFINE_FIELD_PTR(FieldStringPtr,TYPE_STRING,charptr*,StringPtr())


#define DEFINE_SET_FIELD_SCALAR(type)					\
void Value::SetField( const char* field, type val )			\
	{								\
	Value* field_elem = new Value( val );				\
	AssignRecordElement( field, field_elem );			\
	Unref( field_elem );						\
	}

#define DEFINE_SET_FIELD_ARRAY(type)					\
void Value::SetField( const char* field, type val[], int num_elements,	\
			array_storage_type arg_storage )		\
	{								\
	Value* field_elem = new Value( val, num_elements, arg_storage );\
	AssignRecordElement( field, field_elem );			\
	Unref( field_elem );						\
	}

#define DEFINE_SET_FIELD(type)						\
	DEFINE_SET_FIELD_SCALAR(type)					\
	DEFINE_SET_FIELD_ARRAY(type)

DEFINE_SET_FIELD(bool)
DEFINE_SET_FIELD(int)
DEFINE_SET_FIELD(float)
DEFINE_SET_FIELD(double)
DEFINE_SET_FIELD(const char*)


int* Value::GenerateIndices( const Value* index, int& num_indices,
				bool& indices_are_copy ) const
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
		if ( num_indices != length )
			{
			error->Report( "boolean array index has", num_indices,
					"elements, array has", length );
			return 0;
			}

		// First figure out how many elements we're going to be copying.
		bool* vals = index->BoolPtr();
		num_indices = 0;

		for ( int i = 0; i < length; ++i )
			if ( vals[i] )
				++num_indices;

		indices = new int[num_indices];
		indices_are_copy = true;

		num_indices = 0;
		for ( i = 0; i < length; ++i )
			if ( vals[i] )
				indices[num_indices++] = i + 1;
		}

	else
		indices = index->CoerceToIntArray( indices_are_copy,
							num_indices );

	return indices;
	}

Value* Value::ArrayRef( int* indices, int num_indices ) const
	{
	if ( IsRef() )
		return Deref()->ArrayRef( indices, num_indices );

	if ( type == TYPE_RECORD )
		return RecordSlice( indices, num_indices );

	for ( int i = 0; i < num_indices; ++i )
		if ( indices[i] < 1 || indices[i] > length )
			{
			error->Report( "index (=", indices[i],
				") out of range, array length =", length );
			return new Value( false );
			}

	switch ( type )
		{

#define ARRAY_REF_ACTION(tag,type,accessor,copy_func)			\
	case tag:							\
		{							\
		type* source_ptr = accessor;				\
		type* new_values;					\
		new_values = new type[num_indices];			\
		for ( i = 0; i < num_indices; ++i )			\
			new_values[i] = copy_func(source_ptr[indices[i]-1]);\
		return new Value( new_values, num_indices );		\
		}

ARRAY_REF_ACTION(TYPE_BOOL,bool,BoolPtr(),)
ARRAY_REF_ACTION(TYPE_INT,int,IntPtr(),)
ARRAY_REF_ACTION(TYPE_FLOAT,float,FloatPtr(),)
ARRAY_REF_ACTION(TYPE_DOUBLE,double,DoublePtr(),)
ARRAY_REF_ACTION(TYPE_STRING,charptr,StringPtr(),strdup)
ARRAY_REF_ACTION(TYPE_FUNC,funcptr,FuncPtr(),)

		default:
			fatal->Report( "bad type in Value::ArrayRef()" );
			return 0;
		}
	}

Value* Value::RecordSlice( int* indices, int num_indices ) const
	{
	if ( type != TYPE_RECORD )
		fatal->Report( "non-record type in Value::RecordSlice()" );

	int max_index = 0;
	if ( ! IndexRange( indices, num_indices, max_index ) )
		return new Value( false );

	recordptr rptr = RecordPtr();

	if ( max_index > rptr->Length() )
		{
		error->Report( "record index (=", max_index,
			") out of range (> ", rptr->Length(), ")" );
		return new Value( false );
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

	return new Value( new_record );
	}


void Value::AssignElements( const Value* index, Value* value )
	{
	if ( index->Type() == TYPE_STRING )
		AssignRecordElements( index, value );

	else
		{
		bool indices_are_copy;
		int num_indices;
		int* indices = GenerateIndices( index, num_indices,
							indices_are_copy );

		if ( ! indices )
			return;

		if ( type == TYPE_RECORD )
			AssignRecordSlice( value, indices, num_indices );

		else
			AssignArrayElements( value, indices, num_indices );

		if ( indices_are_copy )
			delete indices;
		}

	Unref( value );
	}

void Value::AssignRecordElements( const Value* index, Value* value )
	{
	if ( type != TYPE_RECORD )
		{
		error->Report( this, " is not a record" );
		return;
		}

	if ( index->Length() == 1 )
		{
		AssignRecordElement( index->StringPtr()[0], value );
		return;
		}

	if ( value->Type() != TYPE_RECORD )
		{
		error->Report( "assignment of non-record type to subrecord" );
		return;
		}

	recordptr lhs_rptr = RecordPtr();
	recordptr rhs_rptr = value->RecordPtr();
	charptr* indices = index->StringPtr();

	if ( index->Length() != rhs_rptr->Length() )
		{
		error->Report( "in record assignment: # record indices (",
				index->Length(),
				") differs from # right-hand elements (",
				rhs_rptr->Length(), ")" );
		return;
		}

	for ( int i = 0; i < rhs_rptr->Length(); ++i )
		AssignRecordElement( indices[i], rhs_rptr->NthEntry( i ) );
	}

void Value::AssignRecordElement( const char* index, Value* value )
	{
	if ( type != TYPE_RECORD )
		fatal->Report( this, " is not a record" );

	recordptr rptr = RecordPtr();
	Value* member = (*rptr)[index];

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
	if ( type != TYPE_RECORD )
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
	if ( value->Type() != TYPE_RECORD )
		{
		error->Report(
			"non-record type assigned to record slice" );
		return;
		}

	recordptr rhs_rptr = value->RecordPtr();

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
	int max_index;
	if ( ! IndexRange( indices, num_indices, max_index ) )
		return;

	if ( max_index > length )
		if ( ! Grow( (unsigned int) max_index ) )
			return;

	switch ( type )
		{

#define ASSIGN_ARRAY_ELEMENTS_ACTION(tag,type,accessor,coerce_func,copy_func, delete_old_value)		\
	case tag:							\
		{							\
		bool rhs_copy;						\
		type* rhs_array = value->coerce_func( rhs_copy, rhs_len );\
		type* lhs = accessor;					\
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

ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_BOOL,bool,BoolPtr(),CoerceToBoolArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_INT,int,IntPtr(),CoerceToIntArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_FLOAT,float,FloatPtr(),CoerceToFloatArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_DOUBLE,double,DoublePtr(),
			CoerceToDoubleArray,,)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_STRING,charptr,StringPtr(),
			CoerceToStringArray,
			strdup, delete (char*) (lhs[indices[i]-1]);)
ASSIGN_ARRAY_ELEMENTS_ACTION(TYPE_FUNC,funcptr,FuncPtr(),CoerceToFuncArray,,)

		default:
			fatal->Report(
				"bad type in Value::AssignArrayElements()" );
		}
	}


bool Value::IndexRange( int* indices, int num_indices, int& max_index ) const
	{
	max_index = 0;

	for ( int i = 0; i < num_indices; ++i )
		{
		if ( indices[i] < 1 )
			{
			error->Report( "index (=", indices[i],
				") out of range (< 1)" );
			return false;
			}

		else if ( indices[i] > max_index )
			max_index = indices[i];
		}

	return true;
	}


void Value::Negate()
	{
	if ( ! IsNumeric() )
		{
		error->Report( "negation of non-numeric value:", this );
		return;
		}

	switch ( type )
		{
#define NEGATE_ACTION(tag,type,accessor,func)				\
	case tag:							\
		{							\
		type* ptr = accessor;					\
		for ( int i = 0; i < length; ++i )			\
			ptr[i] = func(ptr[i]);				\
		break;							\
		}

NEGATE_ACTION(TYPE_BOOL,bool,BoolPtr(),(bool) ! (int))
NEGATE_ACTION(TYPE_INT,int,IntPtr(),-)
NEGATE_ACTION(TYPE_FLOAT,float,FloatPtr(),-)
NEGATE_ACTION(TYPE_DOUBLE,double,DoublePtr(),-)

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

	if ( type == TYPE_BOOL )
		{
		bool* ptr = BoolPtr();
		for ( int i = 0; i < length; ++i )
			ptr[i] = bool( ! int( ptr[i] ) );
		return;
		}

	bool* result = new bool[length];
	bool is_copy;
	int* source = CoerceToIntArray( is_copy, length );

	for ( int i = 0; i < length; ++i )
		result[i] = source[i] ? false : true;

	if ( is_copy )
		delete source;

	SetValue( result, length );
	}


#define DEFINE_XXX_ARITH_OP_COMPUTE(name,type,coerce_func)		\
void Value::name( const Value* value, int lhs_len, ArithExpr* expr )	\
	{								\
	bool lhs_copy, rhs_copy;					\
	type* lhs_array = coerce_func( lhs_copy, lhs_len );		\
	type* rhs_array = value->coerce_func( rhs_copy, value->Length() );\
									\
	int rhs_incr = value->Length() == 1 ? 0 : 1;			\
									\
	expr->Compute( lhs_array, rhs_array, lhs_len, rhs_incr );	\
									\
	if ( lhs_copy )							\
		/* Change our value to the new result. */		\
		SetValue( lhs_array, lhs_len );				\
									\
	if ( rhs_copy )							\
		delete rhs_array;					\
	}


DEFINE_XXX_ARITH_OP_COMPUTE(IntOpCompute,int,CoerceToIntArray)
DEFINE_XXX_ARITH_OP_COMPUTE(FloatOpCompute,float,CoerceToFloatArray)
DEFINE_XXX_ARITH_OP_COMPUTE(DoubleOpCompute,double,CoerceToDoubleArray)


void Value::AddToSds( int sds, del_list* dlist, const char* name,
			struct record_header* rh, int level ) const
	{
	if ( IsRef() )
		return Deref()->AddToSds( sds, dlist, name, rh, level );

	if ( ! name )
		name = "";

	switch ( type )
		{
#define ADD_TO_SDS_ACTION(tag,type,accessor,SDS_type)			\
	case tag:							\
		{							\
		type* array = accessor;					\
									\
		if ( rh )						\
			sds_record_entry( rh, SDS_type, Length(),	\
					(char*) array, (char*) name );	\
		else							\
			(void) sds_declare_object( sds, (char*) array,	\
						   (char*) name, Length(),\
						   SDS_type );		\
		break;							\
		}

ADD_TO_SDS_ACTION(TYPE_BOOL,bool,BoolPtr(),SDS_INT)
ADD_TO_SDS_ACTION(TYPE_INT,int,IntPtr(),SDS_INT)
ADD_TO_SDS_ACTION(TYPE_FLOAT,float,FloatPtr(),SDS_FLOAT)
ADD_TO_SDS_ACTION(TYPE_DOUBLE,double,DoublePtr(),SDS_DOUBLE)

		case TYPE_STRING:
			{
			int total_size = 0;
			charptr* strs = StringPtr();

			// First figure out how much total storage is needed
			// for all of the strings.

			for ( int i = 0; i < length; ++i )
				total_size += strlen( strs[i] ) + 1;

			// Create a suitable contiguous heap for holding all
			// the strings.
			char* heap = new char[total_size];

			char* heap_ptr = heap;
			for ( i = 0; i < length; ++i )
				{
				strcpy( heap_ptr, strs[i] );
				heap_ptr += strlen( strs[i] ) + 1;
				}

			if ( rh )
				sds_record_entry( rh, SDS_STRING, total_size,
							heap, (char*) name );
			else
				(void) sds_declare_object( sds, heap,
					(char*) name, total_size, SDS_STRING );

			dlist->append( (void*) heap );

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
			if ( level > 1 )
				sds_begin_sub_record( rh, (char*) name );

			else if ( level == 1 )
				rh = sds_begin_record( (char*) name );

			// else we're doing a top-level record, leave rh nil.

			recordptr rptr = RecordPtr();

			for ( int i = 0; i < rptr->Length(); ++i )
				{
				const char* key;
				Value* member = rptr->NthEntry( i, key );

				member->AddToSds( sds, dlist, key, rh,
							level + 1 );
				}

			if ( level > 1 )
				sds_end_sub_record( rh );

			else if ( level == 1 )
				sds_end_and_declare( rh, sds );

			break;
			}

		case TYPE_OPAQUE:
			fatal->Report( "opaque value in Value::AddToSDS()" );
			break;

		default:
			fatal->Report( "bad type in Value::AddToSds()" );
		}
	}


void Value::Polymorph( glish_type new_type )
	{
	if ( type == new_type )
		return;

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
		bool is_copy;						\
		type* new_val = coerce_func( is_copy, length );		\
		if ( is_copy )						\
			SetValue( new_val, length );			\
		break;							\
		}

POLYMORPH_ACTION(TYPE_BOOL,bool,CoerceToBoolArray)
POLYMORPH_ACTION(TYPE_INT,int,CoerceToIntArray)
POLYMORPH_ACTION(TYPE_FLOAT,float,CoerceToFloatArray)
POLYMORPH_ACTION(TYPE_DOUBLE,double,CoerceToDoubleArray)
POLYMORPH_ACTION(TYPE_STRING,charptr,CoerceToStringArray)
POLYMORPH_ACTION(TYPE_FUNC,funcptr,CoerceToFuncArray)

		case TYPE_RECORD:
			if ( length > 1 )
				warn->Report(
			"array values lost due to conversion to record type" );

			SetValue( create_record_dict(), 0 );

			break;

		default:
			fatal->Report( "bad type in Value::Polymorph()" );
		}
	}


bool Value::Grow( unsigned int new_size )
	{
	int i;

	if ( storage == PRESERVE_ARRAY )
		TakeValue( copy_value( this ) );

	if ( new_size <= length )
		{
		if ( type == TYPE_STRING )
			{
			charptr* string_ptr = StringPtr();
			for ( i = new_size; i < length; ++i )
				delete (char*) string_ptr[i];
			}

		length = new_size;

		return true;
		}

	switch ( type )
		{
#define GROW_ACTION(tag,type,accessor,null_value)			\
	case tag:							\
		{							\
		type* ptr = accessor;					\
		if ( new_size > max_size )				\
			{						\
			values = (void*) realloc( (malloc_t) ptr,	\
						sizeof( type ) * new_size );\
			if ( ! values )					\
				fatal->Report(				\
					"out of memory in Value::Grow" );\
									\
			max_size = new_size;				\
			ptr = accessor;					\
			}						\
									\
		for ( i = length; i < new_size; ++i )			\
			ptr[i] = null_value;				\
									\
		break;							\
		}

GROW_ACTION(TYPE_BOOL,bool,BoolPtr(),false)
GROW_ACTION(TYPE_INT,int,IntPtr(),0)
GROW_ACTION(TYPE_FLOAT,float,FloatPtr(),0.0)
GROW_ACTION(TYPE_DOUBLE,double,DoublePtr(),0.0)
GROW_ACTION(TYPE_STRING,charptr,StringPtr(),strdup( "" ))

		case TYPE_AGENT:
		case TYPE_FUNC:
		case TYPE_RECORD:
		case TYPE_OPAQUE:
			error->Report( "cannot increase array of",
					type_names[Type()], "via assignment" );
			return false;

		default:
			fatal->Report( "bad type in Value::Grow()" );
		}

	length = new_size;

	return true;
	}


void Value::DescribeSelf( ostream& s ) const
	{
	if ( type == TYPE_FUNC )
		{
		// ### what if we're an array of functions?
		FuncVal()->Describe( s );
		}

	else if ( IsRef() )
		{
		if ( IsConst() )
			s << "const ";

		else
			s << "ref ";

		RefPtr()->DescribeSelf( s );
		}

	else
		{
		char* description = StringVal();
		s << description;
		delete description;
		}
	}


Value* create_record()
	{
	return new Value( create_record_dict() );
	}

recordptr create_record_dict()
	{
	return new PDict(Value)( ORDERED );
	}


Value* copy_value( const Value* value )
	{
	if ( value->IsRef() )
		return copy_value( value->RefPtr() );

	switch ( value->Type() )
		{
#define COPY_VALUE(tag,accessor)					\
	case tag:							\
		return new Value( value->accessor, value->Length(),	\
					COPY_ARRAY );

COPY_VALUE(TYPE_BOOL,BoolPtr())
COPY_VALUE(TYPE_INT,IntPtr())
COPY_VALUE(TYPE_FLOAT,FloatPtr())
COPY_VALUE(TYPE_DOUBLE,DoublePtr())
COPY_VALUE(TYPE_STRING,StringPtr())
COPY_VALUE(TYPE_FUNC,FuncPtr())

		case TYPE_AGENT:
			return new Value( value->AgentVal() );

		case TYPE_RECORD:
			{
			if ( value->IsAgentRecord() )
				{
				warn->Report(
		"cannot copy agent record value; returning reference instead" );
				return new Value( (Value*) value, VAL_REF );
				}

			recordptr rptr = value->RecordPtr();
			recordptr new_record = create_record_dict();

			for ( int i = 0; i < rptr->Length(); ++i )
				{
				const char* key;
				Value* member = rptr->NthEntry( i, key );

				new_record->Insert( strdup( key ),
							copy_value( member ) );
				}

			return new Value( new_record );
			}

		case TYPE_OPAQUE:
			return new Value( SDS_Index(value->SDS_IndexVal()) );

		default:
			fatal->Report( "bad type in copy_value()" );
			return 0;
		}
	}


static charptr* make_string_array( char* heap, int len, int& result_len )
        {
	result_len = 0;
        for ( char* heap_ptr = heap; heap_ptr < &heap[len]; ++heap_ptr )
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

static Value* read_single_value_from_SDS( GlishObject* manager,
						struct sds_odesc* odesc )
	{
	void* addr = (void*) odesc->address;
	int length = (int) odesc->nelems;
	int sds_type = (int) odesc->elemcod;

	bool do_value_manager = true;

	Value* result;

	switch ( sds_type )
		{
		case SDS_INT:
			result = new Value( (int *) addr, length );
			break;

		case SDS_FLOAT:
			result = new Value( (float *) addr, length );
			break;

		case SDS_DOUBLE:
			result = new Value( (double *) addr, length );
			break;

		case SDS_STRING:
			{
			int heap_len;
			charptr* heap = make_string_array( (char*) addr,
							length, heap_len );
			result = new Value( heap, heap_len );
			do_value_manager = false;
			break;
			}

		default:
			{
			warn->Report(
			    "don't know how to deal with SDS type ", sds_type );
			result = new Value( false );
			do_value_manager = false;
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
static void traverse_record( Value* record, int sds, int index,
				GlishObject* manager,
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
		result = create_record();
		traverse_record( result, sds, index, manager, thing_list,
				level + 1, level );
		}

	else
		{
		result = read_single_value_from_SDS( manager, odesc );
		level = sds_describe( sds, index, &thing_list );
		}

	return result;
	}

static void traverse_record( Value* record, int sds, int index,
				GlishObject* manager,
				struct sds_odesc*& thing_list,
				int record_level, int& level )
	{
	level = sds_describe( sds, index, &thing_list );

	while ( level == record_level )
		{
		char* name = thing_list[level].name;

		Value* element =
			read_element( sds, index, manager, thing_list, level );

		record->AssignRecordElement( name, element );
		Unref( element );
		}
	}

Value* read_value_from_SDS( int sds, int event_type )
	{
	SDS_ValueManager* manager = new SDS_ValueManager( sds );

	Value* result;

	if ( event_type == SDS_OPAQUE_EVENT )
		{
		result = new Value( SDS_Index(sds) );
		Ref( manager );
		result->SetValueManager( manager );
		}

	else
		{
		struct sds_odesc* thing_list;

		// Check the name of the object; if empty then this
		// is a single array object; otherwise treat it as
		// a record.

		const char* object_name = sds_obind2name( sds, 1 );

		if ( ! object_name || ! *object_name )
			{ // Single array object.
			int level = sds_describe( sds, 1, &thing_list );
			result = read_single_value_from_SDS( manager,
						&thing_list[level] );
			}

		else
			{
			int num_sds_objects =
				(int) sds_array_size( sds, 0 ) - 1;

			result = create_record();

			int level;
			for ( int i = 1; i <= num_sds_objects; ++i )
				traverse_record( result, sds, i, manager,
						 thing_list, 1, level );

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

	sds_cleanup();

	return result;
	}


#define DEFINE_XXX_REL_OP_COMPUTE(name,type,coerce_func)		\
Value* name( const Value* lhs, const Value* rhs, int lhs_len, RelExpr* expr )\
	{								\
	bool lhs_copy, rhs_copy;					\
	type* lhs_array = lhs->coerce_func( lhs_copy, lhs_len );	\
	type* rhs_array = rhs->coerce_func( rhs_copy, rhs->Length() );	\
	bool* result = new bool[lhs_len];				\
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
	return new Value( result, lhs_len );				\
	}


DEFINE_XXX_REL_OP_COMPUTE(bool_rel_op_compute,bool,CoerceToBoolArray)
DEFINE_XXX_REL_OP_COMPUTE(int_rel_op_compute,int,CoerceToIntArray)
DEFINE_XXX_REL_OP_COMPUTE(float_rel_op_compute,float,CoerceToFloatArray)
DEFINE_XXX_REL_OP_COMPUTE(double_rel_op_compute,double,CoerceToDoubleArray)
DEFINE_XXX_REL_OP_COMPUTE(string_rel_op_compute,charptr,CoerceToStringArray)


bool compatible_types( const Value* v1, const Value* v2, glish_type& max_type )
	{
	max_type = v1->Type();
	glish_type t = v2->Type();

	if ( v1->IsNumeric() )
		{
		if ( ! v2->IsNumeric() )
			{
			error->Report( "numeric and non-numeric types mixed" );
			return false;
			}

		if ( max_type == TYPE_DOUBLE || t == TYPE_DOUBLE )
			max_type = TYPE_DOUBLE;
		else if ( max_type == TYPE_FLOAT || t == TYPE_FLOAT )
			max_type = TYPE_FLOAT;
		else if ( max_type == TYPE_INT || t == TYPE_INT )
			max_type = TYPE_INT;
		else
			max_type = TYPE_BOOL;
		}

	else
		{
		if ( t != max_type )
			{
			error->Report( "types are incompatible" );
			return false;
			}
		}

	return true;
	}


void init_values()
	{
	false_value = new Value( false );
	}


void delete_list( del_list* dlist )
	{
	if ( dlist )
		loop_over_list( *dlist, i )
			delete (*dlist)[i];
	}


bool text_to_double( const char text[], double& result )
	{
	char* text_ptr;
	result = strtod( text, &text_ptr );
	return bool(text_ptr == &text[strlen( text )]);
	}


bool text_to_integer( const char text[], int& result )
	{
	char* text_ptr;

	double d = strtod( text, &text_ptr );
	result = int( d );

	return bool(text_ptr == &text[strlen( text )] && d == result);
	}
