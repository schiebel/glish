// $Header$

#ifndef valkern_h
#define valkern_h
#include "Glish/GlishType.h"
#include "Glish/Dict.h"
#include "Glish/Complex.h"

class Value;
class VecRef;

declare(PDict,Value);
typedef PDict(Value)* recordptr;

typedef recordptr attributeptr;
typedef void* voidptr;

extern recordptr copy_record_dict( recordptr );
extern recordptr create_record_dict();
void delete_record( recordptr r );

typedef void (*KernelCopyFunc)( void *, void *, unsigned long len );
typedef void (*KernelZeroFunc)( void *, unsigned long len );
typedef void (*KernelDeleteFunc)( void *, unsigned long len );

extern void copy_strings( void *, void *, unsigned long len );
extern void delete_strings( void *, unsigned long len );

//
// Copy-on-write semantics
//
class ValueKernel {
    protected:
	inline unsigned int ARRAY( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<0; }
	inline unsigned int RECORD( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<1; }
	inline unsigned int VALUE( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<2; }
	inline unsigned int REF( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<3; }
	inline unsigned int OPAQUE( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<4; }
	inline unsigned int CONST( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<5; }
	inline unsigned int FIELD_CONST( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<6; }

#if defined(_AIX)
    public:
#endif

	struct array_t {
		glish_type type;
		unsigned short type_bytes;

		unsigned long length;
		unsigned long alloc_bytes;
		unsigned long bytes() { return type_bytes*length; }

		unsigned long ref_count;

		void* values;
		KernelCopyFunc copy;
		KernelZeroFunc zero;
		KernelDeleteFunc final;

		void SetType( glish_type t, KernelCopyFunc c = 0, KernelZeroFunc z = 0,
			      KernelDeleteFunc d = 0);
		void SetType( glish_type t, unsigned short type_len, KernelCopyFunc c = 0,
			      KernelZeroFunc z = 0, KernelDeleteFunc d = 0 );
		void Grow( unsigned long len, int do_zero = 1 );
		void SetStorage( void *storage, unsigned long len )
			{
			values = storage;
			length = len;
			alloc_bytes = length*type_bytes;
			}
		void clear();

		array_t() : type(TYPE_ERROR), length(0), alloc_bytes(0),
				type_bytes(0), values(0), ref_count(1),
				zero(0), copy(0) { DIAG2((void*)this,"\t\tarray_t alloc") }
		~array_t();
	};

	struct record_t {
		recordptr record;
		unsigned long ref_count;
		void clear();
		record_t() : record(0), ref_count(1) { DIAG2((void*)this,"\t\trecord_t alloc") }
		~record_t() { clear(); }
	};

#if defined(_AIX)
    protected:
#endif
	union {
		array_t *array;
		record_t *record;
		Value *value;
		VecRef *vecref;
		void *opaque;
	};
	unsigned int mode;

	glish_type otherType() const;
	unsigned long otherLength() const;

	void unrefArray(int del=0);
	void unrefRecord(int del=0);
	void unrefOthers();
	void unref( int del=0 )
		{
		if ( ARRAY(mode) ) unrefArray(del);
		else if ( RECORD(mode) ) unrefRecord(del);
		else unrefOthers();
		}

	void refArray() { if ( array ) ++array->ref_count; }
	void refRecord() { if ( record ) ++record->ref_count; }
	void refOthers();
	void ref()
		{ 
		if ( ARRAY(mode) ) refArray();
		else if ( RECORD(mode) ) refRecord();
		else refOthers();
		}

    public:

	void MakeConst() { mode |= CONST(); }
	void MakeFieldConst() { mode |= FIELD_CONST(); }
	int IsConst() const { return CONST(mode); }
	int IsFieldConst() const { return FIELD_CONST(mode); }

	ValueKernel() : array(0), mode(0) { }
	ValueKernel( const ValueKernel &v ) : mode( v.mode & ~ CONST() ),
					      array(v.array) { ref(); }

	ValueKernel &operator=( const ValueKernel &v );

	ValueKernel( glish_type t, unsigned long len, KernelCopyFunc c = 0,
		     KernelZeroFunc z = 0, KernelDeleteFunc d = 0 );

	void SetType( glish_type t, unsigned long l, KernelCopyFunc c=0,
		      KernelZeroFunc z=0, KernelDeleteFunc d=0 );
// 	void SetArray( void *storage, unsigned long len, glish_type t, int copy = 0,
// 		       KernelCopyFunc c=0, KernelZeroFunc z=0, KernelDeleteFunc d=0 );
	void SetArray( glish_bool vec[], unsigned long len, int copy = 0,
		       KernelCopyFunc c=0, KernelZeroFunc z=0, KernelDeleteFunc d=0 );
	void SetArray( byte vec[], unsigned long len, int copy = 0,
		       KernelCopyFunc c=0, KernelZeroFunc z=0, KernelDeleteFunc d=0 );
	void SetArray( short vec[], unsigned long len, int copy = 0,
		       KernelCopyFunc c=0, KernelZeroFunc z=0, KernelDeleteFunc d=0 );
	void SetArray( int vec[], unsigned long len, int copy = 0,
		       KernelCopyFunc c=0, KernelZeroFunc z=0, KernelDeleteFunc d=0 );
	void SetArray( float vec[], unsigned long len, int copy = 0,
		       KernelCopyFunc c=0, KernelZeroFunc z=0, KernelDeleteFunc d=0 );
	void SetArray( double vec[], unsigned long len, int copy = 0,
		       KernelCopyFunc c=0, KernelZeroFunc z=0, KernelDeleteFunc d=0 );
	void SetArray( complex vec[], unsigned long len, int copy = 0,
		       KernelCopyFunc c=0, KernelZeroFunc z=0, KernelDeleteFunc d=0 );
	void SetArray( dcomplex vec[], unsigned long len, int copy = 0,
		       KernelCopyFunc c=0, KernelZeroFunc z=0, KernelDeleteFunc d=0 );
	void SetArray( const char* vec[], unsigned long len, int copy = 0,
		       KernelCopyFunc c=copy_strings, KernelZeroFunc z=0,
		       KernelDeleteFunc d=delete_strings );
	void SetArray( voidptr vec[], unsigned long len, glish_type t, int copy = 0,
		       KernelCopyFunc c=0, KernelZeroFunc z=0, KernelDeleteFunc d=0 );

	void Grow( unsigned long len );

	// May return 0 if Type() == TYPE_ERROR
	void *constArray() const { return array->values; }
	// Shouldn't return 0
	void *modArray();
	void *modArray() const { return (((ValueKernel*)this)->modArray()); }

	void SetRecord( recordptr r );
	// May return 0
	recordptr constRecord() const { return record ? record->record : 0; }
	// Shouldn't return 0
	recordptr modRecord();
	recordptr modRecord() const { return (((ValueKernel*)this)->modRecord()); }

	void SetValue( Value *v );
	Value *GetValue() const { return value; }

	void SetVecRef( VecRef *v );
	VecRef *GetVecRef() const { return vecref; }

	void SetOpaque( void *v );
	void *GetOpaque() const { return opaque; }

	glish_type Type() const
		{
		return ARRAY(mode) ? array->type :
		       RECORD(mode) ? TYPE_RECORD :
		       otherType();
		}
	unsigned long Length() const
		{
		return ARRAY(mode) ? array->length :
		       RECORD(mode) ? record->record->Length() :
		       otherLength();
		}
	  

	~ValueKernel( ) { unref(1); }
};

#endif
