// $Header$

#include "Glish/glish.h"
RCSID("@(#) $Id$")

#include "Glish/Value.h"
#include "Glish/VecRef.h"
#include "system.h"
#include <string.h>
#include <stdlib.h>

void ValueKernel::record_t::clear()
	{
	if (record) delete_record(record);
	record = 0;
	ref_count = 1;
	}

ValueKernel::array_t::~array_t()
	{
	if ( values ) 
		{
		if ( final )
			(*final)( values, length );

		delete (char*) values;
		}
	}

void ValueKernel::array_t::clear()
	{
	type = TYPE_ERROR;
	type_bytes = length = 0;
	ref_count = 1;
	}

void ValueKernel::SetValue( Value *v )
	{
	unref(1);
	mode = VALUE();
	value = v;
	refOthers();
	}

void ValueKernel::SetVecRef( VecRef *v )
	{
	unref(1);
	mode = REF();
	vecref = v;
	refOthers();
	}

void ValueKernel::SetOpaque( void *v )
	{
	unref(1);
	mode = OPAQUE();
	opaque = v;
	}

glish_type ValueKernel::otherType() const
	{
	if ( VALUE(mode) )
		return IsConst() ? TYPE_CONST : TYPE_REF;
	else if ( REF(mode) )
		return IsConst() ? TYPE_SUBVEC_CONST : TYPE_SUBVEC_REF;
	else if ( OPAQUE(mode) )
		return TYPE_OPAQUE;
	else
		return TYPE_ERROR;
	}

unsigned long ValueKernel::otherLength() const
	{
	if ( VALUE(mode) )
		return value->Length();
	else if ( REF(mode) )
		return vecref->Length();
	else if ( OPAQUE(mode) )
		return 1;
	else
		return 0;
	}

void ValueKernel::SetRecord( recordptr r )
	{
	STAT2( (void*) this, "\tValueKernel::SetRecord recordptr")
	unref( RECORD(mode) ? 0 : 1 );
	mode = RECORD();
	if ( ! record ) record = new record_t();
	record->record = r;
	}

void ValueKernel::array_t::SetType( glish_type new_type, KernelCopyFunc c,
				     KernelZeroFunc z, KernelDeleteFunc d )
	{
	copy = c;
	zero = z;
	final = d;

	switch (new_type)
		{

#define SETTYPE_ACTION(TAG,TYPE)	\
case TAG:				\
	type_bytes = sizeof(TYPE);	\
	type = TAG;			\
	break;

SETTYPE_ACTION(TYPE_BOOL,glish_bool)
SETTYPE_ACTION(TYPE_BYTE,byte)
SETTYPE_ACTION(TYPE_SHORT,short)
SETTYPE_ACTION(TYPE_INT,int)
SETTYPE_ACTION(TYPE_FLOAT,float)
SETTYPE_ACTION(TYPE_DOUBLE,double)
SETTYPE_ACTION(TYPE_COMPLEX,complex)
SETTYPE_ACTION(TYPE_DCOMPLEX,dcomplex)
SETTYPE_ACTION(TYPE_STRING,charptr)

		default:
			type_bytes = 0;
			type = TYPE_ERROR;
		}
	}

void ValueKernel::array_t::SetType( glish_type new_type, unsigned short type_len, 
				     KernelCopyFunc c, KernelZeroFunc z, KernelDeleteFunc d )
	{
	copy = c;
	zero = z;
	final = d;
	type = new_type;
	type_bytes = type_len;
	}

void ValueKernel::array_t::Grow( unsigned long len, int do_zero )
	{
	if ( len == length )
		return;

	if ( len < length )
		{
		if ( final )
			(*final)( &(((char *)values)[len*type_bytes]), length-len );
		}
	else
		{
		if ( values == 0 || alloc_bytes == 0 )
			{
			values = (void *) new char[ len*type_bytes ];
			alloc_bytes = len*type_bytes;
			}
		else if ( len*type_bytes > alloc_bytes )
		  	{
			alloc_bytes = len*type_bytes;
			values = (void *) realloc_memory( values, alloc_bytes );
			}

		if ( do_zero )
			{
			if ( zero  )
				(*zero)( &(((char *)values)[length*type_bytes]), len-length );
			else
				memset( &((char *)values)[length*type_bytes], 0, (len-length)*type_bytes );
			}
		}

	length = len;
	}


