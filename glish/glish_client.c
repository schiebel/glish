/* $Header$ */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>	/* Just needed for htonl() and friends */
#include <netinet/in.h>	/* ditto */

#include "system.h"
#include "glish_event.h"
#include "glish_client.h"


#define EVENT_VALUE_BUF_SIZE 8192

static char *prog_name, *client_name;
static int read_fd, write_fd;
static int have_sequencer_connection;

static char event_value_buf[EVENT_VALUE_BUF_SIZE + 1];
static char event_name_buf[MAX_EVENT_NAME_SIZE];


static char *recv_string_event( int fd, char **event_value );
static void send_string_event( int fd, const char *event_name,
				const char *event_value );


int streq( const char* s1, const char* s2 )
	{
	return ! strcmp( s1, s2 );
	}


int client_init( int *argc_addr, char **argv )
	{
	int argc = *argc_addr;
	char** orig_argv = argv;
	int suspend = 0;
	int i;

	prog_name = argv[0];

	--argc, ++argv;	/* remove program name from argument list */

	/* sds_init(); */

	if ( argc < 2 || ! streq( argv[0], "-id" ) )
		{
		/* We weren't invoked by the sequencer.  Set things up
		 * so that we receive "events" from stdin and send them
		 * to stdout.
		 */
		have_sequencer_connection = 0;
		client_name = prog_name;

		read_fd = fileno( stdin );
		write_fd = fileno( stdout );
		}

	else
		{
		char *host;
		int port, socket;

		have_sequencer_connection = 1;
		client_name = argv[1];
		argc -= 2, argv += 2;

		if ( argc < 2 || ! streq( argv[0], "-host" ) )
			{
			fprintf( stderr,
		"invalid glish client argument list - missing host name" );
			exit( 1 );
			}

		host = argv[1];
		argc -= 2, argv += 2;

		if ( argc < 2 || ! streq( argv[0], "-port" ) )
			{
			fprintf( stderr,
		"invalid glish client argument list - missing port number" );
			exit( 1 );
			}

		port = atoi( argv[1] );
		argc -= 2, argv += 2;

		if ( argc > 0 && streq( argv[0], "-suspend" ) )
			{
			suspend = 1;
			--argc, ++argv;
			}

		socket = get_tcp_socket();

		if ( ! remote_connection( socket, host, port ) )
			{
			fprintf( stderr,
		"could not establish glish client connection to sequencer" );
			exit( 1 );
			}

		read_fd = write_fd = socket;
		}

	while ( suspend )
		sleep( 1 );	/* allow debugger to attach */

	if ( have_sequencer_connection )
		client_post_string_event( "established", client_name );

	for ( i = 1; i <= argc; ++i )
		orig_argv[i] = *(argv++);

	orig_argv[i] = 0;
	argv = orig_argv;

	/* Put argv[0] back into the argument count - it was removed
	 * near the top of this routine.
	 */
	++argc;
	*argc_addr = argc;

	return have_sequencer_connection;
	}


void client_terminate()
	{
	if ( have_sequencer_connection )
		client_post_string_event( "done", client_name );

	close( read_fd );
	close( write_fd );
	}


char *client_next_string_event( char **event_value )
	{
	char *event_name;

	if ( have_sequencer_connection )
		event_name = recv_string_event( read_fd, event_value );

	else
		{
		char buf[512];
		char *delim;

		if ( ! fgets( buf, sizeof( buf ), stdin ) )
			return 0;

		/* Nuke final newline, if present. */
		delim = strchr( buf, '\n' );

		if ( delim )
			*delim = '\0';

		/* Find separation between event name and value. */
		delim = strchr( buf, ' ' );

		if ( ! delim )
			{
			*event_value = "";
			event_name = buf;
			}

		else
			{
			*delim++ = '\0';
			*event_value = delim;
			event_name = buf;
			}
		}

	if ( event_name && streq( event_name, "terminate" ) )
		event_name = 0;

	return event_name;
	}


void client_post_string_event( const char *event_name, const char *event_value )
	{
	if ( have_sequencer_connection )
		send_string_event( write_fd, event_name, event_value );
	else
		{
		fprintf( stdout, "%s %s\n", event_name, event_value );
		fflush( stdout );
		}
	}

