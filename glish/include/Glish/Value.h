// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef value_h
#define value_h

#include "Glish/Dict.h"
#include "Glish/glish.h"
#include "Glish/GlishType.h"
#include "Glish/Object.h"
#include "Glish/Complex.h"
#include "Glish/ValKern.h"

// Different types of values: constant references, references, and
// ordinary (non-indirect) values.
typedef enum { VAL_CONST, VAL_REF, VAL_VAL } value_type;

// Different types of storage for an array used to construct a Value.
typedef enum {
	COPY_ARRAY,		// copy the array
	TAKE_OVER_ARRAY,	// use the array, delete it when done with it
	PRESERVE_ARRAY		// use the array, don't delete it or grow it
	} array_storage_type;


class Value;
struct complex;
struct dcomplex;
struct record_header;	// Needed when dealing with SDS; see AddToSds()

// also declared in Sequencer.h
// stubbed for Clients
extern int lookup_print_precision( );
extern int lookup_print_limit( );

declare(PList,Value);
typedef PList(Value) value_list;

typedef const Value* const_value;
#define const_value_to_void void_ptr
#define void_to_const_value const_value
declare(List,const_value);
typedef List(const_value) const_value_list;

// Classes for subvector references.
class VecRef;
#define SubVecRef(type) name2(type,SubVecRef)
class SubVecRef(glish_bool);
class SubVecRef(byte);
class SubVecRef(short);
class SubVecRef(int);
class SubVecRef(float);
class SubVecRef(double);
class SubVecRef(complex);
class SubVecRef(dcomplex);
class SubVecRef(charptr);

typedef SubVecRef(glish_bool) glish_boolref;
typedef SubVecRef(byte) byteref;
typedef SubVecRef(short) shortref;
typedef SubVecRef(int) intref;
typedef SubVecRef(float) floatref;
typedef SubVecRef(double) doubleref;
typedef SubVecRef(complex) complexref;
typedef SubVecRef(dcomplex) dcomplexref;
typedef SubVecRef(charptr) charptrref;


// Used to create lists of objects or dynamic memory that should be freed.
class DelObj;
declare(PList,DelObj);
typedef PList(DelObj) del_list;


#define copy_array(src,dest,len,type) \
	memcpy( (void*) dest, (void*) src, sizeof(type) * len )

#define copy_values(src,type) \
	copy_array( src, (void *) new type[len], length, type )

extern Value* copy_value( const Value* value );

extern const Value* false_value;
extern Value* empty_value();
extern Value* error_value( );
extern Value* error_value( const char *message );
extern Value* error_value( const char *message, const char *file, int line );

extern Value* create_record();

// The number of Value objects created and deleted.  Useful for tracking
// down inefficiencies and leaks.
extern int num_Values_created;
extern int num_Values_deleted;

class Value : public GlishObject {
friend class IValue;
public:
	// Create a <fail> value
	Value( );
	Value( const char *message, const char *file, int lineNum );

	Value( const Value &v ) : kernel(v.kernel), attributes( v.CopyAttributePtr() ),
				value_manager(0)
		{
		DIAG2( (void*) this, "Value( const Value& )" )
		description = 0;
		++num_Values_created;
		}

	Value( glish_bool value );
	Value( byte value );
	Value( short value );
	Value( int value );
	Value( float value );
	Value( double value );
	Value( complex value );
	Value( dcomplex value );
	Value( const char* value );

	Value( recordptr value );

	// Reference constructor.
	Value( Value* ref_value, value_type val_type );

	// Subref constructor.
	Value( Value* ref_value, int index[], int num_elements,
		value_type val_type, int take_index = 0 );