void ValueKernel::unrefArray(int del)
	{
	STAT7((void*) this, "\t\tarray unref c:",array->ref_count,"a:",(void*)array,"d:",del)
	if ( array && --array->ref_count == 0 )
		if ( del )
			{
			delete array;
			array = 0;
			}
		else
			array->clear();
	else
		if ( del )
			array = 0;
		else
			array = new array_t();
	}

void ValueKernel::unrefRecord(int del)
	{
	STAT7((void*) this, "\t\trecord unref c:",record->ref_count,"r:",(void*)record,"d:",del)
	if ( record && --record->ref_count == 0 )
		if ( del )
			{
			delete record;
			record = 0;
			}
		else
			record->clear();
	else
		if ( del )
			record = 0;
		else
			record = new record_t();
	}

ValueKernel::ValueKernel( glish_type t, unsigned long len, KernelCopyFunc c,
			  KernelZeroFunc z, KernelDeleteFunc d ) : mode(ARRAY()), array(new array_t())
	{
	array->SetType( t, c, z, d );
	array->Grow( len );
	}

void ValueKernel::SetType( glish_type t, unsigned long l, 
			KernelCopyFunc c, KernelZeroFunc z, KernelDeleteFunc d )
	{
	unref( ARRAY(mode) ? 0 : 1 );
	mode = ARRAY();
	if ( ! array ) array = new array_t();
	array->SetType( t, c, z, d );
	array->Grow( l );
	}

// void ValueKernel::SetArray( void *storage, unsigned long l, glish_type t, int copy,
// 			    KernelCopyFunc c, KernelZeroFunc z, KernelDeleteFunc d )
// 	{
// 	unref( ARRAY(mode) ? 0 : 1 );
// 	mode = ARRAY();
// 	if ( ! array ) array = new array_t();
// 	array->SetType( t, c, z, d );
// 	if ( copy )
// 		{
// 		array->Grow( l, 0 );
// 		if ( array->copy )
// 			(*array->copy)(array->values, storage, array->length);
// 		else
// 			memcpy(array->values, storage, array->bytes());
// 		}
// 	else
// 		array->SetStorage( storage, l );
// 	}

ValueKernel &ValueKernel::operator=( const ValueKernel &v )
	{
	unref(1);
	array = v.array;
	mode = v.mode;
	ref();
	return *this;
	}


#define ARRAY_SET_BODY(TYPE, GLISH_TYPE)					\
	unref( ARRAY(mode) ? 0 : 1 );						\
	mode = ARRAY();								\
	if ( ! array ) array = new array_t();					\
	array->SetType( GLISH_TYPE, sizeof(TYPE), c , z, d );			\
	if ( copy )								\
		{								\
		array->Grow( len, 0 );						\
		if ( array->copy )						\
			(*array->copy)(array->values, vec, array->length);	\
		else								\
			memcpy(array->values, vec, array->bytes());		\
		}								\
	else									\
		array->SetStorage( vec, len );

