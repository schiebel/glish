//======================================================================
// io.cc
//
// $Id$
//
// Copyright (c) 1997 Associated Universities Inc.
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

// linux sets IOV_MAX (or some other) but lies,
// 16 is the best that is supported
#if defined(__alpha) || defined(__linux__)
#define MAXIOV 16
#else
#if defined(IOV_MAX)
#define MAXIOV IOV_MAX
#elif defined(UIO_MAXIOV)
#define MAXIOV UIO_MAXIOV
#elif defined(MAX_IOVEC)
#define MAXIOV MAX_IOVEC
#else
#define MAXIOV 16
#endif
#endif

sos_status *sos_err_status = (sos_status*) -1;
sos_write_buf_factory *sos_fd_sink::buf_factory = 0;

struct final_info GC_FREE_CLASS {
	final_info( final_func f, void * d ) : func(f), data(d) { }
	final_func func;
	void *data;
};

sos_declare(PList,final_info);
typedef PList(final_info) finalize_list;

struct sos_fd_buf_kernel GC_FINAL_CLASS {

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

	// Finalize functions, set by user.
	// In glish, these are used to clean up values which
	// were Ref()ed to prevent them from being modified
	// before they could be written out.
	void finalize( final_func f, void *d )
			{ final_list.append( new final_info( f, d ) ); }
	finalize_list final_list;

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
unsigned int sos_fd_buf_kernel::tmp_size = 256;
unsigned int sos_fd_buf_kernel::size = MAXIOV;

sos_fd_buf_kernel::sos_fd_buf_kernel( ) : cnt(0), total(0), tmp_cur(0)
	{
	iov = (struct iovec*) alloc_memory( size * sizeof( struct iovec ) );
	status = (sos_sink::buffer_type*) alloc_memory_atomic( size * sizeof( sos_sink::buffer_type ) );
	tmp_bufs = (char**) alloc_zero_memory( tmp_cnt * sizeof(void*) );
	}

sos_fd_buf_kernel::~sos_fd_buf_kernel()
	{
	reset( );
	for ( int x = 0; tmp_bufs[x]; x++ )
		free_memory( tmp_bufs[x] );
	free_memory( iov );
	free_memory( status );
	free_memory( tmp_bufs );
	}

char *sos_fd_buf_kernel::new_tmp( )
	{
	if ( ! tmp_bufs[tmp_cur] )
		tmp_bufs[tmp_cur] = alloc_char( tmp_size );
	return tmp_bufs[tmp_cur++];
	}

void sos_fd_buf_kernel::reset( )
	{
	for ( int x = 0; (unsigned int) x < cnt; x++ )
		if ( status[x] == sos_sink::FREE )
			free_memory( iov[x].iov_base );

	while ( final_list.length() > 0 )
		{
		final_info *info = final_list.remove_nth(final_list.length()-1);
		(*info->func)(info->data);
		delete info;
		}

	cnt = 0;
	tmp_cur = 0;
	total = 0;
	}

sos_fd_buf::sos_fd_buf( )
	{
	buf.append( new sos_fd_buf_kernel );
	}

sos_fd_buf::~sos_fd_buf( )
	{
	for ( int i=0; i < buf.length(); ++i )
		delete buf[i];
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

unsigned char *sos_write_buf::append( int size )
	{
	if ( ! buffer )
		{
		buffer_len = 1024;
		while ( buffer_len < size ) buffer_len *= 2;
		buffer = (unsigned char*) alloc_memory( buffer_len );
		}

	if ( buffer_len - buffer_off < size )
		{
		while ( buffer_len - buffer_off < size ) buffer_len *= 2;
		buffer = (unsigned char*) realloc_memory( buffer, buffer_len );
		}

	unsigned char *ret = &buffer[buffer_off];
	buffer_off += size;
	return ret;
	}

sos_write_buf *sos_write_buf_factory::get( )
	{
	int len = free_stack.length( );

	if ( len <= 0 )
		return new sos_write_buf;

	sos_write_buf *ret = free_stack.remove_nth( len - 1 );
	ret->reset( );
	return ret;
	}

sos_fd_sink::sos_fd_sink( int fd__, sos_common *c ) : sos_sink(c), sent(0), start(0), buf_holder(0),
					fd_(fd__), fill(0), out(0), buffer_written(0) { }

void sos_fd_sink::setFd( int fd__ )
	{
	fd_ = fd__;
	}

sos_status *sos_fd_sink::write_iov( const unsigned char *cbuf, unsigned int len, buffer_type type )
	{
	if ( fd_ < 0 ) return 0;

	sos_fd_buf_kernel &K = *buf.last();

	switch ( type )
		{
		case COPY:
			{
			char *t = K.tmp( len );
			if ( t )
				{
				memcpy( t, cbuf, len );
				type = HOLD;
				cbuf = (const unsigned char *) t;
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
	if ( K.cnt >= K.size || type == COPY )		//!!! Need to pay attention to
		return flush( );			//!!! COPY items in the case of
							//!!! a suspended write
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
	while ( (K = buf.next()) );
	}

sos_status *sos_fd_sink::write_buf( const unsigned char *cbuf, unsigned int len )
	{
	if ( fd_ < 0 ) return 0;

	if ( ! fill )
		{
		if ( ! buf_factory ) buf_factory = new sos_write_buf_factory;
		fill = buf_factory->get( );
		}

	if ( common && (*fill).length( ) == 0 && common->remote_version() > 1 )
		(*fill).append( SOS_HEADER_SIZE );

	(*fill).append( cbuf, len );

	return 0;
	}

sos_status *sos_fd_sink::flush( )
	{

	if ( ! fill ) return 0;

	// nothing to flush
	if ( (*fill).length( ) <= 0 )
		{
		buf_factory->put( fill );
		fill = 0;
		return 0;
		}

	if ( common && common->remote_version() > 1 )
		{
		sos_header header( (*fill).get( ) );
		header.set_version_override( 255 );
		header.set_length_override( (*fill).length( ) - SOS_HEADER_SIZE );
		header.stamp( );
		}

	if ( out )
		out_queue.append( fill );
	else
		out = fill;
			  
	fill = 0;
	return resume( );
	}

sos_status *sos_fd_sink::resume( )
	{

	if ( ! out && out_queue.length( ) > 0 )
		out = out_queue.remove_nth(0);

	if ( ! out ) return 0;

	errno = 0;
	int cur = 0;
	int needed = (*out).length( ) - buffer_written;
	if ( (cur = ::write( fd_, &(*out).get( )[buffer_written], needed )) < needed || cur < 0 )
		{
		if ( cur < 0 )
			{
			// resource temporarily unavailable OR
			// operation would block
			if ( errno == EAGAIN ) return this;
			if ( errno == EPIPE )
				{
				// error, pipe closed
				buf_factory->put( out );
				buffer_written = 0;
				return 0;
				}
			perror( "sos_fd_sink::flush( )" );
			return 0;
			}

		buffer_written += cur;
		return this;
		}

	buf_factory->put( out );
	buffer_written = 0;
	if ( out_queue.length( ) > 0 )
		{
		out = out_queue.remove_nth(0);
		return resume( );
		}

	out = 0;
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

void sos_fd_sink::finalize( final_func f, void *data )
	{
	buf.last()->finalize( f, data );
	}

sos_fd_sink::~sos_fd_sink()
	{
	flush( );
	if ( fd_ >= 0 ) close(fd_);
	}

unsigned int sos_fd_source::read( unsigned char *buf, unsigned int len )
	{
	if ( fd_ < 0 || len == 0 ) return 0;

	unsigned int needed = len;
	unsigned int total = 0;

	if ( buffer && buffer_len > 0 )
		{
		int remains = buffer_len - buffer_off;
		if ( len < remains )
			{
			memcpy( buf, &buffer[buffer_off], len );
			buffer_off += len;
			return len;
			}
		else if ( len == remains )
			{
			memcpy( buf, &buffer[buffer_off], len );
			buffer_off = 0;
			buffer_len = 0;
			return len;
			}
		else
			{
			// error, bytes requested did not match byte bundle boundary
			// recover and read from pipe...
			fprintf( stderr, "inconsistency in buffer management, sos_fd_source::read (%d)\n", getpid() );
			memcpy( buf, &buffer[buffer_off], remains );
			total += remains;
			buf += remains;
			needed -= remains;
			free_memory( buffer );
			buffer = 0;
			buffer_len = 0;
			buffer_size = 0;
			}
		}

	errno = 0;
	register int cur = 0;
	unsigned char *ptr = buf;
	while ( needed && ((cur = ::read( fd_, ptr, needed )) > 0 || errno == EAGAIN ) )
		{
		if ( cur < 0 && errno == EAGAIN ) continue;
		total += cur;
		ptr += cur;
		needed -= cur;
		if ( total >= len ) needed = 0;
		}

	if ( cur < 0 ) perror("sos_fd_source::read()");

	if ( try_buffer_io && len == SOS_HEADER_2_SIZE && *((unsigned char*) buf) == 255 )
		{
		sos_header header(buf);
		total = 0;
		errno = 0;
		cur = 0;

		buffer_len = needed = header.length();

		if ( ! buffer )
			{
			buffer_size = 1024;
			while ( buffer_size < needed ) buffer_size *= 2;
			buffer = (unsigned char*) alloc_memory( buffer_size );
			}
		else if ( buffer_size < needed )
			{
			while ( buffer_size < needed ) buffer_size *= 2;
			buffer = (unsigned char*) realloc_memory( buffer, buffer_size );
			}
			
		ptr = buffer;
		while ( needed && ((cur = ::read( fd_, ptr, needed )) > 0 || errno == EAGAIN ) )
			{
			if ( cur < 0 && errno == EAGAIN ) continue;
			total += cur;
			ptr += cur;
			needed -= cur;
			if ( total >= buffer_len ) needed = 0;
			}

		memcpy( buf, buffer, SOS_HEADER_2_SIZE );
		total = buffer_off = SOS_HEADER_2_SIZE;
		}
	else
		try_buffer_io = 0;

	return total;
	}

sos_fd_source::~sos_fd_source()
	{
	if ( fd_ >= 0 ) close(fd_);
	}

sos_out::sos_out( sos_sink *out_, int integral_header ) : out(out_)
	{
	if ( out ) head.set_version( out->remote_version( ) );

	if ( integral_header )
		not_integral = 0;
	else
		{
		not_integral = alloc_char( sos_header::size(out ? out->remote_version( ) : -1 ) );
		head.set( not_integral, 0, SOS_UNKNOWN );
		}
	}

static unsigned char zero_user_area[] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };


#define PUTNUMERIC_BODY( TYPE, SOSTYPE, PARAM, SOURCE )			\
	{								\
	if ( ! out )							\
		return Error( NO_SINK );				\
									\
	if ( not_integral )						\
		{							\
		head.set(l,SOSTYPE);					\
		memcpy( head.iBuffer() + head.user_offset( ), SOURCE, 6 ); \
		head.stamp( initial_stamp );				\
		out->write( head.iBuffer(), head.size( ), sos_sink::COPY ); \
		return out->write( a, l * sos_size(SOSTYPE), type ); 	\
		}							\
	else								\
		{							\
		head.set(a,l PARAM);					\
		memcpy( head.iBuffer() + head.user_offset( ), SOURCE, 6 ); \
		head.stamp( initial_stamp );				\
		return out->write( head.iBuffer(), l * sos_size(SOSTYPE) + \
			   head.size( ), type );			\
		}							\
	}


#define PUTNUMERIC(TYPE,SOSTYPE)					\
sos_status *sos_out::put( TYPE *a, unsigned int l,			\
		    sos_sink::buffer_type type )			\
	PUTNUMERIC_BODY(TYPE, SOSTYPE,, zero_user_area)			\
sos_status *sos_out::put( TYPE *a, unsigned int l, sos_header &h,	\
		    sos_sink::buffer_type type ) 			\
	PUTNUMERIC_BODY(TYPE, SOSTYPE,, h.iBuffer() + head.user_offset( ))

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
	PUTNUMERIC_BODY(TYPE, t, COMMA(t), h.iBuffer() + head.user_offset( ))

PUTCHAR(char)
PUTCHAR(unsigned char)


#define PUTCHARPTR_BODY(SOURCE)						\
	{								\
	if ( ! out )							\
		return Error( NO_SINK );				\
									\
	unsigned int total = (len+1) * 4;				\
	char *buf = alloc_char( total + head.size( ) );			\
	unsigned int *lptr = (unsigned int *) (buf + head.size( ) );	\
									\
	*lptr++ = len;							\
	for ( unsigned int i = 0; i < len; i++ )			\
		{							\
		lptr[i] = s[i] ? ::strlen(s[i]) : 0;			\
		total += lptr[i];					\
		}							\
									\
	buf = (char*) realloc_memory( buf, total + head.size( ) ); \
	lptr = (unsigned int *) (buf + head.size( ) + 4);		\
									\
	head.set(buf,total,SOS_STRING);					\
	memcpy( head.iBuffer() + head.user_offset( ), SOURCE, 6 );	\
	head.stamp( initial_stamp );					\
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
	sos_status *ret = out->write( buf, total + head.size( ),	\
				     sos_sink::FREE );			\
									\
	if ( not_integral ) head.set( not_integral, 0, SOS_UNKNOWN );	\
									\
	if ( type == sos_sink::FREE )					\
		{							\
		for ( unsigned int X = 0; X < len; X++ )		\
			free_memory( (char*) s[X] );			\
		free_memory( s );					\
		}							\
									\
	return ret;							\
	}

#define PUTSTR_BODY(SOURCE)						\
	{								\
	if ( ! out )							\
		return Error( NO_SINK );				\
									\
	unsigned int len = s.length();					\
	unsigned int total = (len+1) * 4;				\
									\
	for ( unsigned int i = 0; i < len; i++ )			\
		total += s.strlen(i);					\
									\
	char *buf = alloc_char( total + head.size( ) );			\
									\
	head.set(buf,total,SOS_STRING);					\
	memcpy( head.iBuffer() + head.user_offset( ), SOURCE, 6 );	\
	head.stamp( initial_stamp );					\
									\
	unsigned int *lptr = (unsigned int *) (buf + head.size( ) );	\
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
	sos_status *ret = out->write( buf, total + head.size( ),	\
				     sos_sink::FREE );			\
									\
	if ( not_integral ) head.set( not_integral, 0, SOS_UNKNOWN );	\
									\
	return ret;							\
	}

sos_status *sos_out::put( charptr *s, unsigned int len, sos_sink::buffer_type type )
	PUTCHARPTR_BODY(zero_user_area)
sos_status *sos_out::put( charptr *s, unsigned int len, sos_header &h, sos_sink::buffer_type type )
	PUTCHARPTR_BODY(h.iBuffer() + head.user_offset( ))

#if defined(ENABLE_STR)
sos_status *sos_out::put( const str &s )
	PUTSTR_BODY(zero_user_area)
sos_status *sos_out::put( const str &s, sos_header &h )
	PUTSTR_BODY(h.iBuffer() + head.user_offset( ))
#endif


#define PUTREC_BODY( SOURCE )						\
	{								\
	if ( ! out )							\
		return Error( NO_SINK );				\
									\
	/*								\
	** Implicit here is the assumption that each successive		\
	** verion of the header will be larger than the previous...	\
	*/								\
	static char buf[SOS_HEADER_SIZE];				\
									\
	if ( not_integral )						\
		head.set(l,SOS_RECORD);					\
	else								\
		head.set(buf,l,SOS_RECORD);				\
									\
	memcpy( head.iBuffer() + head.user_offset( ), SOURCE, 6 );	\
	head.stamp( initial_stamp );					\
									\
	return out->write( head.iBuffer(), head.size( ),		\
			  sos_sink::COPY );				\
	}

sos_status *sos_out::put_record_start( unsigned int l )
	PUTREC_BODY(zero_user_area)
sos_status *sos_out::put_record_start( unsigned int l, sos_header &h )
	PUTREC_BODY(h.iBuffer() + head.user_offset( ))

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

sos_in::sos_in( sos_source *in_, int use_str_, int integral_header ) : 
			head(alloc_char(SOS_HEADER_SIZE), 0, SOS_UNKNOWN, 1),
			use_str(use_str_), not_integral(integral_header ? 0 : 1), in(in_),
			buffer(0), buffer_length(0), buffer_contents(0)
	{
	}

void *sos_in::get( unsigned int &len, sos_code &type )
	{
	if ( ! in )
		return Error( NO_SOURCE );

	type = SOS_UNKNOWN;
	if ( read( head.iBuffer(),SOS_HEADER_SIZE ) < SOS_HEADER_SIZE )
		return 0;

	if ( head.version( ) == 0 )
		{
		if ( in ) in->set_remote_version( 0 );
		unread( head.iBuffer() + SOS_HEADER_0_SIZE, SOS_HEADER_SIZE - SOS_HEADER_0_SIZE );
		head.adjust_version( );
		}
	else if ( head.version( ) == 1 )
		{
		if ( in ) in->set_remote_version( 1 );
		}

	type = head.type();
	len = head.length();

	switch ( type )
		{
		case SOS_STRING:
			return use_str ? get_string( len ) : get_chars( len );
		case SOS_RECORD:
			return (void*) -1;
		default:
			return get_numeric( type, len );
		}
	}

void *sos_in::get( unsigned int &len, sos_code &type, sos_header &h )
	{
	if ( ! in )
		return Error( NO_SOURCE );

	type = SOS_UNKNOWN;
	if ( read( head.iBuffer(),SOS_HEADER_SIZE ) <= 0 )
		return 0;

	if ( head.version( ) == 0 )
		{
		if ( in ) in->set_remote_version( 0 );
		unread( head.iBuffer() + SOS_HEADER_0_SIZE, SOS_HEADER_SIZE - SOS_HEADER_0_SIZE );
		head.adjust_version( );
		}
	else if ( head.version( ) == 1 )
		{
		if ( in ) in->set_remote_version( 1 );
		}

	type = head.type();
	len = head.length();

// 	cerr << getpid() << ": " << head << endl;
	switch ( type )
		{
		case SOS_STRING:
			{
			void *ret = use_str ? get_string( len ) : get_chars( len );
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
			char *ret = (char*) get_numeric( type, len );
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

void *sos_in::get_numeric( sos_code &type, unsigned int &len )
	{
	char *result_ = 0;
	char *result = 0;

	if ( not_integral )
		{
		result_ = alloc_char( len * head.typeLen() );
		result  = result_;
		}
	else
		{
		result_ = alloc_char(len * head.typeLen() + SOS_HEADER_SIZE);
		memcpy(result_, head.iBuffer(), SOS_HEADER_SIZE);
		result  = result_ + SOS_HEADER_SIZE;
		}

	read( result, len * head.typeLen());
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
void *sos_in::get_string( unsigned int &len )
	{
	int swap = ! (head.magic() & SOS_MAGIC);
	char *buf = alloc_char(len);
	read( buf, len );

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
		ary[i] = alloc_char( slen + 1 );
		lary[i] = slen;
		memcpy(ary[i],cptr,slen);
		ary[i][slen] = '\0';
		cptr += slen;
		}

	free_memory( buf );
	return ns;
	}
#else
void *sos_in::get_string( unsigned int & ) { return 0; }
#endif

void *sos_in::get_chars( unsigned int &len )
	{
	int swap = ! (head.magic() & SOS_MAGIC);
	char *buf = alloc_char(len);
	read( buf, len );

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
		ary[i] = alloc_char( slen + 1 );
		memcpy(ary[i],cptr,slen);
		ary[i][slen] = '\0';
		cptr += slen;
		}

	free_memory( buf );
	return ary;
	}

void sos_in::unread( unsigned char *buf, int len )
	{
	if ( ! buffer )
		buffer = (char*) alloc_memory( buffer_length = len * 2 );

	if ( buffer_length < buffer_contents + len )
		{
		while ( buffer_length < buffer_contents + len ) buffer_length *= 2;
		buffer = (char*) realloc_memory( buffer, buffer_length );
		}

	memcpy( &buffer[buffer_contents], buf, len );
	buffer_contents += len;
	}

int sos_in::readback( unsigned char *buf, unsigned int len )
	{
	if ( buffer_contents >= len )
		{
		memcpy( buf, buffer, len );
		char *to = buffer;
		for ( char *from = buffer + len; from < buffer + buffer_contents; *to++ = *from++ );
		buffer_contents -= len;
		return len;
		}
	else
		{
		int contents = buffer_contents;
		buffer_contents = 0;
		memcpy( buf, buffer, contents );
		return in ? contents + in->read( buf + contents, len - contents ) : contents;
		}
	}

sos_in::~sos_in() { }

sos_status *sos_in::Error( error_mode )
	{ return sos_err_status; }

sos_status *sos_out::Error( error_mode )
	{ return sos_err_status; }
