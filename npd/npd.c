/* $Header$ */

/*
 * Copyright (c) 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* npd - network probe daemon */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <memory.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include "Npd/util.h"
RCSID("@(#) $Id$")
#include "Npd/auth.h"
#include "version.h"

/* Version of Network Probe Protocol. */
#define NPP_VERSION 1

int create_keyfile()
	{
	return create_userkeyfile( KEYS_DIR );
	}

/* Authenticate ourselves to the given peer.  Returns non-zero on success,
 * zero on failure.
 */
static int authenticate_to_peer( FILE *read_, FILE *write_, const char *local_host,
			  const char *remote_host )
	{
	const char *s;
	int version;
	unsigned char *challenge;
	int challenge_len;
	const char *our_username;

	if ( ! create_keyfile() )
		return to_log( "couldn't create key file" );

	if ( ! (our_username = get_our_username()) )
		return to_log( "couldn't get our username" );

	fprintf( write_, "hello %s %s %d\n", local_host, our_username, NPP_VERSION );
	fflush( write_ );

	if ( ! (s = get_word_from_peer( read_ )) )
		return to_log( "peer %s blew us off", remote_host );

	/* Disgusting hack for SunOS systems! */
	if ( ! strcmp( s, "ld.so:" ) )
		{ /* Eat up something like:
	ld.so: warning: /usr/lib/libc.so.1.8 has older revision than expected 9
		   */
		int i;
		for ( i = 0; i < 8; ++i )
			if ( ! (s = get_word_from_peer( read_ )) )
				return to_log(
"peer %s protocol error, hello expected, problem eating ld.so chud\n",
					remote_host );

		if ( ! (s = get_word_from_peer( read_ )) )
			return to_log( "peer %s blew us off", remote_host );
		}

	if ( strcmp( s, "hello" ) )
		{ /* Send error message to stderr */
		fprintf( stderr,
			"peer %s protocol error, hello expected, got:\n%s ",
			remote_host, s );
		while ( (s = get_word_from_peer( read_ )) )
			fprintf( stderr, "%s ", s );
		fprintf( stderr, "\n" );
		return 0;
		}

	if ( ! (s = get_word_from_peer( read_ )) ||
	     (version = atoi( s )) <= 0 )
		return to_log( "peer %s protocol error, bad version: got \"%s\"",
				remote_host, s ? s : "<EOF>" );

	if ( ! (s = get_word_from_peer( read_ )) ||
	     strcmp( s, "challenge" ) )
		return to_log( "peer %s protocol error, hello expected, got \"%s\"",
				remote_host, s ? s : "<EOF>" );

	if ( ! (challenge = read_encoded_binary( read_, &challenge_len )) )
		return to_log( "bad challenge from peer %s - %s", remote_host, errmsg );

	fprintf( write_, "answer " );
	if ( ! answer_challenge( KEYS_DIR, local_host, our_username, write_,
					challenge, challenge_len ) )
		return to_log( "answering peer %s's challenge failed - %s",
				remote_host, errmsg );
	my_free( challenge );
	fflush( write_ );

	if ( ! (s = get_word_from_peer( read_ )) ||
	     strcmp( s, "accepted" ) )
		return to_log( "answer not accepted by peer %s", remote_host );

	return 1;
	}

/* Returns true if the peer successfully authenticates itself, false
 * otherwise.
 */