#define DEFINE_ARRAY_SET(TYPE, GLISH_TYPE)					\
void ValueKernel::SetArray( TYPE vec[], unsigned long len, int copy,		\
		    KernelCopyFunc c, KernelZeroFunc z, KernelDeleteFunc d )	\
	{									\
	STAT4( (void*) this, "\tValueKernel::SetArray ", #TYPE, "[]" )		\
	ARRAY_SET_BODY(TYPE,GLISH_TYPE)						\
	}

DEFINE_ARRAY_SET(glish_bool,TYPE_BOOL)
DEFINE_ARRAY_SET(byte,TYPE_BYTE)
DEFINE_ARRAY_SET(short,TYPE_SHORT)
DEFINE_ARRAY_SET(int,TYPE_INT)
DEFINE_ARRAY_SET(float,TYPE_FLOAT)
DEFINE_ARRAY_SET(double,TYPE_DOUBLE)
DEFINE_ARRAY_SET(complex,TYPE_COMPLEX)
DEFINE_ARRAY_SET(dcomplex,TYPE_DCOMPLEX)
DEFINE_ARRAY_SET(charptr,TYPE_STRING)

void ValueKernel::SetArray( voidptr vec[], unsigned long len, glish_type t, int copy,
		    KernelCopyFunc c, KernelZeroFunc z, KernelDeleteFunc d )
	{
	STAT2( (void*) this, "\tValueKernel::SetArray void*")
	ARRAY_SET_BODY(voidptr,t)
	}

void ValueKernel::Grow( unsigned long len )
	{
	if ( ! ARRAY(mode) || ! array || len == array->length || array->ref_count < 1 )
		return;
	if ( array->ref_count == 1 )
		array->Grow( len );
	else
		{
		unsigned long minlen = len > array->length ? array->length : len;
		array_t *k = array;
		unrefArray();
		array->SetType( k->type, k->copy, k->zero );
		array->Grow( len, 0 );

		if ( array->copy )
			(*array->copy)(array->values, k->values, minlen);
		else
			memcpy(array->values, k->values, minlen*array->type_bytes);

		if ( len > minlen )
			{
			if ( array->zero )
				(*array->zero)( &((char*)array->values)[k->bytes()],len - k->length );
			else
				memset( &((char*)array->values)[k->bytes()], 0,
					(len-k->length)*array->type_bytes );
			}
		}
	}

void *ValueKernel::modArray( )
	{
	if ( ! ARRAY(mode) || ! array || array->ref_count < 1 )
		return 0;
	if ( array->ref_count == 1 )
		return array->values;
	else
		{
		STAT5((void*) this, "\t\tarray copy c:",array->ref_count,"a:",(void*)array)
		array_t *k = array;
		unrefArray();
		array->SetType( k->type, k->copy, k->zero );
		array->Grow( k->length, 0 );
		if ( array->copy )
			(*array->copy)(array->values, k->values, array->length);
		else
			memcpy(array->values, k->values, array->bytes());
		}
	return array->values;
	}


recordptr ValueKernel::modRecord( )
	{
	if ( ! RECORD(mode) || ! record || record->ref_count < 1 )
		return 0;
	if ( record->ref_count == 1 )
		return record->record;
	else
		{
		STAT5((void*) this, "\t\trecord copy c:",record->ref_count,"r:",(void*)record)
		record_t *a = record;
		unrefRecord();
		SetRecord(copy_record_dict(a->record));
		}
	return record->record;
	}

void ValueKernel::refOthers()
	{
	if ( VALUE(mode) )
		Ref( value );
	else if ( REF(mode) )
		Ref( vecref );
	}

void ValueKernel::unrefOthers()
	{
	if ( VALUE(mode) )
		Unref( value );
	else if ( REF(mode) )
		Unref( vecref );
	}

recordptr create_record_dict()
	{
	return new PDict(Value)( ORDERED );
	}

void delete_record( recordptr r )
	{
	if ( r )
		{
		IterCookie* c = r->InitForIteration();

		Value* member;
		const char* key;
		while ( (member = r->NextEntry( key, c )) )
			{
			delete (char*) key;
			Unref( member );
			}

		delete r;
		}
	}

recordptr copy_record_dict( recordptr rptr )
	{
	recordptr new_record = create_record_dict();

	IterCookie *c = rptr->InitForIteration();
	const Value* member;
	const char* key;
	while ( (member = rptr->NextEntry( key, c )) ) 
		new_record->Insert( strdup( key ),
				    copy_value( member ) );
	return new_record;
	}

void copy_strings(void *tgt, void *src, unsigned long len)
	{
	charptr *from = (charptr*)src;
	charptr *to = (charptr*)tgt;
	for ( unsigned long i=0; i < len; i++ )
		*to++ = strdup(*from++);
	}

void delete_strings(void *src, unsigned long len)
	{
	char **ary = (char**)src;
	for ( unsigned long i=0; i < len; i++ )
		delete *ary++;
	}
