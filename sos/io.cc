//======================================================================
// io.cc
//
// $Id$
//
//======================================================================
#include "sos/sos.h"
RCSID("@(#) $Id$")
#include "sos/io.h"
#include "sos/types.h"
#include "convert.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/uio.h>


#define PUTACTION_A(TYPE,SOSTYPE)					\
void sos_sink::put( TYPE *a, unsigned int l, short U1, short U2 )	\
	{								\
	head.set(a,l);							\
	head.usets(U1,0);						\
	head.usets(U2,1);						\
	head.stamp();							\
									\
	out.write(head.iBuffer(), head.iLength() * sos_size(SOSTYPE) +	\
	      sos_header::iSize());					\
	}

PUTACTION_A(byte,SOS_BYTE)
PUTACTION_A(short,SOS_SHORT)
PUTACTION_A(int,SOS_INT)
PUTACTION_A(float,SOS_FLOAT)
PUTACTION_A(double,SOS_DOUBLE)

#define PUTACTION_B(TYPE)						\
void sos_sink::put( TYPE *a, sos_code t, unsigned int l, short U1, short U2 ) \
	{								\
	head.set(a,t,l);						\
	head.usets(U1,0);						\
	head.usets(U2,1);						\
	head.stamp();							\
									\
	out.write(head.iBuffer(), head.iLength() * sos_size(t) +	\
	      sos_header::iSize());					\
	}

PUTACTION_B(char)
PUTACTION_B(unsigned char)

void sos_sink::put( charptr *s, unsigned int len, short U1, short U2 )
	{
	unsigned int total = (len+1) * 4;
	char *buf = (char*) alloc_memory( total + SOS_HEADER_SIZE );
	unsigned int *lptr = (unsigned int *) (buf + SOS_HEADER_SIZE);

	*lptr++ = len;
	for ( unsigned int i = 0; i < len; i++ )
		{
		lptr[i] = s[i] ? ::strlen(s[i]) : 0;
		total += lptr[i];
		}

	buf = (char*) realloc_memory( buf, total + SOS_HEADER_SIZE );

	head.set(buf,SOS_STRING,total);
	head.usets(U1,0);
	head.usets(U2,1);
	head.stamp();

	char *cptr = (char*)(&lptr[len]);

	for ( unsigned int j = 0; j < len; j++ )
		{
		register char *out = (char*) s[j];
		if ( out )
			{
			memcpy( cptr, out, *lptr );
			cptr += *lptr++;
			}
		}

	out.write( buf, total + SOS_HEADER_SIZE );
	free_memory( buf );
	}

void sos_sink::put( const str &s, short U1, short U2 )
	{
	unsigned int len = s.length();
	unsigned int total = (len+1) * 4;

	for ( unsigned int i = 0; i < len; i++ )
		total += s.strlen(i);

	char *buf = (char*) alloc_memory( total + SOS_HEADER_SIZE );

	head.set(buf,SOS_STRING,total);
	head.usets(U1,0);
	head.usets(U2,1);
	head.stamp();

	unsigned int *lptr = (unsigned int *) (buf + SOS_HEADER_SIZE);
	*lptr++ = len;
	char *cptr = (char*)(&lptr[len]);

	for ( unsigned int j = 0; j < len; j++ )
		{
		register char *out = (char*) s.get(j);
		if ( out )
			{
			register unsigned int slen = s.strlen(j);
			memcpy( cptr, out, slen );
			*lptr++ = slen;
			cptr += slen;
			}
		else
			*lptr++ = 0;
		}

	out.write( buf, total + SOS_HEADER_SIZE );
	free_memory( buf );
	}


void sos_sink::put_record_start( unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 )
	{
	static char buf[SOS_HEADER_SIZE];

	head.set(buf,SOS_RECORD,l);
	head.usets(U1,0);
	head.usets(U2,1);
	head.stamp();

	out.write( buf, SOS_HEADER_SIZE );
	}


