//======================================================================
// sos/io.h
//
// $Id$
//
//======================================================================
#ifndef sos_io_h
#define sos_io_h
#include "sos/header.h"
#include "sos/str.h"

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
	FD_SINK( int fd_ );
	unsigned int write( const char *, unsigned int, buffer_type type = HOLD );
	unsigned int flush( );
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
	FD_SOURCE( int fd_ ) : fd(fd_) { }
	unsigned int read( char *, unsigned int );
	~FD_SOURCE();
    private:
	int fd;
};

class sos_sink {
public:
	sos_sink( SINK &out_, int integral_header = 1 );

	void put( byte *a, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( short *a, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( int *a, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( float *a, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( double *a, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( char *a, unsigned int l, sos_code t, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( unsigned char *a, unsigned int l, sos_code t, unsigned short U1 = 0, unsigned short U2 = 0 );

	void put( charptr *cv, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( char **cv, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 )
		{ put( (charptr*)cv, l, U1, U2 ); }

	void put( const str &, unsigned short U1 = 0, unsigned short U2 = 0 );

	void put_record_start( unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );

	unsigned int flush( ) { return out.flush( ); }

	~sos_sink( );

private:
	char *integral;
	sos_header head;
	SINK &out;
};

class sos_source {
public:
	sos_source( SOURCE &in_, int use_str_ = 1, int integral_header = 1 );

	void *get( unsigned int &length, sos_code &type );
	void *get( unsigned int &length, sos_code &type, sos_header &h );

	~sos_source( );

private:
	void *get_numeric( sos_code, unsigned int &, sos_header & );
	void *get_string( unsigned int &, sos_header & );
	void *get_chars( unsigned int &, sos_header & );
	sos_header head;
	int use_str;
	int integral;
	SOURCE &in;
};

#endif
