//======================================================================
// sos/header.h
//
// $Id$
//
//======================================================================
#ifndef sos_header_h
#define sos_header_h
#include "sos/mdep.h"

//	sos header structure
//							      offset
//		1 byte version number				0
//		1 byte architecture				1
//		1 byte type					2
//		1 byte type length				3
//		4 byte magic number				4
//		4 byte length					8
//		4 byte time stamp				12
//		4 byte control info (future use/user data?)	16
//		4 byte user info				20
//
//	( should provide a way for user control info... )
//
class sos_header {
public:
	unsigned char version() const { return buf[0]; }
	unsigned char arch() const { return buf[1]; }
	sos_code type() const { return buf[2]; }
	unsigned char typeLen() const { return buf[3]; }
	unsigned int magic() const { return *((int*)&buf[4]); }
	unsigned int length() const { return buf[8] + (buf[9] << 8) +
				      (buf[10] << 16) + (buf[11] << 24); }
	unsigned int time() const { return buf[12] + (buf[13] << 8) +
				    (buf[14] << 16) + (buf[15] << 24); }

	//
	// access to user data
	//
	unsigned char ugetc( int off = 0 ) const { return buf[20 + (off % 4)]; }
	unsigned short ugets( int off = 0 ) const { off = 20 + (off % 2) * 2;
			return buf[off] + (buf[off+1] << 8); }
	unsigned int ugeti( ) const { return buf[20] + (buf[21] << 8) +
			(buf[22] << 16) + (buf[23] << 24); }

	void usetc( unsigned char c, int off = 0 ) { buf[20 + (off % 4)] = c; }
	void usets( unsigned short s, int off = 0 ) { off = 20 + (off % 2) * 2;
			buf[off] = s & 0xff; buf[off+1] = (s >> 8) & 0xff; }
	void useti( unsigned int i );

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
