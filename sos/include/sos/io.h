//======================================================================
// sos/io.h
//
// $Id$
//
//======================================================================
#ifndef sos_io_h
#define sos_io_h
#include "sos/header.h"

#if defined(ENABLE_STR)
#include "sos/str.h"
#else
typedef const char * const * const_charptr;
typedef const char* charptr;
#endif

class SINK {
    public:
	enum buffer_type { FREE, HOLD, COPY };
	virtual unsigned int write( const char *, unsigned int, buffer_type type = HOLD ) = 0;
	unsigned int write( void *buf, unsigned int len, buffer_type type = HOLD )
			{ return write( (const char *) buf, len, type ); }
	virtual unsigned int flush( ) = 0;
	virtual ~SINK();
};

class SOURCE {
    public:
	virtual unsigned int read( char *, unsigned int ) = 0;
	unsigned int read( unsigned char *buf, unsigned int len )
		{ return read((char*)buf, len); }
	virtual ~SOURCE();
};

class FD_SINK : public SINK {
    public:
	FD_SINK( int fd_ = -1 );
	unsigned int write( const char *, unsigned int, buffer_type type = HOLD );
	unsigned int flush( );

	void setFd( int fd_ ) { fd = fd_; }

	~FD_SINK();

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

class FD_SOURCE : public SOURCE {
    public:
	FD_SOURCE( int fd_ = -1 ) : fd(fd_) { }
	unsigned int read( char *, unsigned int );

	void setFd( int fd_ ) { fd = fd_; }

	~FD_SOURCE();

    private:
	int fd;
};

class sos_sink {
public:
	sos_sink( SINK &out_, int integral_header = 0 );

	void put( byte *a, unsigned int l, SINK::buffer_type type = SINK::HOLD );
	void put( byte *a, unsigned int l, sos_header &h, SINK::buffer_type type = SINK::HOLD );
	void put( short *a, unsigned int l, SINK::buffer_type type = SINK::HOLD );
	void put( short *a, unsigned int l, sos_header &h, SINK::buffer_type type = SINK::HOLD );
	void put( int *a, unsigned int l, SINK::buffer_type type = SINK::HOLD );
	void put( int *a, unsigned int l, sos_header &h, SINK::buffer_type type = SINK::HOLD );
	void put( float *a, unsigned int l, SINK::buffer_type type = SINK::HOLD );
	void put( float *a, unsigned int l, sos_header &h, SINK::buffer_type type = SINK::HOLD );
	void put( double *a, unsigned int l, SINK::buffer_type type = SINK::HOLD );
	void put( double *a, unsigned int l, sos_header &h, SINK::buffer_type type = SINK::HOLD );
	void put( char *a, unsigned int l, sos_code t, SINK::buffer_type type = SINK::HOLD );
	void put( char *a, unsigned int l, sos_code t, sos_header &h, SINK::buffer_type type = SINK::HOLD );
	void put( unsigned char *a, unsigned int l, sos_code t, SINK::buffer_type type = SINK::HOLD );
	void put( unsigned char *a, unsigned int l, sos_code t, sos_header &h, SINK::buffer_type type = SINK::HOLD );

	void put( charptr *cv, unsigned int l, SINK::buffer_type type = SINK::HOLD );
	void put( charptr *cv, unsigned int l, sos_header &h, SINK::buffer_type type = SINK::HOLD );
	void put( char **cv, unsigned int l, SINK::buffer_type type = SINK::HOLD )
		{ put( (charptr*)cv, l, type ); }
	void put( char **cv, unsigned int l, sos_header &h, SINK::buffer_type type = SINK::HOLD )
		{ put( (charptr*)cv, l, h, type ); }

#if defined(ENABLE_STR)
	void put( const str & );
	void put( const str &, sos_header &h );
#endif

	void put_record_start( unsigned int l );
	void put_record_start( unsigned int l, sos_header &h );

	void write( const char *buf, unsigned int len, SINK::buffer_type type = SINK::HOLD )
		{ out.write( buf, len, type ); }

	unsigned int flush( ) { return out.flush( ); }

	~sos_sink( );

private:
	char *not_integral;
	sos_header head;
	SINK &out;
};

class sos_source {
public:
	sos_source( SOURCE &in_, int use_str_ = 0, int integral_header = 0 );

	void *get( unsigned int &length, sos_code &type );
	void *get( unsigned int &length, sos_code &type, sos_header &h );

	void read( char *buf, unsigned int len ) { in.read( buf, len ); }

	~sos_source( );

private:
	void *get_numeric( sos_code, unsigned int &, sos_header & );
	void *get_string( unsigned int &, sos_header & );
	void *get_chars( unsigned int &, sos_header & );
	sos_header head;
	int use_str;
	int not_integral;
	SOURCE &in;
};

#endif
