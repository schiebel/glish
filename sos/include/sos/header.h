//======================================================================
// sos/header.h
//
// $Id$
//
//======================================================================
#ifndef sos_header_h
#define sos_header_h
#include "sos/mdep.h"

typedef unsigned char byte;
#define alloc_memory malloc


//	sos header structure
//							      offset
//		1 byte version number				0
//		1 byte architecture				1
//		1 byte type					2
//		1 byte type length				3
//		4 byte magic number				4
//		4 byte length					8
//		4 byte time stamp				12
//		8 byte control info (future use/user data?)	16
//
//	( should provide a way for user control info... )
//
class sos_header {
public:
	unsigned char version() { return buf[0]; }
	unsigned char arch() { return buf[1]; }
	sos_code type() { return buf[2]; }
	unsigned char typeLen() { return buf[3]; }
	unsigned int magic() { return *((int*)&buf[4]); }
	unsigned int length() { return buf[8] + (buf[9] << 8) +
				  (buf[10] << 16) + (buf[11] << 24); }
	unsigned int time() { return buf[12] + (buf[13] << 8) +
				  (buf[14] << 16) + (buf[15] << 24); }

	static unsigned char iSize() { return current_header_size; }
	static unsigned char iVersion() { return current_version; }
	unsigned char *iBuffer() { return buf; }
	unsigned int iLength() { return length_; }
	sos_code iType() { return type_; }

	void stamp();

	sos_header( byte *a, unsigned int l ) : buf((unsigned char*)a), length_(l),
				type_(SOS_BYTE) { }
	sos_header( short *a, unsigned int l ) : buf((unsigned char*)a), length_(l),
				type_(SOS_SHORT) { }
	sos_header( int *a, unsigned int l ) : buf((unsigned char*)a), length_(l),
				type_(SOS_INT) { }
	sos_header( float *a, unsigned int l ) : buf((unsigned char*)a), length_(l),
				type_(SOS_FLOAT)  { }
	sos_header( double *a, unsigned int l ) : buf((unsigned char*)a), length_(l),
				type_(SOS_DOUBLE) { }

	sos_header( ) : buf(0), type_(SOS_UNKNOWN), length_(0) { }
	sos_header( char *b, sos_code t = SOS_UNKNOWN,
		    unsigned int l = 0  ) : buf( (unsigned char *) b ),
				type_(t), length_(l) { }
	sos_header( unsigned char *b, sos_code t = SOS_UNKNOWN,
		    unsigned int l = 0 ) : buf( b ), 
				type_(t), length_(l) { }

	void set( byte *a, unsigned int l )
		{ buf = (unsigned char*) a; length_ = l; type_ = SOS_BYTE; }
	void set( short *a, unsigned int l )
		{ buf = (unsigned char*) a; length_ = l; type_ = SOS_SHORT; }
	void set( int *a, unsigned int l )
		{ buf = (unsigned char*) a; length_ = l; type_ = SOS_INT; }
	void set( float *a, unsigned int l )
		{ buf = (unsigned char*) a; length_ = l; type_ = SOS_FLOAT; }
	void set( double *a, unsigned int l )
		{ buf = (unsigned char*) a; length_ = l; type_ = SOS_DOUBLE; }

	void set ( )
		{ buf = 0; length_ = 0; type_ = SOS_UNKNOWN; }
	void set( char *a, sos_code t = SOS_UNKNOWN, unsigned int l = 0  )
		{ buf = (unsigned char*) a; length_ = l; type_ = t; }
	void set( unsigned char *a, sos_code t = SOS_UNKNOWN, unsigned int l = 0 )
		{ buf = a; length_ = l; type_ = t; }

	sos_header &operator=(void *b)
		{ buf = (unsigned char*) b; type_ = type(); length_ = length(); }

	~sos_header( ) { }

private:
	static unsigned char current_version;
	static unsigned char current_header_size;
	unsigned char	*buf;
	sos_code	type_;
	unsigned int	length_;	// in units of type_
};

#endif
