/* $Header$ */

/* Routines to do npd authentication. */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "md5.h"
#include "util.h"
RCSID("@(#) $Id$")
#include "auth.h"

#define MD5_DIGEST_SIZE 16
#define DEFAULT_KEY_SIZE 64

static const char user_fmt[] = "%s/users/%s";

int create_userkeyfile( const char *dir )
	{
	char key_file[512];
	int n;
	const char *user = get_our_username();
	unsigned char *user_key;
	FILE *key_f;
	int owner;

	n = strlen( user_fmt ) +
		strlen( dir ) + strlen( user ) + SLOP;

	if ( n >= sizeof key_file )
		{
		strcpy( errmsg, "user file name too large" );
		return 0;
		}

	sprintf( key_file, user_fmt, dir, get_our_username() );

	if ( owner = get_file_owner( key_file ) )
		return owner == get_our_userid() ? 1 : 0;

	(void) umask( 027 );

	if ( ! (key_f = fopen( key_file, "w")) )
		{
		sprintf( errmsg, "couldn't open user key file \"%s\" for output", key_file );
		return 0;
		}

	if ( ! (user_key = random_bytes( DEFAULT_KEY_SIZE )) )
		{
		sprintf( errmsg, "random_bytes() failed - %s\n", errmsg );
		fclose( key_f );
		return 0;
		}

	write_encoded_binary( key_f, user_key, DEFAULT_KEY_SIZE );
	fclose( key_f );
	my_free( user_key );

	return 1;
	}
	

/* Returns a key to be used to authenticate communication to/from a particular
 * host.  Returns the binary key (and its length in *len_p) on success,
 * nil on failure.
 */
static unsigned char *get_key( const char *dir, const char *host,
			       const char *user, int to_flag, int *len_p )
	{
	char key_file[512];
	FILE *key_f;
	unsigned char *host_key;
	int host_len;
	unsigned char *user_key;
	const char *nme;
	int user_len;
	int n,id;

	static const char from_fmt[] = "%s/hosts/%s";
	static const char to_fmt[] = "%s/hosts/%s";

	*len_p = 0;
	n = strlen( to_flag ? to_fmt : from_fmt ) +
		strlen( dir ) + strlen( host ) + SLOP;

	if ( n >= sizeof key_file )
		{
		strcpy( errmsg, "host file name too large" );
		return 0;
		}

	sprintf( key_file, to_flag ? to_fmt : from_fmt, dir, host );

	if ( ! (key_f = fopen( key_file, "r" )) )
		{
		sprintf( errmsg, "couldn't open host key file \"%s\"", key_file );
		return 0;
		}

	host_key = read_encoded_binary( key_f, &host_len );

	fclose( key_f );

	if ( host_len < MD5_DIGEST_SIZE )
		{
		sprintf( errmsg, "host key too small (%d bytes)", host_len );
		my_free( host_key );
		return 0;
		}

	n = strlen( user_fmt ) +
		strlen( dir ) + strlen( user ) + SLOP;

	if ( n >= sizeof key_file )
		{
		strcpy( errmsg, "user file name too large" );
		return 0;
		}

	sprintf( key_file, user_fmt, dir, user );

	if ( ! (key_f = fopen( key_file, "r" )) )
		{
		sprintf( errmsg, "couldn't open user key file \"%s\"", key_file );
		return 0;
		}

	user_key = read_encoded_binary( key_f, &user_len );

	fclose( key_f );

	if ( host_len != user_len )
		{
		sprintf( errmsg, "host key length (%d) != user key length (%d)",host_len,user_len);
		my_free( host_key );
		my_free( user_key );
		return 0;
		}

	if ( ! (id = get_file_owner(key_file)) )
		{
		sprintf( errmsg, "couldn't determine the ownership of \"%s\"", key_file );
		my_free( host_key );
		my_free( user_key );
		return 0;
		}

	if ( ! (nme = get_username(id)) )
		{
		sprintf( errmsg, "couldn't get the username for uid %d", id );
		my_free( host_key );
		my_free( user_key );
		return 0;
		}

	if ( strcmp( nme, user ) )
		{
		sprintf( errmsg, "key file ownership problem (not owned by %s)", user );
		my_free( host_key );
		my_free( user_key );
		return 0;
		}

	if ( user_len < MD5_DIGEST_SIZE )
		{
		sprintf( errmsg, "user key too small (%d bytes)", user_len );
		my_free( host_key );
		my_free( user_key );
		return 0;
		}

	xor_together( host_key, user_key, host_len );
	my_free( user_key );
	*len_p = host_len;

	return host_key;
	}

/* Compute the MD5 checksum for the given binary string, and return it
 * as a binary string.  Returns nil if insufficient memory.
 */
static unsigned char *compute_MD5( unsigned char *b, int n, int *len_p )
	{
	unsigned char *digest;
	MD5_CTX context;

	*len_p = MD5_DIGEST_SIZE;
	digest = (unsigned char *) my_alloc( *len_p );
	if ( ! digest )
		{
		strcpy( errmsg, "no memory for MD5 digest" );
		return 0;
		}

	MD5Init( &context );
	MD5Update( &context, b, n );
	MD5Final( digest, &context );

	return digest;
	}

/* Compose a challenge for communication from the given host.  Write the
 * challenge on the given file, and return the correct answer, or nil
 * on error.
 */
unsigned char *compose_challenge( const char *dir, const char *host,
				  const char *user, FILE *f, int *len_p )
	{
	unsigned char *key;
	unsigned char *challenge;
	unsigned char *answer;
	int n;

	if ( ! (key = get_key( dir, host, user, 0, &n )) )
		return 0;
	if ( ! (challenge = random_bytes( n )) )
		return 0;

	write_encoded_binary( f, challenge, n );

	xor_together( challenge, key, n );
	my_free( key );

	answer = compute_MD5( challenge, n, len_p );
	my_free( challenge );

	return answer;
	}

/* Answer a challenge for communication to a given host.  Write the
 * answer to the given file.  Returns 0 on error, non-zero on success.
 */
int answer_challenge( const char *dir, const char *host, const char *user, 
		      FILE *f, unsigned char *challenge, int len )
	{
	unsigned char *key;
	unsigned char *answer;
	int n;

	if ( ! (key = get_key( dir, host, user, 1, &n )) )
		return 0;

	if ( n != len )
		{
		strcpy( errmsg, "challenge incorrect size" );
		return 0;
		}

	xor_together( key, challenge, n );
	answer = compute_MD5( key, n, &n );
	my_free( key );

	write_encoded_binary( f, answer, n );
	my_free( answer );

	return 1;
	}
