// $Header$

#ifndef value_h
#define value_h

#include "Glish/Dict.h"
#include "Glish/glish.h"
#include "Glish/GlishType.h"
#include "Glish/Object.h"


// Different types of values: constant references, references, and
// ordinary (non-indirect) values.
typedef enum { VAL_CONST, VAL_REF, VAL_VAL } value_type;

// Different types of storage for an array used to construct a Value.
typedef enum {
	COPY_ARRAY,		// copy the array
	TAKE_OVER_ARRAY,	// use the array, delete it when done with it
	PRESERVE_ARRAY,		// use the array, don't delete it or grow it
	} array_storage_type;


class Value;
class Agent;
class Func;
class ArithExpr;
class RelExpr;
struct record_header;	// Needed when dealing with SDS; see AddToSds()

typedef const char* charptr;
typedef Func* funcptr;
typedef Agent* agentptr;

declare(PList,Value);
typedef PList(Value) value_list;

declare(PDict,Value);
typedef PDict(Value)* recordptr;


// Class used to differentiate integers that are SDS indices from
// just plain integers.
class SDS_Index {
public:
	SDS_Index( int ind )	{ index = ind; }
	int Index() const	{ return index; }

protected:
	int index;
	};


// Used to create lists of dynamic memory that should be freed.
declare(PList,void);
typedef PList(void) del_list;


#define copy_array(src,dest,len,type) \
	memcpy( (void*) dest, (void*) src, sizeof(type) * len )

#define copy_values(src,type) \
	copy_array( src, (void *) new type[len], length, type )


// The number of Value objects created and deleted.  Useful for tracking
// down inefficiencies and leaks.
extern int num_Values_created;
extern int num_Values_deleted;


class Value : public GlishObject {
    public:
	Value( bool value );
	Value( int value );
	Value( float value );
	Value( double value );
	Value( const char* value );
	Value( funcptr value );

	Value( agentptr value );

	Value( recordptr value, Agent* agent = 0 );

	Value( SDS_Index& sds_index );

	Value( Value* ref_value, value_type val_type );

