//======================================================================
// sos/header.h
//
// $Id$
//
// Copyright (c) 1997,2000 Associated Universities Inc.
//
//======================================================================
#ifndef sos_header_h
#define sos_header_h
#include "sos/mdep.h"
#if ! defined(ENABLE_GC)
#include "sos/alloc.h"
#else
#include "gcmem/alloc.h"
#endif

#define SOS_HEADER_SIZE		24
#define SOS_VERSION		0

#include <iostream.h>

struct sos_header_kernel GC_FINAL_CLASS {
	sos_header_kernel( void *b, unsigned int l, sos_code t, int freeit = 0 ) :
		buf_((unsigned char*)b), type_(t), length_(l), count_(1), freeit_(freeit) { }
	unsigned int count() { return count_; }
	unsigned int ref() { return ++count_; }
	unsigned int unref() { return --count_; }
	~sos_header_kernel() { if ( freeit_ ) free_memory( buf_ ); }
	void set( void *b, unsigned int l, sos_code t, int freeit = 0 );
	void set( unsigned int l, sos_code t ) { type_ = t; length_ = l; }

	unsigned char	*buf_;
	sos_code	type_;
	unsigned int	length_;	// in units of type_
	unsigned int	count_;
	int		freeit_;
};

//	sos header structure
//							      offset
//		1 byte version number				0
//		1 byte architecture				1
//		1 byte type					2
//		1 byte type length				3
//		4 byte magic number				4
//		4 byte length					8
//		4 byte time stamp				12
//		2 byte future use				16
//		2 byte user data				18
//		4 byte user data				20
//
//	( should provide a way for user control info... )
//
class sos_header GC_FINAL_CLASS {
public:
	//
	// information from the buffer
	//
	unsigned char version() const { return kernel->buf_[0]; }
	unsigned char arch() const { return kernel->buf_[1]; }
	sos_code type() const { return kernel->buf_[2]; }
	unsigned char typeLen() const { return kernel->buf_[3]; }
	unsigned int magic() const { return *((int*)&kernel->buf_[4]); }
	unsigned int length() const { return kernel->buf_[8] + (kernel->buf_[9] << 8) +
				      (kernel->buf_[10] << 16) + (kernel->buf_[11] << 24); }
	unsigned int time() const { return kernel->buf_[12] + (kernel->buf_[13] << 8) +
				    (kernel->buf_[14] << 16) + (kernel->buf_[15] << 24); }

	//
	// information "local" to the class
	//
	static unsigned char iSize() { return current_header_size; }
	static unsigned char iVersion() { return current_version; }
	unsigned char *iBuffer() { return kernel->buf_; }
	unsigned int iLength() { return kernel->length_; }
	sos_code iType() { return kernel->type_; }

	//
	// update buffer information
	//
	void stamp();

	//
	// reference count
	//
	unsigned int count( ) const { return kernel->count(); }

	//
	// constructors
	//
	sos_header( byte *a, unsigned int l, int freeit = 0 ) : kernel( new sos_header_kernel(a,l,SOS_BYTE,freeit) ) { }
	sos_header( short *a, unsigned int l, int freeit = 0 ) : kernel( new sos_header_kernel(a,l,SOS_SHORT,freeit) ) { }
	sos_header( int *a, unsigned int l, int freeit = 0 ) : kernel( new sos_header_kernel(a,l,SOS_INT,freeit) ) { }
	sos_header( float *a, unsigned int l, int freeit = 0 ) : kernel( new sos_header_kernel(a,l,SOS_FLOAT,freeit) ) { }
	sos_header( double *a, unsigned int l, int freeit = 0 ) : kernel( new sos_header_kernel(a,l,SOS_DOUBLE,freeit) ) { }

	sos_header( ) : kernel( new sos_header_kernel(0,SOS_UNKNOWN,0) ) { }
	sos_header( char *b, unsigned int l = 0, sos_code t = SOS_UNKNOWN, int freeit = 0 ) :
		kernel( new sos_header_kernel(b,l,t,freeit) ) { }
	sos_header( unsigned char *b, unsigned int l = 0, sos_code t = SOS_UNKNOWN, int freeit = 0 ) :
		kernel( new sos_header_kernel(b,l,t,freeit) ) { }

	sos_header( const sos_header &h ) : kernel( h.kernel ) { kernel->ref(); }

	//
	// assignment
	//
	sos_header &operator=( sos_header &h );

	//
	// change buffer
	//
	void set( byte *a, unsigned int l, int freeit = 0 );
	void set( short *a, unsigned int l, int freeit = 0 );
	void set( int *a, unsigned int l, int freeit = 0 );
	void set( float *a, unsigned int l, int freeit = 0 );
	void set( double *a, unsigned int l, int freeit = 0 );

	void set ( );
	void set( char *a, unsigned int l, sos_code t, int freeit = 0 );
	void set( unsigned char *a, unsigned int l, sos_code t, int freeit = 0 );
	void set( unsigned int l, sos_code t ) { kernel->set( l, t ); }

	// make sure we have a scratch buffer to write to.
	void scratch( );

	//
	// access to user data
	//
	unsigned char ugetc( int off = 0 ) const { return kernel->buf_[18 + (off % 6)]; }
	unsigned short ugets( int off = 0 ) const { off = 18 + (off % 3) * 2;
			return kernel->buf_[off] + (kernel->buf_[off+1] << 8); }
	unsigned int ugeti( ) const { return kernel->buf_[20] + (kernel->buf_[21] << 8) +
			(kernel->buf_[22] << 16) + (kernel->buf_[23] << 24); }

	void usetc( unsigned char c, int off = 0 ) { kernel->buf_[18 + (off % 6)] = c; }
	void usets( unsigned short s, int off = 0 ) { off = 18 + (off % 3) * 2;
			kernel->buf_[off] = s & 0xff; kernel->buf_[off+1] = (s >> 8) & 0xff; }
	void useti( unsigned int i );

	~sos_header( ) { if ( ! kernel->unref() ) delete kernel; }

// 	sos_header &operator=(void *b)
// 		{ buf = (unsigned char*) b; type_ = type(); length_ = length(); }

private:
	static unsigned char current_version;
	static unsigned char current_header_size;
	sos_header_kernel *kernel;
};

extern ostream &operator<< (ostream &, const sos_header &);

#endif
