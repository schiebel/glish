// $Header$

#ifndef glishlib_h
#define glishlib_h

#define DEFINE_CREATE_VALUE(type)						\
	Value *create_value( )							\
		{ return new type( ); }						\
	Value *create_value( const char *message, const char *file, int line )	\
		{ return new type( message, file, line ); }			\
	Value *create_value( const Value &value )				\
		{ return new type( value ); }					\
	Value *create_value( glish_bool value )					\
		{ return new type( value ); }					\
	Value *create_value( byte value )					\
		{ return new type( value ); }					\
	Value *create_value( short value )					\
		{ return new type( value ); }					\
	Value *create_value( int value )					\
		{ return new type( value ); }					\
	Value *create_value( float value )					\
		{ return new type( value ); }					\
	Value *create_value( double value )					\
		{ return new type( value ); }					\
	Value *create_value( complex value )					\
		{ return new type( value ); }					\
	Value *create_value( dcomplex value )					\
		{ return new type( value ); }					\
	Value *create_value( const char* value )				\
		{ return new type( value ); }					\
	Value *create_value( recordptr value )					\
		{ return new type( value ); }					\
	Value *create_value( SDS_Index& sds_index )				\
		{ return new type( sds_index ); }				\
	Value *create_value( Value* ref_value, value_type val_type )		\
		{ return new type( ref_value, val_type ); }			\
	Value *create_value( Value* ref_value, int index[], int num_elements, value_type val_type, int take_index )\
		{ return new type( ref_value, index, num_elements, val_type, take_index ); }\
	Value *create_value( glish_bool value[], int num_elements,		\
		array_storage_type storage )					\
		{ return new type( value, num_elements, storage ); }		\
	Value *create_value( byte value[], int num_elements,			\
		array_storage_type storage )					\
		{ return new type( value, num_elements, storage ); }		\
	Value *create_value( short value[], int num_elements,			\
		array_storage_type storage )					\
		{ return new type( value, num_elements, storage ); }		\
	Value *create_value( int value[], int num_elements,			\
		array_storage_type storage )					\
		{ return new type( value, num_elements, storage ); }		\
	Value *create_value( float value[], int num_elements,			\
		array_storage_type storage )					\
		{ return new type( value, num_elements, storage ); }		\
	Value *create_value( double value[], int num_elements,			\
		array_storage_type storage )					\
		{ return new type( value, num_elements, storage ); }		\
	Value *create_value( complex value[], int num_elements,			\
		array_storage_type storage )					\
		{ return new type( value, num_elements, storage ); }		\
	Value *create_value( dcomplex value[], int num_elements,		\
		array_storage_type storage )					\
		{ return new type( value, num_elements, storage ); }		\
	Value *create_value( charptr value[], int num_elements,			\
		array_storage_type storage )					\
		{ return new type( value, num_elements, storage ); }		\
	Value *create_value( glish_boolref& value_ref )				\
		{ return new type( value_ref ); }				\
	Value *create_value( byteref& value_ref )				\
		{ return new type( value_ref ); }				\
	Value *create_value( shortref& value_ref )				\
		{ return new type( value_ref ); }				\
	Value *create_value( intref& value_ref )				\
		{ return new type( value_ref ); }				\
	Value *create_value( floatref& value_ref )				\
		{ return new type( value_ref ); }				\
	Value *create_value( doubleref& value_ref )				\
		{ return new type( value_ref ); }				\
	Value *create_value( complexref& value_ref )				\
		{ return new type( value_ref ); }				\
	Value *create_value( dcomplexref& value_ref )				\
		{ return new type( value_ref ); }				\
	Value *create_value( charptrref& value_ref )				\
		{ return new type( value_ref ); }

#endif	/* glishlib_h */