#if defined(VAXFP)
#define FOREIGN_FLOAT   SOS_IFLOAT
#define FOREIGN_DOUBLE  SOS_IDOUBLE
#define RIGHT_FLOAT     ieee2vax_single
#define RIGHT_DOUBLE    ieee2vax_double
#define DOUBLE_OP	'G'
#define FOREIGN_SWAP_A	case SOS_IDOUBLE:
#define FOREIGN_SWAP_B
#else 
#define FOREIGN_FLOAT   SOS_VFLOAT
#define FOREIGN_DOUBLE  SOS_DVDOUBLE: case SOS_GVDOUBLE
#define RIGHT_FLOAT     vax2ieee_single
#define RIGHT_DOUBLE    vax2ieee_double
#define DOUBLE_OP	type == SOS_DVDOUBLE ? 'D' : 'G'
#define FOREIGN_SWAP_A
#define FOREIGN_SWAP_B	if ( sos_big_endian ) swap_abcd_dcba(result,len*2); \
			else swap_abcdefgh_efghabcd(result,len);
#endif

sos_sink::~sos_sink() { if ( FD >= 0 ) close(FD); }

void *sos_source::get( sos_code &type, unsigned int &len )
	{
	type = SOS_UNKNOWN;
	if ( in.read( head.iBuffer(),sos_header::iSize() ) <= 0 )
		return 0;

	type = head.type();
	len = head.length();

	if ( type == SOS_STRING )
		if ( use_str )
			return get_string( len, head );
		else
			return get_chars( len, head );
	else
		return get_numeric( type, len, head );
	}

void *sos_source::get_numeric( sos_code type, unsigned int &len, sos_header &head )
	{
	char *result_ = (char*) alloc_memory(len * head.typeLen() + sos_header::iSize());
	memcpy(result_, head.iBuffer(), sos_header::iSize());
	char *result  = result_ + sos_header::iSize();
	in.read( result, len * head.typeLen());
	if ( ! (head.magic() & SOS_MAGIC) )
		switch( type ) {
		    case SOS_SHORT:
			swap_ab_ba(result,len);
			break;
		    case SOS_INT:
		    case SOS_FLOAT:
		    case FOREIGN_FLOAT:
		    	swap_abcd_dcba(result,len);
			break;
		    FOREIGN_SWAP_A
		    case SOS_DOUBLE:
		    	swap_abcdefgh_hgfedcba(result,len);
			break;
		}

	switch ( type ) {
	    case FOREIGN_FLOAT:
		RIGHT_FLOAT((float*)result,len);
		type = SOS_FLOAT;
		break;
	    case FOREIGN_DOUBLE:
		FOREIGN_SWAP_B
		RIGHT_DOUBLE((double*)result,len,DOUBLE_OP);
		type = SOS_DOUBLE;
		break;
	}
	return result_;
	}

void *sos_source::get_string( unsigned int &len, sos_header &head )
	{
	int swap = ! (head.magic() & SOS_MAGIC);
	char *buf = (char*) alloc_memory(len);
	in.read( buf, len );

	unsigned int *lptr = (unsigned int*) buf;
	len = *lptr++;
	if ( swap )
		{
		swap_abcd_dcba((char*) &len, 1);
		swap_abcd_dcba((char*) lptr, len );
		}

	char *cptr = (char*)(&lptr[len]);
	str *ns = new str( len );
	char **ary = ns->getary();
	unsigned int *lary = ns->getlen();
	for ( unsigned int i = 0; i < len; i++ )
		{
		register unsigned int slen = *lptr++;
		ary[i] = (char*) alloc_memory( slen + 1 );
		lary[i] = slen;
		memcpy(ary[i],cptr,slen);
		ary[i][slen] = '\0';
		cptr += slen;
		}

	free_memory( buf );
	return ns;
	}

void *sos_source::get_chars( unsigned int &len, sos_header &head )
	{
	int swap = ! (head.magic() & SOS_MAGIC);
	char *buf = (char*) alloc_memory(len);
	in.read( buf, len );

	unsigned int *lptr = (unsigned int*) buf;
	len = *lptr++;
	if ( swap )
		{
		swap_abcd_dcba((char*) &len, 1);
		swap_abcd_dcba((char*) lptr, len );
		}

	char *cptr = (char*)(&lptr[len]);
	char **ary = (char **) alloc_memory(len * sizeof(char*));
	for ( unsigned int i = 0; i < len; i++ )
		{
		register unsigned int slen = *lptr++;
		ary[i] = (char*) alloc_memory( slen + 1 );
		memcpy(ary[i],cptr,slen);
		ary[i][slen] = '\0';
		cptr += slen;
		}

	free_memory( buf );
	return ary;
	}

sos_source::~sos_source() { if ( FD >= 0 ) close(FD); }