	Value( glish_bool value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( byte value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( short value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( int value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( float value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( double value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( complex value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( dcomplex value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );
	Value( charptr value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );

	Value( glish_boolref& value_ref );
	Value( byteref& value_ref );
	Value( shortref& value_ref );
	Value( intref& value_ref );
	Value( floatref& value_ref );
	Value( doubleref& value_ref );
	Value( complexref& value_ref );
	Value( dcomplexref& value_ref );
	Value( charptrref& value_ref );


	// Discard present value and instead take new_value.
	void TakeValue( Value* new_value, Str &err = glish_errno );

	virtual ~Value();


	// A value manager is an object that is Unref()'d upon destruction
	// of a Value in lieu of deleting the Value's "values" member
	// variable.  Presumably the value manager knows something about
	// "values" and when deleted performs some sort of cleanup on it
	// (perhaps by deleting it, perhaps by changing its reference count,
	// perhaps by leaving it alone, etc.).
	void SetValueManager( GlishObject* manager )
		{ value_manager = manager; }


	glish_type Type() const			{ return kernel.Type(); }
	int Length() const
		{ return IsRef() ? Deref()->Length() : kernel.Length();	}

	// True if the value is a reference.
	int IsRef() const 
		{ return kernel.Type() == TYPE_REF; }

	// True if the value is a constant value.
	int IsConst() const { return kernel.IsConst(); }
	void MakeConst() { kernel.MakeConst(); }

	// A value can be reassigned, but not changed, e.g. by array operations
	void MakeModConst() { kernel.MakeModConst(); }
	int IsModConst() const { return kernel.IsModConst(); }
	int IsFieldConst() const { return Type() == TYPE_RECORD && IsModConst(); }
	int IsRefConst() const { return Type() == TYPE_REF && IsModConst(); }
  
	// True if the value is a sub-vector reference.
	int IsVecRef() const { return kernel.Type() == TYPE_SUBVEC_REF; }

	// True if the value makes sense as a numeric type (i.e.,
	// bool, integer, or floating-point).
	int IsNumeric() const;

	// Returns the "n"'th element coereced to the corresponding type.
	glish_bool BoolVal( int n = 1, Str &err = glish_errno ) const;
	byte ByteVal( int n = 1, Str &err = glish_errno ) const;
	short ShortVal( int n = 1, Str &err = glish_errno ) const;
	int IntVal( int n = 1, Str &err = glish_errno ) const;
	float FloatVal( int n = 1, Str &err = glish_errno ) const;
	double DoubleVal( int n = 1, Str &err = glish_errno ) const;
	complex ComplexVal( int n = 1, Str &err = glish_errno ) const;
	dcomplex DcomplexVal( int n = 1, Str &err = glish_errno ) const;

	// Returns the entire value converted to a single string, with
	// "sep" used to separate array elements.  "max_elements" allows
	// one to specify the maximum number of elements to be printed. If
	// "max_elements" is zero, all elements will be printed, but if
	// it is greater than zero only "max_elements" will be printed.
	// If "use_attr" is true (non-zero), then the value's attributes
	// are used for determining its shape (as a n-D array).
	//
	// Returns a new string, which should be delete'd when done with.
	char* StringVal( char sep = ' ', unsigned int max_elements = 0, 
			 int use_attr = 0, Str &err = glish_errno ) const;

	// Returns the limit on the number elements to be printed given
	// the state of the "system" value, and the attributes of this
	// value. This may be useful when used with "StringVal()" as the
	// "max_elements" parameter.
	unsigned int PrintLimit() const;

	// The following accessors return pointers to the underlying value
	// array.  The "const" versions complain with a fatal error if the
	// value is not the given type.  The non-const versions first
	// Polymorph() the values to the given type.  If called for a
	// subref, retrieves the complete underlying value, not the
	// just selected subelements.  (See the XXXRef() functions below.)
	//
	// The 'modify' flag indicates that a modifiable copy of the values
	// should be retrieved. If this flag is set to '0', then a
	// non-modifiable version is retrieved.
	glish_bool* BoolPtr( int modify=1 ) const;
	byte* BytePtr( int modify=1 ) const;
	short* ShortPtr( int modify=1 ) const;
	int* IntPtr( int modify=1 ) const;
	float* FloatPtr( int modify=1 ) const;
	double* DoublePtr( int modify=1 ) const;
	complex* ComplexPtr( int modify=1 ) const;
	dcomplex* DcomplexPtr( int modify=1 ) const;
	charptr* StringPtr( int modify=1 ) const;
	recordptr RecordPtr( int modify=1 ) const;

	glish_bool* BoolPtr( int modify=1 );
	byte* BytePtr( int modify=1 );
	short* ShortPtr( int modify=1 );
	int* IntPtr( int modify=1 );
	float* FloatPtr( int modify=1 );
	double* DoublePtr( int modify=1 );
	complex* ComplexPtr( int modify=1 );
	dcomplex* DcomplexPtr( int modify=1 );
	charptr* StringPtr( int modify=1 );
	recordptr RecordPtr( int modify=1 );

	Value* RefPtr() const		{ return kernel.GetValue(); }

	// The following accessors are for accessing sub-array references.
	// They complain with a fatal error if the value is not a sub-array
	// reference.  Otherwise they return a reference to the underlying
	// *sub*elements. The "const" versions complain with a fatal error
	// if the value is not the given type.  The non-const versions 
	// first Polymorph() the values to the given type.
	glish_boolref& BoolRef() const;
	byteref& ByteRef() const;
	shortref& ShortRef() const;
	intref& IntRef() const;
	floatref& FloatRef() const;
	doubleref& DoubleRef() const;
	complexref& ComplexRef() const;
	dcomplexref& DcomplexRef() const;
	charptrref& StringRef() const;

	glish_boolref& BoolRef();
	byteref& ByteRef();
	shortref& ShortRef();
	intref& IntRef();
	floatref& FloatRef();
	doubleref& DoubleRef();
	complexref& ComplexRef();
	dcomplexref& DcomplexRef();
	charptrref& StringRef();

	VecRef* VecRefPtr() const	{ return kernel.GetVecRef(); }

	// Follow the reference chain of a non-constant or constant value
	// until finding its non-reference base value.
	Value* Deref();
	const Value* Deref() const;

	Value* VecRefDeref();
	const Value* VecRefDeref() const;

	// Return a copy of the Value's contents coerced to an array
	// of the given type.  If the Value has only one element then
	// "size" copies of that element are returned (this is used
	// for promoting scalars to arrays in operations that mix
	// the two).  Otherwise, the first "size" elements are coerced
	// and returned.
	//
	// If the value cannot be coerced to the given type then a nil
	// pointer is returned.
	//
	// NOTE: if the value is returned with 'is_copy' clear, then
	//       the value should not be modified.
	glish_bool* CoerceToBoolArray( int& is_copy, int size,
		glish_bool* result = 0 ) const;
	byte* CoerceToByteArray( int& is_copy, int size,
			byte* result = 0 ) const;
	short* CoerceToShortArray( int& is_copy, int size,
			short* result = 0 ) const;
	int* CoerceToIntArray( int& is_copy, int size,
			int* result = 0 ) const;
	float* CoerceToFloatArray( int& is_copy, int size,
			float* result = 0 ) const;
	double* CoerceToDoubleArray( int& is_copy, int size,
			double* result = 0 ) const;
	complex* CoerceToComplexArray( int& is_copy, int size,
			complex* result = 0 ) const;
	dcomplex* CoerceToDcomplexArray( int& is_copy, int size,
			dcomplex* result = 0 ) const;
	charptr* CoerceToStringArray( int& is_copy, int size,
			charptr* result = 0 ) const;

	// Both of the following return a newed value.
	Value* operator[]( const Value* index ) const;
	Value* operator[]( const_value_list *index ) const;

	// Pick distinct elements from an array.
	// Returns a newed value
	Value* Pick( const Value* index ) const;

	// Return a reference to distinct elements from an array.
	// Returns a newed value
	Value* PickRef( const Value* index );

	// Assign to distinct array elements.
	void PickAssign( const Value* index, Value *value );

	// Return a true sub-array reference.
	// Both of the following return a newed value.
	Value* SubRef( const Value* index );
	Value* SubRef( const_value_list *args_val );

	// Returns a newed value
	Value* RecordRef( const Value* index ) const;

	// Returns an (unmodifiable) existing Value, or false_value if the
	// given field does not exist.
	const Value* ExistingRecordElement( const Value* index ) const;
	const Value* ExistingRecordElement( const char field[] ) const;

	// Returns a modifiable existing Value.  If the given field does
	// not exist, it is added, with an initial value of F.
	Value* GetOrCreateRecordElement( const Value* index );
	Value* GetOrCreateRecordElement( const char field[] );

	// Returns the given record element if it exists, 0 otherwise.
	// (The value must already have been tested to determine that it's
	// a record.)
	const Value* HasRecordElement( const char field[] ) const;

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
	glish_bool* FieldBoolPtr( const char field[], int& len, int modify=1 );
	byte* FieldBytePtr( const char field[], int& len, int modify=1 );
	short* FieldShortPtr( const char field[], int& len, int modify=1 );
	int* FieldIntPtr( const char field[], int& len, int modify=1 );
	float* FieldFloatPtr( const char field[], int& len, int modify=1 );
	double* FieldDoublePtr( const char field[], int& len, int modify=1 );
	complex* FieldComplexPtr( const char field[], int& len, int modify=1 );
	dcomplex* FieldDcomplexPtr( const char field[], int& len, int modify=1 );
	charptr* FieldStringPtr( const char field[], int& len, int modify=1 );

	// Looks for a field with the given name.  If present, returns true,
	// and in the second argument the scalar value corresponding to that
	// field polymorphed to the appropriate type.  If not present, returns
	// false.  The optional third argument specifies which element of a
	// multi-element value to return (not applicable when returning a
	// string).
	int FieldVal( const char field[], glish_bool& val, int n = 1, Str &err = glish_errno );
	int FieldVal( const char field[], byte& val, int n = 1, Str &err = glish_errno );
	int FieldVal( const char field[], short& val, int n = 1, Str &err = glish_errno );
	int FieldVal( const char field[], int& val, int n = 1, Str &err = glish_errno );
	int FieldVal( const char field[], float& val, int n = 1, Str &err = glish_errno );
	int FieldVal( const char field[], double& val, int n = 1, Str &err = glish_errno );
	int FieldVal( const char field[], complex& val, int n = 1, Str &err = glish_errno );
	int FieldVal( const char field[], dcomplex& val, int n = 1, Str &err = glish_errno );

	// Returns a new string in "val".
	int FieldVal( const char field[], char*& val );


	// The following SetField member functions take a field name and
	// arguments for creating a numeric or string Value.  The target
	// Value must be a record or a fatal error is generated.  A new
	// Value is constructing given the arguments and assigned to the
	// given field.

	void SetField( const char field[], glish_bool value );
	void SetField( const char field[], byte value );
	void SetField( const char field[], short value );
	void SetField( const char field[], int value );
	void SetField( const char field[], float value );
	void SetField( const char field[], double value );
	void SetField( const char field[], complex value );
	void SetField( const char field[], dcomplex value );
	void SetField( const char field[], const char* value );

	void SetField( const char field[], glish_bool value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetField( const char field[], byte value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetField( const char field[], short value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetField( const char field[], int value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetField( const char field[], float value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetField( const char field[], double value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetField( const char field[], complex value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetField( const char field[], dcomplex value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetField( const char field[], charptr value[], int num_elements,
			array_storage_type storage = TAKE_OVER_ARRAY );

	void SetField( const char field[], Value* value )
		{ AssignRecordElement( field, value ); }


	// General assignment of "this[index] = value", where "this" might
	// be a record or an array type.  Second form is for n-D arrays.
	void AssignElements( const Value* index, Value* value );
	void AssignElements( const_value_list* index, Value* value );

	// Assigns the elements of the value parameter to the corresponding 
	// elements of this.
	void AssignElements( Value* value );

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

	// Change from present type to given type.
	virtual void Polymorph( glish_type new_type );
	void VecRefPolymorph( glish_type new_type );

	// Retrieves all attributes as a Value, non-modifiable
	const Value *GetAttributes() const { return attributes; }

	// Retrieve the non-modifiable set of attributes, possibly nil.
	const attributeptr AttributePtr() const
		{
		return attributes ? attributes->RecordPtr(0) : 0;
		}

	// Retrieve a modifiable, non-nil set of attributes.
	attributeptr ModAttributePtr()
		{
		InitAttributes();
		return attributes->RecordPtr();
		}

	// Retrieve a copy of a (possibly nil) attribute set.
	Value* CopyAttributePtr() const
		{
		return attributes ? copy_value( attributes ) : 0;
		}

	// Returns an (unmodifiable) existing Value, or false_value if the
	// given attribute does not exist.
	const Value* ExistingAttribute( const Value* index ) const
		{
		return attributes ?
			attributes->ExistingRecordElement( index ) :
			false_value;
		}
	const Value* ExistingAttribute( const char attribute[] ) const
		{
		return attributes ?
			attributes->ExistingRecordElement( attribute ) :
			false_value;
		}

	// Returns the given attribute if it exists, 0 otherwise.
	const Value* HasAttribute( const char attribute[] ) const
		{
                return attributes ?
			attributes->HasRecordElement( attribute ) : 0;
		}

	// Returns a new Value with the selected attributes.
	Value* AttributeRef( const Value* index ) const;

	// Returns a modifiable existing Value.  If the given field does
	// not exist, it is added, with an initial value of F.
	Value* GetOrCreateAttribute( const Value* index )
		{
		InitAttributes();
		return attributes->GetOrCreateRecordElement( index );
		}
	Value* GetOrCreateAttribute( const char field[] )
		{
		InitAttributes();
		return attributes->GetOrCreateRecordElement( field );
		}

	// Perform an assignment on an attribute value.
	void AssignAttribute( const char* index, Value* value )
		{
		InitAttributes();
		attributes->AssignRecordElement( index, value );
		}

	// Deletes a particular attribute. If the attribute does
	// not exist, NO action is performed.
	void DeleteAttribute( const Value* index );
	void DeleteAttribute( const char field[] );

	// Take new attributes from the given value.
	void CopyAttributes( const Value* value )
		{
		DeleteAttributes();
		attributes = value->CopyAttributePtr();
		}

	// Sets all of a Value's attributes (can be nil).
	void AssignAttributes( Value* a )
		{
		DeleteAttributes();
		attributes = a;
		}

	// Takes the attributes from a value without modifying them.
	Value* TakeAttributes()
		{
		Value* a = attributes;
		attributes = 0;
		return a;
		}

	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

	// Provide the rudiments of copy on write... i.e. it copies
	// when necessary.
	//
	// Public to allow users to implement their own copy on write
	// until copy on write is a formal part of Glish.
	Value* CopyUnref()
		{
		if ( RefCount() == 1 )
			return this;
		else
			{
			Unref( this );	// Safe!
			Value* copy = copy_value( this );
			return copy;
			}
		}

	int Bytes( int addPerValue = sizeof(ValueKernel::header) ) const;
	int ToMemBlock(char *memory, int offset = 0) const;

protected:

	Value ( glish_type ) { }		// for IValue

	void SetValue( glish_boolref& value_ref );
	void SetValue( byteref& value_ref );
	void SetValue( shortref& value_ref );
	void SetValue( intref& value_ref );
	void SetValue( floatref& value_ref );
	void SetValue( doubleref& value_ref );
	void SetValue( complexref& value_ref );
	void SetValue( dcomplexref& value_ref );
	void SetValue( charptrref& value_ref );

	void SetValue( Value *ref_value, int index[], int num_elements, 
			value_type val_type, int take_index = 0 );

	virtual void DeleteValue();
	void DeleteAttributes();

	void InitValue();
	void InitRecord( recordptr r );

	void InitAttributes()
		{
		if ( ! attributes )
			attributes = create_record();
		}

	// Given an array index value, returns an array of integers
	// listing those elements indicated by the index.  Returns
	// nil if the index was invalid.  "num_indices" is updated
	// with the number of indices returned; "indices_are_copy"
	// indicates that the indices should be deleted once done
	// with using them.
	int* GenerateIndices( const Value* index, int& num_indices,
				int& indices_are_copy,
				int check_size = 1 ) const;

	// Return a new value holding the specified subelement(s).
	Value* ArrayRef( int* indices, int num_indices ) const;

	// Return a new value holding a reference the specified subelement(s).
	Value* TrueArrayRef( int* indices, int num_indices, int take_indices = 0 ) const;

	// Returns a slice of a record at the given indices.
	Value* RecordSlice( int* indices, int num_indices ) const;

	// Assign the specified subelements to copies of the corresponding
	// values.
	void AssignRecordElements( const Value* index, Value* value );
	void AssignRecordSlice( Value* value, int* indices, int num_indices );
	void AssignArrayElements( Value* value, int* indices,
					int num_indices );
	void AssignArrayElements( const_value_list* index, Value* value );

	// Assigns the elements from record parameter to the corresponding 
	// elements in this object.
	void AssignRecordElements( Value* value );

	// Copies the elements from the value parameter. It assumes
	// that the sizes are compatible, and generates a warning,
	// and copies a portion otherwise.
	virtual void AssignArrayElements( Value* value );

	// Does the actual work of assigning a list of array elements,
	// once type-checking has been done.
	virtual void AssignArrayElements( int* indices, int num_indices,
				Value* value, int rhs_len );

	// Searches a list of indices to find the largest and returns
	// it in "max_index".  If an invalid (< 1) index is found, a
	// error is generated and false is returned; otherwise true
	// is returned.
	int IndexRange( int* indices, int num_indices, int& max_index ) const;

	// returns a new string
	char* RecordStringVal( char sep = ' ', unsigned int max_elements = 0, 
			int use_attr = 0, Str &err = glish_errno ) const;

	// Increase array size from present value to given size.
	// If successful, true is returned.  If this can't be done for
	// our present type, an error message is generated and false is return.
	int Grow( unsigned int new_size );

	GlishObject* value_manager;

	ValueKernel kernel;
	Value* attributes;
	};

// We couldn't do this earlier, because some of the inline definitions
// for the VecRef class depend on inline Value functions (and vice versa).

#include "VecRef.h"

extern int compatible_types( const Value* v1, const Value* v2,
				glish_type& max_type );

extern void init_values();
extern void finalize_values();

extern void delete_list( del_list* dlist );

extern charptr *csplit( char* source, int &len, char* split_chars = " \t\n" );
extern Value* split( char* source, char* split_chars = " \t\n" );

// The following convert a string to integer/double/dcomplex.  They
// set successful to return true if the conversion was successful,
// false if the text does not describe a valid integer/double/dcomplex.
extern int text_to_integer( const char text[], int& successful );
extern double text_to_double( const char text[], int& successful );
extern dcomplex text_to_dcomplex( const char text[], int& successful );

const char *print_decimal_prec( const attributeptr attr, const char *default_fmt = "%g" );

extern Value *ValueFromMemBlock( char *memory, int &offset );
extern Value *ValueFromMemBlock( char *memory );

extern Value *Fail( const RMessage&, const RMessage& = EndMessage,
	const RMessage& = EndMessage, const RMessage& = EndMessage,
	const RMessage& = EndMessage, const RMessage& = EndMessage,
	const RMessage& = EndMessage, const RMessage& = EndMessage,
	const RMessage& = EndMessage, const RMessage& = EndMessage,
	const RMessage& = EndMessage, const RMessage& = EndMessage,
	const RMessage& = EndMessage, const RMessage& = EndMessage,
	const RMessage& = EndMessage, const RMessage& = EndMessage,
	const RMessage& = EndMessage 
	);

extern Value *Fail( );

// This is a real bother, but it is the best compromise available. C++ has
// no virtual constructors, but when compiling the Glish interpreter, we
// want IValues to be created, and when compiling the clients, we want
// Values to be created. In main.cc, these are defined and return an
// IValue* they are also defined in glishlib.cc (which goes into libglish)
// and return a Value*
extern Value *create_value( );
extern Value *create_value( const char *message, const char *file, int line );
extern Value *create_value( const Value &value );
extern Value *create_value( glish_bool value );
extern Value *create_value( byte value );
extern Value *create_value( short value );
extern Value *create_value( int value );
extern Value *create_value( float value );
extern Value *create_value( double value );
extern Value *create_value( complex value );
extern Value *create_value( dcomplex value );
extern Value *create_value( const char* value );
extern Value *create_value( recordptr value );
extern Value *create_value( Value* ref_value, value_type val_type );
extern Value *create_value( Value* ref_value, int index[], int num_elements,
			    value_type val_type, int take_index = 0 );
extern Value *create_value( glish_bool value[], int num_elements,
	array_storage_type storage = TAKE_OVER_ARRAY );
extern Value *create_value( byte value[], int num_elements,
	array_storage_type storage = TAKE_OVER_ARRAY );
extern Value *create_value( short value[], int num_elements,
	array_storage_type storage = TAKE_OVER_ARRAY );
extern Value *create_value( int value[], int num_elements,
	array_storage_type storage = TAKE_OVER_ARRAY );
extern Value *create_value( float value[], int num_elements,
	array_storage_type storage = TAKE_OVER_ARRAY );
extern Value *create_value( double value[], int num_elements,
	array_storage_type storage = TAKE_OVER_ARRAY );
extern Value *create_value( complex value[], int num_elements,
	array_storage_type storage = TAKE_OVER_ARRAY );
extern Value *create_value( dcomplex value[], int num_elements,
	array_storage_type storage = TAKE_OVER_ARRAY );
extern Value *create_value( charptr value[], int num_elements,
	array_storage_type storage = TAKE_OVER_ARRAY );
extern Value *create_value( glish_boolref& value_ref );
extern Value *create_value( byteref& value_ref );
extern Value *create_value( shortref& value_ref );
extern Value *create_value( intref& value_ref );
extern Value *create_value( floatref& value_ref );
extern Value *create_value( doubleref& value_ref );
extern Value *create_value( complexref& value_ref );
extern Value *create_value( dcomplexref& value_ref );
extern Value *create_value( charptrref& value_ref );


#endif /* value_h */
