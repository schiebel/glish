/* $Id$
**
**  Generates to stdout a key to be used for npd communication.
*/

#include <stdio.h>
#include "config.h"
#include "util.h"
RCSID("@(#) $Id$")

void usage()
	{
	fprintf( stderr, "usage: genkey [key-size]\n" );
	exit( 1 );
	}

int main( int argc, char **argv )
	{
	int n = 64;
	unsigned char *key;

	++argv, --argc;

	if ( argc > 1 )
		usage();

	if ( argc == 1 )
		n = atoi( argv[0] );

	if ( n <= 0 )
		usage();

	if ( ! (key = random_bytes( n )) )
		{
		fprintf( stderr, "genkey: random_bytes() failed - %s\n",
			errmsg );
		exit( 1 );
		}

	write_encoded_binary( stdout, key, n );

	return 0;
	}