	Value( bool value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( int value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( float value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( double value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( charptr value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( funcptr value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );

	// Discard present value and instead take new_value.
	void TakeValue( Value* new_value );

	~Value();


	// A value manager is an object that is Unref()'d upon destruction
	// of a Value in lieu of deleting the Value's "values" member
	// variable.  Presumably the value manager knows something about
	// "values" and when deleted performs some sort of cleanup on it
	// (perhaps by deleting it, perhaps by changing its reference count,
	// perhaps by leaving it alone, etc.).
	void SetValueManager( GlishObject* manager )
		{ value_manager = manager; }


	glish_type Type() const			{ return type; }
	unsigned int Length() const
		{
		if ( Type() == TYPE_RECORD )
			return RecordPtr()->Length();
		else
			return length;
		}

	// True if the value is a reference.
	bool IsRef() const
		{
		return (type == TYPE_REF || type == TYPE_CONST) ?
			glish_true : glish_false;
		}

	// True if the value is a constant reference.
	bool IsConst() const
		{
		return (type == TYPE_CONST) ? glish_true : glish_false;
		}

	// True if the value makes sense as a numeric type (i.e.,
	// bool, integer, or floating-point).
	bool IsNumeric() const;

	// True if the value is a record corresponding to a agent.
	bool IsAgentRecord() const;

	// Returns the "n"'th element coereced to the corresponding type.
	bool BoolVal( int n = 1 ) const;
	int IntVal( int n = 1 ) const;
	double DoubleVal( int n = 1 ) const;

	// Returns the entire value converted to a single string, with
	// "sep" used to separate array elements.
	char* StringVal( char sep = ' ' ) const;	// returns a new string

	// Returns the agent or function corresponding to the Value.
	Agent* AgentVal() const;
	funcptr FuncVal() const;

	// Returns the Value's SDS index, if TYPE_OPAQUE.  Returns
	// SDS_NO_SUCH_SDS if the value is not TYPE_OPAQUE.
	int SDS_IndexVal() const;


	// The following accessors return pointers to the underlying value
	// array.  The "const" versions complain with a fatal error if the
	// value is not the given type.  The non-const versions first
	// Polymorph() the values to the given type.
	bool* BoolPtr() const;
	int* IntPtr() const;
	float* FloatPtr() const;
	double* DoublePtr() const;
	charptr* StringPtr() const;
	funcptr* FuncPtr() const;
	agentptr* AgentPtr() const;
	recordptr RecordPtr() const;

	bool* BoolPtr();
	int* IntPtr();
	float* FloatPtr();
	double* DoublePtr();
	charptr* StringPtr();
	funcptr* FuncPtr();
	agentptr* AgentPtr();
	recordptr RecordPtr();

	Value* RefPtr() const		{ return (Value*) values; }

	// Follow the reference chain of a non-constant or constant value
	// until finding its non-reference base value.
	Value* Deref();
	const Value* Deref() const;

	// Return a copy of the Value's contents coerced to an array
	// of the given type.  If the Value has only one element then
	// "size" copies of that element are returned (this is used
	// for promoting scalars to arrays in operations that mix
	// the two).  Otherwise, the first "size" elements are coerced
	// and returned.
	//
	// If the value cannot be coerced to the given type then a nil
	// pointer is returned.
	bool* CoerceToBoolArray( bool& is_copy, int size,
					bool* result = 0 ) const;
	int* CoerceToIntArray( bool& is_copy, int size,
					int* result = 0 ) const;
	float* CoerceToFloatArray( bool& is_copy, int size,
					float* result = 0 ) const;
	double* CoerceToDoubleArray( bool& is_copy, int size,
					double* result = 0 ) const;
	charptr* CoerceToStringArray( bool& is_copy, int size,
					charptr* result = 0 ) const;

	// These coercions are very limited: they essentially either
	// return the corresponding xxxPtr() (if the sizes match,
	// no "result" is prespecified, and the Value is already
	// the given type) or generate a fatal error.
	funcptr* CoerceToFuncArray( bool& is_copy, int size,
					funcptr* result = 0 ) const;

	Value* operator[]( const Value* index ) const;	// returns a new Value
	Value* RecordRef( const Value* index ) const;	// returns a new Value

	// Returns an (unmodifiable) existing Value, or false_value if the
	// given field does not exist.
	const Value* ExistingRecordElement( const Value* index ) const;
	const Value* ExistingRecordElement( const char field[] ) const;

	// Returns a modifiable existing Value.  If the given field does
	// not exist, it is added, with an initial value of false.
	Value* GetOrCreateRecordElement( const Value* index );
	Value* GetOrCreateRecordElement( const char field[] );

	// Returns the given record element if it exists, 0 otherwise.
	// (The value must already have been tested to determine that it's
	// a record.)
	Value* HasRecordElement( const char field[] ) const;

	// Returns a modifiable existing Value, or if no field exists
	// with the given name, returns 0.
	Value* Field( const Value* index );
	Value* Field( const char field[] );

	// Returns the given field, polymorphed to the given type.
	Value* Field( const char field[], glish_type t );

	// Returns a modifiable existing Value of the nth field of a record,
	// with the first field being numbered 1.  Returns 0 if the field
	// does not exist (n is out of range) or the Value is not a record.
	Value* NthField( int n );
	const Value* NthField( int n ) const;

	// Returns a non-modifiable pointer to the nth field's name.
	// Returns 0 in the same cases as NthField does.
	const char* NthFieldName( int n ) const;

	// Returns a copy of a unique field name (one not already present)
	// for the given record, or 0 if the Value is not a record.
	//
	// The name has an embedded '*' to avoid collision with user-chosen
	// names.
	char* NewFieldName();

	// Returns a pointer to the underlying values of the given field,
	// polymorphed to the indicated type.  The length of the array is
	// returned in "len".  A nil pointer is returned the Value is not
	// a record or if it doesn't contain the given field.
	bool* FieldBoolPtr( const char field[], int& len );
	int* FieldIntPtr( const char field[], int& len );
	float* FieldFloatPtr( const char field[], int& len );
	double* FieldDoublePtr( const char field[], int& len );
	charptr* FieldStringPtr( const char field[], int& len );

	// Looks for a field with the given name.  If present, returns true,
	// and in the second argument the scalar value corresponding to that
	// field polymorphed to the appropriate type.  If not present, returns
	// false.  The optional third argument specifies which element of a
	// multi-element value to return (not applicable when returning a
	// string).
	bool FieldVal( const char field[], bool& val, int n = 1 );
	bool FieldVal( const char field[], int& val, int n = 1 );
	bool FieldVal( const char field[], double& val, int n = 1 );

	// Returns a new string in "val".
	bool FieldVal( const char field[], charptr& val );


	// The following SetField member functions take a field name and
	// arguments for creating a numeric or string Value.  The target
	// Value must be a record or a fatal error is generated.  A new
	// Value is constructing given the arguments and assigned to the
	// given field.

	void SetField( const char field[], bool value );
	void SetField( const char field[], int value );
	void SetField( const char field[], float value );
	void SetField( const char field[], double value );
	void SetField( const char field[], const char* value );

	void SetField( const char field[], bool value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetField( const char field[], int value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetField( const char field[], float value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetField( const char field[], double value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetField( const char field[], charptr value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );

	void SetField( const char field[], Value* value )
		{ AssignRecordElement( field, value ); }


	// General assignment of "this[index] = value", where "this" might
	// be a record or an array type.
	void AssignElements( const Value* index, Value* value );

	// Assigns a single record element to the given value.  Note
	// that the value may or may not wind up being copied (depending
	// on whether the record element is a reference or not).  The
	// caller should "Unref( xxx )" after the call, where "xxx"
	// is either "value" or some larger value of which "value" is
	// an element.  If AssignRecordElement needs the value to stick
	// around, it will have bumped its reference count.
	void AssignRecordElement( const char* index, Value* value );

	void Negate();	// value <- -value
	void Not();	// value <- ! value

	void IntOpCompute( const Value* value, int lhs_len, ArithExpr* expr );
	void FloatOpCompute( const Value* value, int lhs_len, ArithExpr* expr );
	void DoubleOpCompute( const Value* value, int lhs_len,
				ArithExpr* expr );


	// Add the Value to the sds designated by "sds" using the given
	// name.  "dlist" is a del_list (PList of void) that is used to
	// record any dynamic memory required by AddToSds in order to
	// construct the SDS.  Once done with the SDS (and the SDS has
	// been destroyed), "delete_list( dlist )" should be called to
	// reclaim the memory that AddToSds needed.
	//
	// The "rh" argument is a pointer to the SDS record header
	// describing the record we're inside.  If this Value object is
	// not part of some larger record, then in the AddToSds call "rh"
	// will be nil.  But if we're part of a record, then "rh" will
	// give us a pointer to the SDS data structure corresponding to
	// that record.
	//
	// Additionally, "level" indicates how deep we are into a
	// record.  A level of 0 indicates we're at the top-level; 1
	// indicates we're dealing with a subrecord, 2 a subsubrecord,
	// etc.  We need to know this information because we do different
	// things for the cases level=0, level=1, and level=n for n > 1.
	void AddToSds( int sds, del_list* dlist, const char* name = 0,
			struct record_header* rh = 0, int level = 0 ) const;


	// Change from present type to given type.
	void Polymorph( glish_type new_type );


	void DescribeSelf( ostream& s ) const;

    protected:
	void SetValue( bool array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetValue( int array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetValue( float array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetValue( double array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetValue( const char* array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetValue( agentptr array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetValue( funcptr array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetValue( recordptr value, Agent* agent );
	void SetValue( SDS_Index& array );

	void SetType( glish_type new_type );
	void DeleteValue();

	// Given an array index value, returns an array of integers
	// listing those elements indicated by the index.  Returns
	// nil if the index was invalid.  "num_indices" is updated
	// with the number of indices returned; "indices_are_copy"
	// indicates that the indices should be deleted once done
	// with using them.
	int* GenerateIndices( const Value* index, int& num_indices,
				bool& indices_are_copy ) const;

	// Return a new value holding the specified subelement(s).
	Value* ArrayRef( int* indices, int num_indices ) const;

	// Returns a slice of a record at the given indices.
	Value* RecordSlice( int* indices, int num_indices ) const;

	// Assign the specified subelements to copies of the corresponding
	// values.
	void AssignRecordElements( const Value* index, Value* value );
	void AssignRecordSlice( Value* value, int* indices, int num_indices );
	void AssignArrayElements( Value* value, int* indices,
					int num_indices );

	// Does the actual work of assigning a list of array elements,
	// once type-checking has been done.
	void AssignArrayElements( int* indices, int num_indices,
				Value* value, int rhs_len );

	// Searches a list of indices to find the largest and returns
	// it in "max_index".  If an invalid (< 1) index is found, a
	// error is generated and false is returned; otherwise true
	// is returned.
	bool IndexRange( int* indices, int num_indices, int& max_index )
			const;

	char* RecordStringVal() const; // returns a new string

	// Increase array size from present value to given size.
	// If successful, true is returned.  If this can't be done for
	// our present type, an error message is generated and false is return.
	bool Grow( unsigned int new_size );

	glish_type type;

	unsigned int length;
	unsigned int max_size;
	void* values;
	array_storage_type storage;
	GlishObject* value_manager;
	};


extern const Value* false_value;

extern Value* create_record();
extern recordptr create_record_dict();

extern Value* bool_rel_op_compute( const Value* lhs, const Value* rhs,
				int lhs_len, RelExpr* expr );
extern Value* int_rel_op_compute( const Value* lhs, const Value* rhs,
				int lhs_len, RelExpr* expr );
extern Value* float_rel_op_compute( const Value* lhs, const Value* rhs,
				int lhs_len, RelExpr* expr );
extern Value* double_rel_op_compute( const Value* lhs, const Value* rhs,
				int lhs_len, RelExpr* expr );
extern Value* string_rel_op_compute( const Value* lhs, const Value* rhs,
				int lhs_len, RelExpr* expr );

extern Value* copy_value( const Value* value );

extern Value* read_value_from_SDS( int sds, int event_type );

extern bool compatible_types( const Value* v1, const Value* v2,
				glish_type& max_type );

extern void init_values();

extern void delete_list( del_list* dlist );

// The following convert a string to either a double or an int.  They
// return true if the conversion was successful, false if the text
// does not describe a valid double or integer.
extern bool text_to_double( const char text[], double& result );
extern bool text_to_integer( const char text[], int& result );

#endif /* value_h */
