//======================================================================
// io.cc
//
// $Id$
//
//======================================================================
#include "sos/sos.h"
RCSID("@(#) $Id$")
#include "config.h"
#include "sos/io.h"
#include "sos/types.h"
#include "convert.h"
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include <string.h>
#include <stdio.h>
#include <iostream.h>

#if defined(HAVE_SYS_UIO_H)
#include <sys/uio.h>
#endif
#if defined(HAVE_SYS_LIMITS_H)
#include <sys/limits.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(HAVE_WRITEV) && defined(WRITEV_NOT_DECLARED)
extern "C" int writev(int, const struct iovec *, int);
#endif

#if defined(IOV_MAX)
#define MAXIOV IOV_MAX
#elif defined(UIO_MAXIOV)
#define MAXIOV UIO_MAXIOV
#else
#define MAXIOV 16
#endif

unsigned int sos_fd_sink::tmp_buf_count = 10;
unsigned int sos_fd_sink::tmp_buf_size = 1024;

sos_sink::~sos_sink() { }
sos_source::~sos_source() { }

sos_fd_sink::sos_fd_sink( int fd_ ) : fd(fd_)
	{
	iov = (void*) alloc_memory( MAXIOV * sizeof( struct iovec ) );
	status = (buffer_type*) alloc_memory( MAXIOV * sizeof( buffer_type ) );
	iov_cnt = 0;

	tmp_bufs = (void**) alloc_zero_memory( tmp_buf_count * sizeof(void*) );
	tmp_buf_cur = 0;
	}

unsigned int sos_fd_sink::write( const char *buf, unsigned int len, buffer_type type )
	{
	struct iovec *iov_ = (struct iovec*) iov;
	if ( fd < 0 ) return 0;
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

unsigned int sos_fd_sink::flush( )
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

sos_fd_sink::~sos_fd_sink()
	{
	flush( );
	if ( fd >= 0 ) close(fd);
	}

unsigned int sos_fd_source::read( char *buf, unsigned int len )
	{
	if ( fd < 0 ) return 0;
	return ::read( fd, buf, len );
	}

sos_fd_source::~sos_fd_source()
	{
	if ( fd >= 0 ) close(fd);
	}

sos_out::sos_out( sos_sink &out_, int integral_header ) : out(out_)
	{
	if ( integral_header )
		not_integral = 0;
	else
		{
		not_integral = (char*) alloc_memory(SOS_HEADER_SIZE);
		head.set( not_integral, 0, SOS_UNKNOWN );
		}
	}

static unsigned char zero_user_area[] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };


#define PUTNUMERIC_BODY( TYPE, SOSTYPE, PARAM, SOURCE )		\
	{							\
	if ( not_integral )					\
		{						\
		head.set(l,SOSTYPE);				\
		memcpy( head.iBuffer() + 18, SOURCE, 6 );	\
		head.stamp();					\
		out.write( head.iBuffer(), SOS_HEADER_SIZE, sos_sink::COPY ); \
		out.write( a, l * sos_size(SOSTYPE), type );	\
		}						\
	else							\
		{						\
		head.set(a,l PARAM);				\
		memcpy( head.iBuffer() + 18, SOURCE, 6 );	\
		head.stamp();					\
		out.write( head.iBuffer(), l * sos_size(SOSTYPE) + \
			   SOS_HEADER_SIZE, type );		\
		}						\
	}


#define PUTNUMERIC(TYPE,SOSTYPE)				\
void sos_out::put( TYPE *a, unsigned int l,			\
		    sos_sink::buffer_type type )		\
	PUTNUMERIC_BODY(TYPE, SOSTYPE,, zero_user_area)		\
void sos_out::put( TYPE *a, unsigned int l, sos_header &h,	\
		    sos_sink::buffer_type type ) 		\
	PUTNUMERIC_BODY(TYPE, SOSTYPE,, h.iBuffer() + 18)

