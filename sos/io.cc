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
#include <errno.h>

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

sos_status *sos_err_status = (sos_status*) -1;


struct sos_fd_buf_kernel {

	sos_fd_buf_kernel( );
	void reset( );
	~sos_fd_buf_kernel( );
	char *tmp( unsigned int len ) { return len <= tmp_size && tmp_cur < tmp_cnt ? new_tmp( ) : 0; }

	// iovec struct
	struct iovec *iov;
	// number of iovec struct
	unsigned int cnt;
	// status of buffer of each iovec struct
	sos_sink::buffer_type *status;
	// the total length of all buffers
	unsigned int total;

	// temporary buffers
	char **tmp_bufs;
	unsigned int tmp_cur;

	// constants
	static unsigned int tmp_cnt;
	static unsigned int tmp_size;
	static unsigned int size;

    private:
	char *new_tmp( );
};

//
//  These may need to be tuned for each application that uses
//  this library. With glish, the only writes which are tagged
//  as "COPY" are the writing of record headers (i.e. 
//  SOS_HEADER_SIZE bytes).
//
unsigned int sos_fd_buf_kernel::tmp_cnt = 16;
unsigned int sos_fd_buf_kernel::tmp_size = 64;
unsigned int sos_fd_buf_kernel::size = MAXIOV;

sos_fd_buf_kernel::sos_fd_buf_kernel( ) : cnt(0), tmp_cur(0), total(0)
	{
	iov = (struct iovec*) sos_alloc_memory( size * sizeof( struct iovec ) );
	status = (sos_sink::buffer_type*) sos_alloc_memory( size * sizeof( sos_sink::buffer_type ) );
	tmp_bufs = (char**) sos_alloc_zero_memory( tmp_cnt * sizeof(void*) );
	}

sos_fd_buf_kernel::~sos_fd_buf_kernel()
	{
	reset( );
	for ( int x = 0; tmp_bufs[x]; x++ )
		sos_free_memory( tmp_bufs[x] );
	}

char *sos_fd_buf_kernel::new_tmp( )
	{
	if ( ! tmp_bufs[tmp_cur] )
		tmp_bufs[tmp_cur] = (char *) sos_alloc_memory( tmp_size );
	return tmp_bufs[tmp_cur++];
	}

void sos_fd_buf_kernel::reset( )
	{
	for ( int x = 0; x < cnt; x++ )
		if ( status[x] == sos_sink::FREE )
			sos_free_memory( iov[x].iov_base );
	cnt = 0;
	tmp_cur = 0;
	total = 0;
	}

sos_fd_buf::sos_fd_buf( )
	{
	buf.append( new sos_fd_buf_kernel );
	}

unsigned int sos_fd_buf::total( )
	{
	return first( )->total;
	}

sos_fd_buf_kernel *sos_fd_buf::next( )
	{
	if ( buf.length() > 1 )
		{
		delete buf.remove_nth( 0 );  // later reuse these
		return first( );
		}

	first()->reset( );
	return 0;
	}
		  
sos_fd_buf_kernel *sos_fd_buf::add( )
	{
	buf.append( new sos_fd_buf_kernel ); // later reuse these
	return last( );
	}

sos_sink::~sos_sink() { }
sos_source::~sos_source() { }

int sos_fd_sink::nonblock_all_ = 0;

void sos_fd_sink::nonblock_all()
	{
	nonblock_all_ = 1;
	}

sos_fd_sink::sos_fd_sink( int fd__ ) : fd_(fd__), start(0), buf_holder(0), sent(0)
	{
	if ( nonblock_all_ == 1  && fd_ >= 0 ) nonblock();
	}

void sos_fd_sink::setFd( int fd__ )
	{
	fd_ = fd__;
	if ( nonblock_all_ == 1  && fd_ >= 0 ) nonblock();
	}

sos_status *sos_fd_sink::write( const char *cbuf, unsigned int len, buffer_type type )
	{
	if ( fd_ < 0 ) return 0;

	sos_fd_buf_kernel &K = *buf.last();

	switch ( type )
		{
		case COPY:
			{
			if ( char *t = K.tmp( len ) )
				{
				memcpy( t, cbuf, len );
				type = HOLD;
				cbuf = (const char *) t;
				}
			}
		case HOLD:
		case FREE:
			K.iov[K.cnt].iov_base = (char*) cbuf;
			K.iov[K.cnt].iov_len = len;
			K.status[K.cnt++] = type;
			break;
		}

	K.total += len;
	if ( K.cnt >= K.size || type == COPY )
		return flush( );

	return 0;
	}

