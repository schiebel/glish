/* $Header$ */

/* Utility routines for npd and npd_control. */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <time.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "Npd/util.h"

/* Maximum word that a peer can send us. */
#define MAX_WORD 512

char errmsg[512];
FILE *log_file = 0;
static const char *prog_name = "noname";


/* strdup() functionality */
char *copy_string( const char *str )
	{
	char *result = (char *) my_alloc( strlen( str ) + 1 );
	strcpy( result, str );
	return result;
	}

/* Encodes the given binary representation as a readable ASCII string,
 * which can later be decoded by decode_string_to_binary.  Returns
 * a heap pointer to the string, or nil if not enough memory available.
 */
static char *encode_binary_to_string( const unsigned char *b, int len )
	{
	char *s = (char *) my_alloc( len * 2 + SLOP );
	int i;

	if ( ! s )
		{
		strcpy( errmsg, "not enough memory in encode_binary_to_string" );
		return 0;
		}

	for ( i = 0; i < len; ++i )
		sprintf( &s[i*2], "%02x", b[i] );

	s[i*2] = '\0';

	return s;
	}


/* Decodes the given string to its corresponding binary representation.
 * Returns a heap pointer and the number of bytes in the binary representation
 * (in *len_p), or nil if not enough memory available or the string is invalid.
 */
static unsigned char *decode_string_to_binary( const char *str, int *len_p )
	{
	int n = strlen( str );
	unsigned char *b = (unsigned char *) my_alloc( n / 2 + SLOP );

	if ( ! b )
		{
		strcpy( errmsg, "not enough memory in decode_string_to_binary" );
		return 0;
		}

	n = 0;	/* n is from now on an index into b */

	while ( *str )
		{
		int next_byte;

		if ( sscanf( str, "%2x", &next_byte ) != 1 )
			{
			my_free( (void *) b );
			strcpy( errmsg, "bad format in decode_string_to_binary" );
			return 0;
			}

		b[n++] = next_byte;

		/* Skip over the two bytes we just scanned, if both
		 * are present.
		 */
		if ( *++str )
			++str;
		}

	*len_p = n;

	return b;
	}

/* Seeds the random number generator. */
static void seed_random_number_generator()
	{
	static int did_seed = 0;

	if ( ! did_seed )
		{
		struct timeval t;

#ifdef HAVE_RANDOM
		static long state[2];
		extern char *initstate( unsigned seed, char *state, int n );

		if ( gettimeofday( &t, (struct timezone *) 0 ) < 0 )
			abort();

		state[0] = (long) t.tv_sec;
		state[1] = (long) t.tv_usec;

		(void) initstate( (unsigned) getpid(),
					(char *) state, sizeof state );
#elif defined(HAVE_LRAND48)
		static unsigned short state[3];
		unsigned short int *seed48(unsigned short int seed16v[3]);

		if ( gettimeofday( &t, (struct timezone *) 0 ) < 0 )
			abort();

		state[0] = (unsigned short) t.tv_sec;
		state[1] = (unsigned short) t.tv_usec;
		state[2] = (unsigned short) getpid();

		(void) seed48( state );
#else
		if ( gettimeofday( &t, (struct timezone *) 0 ) < 0 )
			abort();
		(void) srand( (int) (t.tv_sec + t.tv_usec) );
#endif

		did_seed = 1;
		}
	}

/* Given two byte arrays b1 and b2, both of length n, executes b1 ^= b2. */
void xor_together( unsigned char *b1, unsigned char *b2, int len )
	{
	int n;

	for ( n = 0; n < len; ++n )
		b1[n] ^= b2[n];
	}

/* Returns true if byte arrays b1 and b2 (both of length n) are equal, false
 * otherwise.
 */
int byte_arrays_equal( unsigned char *b1, unsigned char *b2, int len )
	{
	int n;

	for ( n = 0; n < len; ++n )
		if ( b1[n] != b2[n] )
			return 0;

	return 1;
	}

