static char rcsid[]  = "$Id$";
//======================================================================
// header.cc
//
// $Id$
// $Revision 1.1.1.1  1996/04/01  19:37:16  dschieb $
//
//======================================================================
#include <sys/time.h>
#include <stdlib.h>
#include "sos/header.h"
#include "sos/mdep.h"
#include "sos/types.h"

#define SOS_HEADER_SIZE		24
#define SOS_VERSION		0

unsigned char sos_header::current_version = SOS_VERSION;
unsigned char sos_header::current_header_size = SOS_HEADER_SIZE;

void sos_header::stamp()
	{
	int mn_ = SOS_MAGIC;
	unsigned char *mn = (unsigned char *) &mn_;
	unsigned char *ptr = buf;
	*ptr++ = SOS_VERSION;			// 0
	*ptr++ = SOS_ARC;			// 1
	*ptr++ = type_;				// 2
	*ptr++ = sos_size(type_);		// 3
	// magic number
	*ptr++ = *mn++;				// 4
	*ptr++ = *mn++;				// 5
	*ptr++ = *mn++;				// 6
	*ptr++ = *mn++;				// 7
	// length
	unsigned int l = length_;
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
