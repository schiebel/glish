// $Id$
// Copyright (c) 1997 Associated Universities Inc.
//

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

int ValueKernel::record_t::bytes( int addPerValue ) const
	{
	int size = addPerValue;
	IterCookie* c = record->InitForIteration();

	Value* member;
	const char* key;

	while ( (member = record->NextEntry( key, c )) )
		size += strlen(key) + 1 + member->Bytes(addPerValue);
	return size;
	}

ValueKernel::array_t::~array_t()
	{
	if ( values ) 
		{
		if ( final )
			(*final)( values, length );

		free_memory( values );
		}
	}

void ValueKernel::array_t::clear()
	{
	type = TYPE_ERROR;
	length = type_bytes = 0;
	ref_count = 1;
	}

void ValueKernel::SetValue( Value *v )
	{
	unref(1);
	mode = VALUE();
	value = v;
	refOthers();
	}

void ValueKernel::SetFail( )
	{
	unref(1);
	mode = FAIL();
	opaque = 0;
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
		return TYPE_REF;
	else if ( REF(mode) )
		return TYPE_SUBVEC_REF;
	else if ( OPAQUE(mode) )
		return TYPE_OPAQUE;
	else if ( FAIL(mode) )
		return TYPE_FAIL;
	else
		return TYPE_ERROR;
	}

unsigned int ValueKernel::otherLength() const
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

unsigned int ValueKernel::otherBytes(int addPerValue) const
	{
	if ( VALUE(mode) )
		return value->Bytes(addPerValue);
	else if ( REF(mode) )
		return vecref->Bytes() + addPerValue;
	else
		return 0;
	}

void ValueKernel::SetRecord( recordptr r )
	{
	DIAG2( (void*) this, "\tValueKernel::SetRecord recordptr")
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

void ValueKernel::array_t::Grow( unsigned int len, int do_zero )
	{
	unsigned int alen = len ? len : 1;

	if ( len && len == length )
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
			values = (void *) alloc_memory( alen*type_bytes );
			alloc_bytes = alen*type_bytes;
			}
		else if ( len*type_bytes > alloc_bytes )
		  	{
			alloc_bytes = len*type_bytes;
			values = (void *) realloc_memory( values, alloc_bytes );
			}

		if ( do_zero || ! len )
			{
			if ( zero  )
				(*zero)( &(((char *)values)[length*type_bytes]), alen-length );
			else
				memset( &((char *)values)[length*type_bytes], 0, (alen-length)*type_bytes );
			}
		}

	length = len;
	}


