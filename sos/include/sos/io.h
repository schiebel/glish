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

class sos_sink {
public:
	sos_sink( int fd ) : FD(fd) { }

	void put( byte *a, unsigned int l, short U1 = 0, short U2 = 0 );
	void put( short *a, unsigned int l, short U1 = 0, short U2 = 0 );
	void put( int *a, unsigned int l, short U1 = 0, short U2 = 0 );
	void put( float *a, unsigned int l, short U1 = 0, short U2 = 0 );
	void put( double *a, unsigned int l, short U1 = 0, short U2 = 0 );
	void put( char *a, sos_code t, unsigned int l, short U1 = 0, short U2 = 0 );
	void put( unsigned char *a, sos_code t, unsigned int l, short U1 = 0, short U2 = 0 );
	void put( const str &, short U1 = 0, short U2 = 0 );

	~sos_sink( );

private:
	sos_header head;
	int FD;
};

class sos_source {
public:
	sos_source( int fd ) : FD(fd), head((char*) alloc_memory(sos_header::iSize())) { }

	void *get( sos_code &type, unsigned int &length );

	~sos_source( );

private:
	void *get_numeric( sos_code, unsigned int &, sos_header & );
	void *get_string( unsigned int &, sos_header & );
	sos_header head;
	int FD;
};

#endif
