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
#include <sys/types.h>

#if defined(IOV_MAX)
#define MAXIOV IOV_MAX
#elif defined(UIO_MAXIOV)
#define MAXIOV UIO_MAXIOV
#else
#define MAXIOV 16
#endif

unsigned int FD_SINK::tmp_buf_count = 10;
unsigned int FD_SINK::tmp_buf_size = 1024;


SINK::~SINK() { }
SOURCE::~SOURCE() { }

FD_SINK::FD_SINK( int fd_ ) : fd(fd_)
	{
	iov = (void*) alloc_memory( MAXIOV * sizeof( struct iovec ) );
	status = (buffer_type*) alloc_memory( MAXIOV * sizeof( buffer_type ) );
	iov_cnt = 0;

	tmp_bufs = (void**) alloc_zero_memory( 10 * sizeof(void*) );
	tmp_buf_cur = 0;
	}

unsigned int FD_SINK::write( const char *buf, unsigned int len, buffer_type type )
	{
	struct iovec *iov_ = (struct iovec*) iov;
	switch ( type )
		{
		case COPY:
			if ( len <= tmp_buf_size && tmp_buf_cur < tmp_buf_count )
				{
				if ( ! tmp_bufs[tmp_buf_cur] )
					tmp_bufs[tmp_buf_cur] = alloc_memory( tmp_buf_size );
				memcpy( tmp_bufs[tmp_buf_cur], buf, len );
				type = HOLD;
				buf = (const char *) tmp_bufs[tmp_buf_cur++];
				}
		case HOLD:
		case FREE:
			iov_[iov_cnt].iov_base = (char*) buf;
			iov_[iov_cnt].iov_len = len;
			status[iov_cnt++] = type;
			break;
		}

	if ( iov_cnt >= MAXIOV || type == COPY )
		return flush( );

	return len;
	}

unsigned int FD_SINK::flush( )
	{
	if ( iov_cnt == 0 ) return 0;
	struct iovec *iov_ = (struct iovec*) iov;
	unsigned int ret = writev( fd, iov_, iov_cnt );
	for ( int x = 0; x < iov_cnt; x++ )
		if ( status[x] == FREE )
			free_memory( iov_[x].iov_base );
	iov_cnt = tmp_buf_cur = 0;
	return ret;
	}

FD_SINK::~FD_SINK()
	{
	flush( );
	if ( fd >= 0 ) close(fd);
	}

unsigned int FD_SOURCE::read( char *buf, unsigned int len )
	{
	return ::read( fd, buf, len );
	}

FD_SOURCE::~FD_SOURCE()
	{
	if ( fd >= 0 ) close(fd);
	}

sos_sink::sos_sink( SINK &out_, int integral_header ) : out(out_)
	{
	if ( integral_header )
		{
		integral = (char*) alloc_memory(SOS_HEADER_SIZE);
		head.set( integral, 0, SOS_UNKNOWN );
		}
	else
		integral = 0;
	}

#define PUTACTION_A(TYPE,SOSTYPE)				\
void sos_sink::put( TYPE *a, unsigned int l, unsigned short U1,	\
		    unsigned short U2 )				\
	{							\
	if ( integral )						\
		{						\
		head.set(a,l);					\
		head.usets(U1,0);				\
		head.usets(U2,1);				\
		head.stamp();					\
		out.write( head.iBuffer(), l * sos_size(SOSTYPE) + \
			   SOS_HEADER_SIZE );			\
		}						\
	else							\
		{						\
		head.set(l,SOSTYPE);				\
		head.usets(U1,0);				\
		head.usets(U2,1);				\
		head.stamp();					\
		out.write( head.iBuffer(), SOS_HEADER_SIZE );	\
		out.write( a, l * sos_size(SOSTYPE) );		\
		}						\
	}

PUTACTION_A(byte,SOS_BYTE)
PUTACTION_A(short,SOS_SHORT)
PUTACTION_A(int,SOS_INT)
PUTACTION_A(float,SOS_FLOAT)
PUTACTION_A(double,SOS_DOUBLE)

#define PUTACTION_B(TYPE)					\
void sos_sink::put( TYPE *a, unsigned int l, sos_code t,	\
		    unsigned short U1, unsigned short U2 ) 	\
	{							\
	if ( integral )						\
		{						\
		head.set(a,l,t);				\
		head.usets(U1,0);				\
		head.usets(U2,1);				\
		head.stamp();					\
		out.write(head.iBuffer(), l * sos_size(t) +	\
			  SOS_HEADER_SIZE);			\
		}						\
	else							\
		{						\
		head.set(l,t);					\
		head.usets(U1,0);				\
		head.usets(U2,1);				\
		head.stamp();					\
		out.write( head.iBuffer(), SOS_HEADER_SIZE );	\
		out.write( a, l * sos_size(t) );		\
		}						\
	}

