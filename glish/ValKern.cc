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

#ifdef GGC
extern int glish_collecting_garbage;
#endif

glish_typeinfo_t glish_typeinfo[NUM_GLISH_TYPES] =
	{ { 0, 0, 0, 0 },  					/* TYPE_ERROR */
	  { 0, 0, 0, 0 },  					/* TYPE_REF */
	  { 0, 0, 0, 0 },  					/* TYPE_SUBVEC_REF */
	  { sizeof(glish_bool), 0, 0, 0 },			/* TYPE_BOOL */
	  { sizeof(byte), 0, 0, 0 },				/* TYPE_BYTE */
	  { sizeof(short), 0, 0, 0 },				/* TYPE_SHORT */
	  { sizeof(int), 0, 0, 0 },				/* TYPE_INT */
	  { sizeof(float), 0, 0, 0 },				/* TYPE_FLOAT */
	  { sizeof(double), 0, 0, 0 },				/* TYPE_DOUBLE */
	  { sizeof(charptr), copy_strings, delete_strings, 0 },	/* TYPE_STRING */
	  { sizeof(void*), 0, 0, 0 },				/* TYPE_AGENT */
	  { sizeof(void*), 0, 0, 0 },				/* TYPE_FUNC */
	  { 0, 0, 0, 0 },					/* TYPE_RECORD */
	  { sizeof(complex), 0, 0, 0 },				/* TYPE_COMPLEX */
	  { sizeof(dcomplex), 0, 0, 0 },			/* TYPE_DCOMPLEX */
	  { 0, 0, 0, 0 },					/* TYPE_FAIL */
	  { sizeof(void*), 0, 0, 0 }				/* TYPE_REGEX */
	};

void register_type_funcs( glish_type t, KernelCopyFunc c=0,
			  KernelDeleteFunc d=0, KernelZeroFunc z=0 )
	{
	glish_typeinfo[t].copy = c;
	glish_typeinfo[t].final = d;
	glish_typeinfo[t].zero = z;
	}

void ValueKernel::record_t::clear()
	{
	if (record) delete_record(record);
	record = 0;
	ref_count = 1;
	}

int ValueKernel::record_t::Sizeof( ) const
	{
	int size = 0;
	IterCookie* c = record->InitForIteration();

	Value* member;
	const char* key;

	while ( (member = record->NextEntry( key, c )) )
		size += strlen(key) + 1 + member->Sizeof( );
	return size + record->Sizeof() + sizeof(ValueKernel);
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
		if ( final() )
			(*final())( values, length );

		free_memory( values );
		}
	}

void ValueKernel::array_t::clear()
	{
	type = TYPE_ERROR;
	length = 0;
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
	array = 0;
	}

void ValueKernel::SetVecRef( VecRef *v )
	{
	unref(1);
	mode = REF();
	vecref = v;
	refOthers();
	}

glish_type ValueKernel::otherType() const
	{
	if ( VALUE(mode) )
		return TYPE_REF;
	else if ( REF(mode) )
		return TYPE_SUBVEC_REF;
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
	else
		return 0;
	}