static int authenticate_peer( FILE *npd_in, FILE *npd_out, struct sockaddr_in *sin )
	{
	unsigned char *answer, *peer_answer;
	char peer_hostname[256];
	char peer_username[9];
	long peer_addr;
	u_long ip_addr;
	int answer_len, peer_answer_len;
	int version;
	struct hostent *h;
	const char *s;
	int uid;
	int i;

	/* First, figure out who we're talking to. */
	peer_addr = sin->sin_addr.s_addr;
	if ( ! (h = gethostbyaddr( (char *) &peer_addr,
					sizeof peer_addr, AF_INET )) )
		{
		to_log( "could not get peer 0x%X's address", peer_addr );
		return 0;
		}

	ip_addr = ntohl( sin->sin_addr.s_addr );

	to_log( "npd peer connection from %s (%d.%d.%d.%d.%d)", h->h_name,
		(ip_addr >> 24) & 0xff, (ip_addr >> 16) & 0xff,
		(ip_addr >> 8) & 0xff, (ip_addr >> 0) & 0xff,
		sin->sin_port );

	/* Search the list of addresses associated with this host to see
	 * if we're being spoofed.
	 */
	for ( i = 0; h->h_addr_list[i]; ++i )
		if ( memcmp( (char *) &peer_addr, (char *) h->h_addr_list[i],
				h->h_length ) == 0 )
			break;

	if ( ! h->h_addr_list[i] )
		return to_log( "peer appears to be spoofing" );

	s = get_word_from_peer( npd_in );
	if ( ! s || strcmp( s, "hello" ) )
		return to_log( "peer protocol error, hello expected, got \"%s\"",
				s ? s : "<EOF>" );
	if ( ! (s = get_word_from_peer( npd_in )) )
		return to_log( "peer protocol error, hostname expected, got \"%s\"",
				s ? s : "<EOF>" );
	if ( strlen( s ) >= sizeof peer_hostname )
		return to_log( "ridiculously long hostname: \"%s\"", s );
	strcpy( peer_hostname, s );

	if ( ! (s = get_word_from_peer( npd_in )) )
		return to_log( "peer protocol error, username expected, got \"%s\"",
				s ? s : "<EOF>" );
	if ( strlen( s ) >= sizeof peer_username )
		return to_log( "ridiculously long username: \"%s\"", s );
	strcpy( peer_username, s );

	/* Verify the peer's alleged host name. */
	if ( ! (h = gethostbyname( peer_hostname )) )
		return to_log( "can't lookup peer's alleged name: %s", peer_hostname );

	/* Again, search address list to see if we're being spoofed. */
	for ( i = 0; h->h_addr_list[i]; ++i )
		if ( memcmp( (char *) &peer_addr, (char *) h->h_addr_list[i],
				h->h_length ) == 0 )
			break;

	if ( ! h->h_addr_list[i] )
		return to_log( "peer appears to be spoofing" );

	/* Verify the peer's alleged user name. */
	if ( ! (uid = get_userid( peer_username )) )
		return to_log( "peer sent bogus user name" );

	if ( ! (s = get_word_from_peer( npd_in )) || (version = atoi( s )) < 1 )
		return to_log( "peer protocol error, version expected, got \"%s\"",
				s ? s : "<EOF>" );

	fprintf( npd_out, "hello %d challenge\n", NPP_VERSION );
	answer = compose_challenge( KEYS_DIR, peer_hostname, peer_username, npd_out, &answer_len );
	if ( ! answer )
		return to_log( "couldn't compose challenge - %s", errmsg );
	fflush( npd_out );

	if ( ! (s = get_word_from_peer( npd_in )) || strcmp( s, "answer" ) )
		return to_log( "peer protocol error, answer expected, got \"%s\"",
				s ? s : "<EOF>" );

	if ( ! (peer_answer = read_encoded_binary( npd_in, &peer_answer_len )) )
		return to_log( "peer protocol error, couldn't get answer - %s",
				errmsg );

	if ( peer_answer_len != answer_len )
		return to_log( "peer challenge answer wrong length, %d != %d",
				peer_answer_len, answer_len );

	if ( ! byte_arrays_equal( peer_answer, answer, answer_len ) )
		return to_log( "peer answer incorrect" );

	fprintf( npd_out, "accepted\n" );
	fflush( npd_out );

	to_log( "peer %s authenticated", peer_hostname );

	return 1;
	}

int authenticate_client(int sock)
	{
	FILE *in, *out;
	struct sockaddr_in sin;
	int len = sizeof( sin );
	int result = 0;

	if (! (in = fdopen(dup(sock),"r")))
		{
		return to_log( "could not create FILE* (in)" );
		}
			    
	if (! (out = fdopen(dup(sock),"w")))
		{
		fclose(in);
		return to_log( "could not create FILE* (out)" );
		}

	if ( getpeername( sock, (struct sockaddr*) &sin, &len ) < 0 )
		{
		fclose(in);
		fclose(out);
		return to_log( "could not get peer information (fd: 0x%X)", sock );
		}

	result = authenticate_peer(in, out, &sin);
	fclose(in);
	fclose(out);
	return result;
	}

int authenticate_to_server(int sock)
	{
	FILE *in, *out;
	struct sockaddr_in sin;
	int len = sizeof( sin );
	long addr;
	struct hostent *h;
	char local_host[256];
	char remote_host[256];
	int result = 0;

	if (! (in = fdopen(dup(sock),"r")))
		{
		return to_log( "could not create FILE* (in)" );
		}
			    
	if (! (out = fdopen(dup(sock),"w")))
		{
		fclose(in);
		return to_log( "could not create FILE* (out)" );
		}

	if ( getsockname( sock, (struct sockaddr*) &sin, &len ) < 0 )
		{
		fclose(in);
		fclose(out);
		return to_log( "could not get host information (0x%X)", sock );
		}

	addr = sin.sin_addr.s_addr;
	if ( ! (h = gethostbyaddr( (char *) &addr,
					sizeof addr, AF_INET )) )
		{
		fclose(in);
		fclose(out);
		return to_log( "could not get our information 0x%X's address", addr );
		}

	if ( strlen(h->h_name) > sizeof(local_host) )
		{
		fclose(in);
		fclose(out);
		return to_log( "ridiculously long hostname: \"%s\"", h->h_name );
		}

	strcpy(local_host,h->h_name);

	if ( getpeername( sock, (struct sockaddr*) &sin, &len ) < 0 )
		{
		fclose(in);
		fclose(out);
		return to_log( "could not get peer information (fd: 0x%X)", sock );
		}

	addr = sin.sin_addr.s_addr;
	if ( ! (h = gethostbyaddr( (char *) &addr,
					sizeof addr, AF_INET )) )
		{
		fclose(in);
		fclose(out);
		return to_log( "could not get our information 0x%X's address", addr );
		}

	if ( strlen(h->h_name) > sizeof(remote_host) )
		{
		fclose(in);
		fclose(out);
		return to_log( "ridiculously long hostname: \"%s\"", h->h_name );
		}

	strcpy(remote_host,h->h_name);

	result = authenticate_to_peer(in, out, local_host, remote_host);

	fclose(in);
	fclose(out);
	return result;
	}
