// $Header$

#ifndef ivalue_h
#define ivalue_h

#include "Glish/Value.h"

class Agent;
class Func;
class ArithExpr;
class RelExpr;

typedef Func* funcptr;
typedef Agent* agentptr;

class IValue : public Value {
public:
	IValue( glish_bool v ) : Value( v ) { }
	IValue( byte v ) : Value( v ) { }
	IValue( short v ) : Value( v ) { }
	IValue( int v ) : Value( v ) { }
	IValue( float v ) : Value( v ) { }
	IValue( double v ) : Value( v ) { }
	IValue( complex v ) : Value( v ) { }
	IValue( dcomplex v ) : Value( v ) { }
	IValue( const char* v ) : Value( v ) { }
	IValue( funcptr v );

	// If "storage" is set to "COPY_ARRAY", then the underlying
	// "Agent" in value will be Ref()ed. Otherwise, if 
	// "TAKE_OVER_ARRAY" it will simply be used.
	IValue( agentptr value, array_storage_type storage = COPY_ARRAY );

	IValue( recordptr v ) : Value( v ) { }
	IValue( recordptr v, Agent* agent );

	IValue( SDS_Index& sds_index ) : Value( sds_index ) { }

	// Reference constructor.
	IValue( Value* ref_value, value_type val_type ) :
			Value( ref_value, val_type ) { }

	// Subref constructor.
	IValue( Value* ref_value, int index[], int num_elements,
		value_type val_type ) :
			Value( ref_value, index, num_elements, val_type ) { }

	IValue( glish_bool value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY ) :
			Value( value, num_elements, storage ) { }
	IValue( byte value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY ) :
			Value( value, num_elements, storage ) { }
	IValue( short value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY ) :
			Value( value, num_elements, storage ) { }
	IValue( int value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY ) :
			Value( value, num_elements, storage ) { }
	IValue( float value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY ) :
			Value( value, num_elements, storage ) { }
	IValue( double value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY ) :
			Value( value, num_elements, storage ) { }
	IValue( complex value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY ) :
			Value( value, num_elements, storage ) { }
	IValue( dcomplex value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY ) :
			Value( value, num_elements, storage ) { }
	IValue( charptr value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY ) :
			Value( value, num_elements, storage ) { }
	IValue( funcptr value[], int num_elements,
		array_storage_type storage = TAKE_OVER_ARRAY );

	IValue( glish_boolref& value_ref ) : Value( value_ref ) { }
	IValue( byteref& value_ref ) : Value( value_ref ) { }
	IValue( shortref& value_ref ) : Value( value_ref ) { }
	IValue( intref& value_ref ) : Value( value_ref ) { }
	IValue( floatref& value_ref ) : Value( value_ref ) { }
	IValue( doubleref& value_ref ) : Value( value_ref ) { }
	IValue( complexref& value_ref ) : Value( value_ref ) { }
	IValue( dcomplexref& value_ref ) : Value( value_ref ) { }
	IValue( charptrref& value_ref ) : Value( value_ref ) { }

	~IValue();

	// True if the value is a record corresponding to a agent.
	int IsAgentRecord() const;

	// Returns the agent or function corresponding to the Value.
	Agent* AgentVal() const;
	funcptr FuncVal() const;

	// The following accessors return pointers to the underlying value
	// array.  The "const" versions complain with a fatal error if the
	// value is not the given type.  The non-const versions first
	// Polymorph() the values to the given type.  If called for a
	// subref, retrieves the complete underlying value, not the
	// just selected subelements.  (See the XXXRef() functions below.)
	funcptr* FuncPtr() const;
	agentptr* AgentPtr() const;

	funcptr* FuncPtr();
	agentptr* AgentPtr();

	// These coercions are very limited: they essentially either
	// return the corresponding xxxPtr() (if the sizes match,
	// no "result" is prespecified, and the Value is already
	// the given type) or generate a fatal error.
	funcptr* CoerceToFuncArray( int& is_copy, int size,
			funcptr* result = 0 ) const;

	void Polymorph( glish_type new_type );

	void ByteOpCompute( const IValue* value, int lhs_len, ArithExpr* expr );
	void ShortOpCompute( const IValue* value, int lhs_len, ArithExpr* expr );
	void IntOpCompute( const IValue* value, int lhs_len, ArithExpr* expr );
	void FloatOpCompute( const IValue* value, int lhs_len, ArithExpr* expr );
	void DoubleOpCompute( const IValue* value, int lhs_len,
				ArithExpr* expr );
	void ComplexOpCompute( const IValue* value, int lhs_len,
				ArithExpr* expr );
	void DcomplexOpCompute( const IValue* value, int lhs_len,
				ArithExpr* expr );

	void DescribeSelf( ostream& s ) const;

protected:
	void SetValue( agentptr array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY );
	void SetValue( funcptr array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY );

