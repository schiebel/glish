//======================================================================
// sos/io.h
//
// $Id$
//
//======================================================================
#ifndef sos_io_h_
#define sos_io_h_
#include "sos/header.h"

#if defined(ENABLE_STR)
#include "sos/str.h"
#else
#if !defined(sos_str_h_)
typedef const char * const * const_charptr;
typedef const char* charptr;
#endif
#endif

class sos_sink {
    public:
	enum buffer_type { FREE, HOLD, COPY };
	virtual unsigned int write( const char *, unsigned int, buffer_type type = HOLD ) = 0;
	unsigned int write( void *buf, unsigned int len, buffer_type type = HOLD )
			{ return write( (const char *) buf, len, type ); }
	virtual unsigned int flush( ) = 0;
	virtual ~sos_sink();
};

class sos_source {
    public:
	virtual unsigned int read( char *, unsigned int ) = 0;
	unsigned int read( unsigned char *buf, unsigned int len )
		{ return read((char*)buf, len); }
	virtual ~sos_source();
};

class sos_fd_sink : public sos_sink {
    public:
	sos_fd_sink( int fd_ = -1 );
	unsigned int write( const char *, unsigned int, buffer_type type = HOLD );
	unsigned int flush( );

	void setFd( int fd_ ) { fd = fd_; }

	~sos_fd_sink();

    private:
	void *iov;
	unsigned int iov_cnt;
	buffer_type *status;

	void **tmp_bufs;
	unsigned int tmp_buf_cur;

	static unsigned int tmp_buf_count;
	static unsigned int tmp_buf_size;

	int fd;
};

class sos_fd_source : public sos_source {
    public:
	sos_fd_source( int fd_ = -1 ) : fd(fd_) { }
	unsigned int read( char *, unsigned int );

	void setFd( int fd_ ) { fd = fd_; }

	~sos_fd_source();

    private:
	int fd;
};

class sos_out {
public:
	sos_out( sos_sink &out_, int integral_header = 0 );

	void put( byte *a, unsigned int l, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( byte *a, unsigned int l, sos_header &h, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( short *a, unsigned int l, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( short *a, unsigned int l, sos_header &h, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( int *a, unsigned int l, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( int *a, unsigned int l, sos_header &h, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( float *a, unsigned int l, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( float *a, unsigned int l, sos_header &h, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( double *a, unsigned int l, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( double *a, unsigned int l, sos_header &h, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( char *a, unsigned int l, sos_code t, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( char *a, unsigned int l, sos_code t, sos_header &h, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( unsigned char *a, unsigned int l, sos_code t, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( unsigned char *a, unsigned int l, sos_code t, sos_header &h, sos_sink::buffer_type type = sos_sink::HOLD );

	void put( charptr *cv, unsigned int l, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( charptr *cv, unsigned int l, sos_header &h, sos_sink::buffer_type type = sos_sink::HOLD );
	void put( char **cv, unsigned int l, sos_sink::buffer_type type = sos_sink::HOLD )
		{ put( (charptr*)cv, l, type ); }
	void put( char **cv, unsigned int l, sos_header &h, sos_sink::buffer_type type = sos_sink::HOLD )
		{ put( (charptr*)cv, l, h, type ); }

#if defined(ENABLE_STR)
	void put( const str & );
	void put( const str &, sos_header &h );
#endif

	void put_record_start( unsigned int l );
	void put_record_start( unsigned int l, sos_header &h );

	void write( const char *buf, unsigned int len, sos_sink::buffer_type type = sos_sink::HOLD )
		{ out.write( buf, len, type ); }

	unsigned int flush( ) { return out.flush( ); }

	~sos_out( );

private:
	char *not_integral;
	sos_header head;
	sos_sink &out;
};

class sos_in {
public:
	sos_in( sos_source &in_, int use_str_ = 0, int integral_header = 0 );

	void *get( unsigned int &length, sos_code &type );
	void *get( unsigned int &length, sos_code &type, sos_header &h );

	void read( char *buf, unsigned int len ) { in.read( buf, len ); }

	~sos_in( );

private:
	void *get_numeric( sos_code &, unsigned int &, sos_header & );
	void *get_string( unsigned int &, sos_header & );
	void *get_chars( unsigned int &, sos_header & );
	sos_header head;
	int use_str;
	int not_integral;
	sos_source &in;
};

#endif
