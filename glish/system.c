/* $Id$
** Copyright (c) 1993 The Regents of the University of California.
** Copyright (c) 1997 Associated Universities Inc.
*/

#include "config_p.h"
#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"

#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <string.h>
#include <termio.h>
#include <sys/uio.h>
#include <stddef.h>

#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#ifndef HAVE_GETHOSTNAME
#include <sys/utsname.h>
#endif

typedef RETSIGTYPE (*correct_sig_handler)( );

#ifdef SETRLIMIT_NOT_DECLARED
extern int setrlimit(int, const struct rlimit *);
extern int getrlimit(int, struct rlimit *);
#endif

/*
 * The stuff below is needed by the "time" command.  If this
 * system has no gettimeofday call, then must use times and the
 * CLK_TCK #define (from sys/param.h) to compute elapsed time.
 * Unfortunately, some systems only have HZ and no CLK_TCK, and
 * some might not even have HZ.
 */

#ifdef GETTOD_NOT_DECLARED
extern int gettimeofday (struct timeval *, struct timezone *);
#endif

#ifdef NO_GETTOD
#   include <sys/timeb.h>
#else
#   ifdef HAVE_BSDGETTIMEOFDAY
#       define gettimeofday BSDgettimeofday
#   endif
#endif

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

	if ( getsockname( sock, (struct sockaddr*) &addr, &len ) < 0 )
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

	result = bind( socket, (struct sockaddr*) &addr, sizeof( addr ) );

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

	result = bind( socket, (struct sockaddr*) &addr, sizeof( addr ) );

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
		accept( connection_socket, (struct sockaddr*) &remote_addr, &remote_addr_len );

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
		accept( connection_socket, (struct sockaddr*) &remote_addr, &remote_addr_len );

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

	if ( connect( sock, (struct sockaddr*)  &target_addr, sizeof( target_addr ) ) < 0 )
		return 0;

	return 1;
	}


int local_connection( int sock, const char* path )
	{
	struct sockaddr_un target_addr;

	target_addr.sun_family = AF_UNIX;
	strcpy( target_addr.sun_path, path );

	if ( connect( sock, (struct sockaddr*) &target_addr, sizeof( target_addr ) ) < 0 )
		return 0;

	return 1;
	}

char *canonic_path( const char *path_in )
	{
#ifdef S_ISLNK
	char scratch_[2048];
	char *scratch = scratch_;

	char path_[2048];
	char *path = path_;
	char *pptr = path;
	char newpath_[2048];
	char *newpath = newpath_;
	char *nptr = newpath;

	int len, slen;
	struct stat stat_buf;

	if ( ! path_in || ! *path_in ) return 0;

	strcpy( path, path_in );

	if ( *pptr == '.' )
		{
		getcwd( newpath, 2048 );
		nptr += strlen(newpath) - 1;
		if ( *nptr++ != '/' ) *nptr++ = '/';
		}
	else if ( *pptr == '/' )
		*nptr++ = *pptr++;

	while ( *pptr )
		{
		if ( pptr[0] == '.' )
			{
			if ( pptr[1] == '.' )
				{
				nptr -= 1;
				while ( nptr != newpath && *--nptr != '/' );
				if (nptr == newpath ) return 0;
				pptr += 2;
				if ( *pptr == '/' ) ++pptr;
				++nptr;
				continue;
				}
			else if ( pptr[1] == '/' )
				{
				pptr += 2;
				continue;
				}
			}

		while ( *pptr && *pptr != '/' ) *nptr++ = *pptr++;
		*nptr = '\0';
		if ( lstat( newpath, &stat_buf ) < 0 ) return 0;
		if ( S_ISLNK(stat_buf.st_mode) )
			{
			len = readlink( newpath, scratch, 2048 );
			if ( len <= 0 ) return 0;
			if ( *pptr )
				{
				slen = strlen(pptr);
				memcpy( &scratch[len], pptr, slen );
				scratch[len+slen] = '\0';
				}
			else
				scratch[len] = '\0';

			pptr = scratch;
			scratch = path;
			path = pptr;

			if ( *pptr == '/' )
				{
				nptr = newpath;
				*nptr++ = *pptr++;
				}
			else
				{
				while ( nptr != newpath && *--nptr != '/' );
				++nptr;
				}
			}

		if ( *pptr == '/' ) *nptr++ = *pptr++;
		}

	*nptr = '\0';
	return strdup(newpath);

#else
	return 0;
#endif
	}

int stream_pipe( int fd[2] )
	{
#ifdef HAVE_STREAMLESS_PIPE
	return socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
#else
	return pipe(fd);
#endif
	}

#ifdef HAVE_BSD_RENO
static struct cmsghdr *cmptr = 0;
#define CONTROLLEN (sizeof(struct cmsghdr) + sizeof(int))
#endif

