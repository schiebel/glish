/* $Header$ */

#include <stdio.h>
#include "glish_client.h"

double atof();

int main( int argc, char** argv )
	{
	int have_sequencer_connection;
	int i;
	char *name, *value;
	
	have_sequencer_connection = client_init( &argc, argv );

	fprintf( stderr,
		"%s: fired up with%s sequencer connection, arg list is:\n",
		argv[0], have_sequencer_connection ? "" : "out" );

	for ( i = 1; i < argc; ++i )
		{
		fprintf( stderr, "%s", argv[i] );
		if ( i < argc - 1 )
			fprintf( stderr, ", " );
		}

	fprintf( stderr, "\n" );

	while ( (name = client_next_string_event( &value )) )
		{
		fprintf( stderr, "received event %s (%s)\n", name, value );
		client_post_string_event( name, value );
		client_post_double_event( name, atof( value ) + 1.0 );
		client_post_int_event( name, atoi( value ) + 2 );
		}

	client_terminate();

	return 0;
	}