long random_long( )
	{
#ifdef HAVE_RANDOM
	extern long random();
	long l = random();
#elif defined(HAVE_LRAND48)
	extern long lrand48();
	long l = lrand48();
#else
	extern int rand();
	long l = (long) rand();
#endif
	return l;
	}

/* Returns a heap pointer to an array of len random bytes, or nil if not
 * enough memory available.
 */
unsigned char *random_bytes( int len )
	{
	unsigned char *b = (unsigned char *) my_alloc( len );
	int n = 0;

	if ( ! b )
		return 0;

	seed_random_number_generator();

	for ( n = 0; n < len; ++n )

		{
		long l = random_long();

		/* Take some middle bits of the random number. */
		b[n] = (unsigned char) ((l & 0xff00) >> 8);
		}

	return b;
	}


/* Reads an encoded binary representation from the given file.  The file
 * format is as an ASCII string.  Whitespace is ignored, as are newlines
 * preceded by '\' (and initial newlines).  Returns a heap pointer and length
 * (in *len_p), or nil if the file format is incorrect or exceeds
 * MAX_ENCODED_BINARY in size.
 */
unsigned char *read_encoded_binary( FILE *f, int *len_p )
	{
	char encoded_str[MAX_ENCODED_BINARY * 2 + SLOP];
	int n = 0;
	int c;

	while ( (c = getc( f )) != EOF )
		{
		if ( c == '\n' )
			{
			if ( n == 0 )
				continue;	/* an initial newline */
			else
				break;
			}

		if ( isspace(c) )
			continue;

		if ( c == '\\' )
			{
			c = getc( f );
			if ( c == '\n' )
				continue;
			sprintf( errmsg,
				"bad character in encoded binary: 0x%02x\n",
				c );
			return 0;
			}

		if ( n < MAX_ENCODED_BINARY * 2 )
			encoded_str[n++] = c;
		else
			{
			strcpy( errmsg, "encoded binary in file too large" );
			return 0;
			}
		}

	if ( n == 0 )
		{
		strcpy( errmsg, "empty encoded binary in file" );
		return 0;
		}

	encoded_str[n] = '\0';

	return decode_string_to_binary( encoded_str, len_p );
	}

/* Writes an encoded binary representation to the given file.  The file format
 * is as an ASCII string, suitable for reading via read_encoded_binary().
 */
void write_encoded_binary( FILE *f, unsigned char *b, int len )
	{
	char *s = encode_binary_to_string( b, len );
	int n = 0;

	while ( s[n] )
		{
		putc( s[n++], f );
		if ( n % 64 == 0 && s[n] )
			/* Wrap to next line. */
			fprintf( f, " \\\n" );
		}

	putc( '\n', f );

	my_free( (void *) s );
	}

/* Returns a pointer to a (static region) string giving the next 
 * whitespace-delimited word sent by the peer.  Returns nil on EOF
 * or an excessively large word.
 */
const char *get_word_from_peer( FILE *peer )
	{
	static char word[MAX_WORD + SLOP];
	int i, c;

	while ( isspace(c = getc( peer )) )
		;

	for ( i = 0; i < MAX_WORD; ++i )
		{
		if ( c == EOF )
			return 0;

		if ( isspace(c) )
			break;

		word[i] = c;
		c = getc( peer );
		}

	if ( i >= MAX_WORD )
		return 0;

	word[i] = '\0';

	return word;
	}


/* Send the entire contents of the given file (regardless of our current
 * position in it) to the given peer.
 */
void send_file_to_peer( FILE *file, FILE *peer )
	{
	int size, c;

	/* First, count up the number of bytes in the file. */
	rewind( file );
	for ( size = 0; getc( file ) != EOF; ++size )
		;

	/* Okay, tell the peer the size and then spray the file at them. */
	fprintf( peer, "\n%d\n", size );

	rewind( file );
	for ( c = getc( file ); c != EOF; c = getc( file ) )
		putc( c, peer );
	}