#ifdef HAVE_STREAMLESS_PIPE
int send_fd( int pipe, int fd )
	{
	char buf;
	struct iovec iov[1];
	struct msghdr msg;

	if ( fd <= 0 ) return -1;

	iov[0].iov_base = &buf;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;

#ifndef HAVE_BSD_RENO
	msg.msg_accrights = (caddr_t) &fd;
	msg.msg_accrightslen = sizeof(int);
#else
	if ( cmptr == 0 && (cmptr = (struct cmsghdr *) malloc(CONTROLLEN)) == NULL )
		return -1;
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_RIGHTS;
	cmptr->cmsg_len = CONTROLLEN;
	msg.msg_control = (caddr_t) cmptr;
	msg.msg_controllen = CONTROLLEN;
#ifdef CMSG_DATA
	*(int*)CMSG_DATA(cmptr) = fd;
#else
	*(int*)cmptr->cmsg_data = fd;
#endif
#endif
	if ( sendmsg(pipe, &msg, 0) != 1 )
		return -1;

	return 0;
	}

int recv_fd( int pipe )
	{
	int newfd = -1;
	int len;
	char buf;
	struct iovec iov[1];
	struct msghdr msg;

	iov[0].iov_base = &buf;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;

#ifndef HAVE_BSD_RENO
	msg.msg_accrights = (caddr_t) &newfd;
	msg.msg_accrightslen = sizeof(int);
#else
	if ( ! cmptr && ! (cmptr = (struct cmsghdr *) malloc(CONTROLLEN)) )
		return -1;
	msg.msg_control = (caddr_t) cmptr;
	msg.msg_controllen = CONTROLLEN;
#endif

	if ( (len = recvmsg(pipe, &msg, 0)) < 0 )
		{
		fprintf(stderr,"recvmsg error\n");
		return -1;
		}
	else if ( len == 0 )
		{
		fprintf(stderr,"connection closed by server\n");
		return -1;
		}

#ifndef HAVE_BSD_RENO
	if ( msg.msg_accrightslen != sizeof(int) )
#else
	if ( msg.msg_controllen == CONTROLLEN )
#ifdef CMSG_DATA
	newfd = *(int*)CMSG_DATA(cmptr);
#else
	newfd = *(int*)cmptr->cmsg_data;
#endif
	else
#endif
		{
		fprintf(stderr,"no fd found\n");
		return -1;
		}

	return newfd;
	}
#else
int send_fd( int pipe, int fd )
	{
	if ( ioctl(pipe, I_SENDFD, fd) < 0 )
		return -1;
	return 0;
	}
int recv_fd( int pipe )
	{
	struct strrecvfd fdrec;

	if ( ioctl( pipe, I_RECVFD, &fdrec ) < 0 )
		return -1;

	return fdrec.fd;
	}
#endif

#ifndef HAVE_WAITPID
pid_t waitpid( pid_t pid, int *loc, int opts )
	{
	int status = wait4( pid, (union wait*) loc, opts, (struct rusage*) 0 );

	if ( status == 0 )
		return 0;

	return pid;
	}
#endif

int wait_for_pid( int pid, int *loc, int opts )
	{
	return (int) waitpid( (pid_t) pid, loc, opts );
	}

int reap_terminated_process()
	{
	int status = wait_for_pid( -1, (int *) 0, WNOHANG );

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

	if ( status == -1 && errno == ECHILD )
		return 0;			/* our SIGCHLD handler got it */

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
#ifdef HAVE_SETRLIMIT
	struct rlimit rl;

	if ( getrlimit( RLIMIT_NOFILE, &rl ) < 0 )
		pgripe( "maximize_num_fds(): getrlimit failed" );

	rl.rlim_cur = rl.rlim_max;

	if ( setrlimit( RLIMIT_NOFILE, &rl ) < 0 )
		pgripe( "maximize_num_fds(): setrlimit failed" );
#endif
	}

/*
**  The old behavior was:
**
**	return (signal_handler) signal( sig, (correct_sig_handler) handler );
**
**  but due to differences in SYSV & BSD it was changed.
*/
signal_handler install_signal_handler( int sig, signal_handler handler )
	{
	struct sigaction act, old;

	act.sa_handler = (correct_sig_handler) handler;
	sigemptyset( &act.sa_mask );
	act.sa_flags = 0;
	if ( sig == SIGALRM )
		{
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;		/* SunOS */
#endif
		}
	else
		{
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;		/* SVR4, 4.3+BSD */
#endif
		}

#ifdef SA_SIGINFO
	act.sa_flags |= SA_SIGINFO;
#endif

	if ( sigaction(sig, &act, &old) < 0 )
		return (signal_handler) SIG_ERR;

	return (signal_handler) old.sa_handler;
	}