sos_status *sos_fd_sink::flush( )
	{
	sos_status *stat = real_flush( );
	while ( stat ) stat = stat->resume( );
	return 0;
	}

void sos_fd_sink::reset( )
	{
	sos_fd_buf_kernel *K = buf.first();

	if ( K->cnt == 0 ) return;

	struct iovec *iov = K->iov;

	if ( buf_holder )
		{
		iov[start].iov_base = (char*) buf_holder;
		buf_holder = 0;
		}

	start = sent = 0;
	while ( K = buf.next() );
	}

sos_status *sos_fd_sink::real_flush( )
	{
	sos_fd_buf_kernel *K = buf.first();

	if ( K->cnt == 0 ) return 0;

	int done = 0;
	while ( K )
		{
		struct iovec *iov = K->iov;
		unsigned int needed = K->total - sent;
		unsigned int buckets = K->cnt - start;
		int cur = 0;

		if ( (cur = writev( fd_, &iov[start], buckets )) < needed  || cur < 0 )
			{
			if ( cur < 0 )
				{
				// resource temporarily unavailable
				if ( errno == EAGAIN ) return this;
				perror( "sos_fd_sink::flush( )" );
				// broken pipe
				if ( errno == EPIPE ) { reset(); return 0; }
				exit(1);
				}

			int old_start = start;

			unsigned int cnt;
			for ( cnt = iov[start].iov_len; cnt < cur; cnt += iov[++start].iov_len );

			if ( buf_holder && old_start != start )
				{
				iov[old_start].iov_base = (char*) buf_holder;
				buf_holder = 0;
				}

			if ( cnt > cur )
				{
				if ( ! buf_holder ) buf_holder = iov[start].iov_base;
				iov[start].iov_base = iov[start].iov_base + iov[start].iov_len - (cnt - cur);
				iov[start].iov_len = cnt - cur;
				}

			sent += cur;
			return this;
			}

		if ( buf_holder )
			{
			iov[start].iov_base = (char*) buf_holder;
			buf_holder = 0;
			}

		start = sent = 0;
		K = buf.next( );
		}

	return 0;
	}

#define DEFINE_FDBLOCK(NAME,OP)						\
void sos_fd_sink::NAME( )						\
	{								\
	if ( fd_ < 0 ) return;						\
									\
	int val = -1;							\
	if ( (val = fcntl(fd_, F_GETFL, 0)) < 0 )			\
		perror("sos_fd_sink::block() F_GETFL");			\
	else								\
		if ( fcntl(fd_, F_SETFL, val OP O_NONBLOCK) < 0 )	\
			perror("sos_fd_sink::block() F_SETFL");		\
	}

DEFINE_FDBLOCK(block,& ~)
DEFINE_FDBLOCK(nonblock, |)

sos_fd_sink::~sos_fd_sink()
	{
	flush( );
	if ( fd_ >= 0 ) close(fd_);
	}

unsigned int sos_fd_source::read( char *buf, unsigned int len )
	{
	if ( fd_ < 0 || len == 0 ) return 0;

	unsigned int needed = len;
	unsigned int total = 0;

	register unsigned int cur = 0;
	while ( needed && (cur = ::read( fd_, buf, needed )) > 0 )
		{
		total += cur;
		buf += cur;
		needed -= cur;
		if ( total >= len ) needed = 0;
		}

	if ( cur < 0 ) perror("sos_fd_source::read()");

	return total;
	}

sos_fd_source::~sos_fd_source()
	{
	if ( fd_ >= 0 ) close(fd_);
	}

sos_out::sos_out( sos_sink *out_, int integral_header ) : out(out_)
	{
	if ( integral_header )
		not_integral = 0;
	else
		{
		not_integral = (char*) sos_alloc_memory(SOS_HEADER_SIZE);
		head.set( not_integral, 0, SOS_UNKNOWN );
		}
	}

static unsigned char zero_user_area[] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };


