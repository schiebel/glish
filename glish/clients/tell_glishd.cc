// $Header$

#include <stdio.h>
#include <stdlib.h>
#include <osfcn.h>
#include <string.h>

#include "Sds/sdsgen.h"
#include "Glish/Client.h"

#include "Reporter.h"
#include "ports.h"
#include "system.h"

const char* prog_name;

void usage()
	{
	fprintf( stderr, "usage: %s -k [host]\n", prog_name );
	fprintf( stderr, "\t-k kill Glish daemon on given host\n" );
	exit( 1 );
	}


int main( int argc, char** argv )
	{
	prog_name = argv[0];
	++argv, --argc;

	if ( argc <= 0 )
		usage();

	if ( strcmp( argv[0], "-k" ) )
		usage();

	++argv, --argc;	// skip control flag

	if ( argc > 1 )
		usage();

	char host[64];

	if ( argc == 1 )
		strcpy( host, argv[0] );
	else
		{
		if ( gethostname( host, sizeof( host ) ) < 0 )
			{
			fprintf( stderr, "%s: unknown local host\n",
					prog_name );
			exit( 1 );
			}
		}

	sds_init();
	init_reporters();
	init_values();

	int daemon_socket = get_tcp_socket();
	if ( ! remote_connection( daemon_socket, host, DAEMON_PORT ) )
		{
		fprintf( stderr, "%s: couldn't connect to glishd on host %s\n",
				prog_name, host );
		exit( 1 );
		}

	Value dummy_val( 1 );
	send_event( daemon_socket, "*terminate-daemon*", &dummy_val );

	return 0;
	}