/*
** This is needed because a longjump in a signal handler
** does not result in the signal being unblock.
*/
void unblock_signal( int sig )
	{
	sigset_t newmask, oldmask;
	sigemptyset( &newmask );
	sigaddset( &newmask, sig );
	sigprocmask( SIG_UNBLOCK, &newmask, &oldmask );
	}

static int tcp_proto_number()
	{
	static int tcp_proto = -1;
	struct protoent *tcp_protocol = 0;

	if ( tcp_proto < 0 )
		if ( ! (tcp_protocol = getprotobyname( "tcp" )) )
			gripe( "can't find protocol entry for TCP" );
		else
			tcp_proto = tcp_protocol->p_proto;

	return tcp_proto;
	}


void set_tcp_nodelay( int socket )
	{
	int enable_option = 1;

#if defined(HAVE_SETSOCKOPT) && defined(TCP_NODELAY)
	if ( setsockopt( socket, tcp_proto_number(), TCP_NODELAY,
			 (char *) &enable_option, sizeof( int ) ) < 0 )
                pgripe( "can't set TCP_NODELAY on socket" );
#endif
	}

static struct termio tbufsave;
static char char_mode = 0;

void set_term_char_mode()
	{
	struct termio tbuf;
	
	if ( ! char_mode )
		{
		if ( ioctl( 0, TCGETA, &tbuf ) == -1 )
			pgripe( "set_term_char_mode ioctl(TCGETA)" );

		tbufsave = tbuf;
		tbuf.c_lflag &= ~ICANON;
		tbuf.c_cc[VMIN] = 1;
		tbuf.c_cc[VTIME] = 0;

		if ( ioctl( 0, TCSETAF, &tbuf ) == -1 )
			pgripe("set_term_char_mode ioctl(TCSETAF)");

		char_mode = 1;
		}
	}

void set_term_unchar_mode()
	{
	if ( char_mode )
		{
		if ( ioctl( 0, TCSETAF, &tbufsave ) == -1 )
			pgripe("set_term_unchar_mode ioctl(TCSETAF)");

		char_mode = 0;
		}
	}

#ifndef HAVE_GETHOSTNAME
int gethostname( char *name, int namelen )
	{
	struct utsname name_struct;

	if ( uname( &name_struct ) < 0 )
		return -1;

	strncpy( name, name_struct.nodename, namelen );

	return 0;
	}
#endif

const char* local_host_name()
	{
	static char local_host[64];

	if ( gethostname( local_host, sizeof( local_host ) ) < 0 )
		strcpy( local_host, "<unknown>" );

	return local_host;
	}

#ifndef HAVE_STRDUP
char *strdup( const char *str )
	{
	int str_length = strlen( str );
	char *tmp_str = (char*) alloc_memory(str_length + 1);

	return strcpy( tmp_str, str );
	}
#endif


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


double get_current_time()
	{

#if NO_GETTOD
	struct timeb ftm;

	if ( ftime(&ftm) )
		return 0.0

	return (double) ftm.time +
	       (double) ftm.millitm *
	       (double) 0.001;
#else
	struct timeval tp;
	struct timezone tz;

	if ( gettimeofday(&tp, &tz) )
		return 0.0;

	return (double) tp.tv_sec + 
	       (double) tp.tv_usec *
	       (double) 0.000001;
#endif
	}

/*******************************************************************
**** If you change these two routines, also change npd/util.c   ****
****                                                            ****
**** If AUTHENTICATE is defined, it is assumed that libnpd      ****
**** supplies these functions.                                  ****
*******************************************************************/
#if !defined(AUTHENTICATE)
/* Seeds the random number generator. */
void seed_random_number_generator()
	{
	static int did_seed = 0;

	if ( ! did_seed )
		{
		struct timeval t;

#if defined(HAVE_LRAND48)
		static unsigned short state[3];

		if ( gettimeofday( &t, (struct timezone *) 0 ) < 0 )
			abort();

		state[0] = (unsigned short) t.tv_sec;
		state[1] = (unsigned short) t.tv_usec;
		state[2] = (unsigned short) getpid();

		(void) seed48( state );
#elif defined(HAVE_RANDOM)
		static long state[2];
		extern char *initstate( unsigned seed, char *state, int n );

		if ( gettimeofday( &t, (struct timezone *) 0 ) < 0 )
			abort();

		state[0] = (long) t.tv_sec;
		state[1] = (long) t.tv_usec;

		(void) initstate( (unsigned) getpid(),
					(char *) state, sizeof state );
#else
		if ( gettimeofday( &t, (struct timezone *) 0 ) < 0 )
			abort();
		(void) srand( (int) (t.tv_sec + t.tv_usec) );
#endif

		did_seed = 1;
		}
	}

long random_long( )
	{
#if defined(HAVE_LRAND48)
	long l = (long) lrand48();
#elif defined(HAVE_RANDOM)
	long l = (long) random();
#else
	long l = (long) rand();
#endif
	return l;
	}
#endif