int ValueKernel::otherSizeof( ) const
	{
	if ( VALUE(mode) )
		return sizeof(ValueKernel) + value->Sizeof( );
	else if ( REF(mode) )
		return sizeof(ValueKernel) + vecref->Sizeof( );
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

void ValueKernel::array_t::Grow( unsigned int len, int do_zero )
	{
	unsigned int alen = len ? len : 1;

	if ( len && len == length )
		return;

	if ( len < length )
		{
		if ( final() )
			(*final())( &(((char *)values)[len*type_bytes()]), length-len );
		}
	else
		{
		if ( values == 0 || alloc_bytes == 0 )
			{
			values = (void *) alloc_memory( alen*type_bytes() );
			alloc_bytes = alen*type_bytes();
			}
		else if ( len*type_bytes() > alloc_bytes )
		  	{
			alloc_bytes = len*type_bytes();
			values = (void *) realloc_memory( values, alloc_bytes );
			}

		if ( do_zero || ! len )
			{
			if ( zero()  )
				(*zero())( &(((char *)values)[length*type_bytes()]), alen-length );
			else
				memset( &((char *)values)[length*type_bytes()], 0, (alen-length)*type_bytes() );
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

ValueKernel::ValueKernel( glish_type t, unsigned int len ) : mode(ARRAY()), array(new array_t())
	{
	array->SetType( t );
	array->Grow( len );
	}

void ValueKernel::SetType( glish_type t, unsigned int l )
	{
	unref( ARRAY(mode) ? 0 : 1 );
	mode = ARRAY();
	if ( ! array ) array = new array_t();
	array->SetType( t );
	array->Grow( l );
	}

void ValueKernel::BoolToInt()
	{
	if ( Type() != TYPE_BOOL ) return;
	modArray();
	array->type = TYPE_INT;
	}

ValueKernel &ValueKernel::operator=( const ValueKernel &v )
	{
	unref(1);
	array = v.array;
	mode = v.mode;
	ref();
	return *this;
	}


#define ARRAY_SET_BODY(GLISH_TYPE)						\
	unref( ARRAY(mode) ? 0 : 1 );						\
	mode = ARRAY();								\
	if ( ! array ) array = new array_t();					\
	array->SetType( GLISH_TYPE );						\
	if ( copy )								\
		{								\
		array->Grow( len, 0 );						\
		if ( array->copy() )						\
			(*array->copy())(array->values, vec, array->length);	\
		else								\
			memcpy(array->values, vec, array->bytes());		\
		}								\
	else									\
		array->SetStorage( vec, len );

#define DEFINE_ARRAY_SET(TYPE, GLISH_TYPE)					\
void ValueKernel::SetArray( TYPE vec[], unsigned int len, int copy )		\
	{									\
	DIAG4( (void*) this, "\tValueKernel::SetArray ", #TYPE, "[]" )		\
	ARRAY_SET_BODY(GLISH_TYPE)						\
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

void ValueKernel::SetArray( voidptr vec[], unsigned int len, glish_type t, int copy )
	{
	DIAG2( (void*) this, "\tValueKernel::SetArray void*")
	ARRAY_SET_BODY(t)
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
		array->SetType( k->type );
		mode = m;
		array->Grow( len, 0 );

		if ( array->copy() )
			(*array->copy())(array->values, k->values, minlen);
		else
			memcpy(array->values, k->values, minlen*array->type_bytes());

		if ( len > minlen )
			{
			if ( array->zero() )
				(*array->zero())( &((char*)array->values)[k->bytes()],len - k->length );
			else
				memset( &((char*)array->values)[k->bytes()], 0,
					(len-k->length)*array->type_bytes() );
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
		array->SetType( k->type );
		mode = m;
		array->Grow( k->length, 0 );
		if ( array->copy() )
			(*array->copy())(array->values, k->values, array->length);
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
	if ( VALUE(mode) 
#ifdef GGC
	     && ! glish_collecting_garbage
#endif
	     )
		Unref( value );
	else if ( REF(mode) )
		Unref( vecref );
	}

int ValueKernel::Sizeof( ) const
	{
	if ( ARRAY(mode) )
		{
		if ( Type() != TYPE_STRING )
			return (int) array->bytes() + sizeof(array_t) + sizeof(ValueKernel);
		else
			{
			int cnt = 0;
			for ( unsigned int i = 0; i < array->length; i++ )
				cnt += strlen(((char**)array->values)[i])+1;
			return cnt + array->bytes() + sizeof(array_t) + sizeof(ValueKernel);
			}
		}
	else if ( RECORD(mode) )
		return sizeof(record_t) + record->Sizeof( );
	else
		return otherSizeof();
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
			for ( unsigned int i = 0; i < array->length; i++ )
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

#ifdef GGC
		if ( glish_collecting_garbage )
			while ( (member = r->NextEntry( key, c )) )
				free_memory( (void*) key );
		else
#endif
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
	Value* new_member;

	if ( ordered )
		{
		for ( int i = 0; i < rptr->Length(); i++ )
			{
			member = rptr->NthEntry(i,key);
			new_member = copy_value( member );
			if ( member->IsConst() )
				new_member->MakeConst();
			if ( member->IsModConst() )
				new_member->MakeModConst();
			new_record->Insert( strdup(key), new_member );
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
