static char rcsid[]  = "$Id$";
//======================================================================
// longint.cc
//
// $Id$
// $Revision 1.1.1.1  1996/04/01  19:37:16  dschieb $
//
//======================================================================
#include "longint.h"

int long_int::LOW = 0;
int long_int::HIGH = 1;

int long_int_init::initialized = 0;

long_int_init::long_int_init()
	{
	if ( ! initialized )
		{
		unsigned char b[4];
		b[0] = 0x96; b[1] = 0x3c; b[2] = 0x0; b[3] = 0xa1;
		//
		// are we dealing with little endian, e.g. alpha, pc, etc.
		//
		if ( *(unsigned int*)&b != 0x963c00a1 )
			{
			long_int::LOW = 1;
			long_int::HIGH = 0;
			}
		initialized = 1;
		}
        }