PUTNUMERIC(byte,SOS_BYTE)
PUTNUMERIC(short,SOS_SHORT)
PUTNUMERIC(int,SOS_INT)
PUTNUMERIC(float,SOS_FLOAT)
PUTNUMERIC(double,SOS_DOUBLE)

#define COMMA(X) , X
#define PUTCHAR(TYPE)						\
void sos_out::put( TYPE *a, unsigned int l, sos_code t,		\
		    sos_sink::buffer_type type )		\
	PUTNUMERIC_BODY(TYPE, t, COMMA(t), zero_user_area)	\
void sos_out::put( TYPE *a, unsigned int l, sos_code t,		\
		    sos_header &h, sos_sink::buffer_type type )	\
	PUTNUMERIC_BODY(TYPE, t, COMMA(t), h.iBuffer() + 18)

PUTCHAR(char)
PUTCHAR(unsigned char)


#define PUTCHARPTR_BODY(SOURCE)						\
	{								\
	unsigned int total = (len+1) * 4;				\
	char *buf = (char*) alloc_memory( total + SOS_HEADER_SIZE );	\
	unsigned int *lptr = (unsigned int *) (buf + SOS_HEADER_SIZE);	\
									\
	*lptr++ = len;							\
	for ( unsigned int i = 0; i < len; i++ )			\
		{							\
		lptr[i] = s[i] ? ::strlen(s[i]) : 0;			\
		total += lptr[i];					\
		}							\
									\
	buf = (char*) realloc_memory( buf, total + SOS_HEADER_SIZE );	\
	lptr = (unsigned int *) (buf + SOS_HEADER_SIZE + 4);		\
									\
	head.set(buf,total,SOS_STRING);					\
	memcpy( head.iBuffer() + 18, SOURCE, 6 );			\
	head.stamp();							\
									\
	char *cptr = (char*)(&lptr[len]);				\
									\
	for ( unsigned int j = 0; j < len; j++ )			\
		{							\
		register char *out = (char*) s[j];			\
		if ( out )						\
			{						\
			memcpy( cptr, out, *lptr );			\
			cptr += *lptr++;				\
			}						\
		}							\
									\
	out.write( buf, total + SOS_HEADER_SIZE, sos_sink::FREE );	\
	if ( not_integral ) head.set( not_integral, 0, SOS_UNKNOWN );	\
									\
	if ( type == sos_sink::FREE )					\
		{							\
		for ( int X = 0; X < len; X++ )				\
			free_memory( (char*) s[X] );			\
		free_memory( s );					\
		}							\
	}

#define PUTSTR_BODY(SOURCE)						\
	{								\
	unsigned int len = s.length();					\
	unsigned int total = (len+1) * 4;				\
									\
	for ( unsigned int i = 0; i < len; i++ )			\
		total += s.strlen(i);					\
									\
	char *buf = (char*) alloc_memory( total + SOS_HEADER_SIZE );	\
									\
	head.set(buf,total,SOS_STRING);					\
	memcpy( head.iBuffer() + 18, SOURCE, 6 );			\
	head.stamp();							\
									\
	unsigned int *lptr = (unsigned int *) (buf + SOS_HEADER_SIZE);	\
	*lptr++ = len;							\
	char *cptr = (char*)(&lptr[len]);				\
									\
	for ( unsigned int j = 0; j < len; j++ )			\
		{							\
		register char *out = (char*) s.get(j);			\
		if ( out )						\
			{						\
			register unsigned int slen = s.strlen(j);	\
			memcpy( cptr, out, slen );			\
			*lptr++ = slen;					\
			cptr += slen;					\
			}						\
		else							\
			*lptr++ = 0;					\
		}							\
									\
	out.write( buf, total + SOS_HEADER_SIZE, sos_sink::FREE );	\
	if ( not_integral ) head.set( not_integral, 0, SOS_UNKNOWN );	\
	}