void client_post_int_event( const char *event_name, int event_value )
	{
	char val_buf[512];
	sprintf( val_buf, "%d", event_value );

	client_post_string_event( event_name, val_buf );
	}

void client_post_double_event( const char *event_name, double event_value )
	{
	char val_buf[512];
	sprintf( val_buf, "%12g", event_value );

	client_post_string_event( event_name, val_buf );
	}

void client_post_fmt_event( const char *event_name, const char *event_fmt,
				const char *fmt_arg )
	{
	char val_buf[512];
	sprintf( val_buf, event_fmt, fmt_arg );

	client_post_string_event( event_name, val_buf );
	}


static int read_buffer( int fd, char* buffer, int buf_size )
	{
	int status = read( fd, buffer, buf_size );

	if ( status == 0 )
		return 0;

	else if ( status < 0 )
		{
		if ( errno != EPIPE && errno != ECONNRESET )
			fprintf( stderr,
				"%s: problem reading buffer, errno = %d\n",
				prog_name, errno );
		return 0;
		}

	else if ( status < buf_size )
		return read_buffer( fd, buffer + status, buf_size - status );

	return 1;
	}

char *recv_string_event( int fd, char **event_value )
	{
	struct event_header hdr;
	int size;
	char *name;

	if ( ! read_buffer( fd, (char*) &hdr, sizeof( hdr ) ) )
		return 0;

	hdr.code = ntohl( hdr.code );
	size = (int) ntohl( hdr.event_length );
	strcpy( event_name_buf, hdr.event_name );
	name = event_name_buf;

	if ( streq( name, "terminate" ) )
		return 0;

	if ( hdr.code == SDS_EVENT )
		{
		fprintf( stderr,
			"%s (%s): received non-string-valued event \"%s\"\n",
			 prog_name, client_name, name );
		return 0;
		}

	else if ( size > EVENT_VALUE_BUF_SIZE )
		{
		fprintf( stderr,
		    "%s (%s): event \"%s\"'s value is too long (= %d bytes)\n",
			prog_name, client_name, name, size );
		return 0;
		}

	else
		{
		if ( size > 0 && ! read_buffer( fd, event_value_buf, size ) )
			{
			fprintf( stderr,
		"%s (%s): read only partial string for event \"%s\", errno = ",
			prog_name, client_name, name, errno );
			return 0;
			}

		else
			{
			event_value_buf[size] = '\0';
			*event_value = event_value_buf;
			return name;
			}
		}
	}


static int send_event_header( int fd, int code, int length, const char *name )
	{
	struct event_header hdr;

	hdr.code = htonl( (u_long) code );
	hdr.event_length = htonl( (u_long) length );

	if ( strlen( name ) >= MAX_EVENT_NAME_SIZE )
		fprintf( stderr,
		    "%s (%s): event name \"%s\" truncated to %d characters\n",
			prog_name, client_name, name, MAX_EVENT_NAME_SIZE - 1 );

	strncpy( hdr.event_name, name, MAX_EVENT_NAME_SIZE - 1 );
	hdr.event_name[MAX_EVENT_NAME_SIZE - 1] = '\0';

	if ( write( fd, (char*) &hdr, sizeof( hdr ) ) < 0 )
		{
		if ( errno != EPIPE && errno != ECONNRESET )
			fprintf( stderr,
	    "%s (%s): problem writing header for \"%s\" event, errno = %d\n",
				prog_name, client_name, name, errno );
		return 0;
		}

	return 1;
	}

void send_string_event( int fd, const char* event_name,
			const char* event_value )
	{
	int size = strlen( event_value );

	if ( ! send_event_header( fd, STRING_EVENT, size, event_name ) )
		return;

	if ( write( fd, event_value, size ) < 0 )
		{
		if ( errno != EPIPE && errno != ECONNRESET )
			fprintf( stderr,
			    "%s (%s): problem writing event \"%s\", errno = ",
				prog_name, client_name, event_name, errno );
		}
	}