#define PUTNUMERIC_BODY( TYPE, SOSTYPE, PARAM, SOURCE )			\
	{								\
	if ( ! out )							\
		return error( NO_SINK );				\
									\
	if ( not_integral )						\
		{							\
		head.set(l,SOSTYPE);					\
		memcpy( head.iBuffer() + 18, SOURCE, 6 );		\
		head.stamp();						\
		out->write( head.iBuffer(), SOS_HEADER_SIZE, sos_sink::COPY ); \
		return out->write( a, l * sos_size(SOSTYPE), type ); 	\
		}							\
	else								\
		{							\
		head.set(a,l PARAM);					\
		memcpy( head.iBuffer() + 18, SOURCE, 6 );		\
		head.stamp();						\
		return out->write( head.iBuffer(), l * sos_size(SOSTYPE) + \
			   SOS_HEADER_SIZE, type );			\
		}							\
	}


#define PUTNUMERIC(TYPE,SOSTYPE)					\
sos_status *sos_out::put( TYPE *a, unsigned int l,			\
		    sos_sink::buffer_type type )			\
	PUTNUMERIC_BODY(TYPE, SOSTYPE,, zero_user_area)			\
sos_status *sos_out::put( TYPE *a, unsigned int l, sos_header &h,	\
		    sos_sink::buffer_type type ) 			\
	PUTNUMERIC_BODY(TYPE, SOSTYPE,, h.iBuffer() + 18)

PUTNUMERIC(byte,SOS_BYTE)
PUTNUMERIC(short,SOS_SHORT)
PUTNUMERIC(int,SOS_INT)
PUTNUMERIC(float,SOS_FLOAT)
PUTNUMERIC(double,SOS_DOUBLE)

#define COMMA(X) , X
#define PUTCHAR(TYPE)							\
sos_status *sos_out::put( TYPE *a, unsigned int l, sos_code t,		\
		    sos_sink::buffer_type type )			\
	PUTNUMERIC_BODY(TYPE, t, COMMA(t), zero_user_area)		\
sos_status *sos_out::put( TYPE *a, unsigned int l, sos_code t,		\
		    sos_header &h, sos_sink::buffer_type type )		\
	PUTNUMERIC_BODY(TYPE, t, COMMA(t), h.iBuffer() + 18)

PUTCHAR(char)
PUTCHAR(unsigned char)


#define PUTCHARPTR_BODY(SOURCE)						\
	{								\
	if ( ! out )							\
		return error( NO_SINK );				\
									\
	unsigned int total = (len+1) * 4;				\
	char *buf = (char*) sos_alloc_memory( total + SOS_HEADER_SIZE ); \
	unsigned int *lptr = (unsigned int *) (buf + SOS_HEADER_SIZE);	\
									\
	*lptr++ = len;							\
	for ( unsigned int i = 0; i < len; i++ )			\
		{							\
		lptr[i] = s[i] ? ::strlen(s[i]) : 0;			\
		total += lptr[i];					\
		}							\
									\
	buf = (char*) sos_realloc_memory( buf, total + SOS_HEADER_SIZE ); \
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
		register char *o = (char*) s[j];			\
		if ( o )						\
			{						\
			memcpy( cptr, o, *lptr );			\
			cptr += *lptr++;				\
			}						\
		}							\
									\
	sos_status *ret = out->write( buf, total + SOS_HEADER_SIZE,	\
				     sos_sink::FREE );			\
									\
	if ( not_integral ) head.set( not_integral, 0, SOS_UNKNOWN );	\
									\
	if ( type == sos_sink::FREE )					\
		{							\
		for ( int X = 0; X < len; X++ )				\
			sos_free_memory( (char*) s[X] );		\
		sos_free_memory( s );					\
		}							\
									\
	return ret;							\
	}

#define PUTSTR_BODY(SOURCE)						\
	{								\
	if ( ! out )							\
		return error( NO_SINK );				\
									\
	unsigned int len = s.length();					\
	unsigned int total = (len+1) * 4;				\
									\
	for ( unsigned int i = 0; i < len; i++ )			\
		total += s.strlen(i);					\
									\
	char *buf = (char*) sos_alloc_memory( total + SOS_HEADER_SIZE );\
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
		register char *o = (char*) s.get(j);			\
		if ( o )						\
			{						\
			register unsigned int slen = s.strlen(j);	\
			memcpy( cptr, o, slen );			\
			*lptr++ = slen;					\
			cptr += slen;					\
			}						\
		else							\
			*lptr++ = 0;					\
		}							\
									\
	sos_status *ret = out->write( buf, total + SOS_HEADER_SIZE,	\
				     sos_sink::FREE );			\
									\
	if ( not_integral ) head.set( not_integral, 0, SOS_UNKNOWN );	\
									\
	return ret;							\
	}