void sos_out::put( charptr *s, unsigned int len, sos_sink::buffer_type type )
	PUTCHARPTR_BODY(zero_user_area)
void sos_out::put( charptr *s, unsigned int len, sos_header &h, sos_sink::buffer_type type )
	PUTCHARPTR_BODY(h.iBuffer() + 18)

#if defined(ENABLE_STR)
void sos_out::put( const str &s )
	PUTSTR_BODY(zero_user_area)
void sos_out::put( const str &s, sos_header &h )
	PUTSTR_BODY(h.iBuffer() + 18)
#endif


#define PUTREC_BODY( SOURCE )				\
	{						\
	static char buf[SOS_HEADER_SIZE];		\
							\
	if ( not_integral )				\
		head.set(l,SOS_RECORD);			\
	else						\
		head.set(buf,l,SOS_RECORD);		\
							\
	memcpy( head.iBuffer() + 18, SOURCE, 6 );	\
	head.stamp();					\
							\
	out.write( head.iBuffer(), SOS_HEADER_SIZE, sos_sink::COPY ); \
	}

void sos_out::put_record_start( unsigned int l )
	PUTREC_BODY(zero_user_area)
void sos_out::put_record_start( unsigned int l, sos_header &h )
	PUTREC_BODY(h.iBuffer() + 18)

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

sos_out::~sos_out() { if ( not_integral ) free_memory(not_integral); }

sos_in::sos_in( sos_source &in_, int use_str_, int integral_header ) : in(in_), use_str(use_str_),
			head((char*) alloc_memory(SOS_HEADER_SIZE), 0, SOS_UNKNOWN, 1), not_integral(integral_header ? 0 : 1)
	{
	}

void *sos_in::get( unsigned int &len, sos_code &type )
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

void *sos_in::get( unsigned int &len, sos_code &type, sos_header &h )
	{
	type = SOS_UNKNOWN;
	if ( in.read( head.iBuffer(),SOS_HEADER_SIZE ) <= 0 )
		return 0;

	type = head.type();
	len = head.length();

// 	cerr << getpid() << ": " << head << endl;
	switch ( type )
		{
		case SOS_STRING:
			{
			void *ret = use_str ? get_string( len, head ) : get_chars( len, head );
			h.scratch();
			h.set(len,SOS_STRING);
			memcpy( h.iBuffer(), head.iBuffer(), SOS_HEADER_SIZE );
			return ret;
			}
		case SOS_RECORD:
			{
			h.scratch();
			h.set(len,SOS_RECORD);
			memcpy( h.iBuffer(), head.iBuffer(), SOS_HEADER_SIZE );
			return (void*) -1;
			}
		default:
			{
			char *ret = (char*) get_numeric( type, len, head );
			if ( not_integral )
				{
				h.scratch();
				h.set(len,type);
				memcpy( h.iBuffer(), head.iBuffer(), SOS_HEADER_SIZE );
				}
			else
				h.set(ret,len,type);
			return ret;
			}
		}
	}

void *sos_in::get_numeric( sos_code &type, unsigned int &len, sos_header &head )
	{
	char *result_ = 0;
	char *result = 0;

	if ( not_integral )
		{
		result_ = (char*) alloc_memory( len * head.typeLen() );
		result  = result_;
		}
	else
		{
		result_ = (char*) alloc_memory(len * head.typeLen() + SOS_HEADER_SIZE);
		memcpy(result_, head.iBuffer(), SOS_HEADER_SIZE);
		result  = result_ + SOS_HEADER_SIZE;
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

#if defined(ENABLE_STR)
void *sos_in::get_string( unsigned int &len, sos_header &head )
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
#else
void *sos_in::get_string( unsigned int &, sos_header & ) { return 0; }
#endif

void *sos_in::get_chars( unsigned int &len, sos_header &head )
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

sos_in::~sos_in() { }