/* Receive the entire contents of a file from a peer and copy it to
 * the given file.  Returns non-zero on success, zero on failure (with
 * a message in errmsg).
 */
int receive_file_from_peer( FILE *peer, FILE *file )
	{
	int size, c, cnt;

	size = atoi( get_word_from_peer( peer ) );

	for ( cnt = 0; cnt < size; ++cnt )
		{
		if ( (c = getc( peer )) == EOF )
			return 0;

		putc( c, file );
		}

	return 1;
	}

/* Connect to the given host/port, returning a socket in *sock_ptr.  Returns
 * non-zero on success, zero on failure (with an error message in errmsg).
 */
int connect_to_host( const char *host, int port, int *sock_ptr )
	{
	struct hostent *target_host;
	struct sockaddr_in target_addr;
	int s;

	if ( ! (target_host = gethostbyname( host )) )
		{
		sprintf( errmsg, "couldn't look up host \"%s\"", host );
		return 0;
		}

	target_addr.sin_family = target_host->h_addrtype;
	target_addr.sin_port = htons( port );

	if ( target_host->h_length > sizeof target_addr.sin_addr )
		{
		sprintf( errmsg, "host addr too large!" );
		return 0;
		}

	memcpy( (void*) &target_addr.sin_addr,
		(const void*) target_host->h_addr_list[0],
		 target_host->h_length );

	if ( (s = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 )
		{
		sprintf( errmsg, "couldn't create socket (%s)", sys_error_desc() );
		return 0;
		}

	if ( connect( s, (struct sockaddr *) &target_addr,
			sizeof target_addr ) < 0 )
		{
		sprintf( errmsg, "couldn't connect (%s)", sys_error_desc() );
		return 0;
		}

	*sock_ptr = s;

	return 1;
	}

/* Connect the given socket to the given host/port.  Returns non-zero on
 * success, zero on failure (with an error message in errmsg).
 */
int connect_socket_to_host( int sock, const char *host, int port )
	{
	struct hostent *target_host;
	struct sockaddr_in target_addr;

	if ( ! (target_host = gethostbyname( host )) )
		{
		u_int addr[4];
		u_long full_addr;

		if ( ! isdigit( host[0] ) )
			{
			sprintf( errmsg, "couldn't look up host \"%s\"", host );
			return 0;
			}

		/* Try it as a dotted address. */
		if ( sscanf( host, "%d.%d.%d.%d",
				&addr[0], &addr[1], &addr[2], &addr[3] ) != 4 )
			{
			sprintf( errmsg,
				"couldn't look up host \"%s\"", host );
			return 0;
			}

		target_addr.sin_family = AF_INET;
		full_addr = (addr[0] << 24) | (addr[1] << 16) |
			    (addr[2] << 8) | addr[3];

		full_addr = htonl( full_addr );

		memcpy( (void*) &target_addr.sin_addr,
			(const void*) &full_addr, sizeof( full_addr ) );
		}

	else
		{ /* Could look up the name */
		target_addr.sin_family = target_host->h_addrtype;

		if ( target_host->h_length > sizeof target_addr.sin_addr )
			{
			sprintf( errmsg, "host addr too large!" );
			return 0;
			}

		memcpy( (void*) &target_addr.sin_addr,
			(const void*) target_host->h_addr_list[0],
			 target_host->h_length );
		}

	target_addr.sin_port = htons( port );

	if ( connect( sock, (struct sockaddr *) &target_addr,
			sizeof target_addr ) < 0 )
		{
		sprintf( errmsg, "couldn't connect (%s)", sys_error_desc() );
		return 0;
		}

	return 1;
	}

int get_our_userid()
	{
	return geteuid();
	}

const char *get_our_username()
	{
	struct passwd *pw;
	static char name[1024];
	pw = getpwuid(get_our_userid());
	if ( pw )
		if ( strlen(pw->pw_name) + 1 > sizeof(name) )
			return 0;
		else
			strcpy(name,pw->pw_name);
	else
		return 0;

	return name;
	}

const char *get_username( int id )
	{
	struct passwd *pw;
	static char name[1024];
	pw = getpwuid( id );
	if ( pw )
		if ( strlen(pw->pw_name) + 1 > sizeof(name) )
			return 0;
		else
			strcpy(name,pw->pw_name);
	else
		return 0;

	return name;
	}

int get_userid( const char *name )
	{
	struct passwd *pw;
	pw = getpwnam( name );
	return pw ? pw->pw_uid : 0;
	}

int get_file_owner( const char *filename )
	{
	struct stat statrec;
	int result = stat(filename, &statrec);
	return result >= 0 ? statrec.st_uid : 0;
	}

static void start_log()
	{
#ifdef LOG_FMT
	const char *npd_log = npd_log_file();

	(void) umask( 027 );
	if ( ! (log_file = fopen( npd_log, "a+" )) )
		{
		fprintf( stderr, "%s: can't open log file %s\n",
				prog_name, npd_log );
		exit( 1 );
		}

#ifndef NO_LOCKING
#ifdef HAVE_FLOCK
	if ( flock( fileno(log_file), LOCK_EX | LOCK_NB ) < 0 )
		{
		fprintf( stderr, "%s: can't lock log file %s (%s)\n",
				prog_name, npd_log,
				errno == EWOULDBLOCK ?
					"already locked" :
					sys_error_desc() );
		exit( 1 );
		}
#elif defined(HAVE_LOCKF)
	{
	off_t offset;
	offset = 0;
	if ( lockf( fileno(log_file), F_TLOCK, offset ) < 0 )
		{
		fprintf( stderr, "%s: can't lock log file %s (%s)\n",
				prog_name, npd_log,
				errno == EACCES ?
					"already locked" :
					sys_error_desc() );
		exit( 1 );
		}
	}
#endif
#else
	/* NO_LOCKING */
#endif
#else
	log_file = stderr;
#endif
	}

/* Restart the log, reporting success to the given peer. */
int restart_log( FILE *peer )
	{
#ifdef LOG_FMT
	char buf[512];
	static const char fmt[] = "%s %s <%s";
	int status;
	struct stat stat_buf;
	const char *npd_log = npd_log_file();

	if ( stat( npd_log, &stat_buf ) < 0 )
		{
		to_log( "can't stat log file %s (%s)\n", npd_log, sys_error_desc() );
		fprintf( peer, "fail no-log\n" );
		return 1;
		}

	if ( log_file != stderr )
		fclose( log_file );

/**	if ( strlen( fmt ) + strlen( NPD_MAIL_PGM ) + strlen( NPD_LOG_MAIL ) +
	     strlen( npd_log ) + SLOP >= sizeof buf )
		{
		fprintf( stderr, "%s: can't form log-file mail command\n",
				prog_name );
		exit( 1 );
		}

	sprintf( buf, fmt, NPD_MAIL_PGM, NPD_LOG_MAIL, npd_log );

	if ( (status = system( buf )) != 0 )
		{
		fprintf( stderr, "%s: bad status executing \"%s\": %d\n",
				prog_name, buf, status );
		exit( 1 );
		}
**/
	if ( unlink( npd_log ) < 0 )
		fprintf( stderr, "%s: couldn't unlink %s (%s)\n",
				prog_name, npd_log, sys_error_desc() );

	start_log();
	fprintf( peer, "done\n" );
	return 1;

#else
	fprintf( peer, "fail no-log-file\n" );
	return 1;
#endif
	}

void stamp_log( const char *msg )
	{
	time_t t;
	char *time_str;

	t = time( (time_t *) 0 );
	time_str = ctime( &t );
	time_str[19] = '\0';	/* don't want timezone & year */
	time_str += 4;	/* don't want day-of-week */

	to_log( "%s %s [%d/%s]: %s",
		time_str, prog_name, getpid(), npd_log_file(), msg );
	}

int to_log( const char *fmt, ... )
	{
#ifdef HAVE_VPRINTF
	va_list ap;

	if ( ! log_file )
		start_log();

	va_start(ap, fmt);
	vfprintf( log_file, fmt, ap );
	va_end(ap);

	putc( '\n', log_file );

	fflush( log_file );

	if ( ferror( log_file ) )
		{
		fprintf( stderr, "%s: log file error", prog_name );
		/* What the hell, try to log it too */
		fprintf( log_file, "%s: log file error", prog_name );
		exit( 1 );
		}

#ifdef NO_LOCKING
	/* Close the log so next call will start it afresh. */
	if ( log_file != stderr )
		fclose( log_file );
	log_file = 0;
#endif
#endif
	return 0;
	}

#ifndef HAVE_GETHOSTNAME
static int gethostname( char *name, int namelen )
        {
        struct utsname name_struct;

        if ( uname( &name_struct ) < 0 )
                return -1;

        strncpy( name, name_struct.nodename, namelen );

        return 0;
        }
#endif

const char *npd_log_file()
	{
	static char npd_log[512];

#ifdef LOG_FMT
	char hostname[256];
	const char *user;

	if ( gethostname( hostname, (sizeof hostname) - SLOP ) < 0 )
		{
		fprintf( stderr, "%s: can't get host name! (%s)\n",
				prog_name, sys_error_desc() );
		exit( 1 );
		}

	if ( ! (user = get_our_username() ) )
		{
		fprintf( stderr, "%s: can't get user name!\n",
				prog_name );
		exit( 1 );
		}

	if ( strlen( LOG_FMT ) + strlen( hostname ) + strlen(user) +
	     strlen(prog_name) + SLOP >= sizeof npd_log )
		{
		fprintf( stderr,
			"%s: can't construct log file from %s, %s and %s\n",
			prog_name, LOG_FMT, hostname, user );
		exit( 1 );
		}

	sprintf( npd_log, LOG_FMT, prog_name, user, hostname );
#else
	strcpy( npd_log, "logging disabled" );
#endif

	return npd_log;
	}

const char *sys_error_desc()
	{
	static char buf[64];

	switch ( errno )
		{
		/* We do not try to be comprehensive, just to get text
		 * for the ones we expect we might encounter.
		 */
		case EPERM:
		case EACCES:
			return "Permission denied";
		case ENOENT:
			return "No such file or directory";
		case ESRCH:
			return "No such process";
		case ENOEXEC:
			return "Exec format error";
		case EINVAL:
			return "Invalid argument";
		case EADDRINUSE:
			return "Address already in use";
		case ECONNREFUSED:
			return "Connection refused";

		default:
			sprintf( buf, "errno %d", errno );
			return buf;
		}
	}

void init_log( const char *program_name )
	{
	static unsigned int init = 0;
	static char name[512];
	char message[512];
	char *ptr;
	int len;

	if ( init++ == 0 )
		{
		len = strlen(program_name);
		if ( len-- > 0 )
			{
			for (ptr = program_name + len; len && *ptr != '/'; --ptr, --len);
			if ( len ) ++ptr;
			strcpy(name,ptr);
			}
		prog_name = name;
#ifdef LOG_FMT
		sprintf(message,"logging initialized for %s.",prog_name);
		}
	else
		sprintf(message,"logging initialized (%dx) for %s.",init, prog_name);

	stamp_log(message);
#else
		}
#endif
	}

void *my_alloc( int size )
	{
	return (void *) malloc( size );
	}

void my_free( void *ptr )
	{
	free( ptr );
	}