void ValueKernel::unrefArray(int del)
	{
	DIAG7((void*) this, "\t\tarray unref c:",array->ref_count,"a:",(void*)array,"d:",del)
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
	DIAG7((void*) this, "\t\trecord unref c:",record->ref_count,"r:",(void*)record,"d:",del)
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

ValueKernel::ValueKernel( glish_type t, unsigned int len, KernelCopyFunc c,
			  KernelZeroFunc z, KernelDeleteFunc d ) : mode(ARRAY()), array(new array_t())
	{
	array->SetType( t, c, z, d );
	array->Grow( len );
	}

void ValueKernel::SetType( glish_type t, unsigned int l, 
			KernelCopyFunc c, KernelZeroFunc z, KernelDeleteFunc d )
	{
	unref( ARRAY(mode) ? 0 : 1 );
	mode = ARRAY();
	if ( ! array ) array = new array_t();
	array->SetType( t, c, z, d );
	array->Grow( l );
	}

void ValueKernel::BoolToInt()
	{
	if ( Type() != TYPE_BOOL ) return;
	modArray();
	array->type = TYPE_INT;
	}

// void ValueKernel::SetArray( void *storage, unsigned int l, glish_type t, int copy,
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
void ValueKernel::SetArray( TYPE vec[], unsigned int len, int copy,		\
		    KernelCopyFunc c, KernelZeroFunc z, KernelDeleteFunc d )	\
	{									\
	DIAG4( (void*) this, "\tValueKernel::SetArray ", #TYPE, "[]" )		\
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

void ValueKernel::SetArray( voidptr vec[], unsigned int len, glish_type t, int copy,
		    KernelCopyFunc c, KernelZeroFunc z, KernelDeleteFunc d )
	{
	DIAG2( (void*) this, "\tValueKernel::SetArray void*")
	ARRAY_SET_BODY(voidptr,t)
	}

void ValueKernel::Grow( unsigned int len )
	{
	if ( ! ARRAY(mode) || ! array || len == array->length || array->ref_count < 1 )
		return;
	if ( array->ref_count == 1 )
		array->Grow( len );
	else
		{
		unsigned int minlen = len > array->length ? array->length : len;
		array_t *k = array;
		unrefArray();
		int m = mode;
		array->SetType( k->type, k->copy, k->zero );
		mode = m;
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
		DIAG5((void*) this, "\t\tarray copy c:",array->ref_count,"a:",(void*)array)
		array_t *k = array;
		unrefArray();
		int m = mode;
		array->SetType( k->type, k->copy, k->zero );
		mode = m;
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
		DIAG5((void*) this, "\t\trecord copy c:",record->ref_count,"r:",(void*)record)
		record_t *a = record;
		unrefRecord();
		int m = mode;
		SetRecord(copy_record_dict(a->record));
		mode = m;
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

int ValueKernel::Bytes( int addPerValue ) const
	{
	if ( ARRAY(mode) )
		{
		if ( Type() != TYPE_STRING )
			return (int) array->bytes() + addPerValue;
		else
			{
			int cnt = addPerValue;
			for ( int i = 0; i < array->length; i++ )
				cnt += strlen(((char**)array->values)[i])+1;
			return cnt;
			}
		}
	else if ( RECORD(mode) )
		return record->bytes( addPerValue );
	else
		return otherBytes();
	}


int ValueKernel::ToMemBlock(char *memory, int offset, int have_attributes) const
	{
	header h;
	glish_type type = Type();
	if ( ARRAY(mode) )
		{
		h.type = type;
		h.have_attr = have_attributes ? 1 : 0;

		if ( type != TYPE_STRING )
			{
			h.len = array->bytes();
			memcpy(&memory[offset],&h,sizeof(h));
			offset += sizeof(h);

			memcpy(&memory[offset],array->values,h.len);
			offset += h.len;
			}
		else
			{
			h.len = array->length;
			memcpy(&memory[offset],&h,sizeof(h));
			offset += sizeof(h);

			for (int i=0; i < h.len; i++)
				{
				int l = strlen(((char**)array->values)[i]);
				memcpy(&memory[offset],((char**)array->values)[i],l+1);
				offset += l+1;
				}
			}
		}
	else if ( RECORD(mode) )
		{
		h.type = type;
		h.len = record->record->Length();
		h.have_attr = have_attributes ? 1 : 0;

		memcpy(&memory[offset],&h,sizeof(h));
		offset += sizeof(h);

		IterCookie* c = record->record->InitForIteration();

		Value* member;
		const char* key;

		while ( (member = record->record->NextEntry( key, c )) )
			{
			int l = strlen(key);
			memcpy(&memory[offset],key,l+1);
			offset += l+1;
			offset = member->ToMemBlock(memory, offset);
			}
		}

	else
		return -1;

	return offset;
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
			free_memory( (void*) key );
			Unref( member );
			}

		delete r;
		}
	}

recordptr copy_record_dict( recordptr rptr )
	{
	int ordered = rptr->IsOrdered();
	recordptr new_record = new PDict(Value)( ordered ? ORDERED : UNORDERED );

	const char* key;
	const Value* member;

	if ( ordered )
		{
		for ( int i = 0; i < rptr->Length(); i++ )
			{
			member = rptr->NthEntry(i,key);
			new_record->Insert( strdup(key), copy_value( member ) );
			}
		}
	else
		{
		IterCookie *c = rptr->InitForIteration();
		while ( (member = rptr->NextEntry( key, c )) ) 
			new_record->Insert( strdup( key ), copy_value( member ) );
		}

	return new_record;
	}

void copy_strings(void *tgt, void *src, unsigned int len)
	{
	charptr *from = (charptr*)src;
	charptr *to = (charptr*)tgt;
	for ( unsigned int i=0; i < len; i++ )
		*to++ = strdup(*from++);
	}

void delete_strings(void *src, unsigned int len)
	{
	char **ary = (char**)src;
	for ( unsigned int i=0; i < len; i++ )
		free_memory( *ary++ );
	}
