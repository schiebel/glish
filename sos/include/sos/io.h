// $Header$
//======================================================================
// header.h
//
// $Log$
// Revision 1.1  1997/03/04 15:15:01  dschieb
// Initial revision
//
// $Revision 1.1.1.1  1996/04/01  19:37:23  dschieb $
//
//======================================================================
#ifndef sos_io_h
#define sos_io_h
#include "sos/header.h"

class sos_sink {
public:
	sos_sink( int fd ) : FD(fd) { }

	void put( byte *a, unsigned int l );
	void put( short *a, unsigned int l );
	void put( int *a, unsigned int l );
	void put( float *a, unsigned int l );
	void put( double *a, unsigned int l );
	void put( char *a, sos_code t, unsigned int l );
	void put( unsigned char *a, sos_code t, unsigned int l );

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
	sos_header head;
	int FD;
};

#endif
