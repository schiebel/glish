/* $Header$ */


#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <string.h>

static int tcp_proto_number();
static void set_tcp_nodelay( int socket );
static void gripe( char msg[] );
static void pgripe( char msg[] );


char *mktemp( char *template );


void set_fd_non_blocking( int fd, int non_blocking_flag )
	{
	if ( ioctl( fd, FIONBIO, &non_blocking_flag ) )
		pgripe( "set_fd_non_blocking ioctl(FIONBIO)" );
	}


int get_tcp_socket()
        {
        int result_socket = socket( PF_INET, SOCK_STREAM, tcp_proto_number() );

        if ( ! result_socket )
                pgripe( "can't create socket" );

	set_tcp_nodelay( result_socket );

        return result_socket;
        }


int get_local_socket()
        {
        int result_socket = socket( PF_UNIX, SOCK_STREAM, 0 );

        if ( ! result_socket )
                pgripe( "can't create socket" );

        return result_socket;
        }


char* local_socket_name( int sock )
	{
	struct sockaddr_un addr;
	int len = sizeof( addr );

	if ( getsockname( sock, &addr, &len ) < 0 )
		pgripe( "getsockname() failed in local_socket_name()" );

	return strdup( addr.sun_path );
	}


int bind_socket( int socket, int port )
	{
	struct sockaddr_in addr;
	int result;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons( port );

	result = bind( socket, &addr, sizeof( addr ) );

	if ( result >= 0 )
		{
		if ( listen( socket, 5 ) < 0 )
			return -1;

		return 1;
		}

	else if ( errno == EADDRINUSE )
		return 0;

	else
		return -1;
	}


int bind_local_socket( int socket )
	{
	struct sockaddr_un addr;
	int result;
	static char template[64];

	strcpy( template, "/tmp/local.XXXXXX" );

	if ( ! mktemp( template ) )
		gripe( "mktemp() failed in bind_local_socket()" );

	addr.sun_family = AF_UNIX;
	strcpy( addr.sun_path, template );

	result = bind( socket, &addr, sizeof( addr ) );

	if ( result >= 0 )
		{
		if ( listen( socket, 5 ) < 0 )
			return -1;

		return 1;
		}

	else if ( errno == EADDRINUSE )
		return 0;

	else
		return -1;
	}


int accept_connection( int connection_socket )
	{
	struct sockaddr_in remote_addr; /* address of peer */
	int remote_addr_len;
	int new_socket;

	remote_addr_len = sizeof( remote_addr );

	new_socket =
		accept( connection_socket, &remote_addr, &remote_addr_len );

	if ( new_socket >= 0 )
		set_tcp_nodelay( new_socket );

	return new_socket;
	}


int accept_local_connection( int connection_socket )
	{
	struct sockaddr_un remote_addr; /* address of peer */
	int remote_addr_len;
	int new_socket;

	remote_addr_len = sizeof( remote_addr );

	new_socket =
		accept( connection_socket, &remote_addr, &remote_addr_len );

	return new_socket;
	}


int remote_connection( int sock, const char* hostname, int port )
	{
	struct hostent *target_host;
	struct sockaddr_in target_addr;

	target_host = gethostbyname( (char*) hostname );

	if ( ! target_host )
		return 0;

	target_addr.sin_family = target_host->h_addrtype;
	target_addr.sin_port = htons( port );

	if ( target_host->h_length > sizeof( target_addr.sin_addr ) )
		return 0;

	memcpy( (void*) &target_addr.sin_addr, 
		(const void*) target_host->h_addr_list[0],
		 target_host->h_length );

	if ( connect( sock, &target_addr, sizeof( target_addr ) ) < 0 )
		return 0;

	return 1;
	}


int local_connection( int sock, const char* path )
	{
	struct sockaddr_un target_addr;

	target_addr.sun_family = AF_UNIX;
	strcpy( target_addr.sun_path, path );

	if ( connect( sock, &target_addr, sizeof( target_addr ) ) < 0 )
		return 0;

	return 1;
	}


int reap_terminated_process()
	{
	int status = waitpid( -1, (int *) 0, WNOHANG );

	return status == -1 ? 0 : status;
	}


void mark_close_on_exec( int fd )
	{
	if ( fcntl( fd, F_SETFD, 1 ) == -1 )
		pgripe( "mark_close_on_exec(): fcntl failed" );
	}


static char* input_file_name = 0;

FILE* popen_with_input( const char* command, const char* input )
	{
	FILE *input_file;
	FILE *result;
	static char template[64];
	char new_command[1024];

	if ( ! input )
		{
		input_file_name = 0;
		return popen( command, "r" );
		}

	strcpy( template, "/tmp/glish.XXXXXX" );

	if ( ! (input_file_name = mktemp( template )) )
		gripe( "mktemp() failed in popen_with_input()" );

	input_file = fopen( input_file_name, "w" );
	if ( ! input_file )
		gripe( "could not create temp file in popen_with_input()" );

	if ( fputs( input, input_file ) == EOF )
		gripe( "out of /tmp space in popen_with_input()" );

	if ( fclose( input_file ) )
		gripe( "could not close temp file in popen_with_input()" );

	/* Now create a modified shell command that takes its standard input
	 * from the given file.
	 */
	if ( strlen( command ) > 512 )
		gripe( "command too large in popen_with_input()" );

	sprintf( new_command, "(%s) <%s", command, template );

	result = popen( new_command, "r" );

	if ( ! result )
		{
		if ( unlink( input_file_name ) < 0 )
			pgripe(
		"could not delete temporary file in popen_with_input()" );
		}

	return result;
	}

int pclose_with_input( FILE* pipe )
	{
	int status = pclose( pipe );

	if ( input_file_name )
		{
		if ( unlink( input_file_name ) < 0 )
			pgripe(
		"could not delete temporary file in pclose_with_input()" );
		}

	return status;
	}


char* make_named_pipe()
	{
	static char template[64];

	strcpy( template, "/tmp/client.XXXXXX" );

	if ( ! mktemp( template ) )
		gripe( "mktemp() failed in make_named_pipe()" );

	if ( mkfifo( template, 0666 ) < 0 )
		{
		perror( "named-pipe creation failed" );
		return 0;
		}

	return template;
	}

void maximize_num_fds()
	{
#ifdef NOTDEF
	struct rlimit rl;

	if ( getrlimit( RLIMIT_NOFILE, &rl ) < 0 )
		pgripe( "maximize_num_fds(): getrlimit failed" );

	rl.rlim_cur = rl.rlim_max;

	if ( setrlimit( RLIMIT_NOFILE, &rl ) < 0 )
		pgripe( "maximize_num_fds(): setrlimit failed" );
#endif
	}


static int tcp_proto_number()
	{
        static struct protoent *tcp_protocol = 0;

        if ( ! tcp_protocol && ! (tcp_protocol = getprotobyname( "tcp" )) )
                gripe( "can't find protocol entry for TCP" );

	return tcp_protocol->p_proto;
	}


void set_tcp_nodelay( int socket )
	{
	int enable_option = 1;

	if ( setsockopt( socket, tcp_proto_number(), TCP_NODELAY,
			 (char *) &enable_option, sizeof( int ) ) < 0 )
                pgripe( "can't set TCP_NODELAY on socket" );
	}


void gripe( char msg[] )
	{
	fprintf( stderr, "system error: %s\n", msg );
	exit( 1 );
	}


void pgripe( char msg[] )
	{
	fprintf( stderr, "system error: " );
	perror( msg );
	exit( 1 );
	}
