/* $Header$ */

#include <stdio.h>

/* Amount to add to size computations to be sure to avoid fencepost errors. */
#define SLOP 10

/* Maximum size of an encoded binary string. */
#define MAX_ENCODED_BINARY 1024


/* A buffer for holding an error message. */
extern char errmsg[512];

extern FILE *log_file;


/* Grrr, strdup() isn't portable! */
extern char *copy_string( const char *str );

/* Given two byte arrays b1 and b2, both of length n, executes b1 ^= b2. */
extern void xor_together( unsigned char *b1, unsigned char *b2, int len );

/* Returns true if byte arrays b1 and b2 (both of length n) are equal, false
 * otherwise.
 */
extern int byte_arrays_equal( unsigned char *b1, unsigned char *b2, int len );

/* Returns a heap pointer to an array of len random bytes, or nil if not
 * enough memory available.
 */
extern unsigned char *random_bytes( int len );

/* Reads an encoded binary representation from the given file.  The file
 * format is as an ASCII string.  Whitespace is ignored, as are newlines
 * preceded by '\' (and initial newlines).  Returns a heap pointer and length
 * (in *len_p), or nil if the file format is incorrect or exceeds
 * MAX_ENCODED_BINARY in size.
 */
extern unsigned char *read_encoded_binary( FILE *f, int *len_p );

/* Writes an encoded binary representation to the given file.  The file format
 * is as an ASCII string, suitable for reading via read_encoded_binary().
 */
extern void write_encoded_binary( FILE *f, unsigned char *b, int len );

/* Returns a pointer to a (static region) string giving the next 
 * whitespace-delimited word sent by the peer.  Returns nil on EOF
 * or an excessively large word.
 */
extern const char *get_word_from_peer( FILE *npd_in );

/* Send the entire contents of the given file (regardless of our current
 * position in it) to the given peer.
 */
extern void send_file_to_peer( FILE *file, FILE *peer );

/* Receive the entire contents of a file from a peer and copy it to
 * the given file.  Returns non-zero on success, zero on failure (with
 * a message in errmsg).
 */
extern int receive_file_from_peer( FILE *peer, FILE *file );

/* Connect to the given host/port, returning a socket in *sock_ptr.  Returns
 * non-zero on success, zero on failure (with an error message in errmsg).
 */
extern int connect_to_host( const char *host, int port, int *sock_ptr );

/* Connect the given socket to the given host/port.  Returns non-zero on
 * success, zero on failure (with an error message in errmsg).
 */
extern int connect_socket_to_host( int sock, const char *host, int port );

/*
 * Get the username and userid of the currently running process
 */
extern const char *get_our_username();
extern int get_our_userid();

/*
 * Get any username given a userid or a userid given a username.
 * Return 0 if not found. Note that behavior with get_userid("root").
 */
extern const char *get_username( int id );
extern int get_userid( const char *name );

/*
 * Get the uid of the owner of a file. Returns 0 upon failure.
 * Note the effect of this on files owned by root.
 */
extern int get_file_owner( const char *filename );

/* Restart the log, reporting success to the given peer. */
extern int restart_log( FILE *peer );

/* Initialize the log functionality, i.e set program name etc. */
extern void init_log(const char *program_name);

/* Put an ID stamp in the log, along with the given message. */
extern void stamp_log( const char *msg );

/* Log a message using the given format.  Logging is done to FILE *log_file,
 * which if not explicitly initialized prior to the first call to log(),
 * defaults to stderr.
 */
extern int to_log( const char *fmt, ... );

/* Returns the name of the log file. */
extern const char *npd_log_file();

/* Returns a string description of the given errno.  (Necessary because you
 * can't portably declare sys_errlist! :-()
 */
extern const char *sys_error_desc();

/* Versions of malloc() and free() so we can isolate the proper definition
 * of a generic pointer type.
 */
extern void *my_alloc( int size );
extern void my_free( void *ptr );

#if ! defined(RCSID)
#if ! defined(NO_RCSID)
#if defined(__STDC__) || defined(__ANSI_CPP__)
#define UsE_PaStE(b) UsE__##b##_
#else
#define UsE_PaStE(b) UsE__/**/b/**/_
#endif
#if defined(__cplusplus)
#define UsE(x) inline void UsE_PaStE(x)(char *) { UsE_PaStE(x)(x); }
#else
#define UsE(x) static void UsE_PaStE(x)(char *d) { UsE_PaStE(x)(x); }
#endif
#define RCSID(str)			\
	static char *rcsid_ = str;	\
	UsE(rcsid_)
#else
#define RCSID(str)
#endif
#endif
