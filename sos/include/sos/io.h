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
	virtual unsigned int write( const char *, unsigned int ) = 0;
	virtual void flush( ) = 0;
};

class SOURCE {
public:
	virtual unsigned int read( const char *, unsigned int ) = 0;
};

class FD_SINK {
public:
	FD_SINK( int fd_ ) : fd(fd_) { }
	unsigned int write( const char *, unsigned int );
	void flush( );
};
	
class sos_sink {
public:
	sos_sink( SINK &out_ ) : out(out_) { }

	void put( byte *a, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( short *a, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( int *a, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( float *a, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( double *a, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( char *a, sos_code t, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( unsigned char *a, sos_code t, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );

	void put( charptr *cv, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );
	void put( char **cv, unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 )
		{ put( (charptr*)cv, l, U1, U2 ); }

	void put( const str &, unsigned short U1 = 0, unsigned short U2 = 0 );

	void put_record_start( unsigned int l, unsigned short U1 = 0, unsigned short U2 = 0 );

	~sos_sink( );

private:
	sos_header head;
	SINK &out;
};

class sos_source {
public:
	sos_source( SOURCE &in_, int use_str_ = 1 ) : in(in_), use_str(use_str_),
			head((char*) alloc_memory(sos_header::iSize())) { }

	void *get( sos_code &type, unsigned int &length );

	~sos_source( );

private:
	void *get_numeric( sos_code, unsigned int &, sos_header & );
	void *get_string( unsigned int &, sos_header & );
	void *get_chars( unsigned int &, sos_header & );
	sos_header head;
	int use_str;
	SOURCE &in;
};

#endif