PUTACTION_B(char)
PUTACTION_B(unsigned char)

void sos_sink::put( charptr *s, unsigned int len, unsigned short U1, unsigned short U2 )
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

	head.set(buf,total,SOS_STRING);
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

	out.write( buf, total + SOS_HEADER_SIZE, SINK::FREE );
	if ( integral ) head.set( integral, 0, SOS_UNKNOWN );
	}

void sos_sink::put( const str &s, unsigned short U1, unsigned short U2 )
	{
	unsigned int len = s.length();
	unsigned int total = (len+1) * 4;

	for ( unsigned int i = 0; i < len; i++ )
		total += s.strlen(i);

	char *buf = (char*) alloc_memory( total + SOS_HEADER_SIZE );

	head.set(buf,total,SOS_STRING);
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

	out.write( buf, total + SOS_HEADER_SIZE, SINK::FREE );
	if ( integral ) head.set( integral, 0, SOS_UNKNOWN );
	}


void sos_sink::put_record_start( unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 )
	{
	static char buf[SOS_HEADER_SIZE];

	if ( ! integral )
		head.set(buf,l,SOS_RECORD);
	else
		head.set(l,SOS_RECORD);

	head.usets(U1,0);
	head.usets(U2,1);
	head.stamp();

	out.write( buf, SOS_HEADER_SIZE, SINK::COPY );
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

sos_sink::~sos_sink() { if ( integral ) free_memory(integral); }

sos_source::sos_source( SOURCE &in_, int use_str_ = 1, int integral_header = 1 ) : in(in_), use_str(use_str_),
			head((char*) alloc_memory(SOS_HEADER_SIZE), 0, SOS_UNKNOWN, 1), integral(integral_header)
	{
	}

void *sos_source::get( unsigned int &len, sos_code &type )
	{
	type = SOS_UNKNOWN;
	if ( in.read( head.iBuffer(),SOS_HEADER_SIZE ) <= 0 )
		return 0;

	type = head.type();
	len = head.length();

	switch ( type )
		{
		case SOS_STRING:
			return use_str ? get_string( len, head ) : get_chars( len, head );
		case SOS_RECORD:
			return (void*) -1;
		default:
			return get_numeric( type, len, head );
		}
	}

void *sos_source::get( unsigned int &len, sos_code &type, sos_header &h )
	{
	type = SOS_UNKNOWN;
	if ( in.read( head.iBuffer(),SOS_HEADER_SIZE ) <= 0 )
		return 0;

	type = head.type();
	len = head.length();

	switch ( type )
		{
		case SOS_STRING:
			{
			void *ret = use_str ? get_string( len, head ) : get_chars( len, head );
			char *b = (char*) alloc_memory( SOS_HEADER_SIZE );
			memcpy( b, head.iBuffer(), SOS_HEADER_SIZE );
			h.set(b,len,SOS_STRING,1);
			return ret;
			}
		case SOS_RECORD:
			{
			char *b = (char*) alloc_memory( SOS_HEADER_SIZE );
			memcpy( b, head.iBuffer(), SOS_HEADER_SIZE );
			h.set(b,len,SOS_RECORD,1);
			return (void*) -1;
			}
		default:
			{
			char *ret = (char*) get_numeric( type, len, head );
			if ( integral )
				h.set(ret,len,type);
			else
				{
				char *b = (char*) alloc_memory(SOS_HEADER_SIZE);
				memcpy( b, head.iBuffer(), SOS_HEADER_SIZE );
				h.set(b,len,type,1);
				}
			return ret;
			}
		}
	}

void *sos_source::get_numeric( sos_code type, unsigned int &len, sos_header &head )
	{
	char *result_ = 0;
	char *result = 0;

	if ( integral )
		{
		result_ = (char*) alloc_memory(len * head.typeLen() + SOS_HEADER_SIZE);
		memcpy(result_, head.iBuffer(), SOS_HEADER_SIZE);
		result  = result_ + SOS_HEADER_SIZE;
		}
	else
		{
		result_ = (char*) alloc_memory( len * head.typeLen() );
		result  = result_;
		}

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

sos_source::~sos_source() { }