	void SetValue( recordptr value );
	void SetValue( recordptr value, Agent* agent );

	void DeleteValue();

	void InitRecord( recordptr r );
	void InitRecord( recordptr r, Agent* agent = 0 );

	// ** NOTE THIS CAN PROBABLY BE REMOVED, BUT IT IS LEFT IN FOR NOW **
	// Does the actual work of assigning a list of array elements,
	// once type-checking has been done.
	void AssignArrayElements( int* indices, int num_indices,
				Value* value, int rhs_len );
	void AssignArrayElements( Value* value );


	// Forward all of these calls to the base class to make
	// it more convenient to use IValue
	void SetValue( glish_bool array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY )
			{ Value::SetValue( array, len, storage ); }
	void SetValue( byte array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY )
			{ Value::SetValue( array, len, storage ); }
	void SetValue( short array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY )
			{ Value::SetValue( array, len, storage ); }
	void SetValue( int array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY )
			{ Value::SetValue( array, len, storage ); }
	void SetValue( float array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY )
			{ Value::SetValue( array, len, storage ); }
	void SetValue( double array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY )
			{ Value::SetValue( array, len, storage ); }
	void SetValue( complex array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY )
			{ Value::SetValue( array, len, storage ); }
	void SetValue( dcomplex array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY )
			{ Value::SetValue( array, len, storage ); }
	void SetValue( const char* array[], int len,
			array_storage_type storage = TAKE_OVER_ARRAY )
			{ Value::SetValue( array, len, storage ); }
	void SetValue( SDS_Index& array )
			{ Value::SetValue( array ); }

	void SetValue( glish_boolref& value_ref )
			{ Value::SetValue( value_ref ); }
	void SetValue( byteref& value_ref )
			{ Value::SetValue( value_ref ); }
	void SetValue( shortref& value_ref )
			{ Value::SetValue( value_ref ); }
	void SetValue( intref& value_ref )
			{ Value::SetValue( value_ref ); }
	void SetValue( floatref& value_ref )
			{ Value::SetValue( value_ref ); }
	void SetValue( doubleref& value_ref )
			{ Value::SetValue( value_ref ); }
	void SetValue( complexref& value_ref )
			{ Value::SetValue( value_ref ); }
	void SetValue( dcomplexref& value_ref )
			{ Value::SetValue( value_ref ); }
	void SetValue( charptrref& value_ref )
			{ Value::SetValue( value_ref ); }

	void SetValue( Value *ref_value, int index[], int num_elements, 
			value_type val_type )
			{ Value::SetValue( ref_value, index, num_elements, val_type ); }

    protected:
	// Note: if member variables are added, Value::TakeValue()
	//       will need to be examined
	};

typedef const IValue* const_ivalue;
declare(List,const_ivalue);
typedef List(const_ivalue) const_ivalue_list;

declare(PList,IValue);
typedef PList(IValue) ivalue_list;

extern IValue* copy_value( const IValue* value );

#define false_ivalue (const IValue *) false_value
inline IValue* empty_ivalue() { return (IValue*) empty_value(); }
inline IValue* error_ivalue() { return (IValue*) error_value(); }
inline IValue* create_irecord() { return (IValue*) create_record(); }
inline IValue* isplit( char* source, char* split_chars = " \t\n" )
	{ return (IValue*) split(source, split_chars); }


extern IValue* bool_rel_op_compute( const IValue* lhs, const IValue* rhs,
				int lhs_len, RelExpr* expr );
extern IValue* byte_rel_op_compute( const IValue* lhs, const IValue* rhs,
				int lhs_len, RelExpr* expr );
extern IValue* short_rel_op_compute( const IValue* lhs, const IValue* rhs,
				int lhs_len, RelExpr* expr );
extern IValue* int_rel_op_compute( const IValue* lhs, const IValue* rhs,
				int lhs_len, RelExpr* expr );
extern IValue* float_rel_op_compute( const IValue* lhs, const IValue* rhs,
				int lhs_len, RelExpr* expr );
extern IValue* double_rel_op_compute( const IValue* lhs, const IValue* rhs,
				int lhs_len, RelExpr* expr );
extern IValue* complex_rel_op_compute( const IValue* lhs, const IValue* rhs,
				int lhs_len, RelExpr* expr );
extern IValue* dcomplex_rel_op_compute( const IValue* lhs, const IValue* rhs,
				int lhs_len, RelExpr* expr );
extern IValue* string_rel_op_compute( const IValue* lhs, const IValue* rhs,
				int lhs_len, RelExpr* expr );

inline IValue* read_ivalue_from_SDS( int sds, int is_opaque_sds = 0 )
	{ return (IValue*) read_value_from_SDS( sds, is_opaque_sds ); }

#endif /* ivalue_h */