sos_status *sos_out::put( charptr *s, unsigned int len, sos_sink::buffer_type type )
	PUTCHARPTR_BODY(zero_user_area)
sos_status *sos_out::put( charptr *s, unsigned int len, sos_header &h, sos_sink::buffer_type type )
	PUTCHARPTR_BODY(h.iBuffer() + 18)

#if defined(ENABLE_STR)
sos_status *sos_out::put( const str &s )
	PUTSTR_BODY(zero_user_area)
sos_status *sos_out::put( const str &s, sos_header &h )
	PUTSTR_BODY(h.iBuffer() + 18)
#endif


#define PUTREC_BODY( SOURCE )						\
	{								\
	if ( ! out )							\
		return error( NO_SINK );				\
									\
	static char buf[SOS_HEADER_SIZE];				\
									\
	if ( not_integral )						\
		head.set(l,SOS_RECORD);					\
	else								\
		head.set(buf,l,SOS_RECORD);				\
									\
	memcpy( head.iBuffer() + 18, SOURCE, 6 );			\
	head.stamp();							\
									\
	return out->write( head.iBuffer(), SOS_HEADER_SIZE,		\
			  sos_sink::COPY );				\
	}

sos_status *sos_out::put_record_start( unsigned int l )
	PUTREC_BODY(zero_user_area)
sos_status *sos_out::put_record_start( unsigned int l, sos_header &h )
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

sos_out::~sos_out() { if ( not_integral ) sos_free_memory(not_integral); }

sos_in::sos_in( sos_source *in_, int use_str_, int integral_header ) : in(in_), use_str(use_str_),
			head((char*) sos_alloc_memory(SOS_HEADER_SIZE), 0, SOS_UNKNOWN, 1), not_integral(integral_header ? 0 : 1)
	{
	}

void *sos_in::get( unsigned int &len, sos_code &type )
	{
	if ( ! in )
		return error( NO_SOURCE );

	type = SOS_UNKNOWN;
	if ( in->read( head.iBuffer(),SOS_HEADER_SIZE ) < SOS_HEADER_SIZE )
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
	if ( ! in )
		return error( NO_SOURCE );

	type = SOS_UNKNOWN;
	if ( in->read( head.iBuffer(),SOS_HEADER_SIZE ) <= 0 )
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
		result_ = (char*) sos_alloc_memory( len * head.typeLen() );
		result  = result_;
		}
	else
		{
		result_ = (char*) sos_alloc_memory(len * head.typeLen() + SOS_HEADER_SIZE);
		memcpy(result_, head.iBuffer(), SOS_HEADER_SIZE);
		result  = result_ + SOS_HEADER_SIZE;
		}

	in->read( result, len * head.typeLen());
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
	char *buf = (char*) sos_alloc_memory(len);
	in->read( buf, len );

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
		ary[i] = (char*) sos_alloc_memory( slen + 1 );
		lary[i] = slen;
		memcpy(ary[i],cptr,slen);
		ary[i][slen] = '\0';
		cptr += slen;
		}

	sos_free_memory( buf );
	return ns;
	}
#else
void *sos_in::get_string( unsigned int &, sos_header & ) { return 0; }
#endif

void *sos_in::get_chars( unsigned int &len, sos_header &head )
	{
	int swap = ! (head.magic() & SOS_MAGIC);
	char *buf = (char*) sos_alloc_memory(len);
	in->read( buf, len );

	unsigned int *lptr = (unsigned int*) buf;
	len = *lptr++;
	if ( swap )
		{
		swap_abcd_dcba((char*) &len, 1);
		swap_abcd_dcba((char*) lptr, len );
		}

	char *cptr = (char*)(&lptr[len]);
	char **ary = (char **) sos_alloc_memory(len * sizeof(char*));
	for ( unsigned int i = 0; i < len; i++ )
		{
		register unsigned int slen = *lptr++;
		ary[i] = (char*) sos_alloc_memory( slen + 1 );
		memcpy(ary[i],cptr,slen);
		ary[i][slen] = '\0';
		cptr += slen;
		}

	sos_free_memory( buf );
	return ary;
	}

sos_in::~sos_in() { }

sos_status *sos_in::error( error_mode )
	{ return sos_err_status; }

sos_status *sos_out::error( error_mode )
	{ return sos_err_status; }
