//======================================================================
// header.cc
//
// $Id$
//
//======================================================================
#include "sos/sos.h"
RCSID("@(#) $Id$")
#include <sys/time.h>
#include <stdlib.h>
#include "sos/header.h"
#include "sos/mdep.h"
#include "sos/types.h"
#include <iostream.h>
#include <string.h>

unsigned char sos_header::current_version = SOS_VERSION;
unsigned char sos_header::current_header_size = SOS_HEADER_SIZE;

void sos_header_kernel::set( void *b, unsigned int l, sos_code t, int freeit )
	{
	if ( buf_ && freeit_ )
		delete buf_;
	buf_ = (unsigned char*) b;
	length_ = l;
	type_ = t;
	freeit_ = freeit;
	}

sos_header &sos_header::operator=( sos_header &h )
	{
	if ( kernel->unref() ) delete kernel;
	kernel = h.kernel;
	kernel->ref();
	return *this;
	}


#define HEADER_SET( TYPE, TAG )						\
void sos_header::set( TYPE *a, unsigned int l, int freeit )		\
	{								\
	if ( kernel->count() > 1 )					\
		{							\
		kernel->unref();					\
		kernel = new sos_header_kernel( a, l, TAG, freeit );	\
		}							\
	else								\
		kernel->set( a, l, TAG, freeit );			\
	}

HEADER_SET(byte,SOS_BYTE)
HEADER_SET(short,SOS_SHORT)
HEADER_SET(int,SOS_INT)
HEADER_SET(float,SOS_FLOAT)
HEADER_SET(double,SOS_DOUBLE)

void sos_header::set ( )
	{
	if ( kernel->count() > 1 )
		kernel = new sos_header_kernel( 0, 0, SOS_UNKNOWN );
	else
		kernel->set( 0, 0, SOS_UNKNOWN);
	}

void sos_header::set( char *a, unsigned int l, sos_code t, int freeit )
	{
	if ( kernel->count() > 1 )
		kernel = new sos_header_kernel( a, l, t, freeit );
	else
		kernel->set( a, l, t, freeit );
	}

void sos_header::set( unsigned char *a, unsigned int l, sos_code t, int freeit )
	{
	if ( kernel->count() > 1 )
		kernel = new sos_header_kernel( a, l, t, freeit );
	else
		kernel->set( a, l, t, freeit );
	}

void sos_header::useti( unsigned int i )
	{
	kernel->buf_[20] = i & 0xff; i >>= 8;
	kernel->buf_[21] = i & 0xff; i >>= 8;
	kernel->buf_[22] = i & 0xff; i >>= 8;
	kernel->buf_[23] = i & 0xff; i >>= 8;
	}

void sos_header::stamp()
	{
	int mn_ = SOS_MAGIC;
	unsigned char *mn = (unsigned char *) &mn_;
	unsigned char *ptr = kernel->buf_;
	*ptr++ = SOS_VERSION;			// 0
	*ptr++ = SOS_ARC;			// 1
	*ptr++ = kernel->type_;			// 2
	*ptr++ = sos_size(kernel->type_);	// 3
	// magic number
	*ptr++ = *mn++;				// 4
	*ptr++ = *mn++;				// 5
	*ptr++ = *mn++;				// 6
	*ptr++ = *mn++;				// 7
	// length
	unsigned int l = kernel->length_;
	*ptr++ = l & 0xff; l >>= 8;		// 8
	*ptr++ = l & 0xff; l >>= 8;		// 9
	*ptr++ = l & 0xff; l >>= 8;		// 10
	*ptr++ = l & 0xff; l >>= 8;		// 11
	// time
	struct timeval tp;
	struct timezone tz;
	gettimeofday(&tp, &tz);
	int t = tp.tv_sec;
	*ptr++ = t & 0xff; t >>= 8;		// 12
	*ptr++ = t & 0xff; t >>= 8;		// 13
	*ptr++ = t & 0xff; t >>= 8;		// 14
	*ptr++ = t & 0xff; t >>= 8;		// 15
	// future use
	ptr += 8;
	}

ostream &operator<< (ostream &ios, const sos_header &h)
	{
	long v = h.time();
	char *time = strdup(ctime(&v));
	int i = 0;
	for (i = strlen(time) - 1; i > 0 && time[i] == '\n'; --i );
	time[i+1] = '\0';

	ios << time << " ";
	switch( h.type() )
		{
		case SOS_BYTE: ios << "BYTE: "; break;
		case SOS_SHORT: ios << "SHORT: "; break;
		case SOS_INT: ios << "INT: "; break;
		case SOS_FLOAT: ios << "FLOAT: "; break;
		case SOS_DOUBLE: ios << "DOUBLE: "; break;
		case SOS_STRING: ios << "STRING: "; break;
		case SOS_RECORD: ios << "RECORD: "; break;
		default: ios << "UNKNOWN: ";
		}
	ios << "len=" << h.length() << ", type_len=" << (int) h.typeLen();
	ios << ", arch=";
	switch( h.arch() )
		{
		case SOS_SUN3ARC: ios << "sun3arc"; break;
		case SOS_SPARC: ios << "sparc"; break;
		case SOS_VAXARC: ios << "vaxarc"; break;
		case SOS_ALPHAARC: ios << "alphaarc"; break;
		case SOS_HCUBESARC: ios << "hcubesarc"; break;
		default: ios << "*error*";
		}
	}
