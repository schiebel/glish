//======================================================================
// io.cc
//
// $Id$
//
//======================================================================
#include "sos/sos.h"
RCSID("@(#) $Id$")
#include "sos/io.h"
#include "sos/types.h"
#include "convert.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>


#define PUTACTION_A(TYPE,SOSTYPE)					\
void sos_sink::put( TYPE *a, unsigned int l )				\
	{								\
	head.set(a,l);							\
	head.stamp();							\
									\
	write(FD,head.iBuffer(), head.iLength() * sos_size(SOSTYPE) +	\
	      sos_header::iSize());					\
	}

PUTACTION_A(byte,SOS_BYTE)
PUTACTION_A(short,SOS_SHORT)
PUTACTION_A(int,SOS_INT)
PUTACTION_A(float,SOS_FLOAT)
PUTACTION_A(double,SOS_DOUBLE)

#define PUTACTION_B(TYPE)						\
void sos_sink::put( TYPE *a, sos_code t, unsigned int l )		\
	{								\
	head.set(a,t,l);						\
	head.stamp();							\
									\
	write(FD, head.iBuffer(), head.iLength() * sos_size(t) +	\
	      sos_header::iSize());					\
	}

PUTACTION_B(char)
PUTACTION_B(unsigned char)

#if defined(VAXFP)
#define FOREIGN_FLOAT   SOS_IFLOAT
#define FOREIGN_DOUBLE  SOS_IDOUBLE
#define RIGHT_FLOAT     ieee2vax_single
#define RIGHT_DOUBLE    ieee2vax_double
#define DOUBLE_OP	'G'
#define FOREIGN_SWAP_A	case SOS_IDOUBLE:
#define FOREIGN_SWAP_B
#else 
#define FOREIGN_FLOAT   SOS_VFLOAT
#define FOREIGN_DOUBLE  SOS_DVDOUBLE: case SOS_GVDOUBLE
#define RIGHT_FLOAT     vax2ieee_single
#define RIGHT_DOUBLE    vax2ieee_double
#define DOUBLE_OP	type == SOS_DVDOUBLE ? 'D' : 'G'
#define FOREIGN_SWAP_A
#define FOREIGN_SWAP_B	if ( sos_big_endian ) swap_abcd_dcba(result,len*2); \
			else swap_abcdefgh_efghabcd(result,len);
#endif

sos_sink::~sos_sink() { if ( FD >= 0 ) close(FD); }

void *sos_source::get( sos_code &type, unsigned int &len )
	{
	if ( read(FD,head.iBuffer(),sos_header::iSize()) <= 0 )
		return 0;

	type = head.type();
	len = head.length();
	char *result_ = (char*) alloc_memory(len * head.typeLen() + sos_header::iSize());
	memcpy(result_, head.iBuffer(), sos_header::iSize());
	char *result  = result_ + sos_header::iSize();
	read(FD, result, len * head.typeLen());
	if ( ! (head.magic() & SOS_MAGIC) )
		switch( type ) {
		    case SOS_SHORT:
			swap_ab_ba(result,len);
			break;
		    case SOS_INT:
		    case SOS_FLOAT:
		    case FOREIGN_FLOAT:
		    	swap_abcd_dcba(result,len);
			break;
		    FOREIGN_SWAP_A
		    case SOS_DOUBLE:
		    	swap_abcdefgh_hgfedcba(result,len);
			break;
		}

	switch ( type ) {
	    case FOREIGN_FLOAT:
		RIGHT_FLOAT((float*)result,len);
		type = SOS_FLOAT;
		break;
	    case FOREIGN_DOUBLE:
		FOREIGN_SWAP_B
		RIGHT_DOUBLE((double*)result,len,DOUBLE_OP);
		type = SOS_DOUBLE;
		break;
	}
	return result_;
	}


sos_source::~sos_source() { if ( FD >= 0 ) close(FD); }
