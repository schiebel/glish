// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "Glish/Client.h"
#include "Daemon.h"
#include "Npd/npd.h"
#include "Reporter.h"
#include "Channel.h"
#include "Socket.h"
#include "LocalExec.h"
#include "ports.h"

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#define DAEMON_NAME "glishd2"

//
//  parameters:		-id <NAME> -host <HOSTNAME> -port <PORTNUM>
//
//  glishd no longer does authentication when started by non-root users.
//
Channel *start_remote_daemon( const char *host )
	{
	char *argv[10];
	char command_line[2048];
	AcceptSocket connection_socket( 0, INTERPRETER_DEFAULT_PORT );
	mark_close_on_exec( connection_socket.FD() );

	sprintf(command_line, "%s %s -n %s -id glish-daemon -host %s -port %d -+- &",
		RSH, host, DAEMON_NAME, local_host_name(), connection_socket.Port() );

	message->Report( "activating Glish daemon on ", host, " ..." );
	system( command_line );

	message->Report( "waiting for daemon ..." );
	int new_conn = accept_connection( connection_socket.FD() );

	return new Channel( new_conn, new_conn );
	}

//
//  parameters:		-id <NAME> -pipes <READFD> <WRITEFD>
//
//  glishd no longer does authentication when started by non-root users.
//
//  This has NOT been tested, and probably isn't presently used. A glishd
//  started by a user can no longer be used to hold "shared" (AKA multi-threaded)
//  client information because they are no longer shared by more than one user.
//  The reason for this is security/ownership sorts of problems.
//
Channel *start_local_daemon( )
	{
	int argc = 0;
	char *argv[10], rpipe[15], wpipe[15];
	int read_pipe[2], write_pipe[2];
	char *exec_name = which_executable( DAEMON_NAME );

	if ( ! exec_name )
		return 0;

	if ( pipe( read_pipe ) < 0 || pipe( write_pipe ) < 0 )
		{
		perror( "start_local_daemon(): problem creating pipe" );
		return 0;
		}

	mark_close_on_exec( read_pipe[0] );
	mark_close_on_exec( write_pipe[1] );

	sprintf(rpipe, "%d", write_pipe[0]);
	sprintf(wpipe, "%d", read_pipe[1]);

	argv[argc++] = exec_name;
	argv[argc++] = "-id";
	argv[argc++] = "glish-daemon";
	argv[argc++] = "-pipes";
	argv[argc++] = rpipe;
	argv[argc++] = wpipe;
	argv[argc++] = "-+-";
	argv[argc++] = 0;

	new LocalExec( argv[0], (const char**) argv );

	close( read_pipe[1] );
	close( write_pipe[0] );

	return new Channel( write_pipe[0], read_pipe[1] );
	}

RemoteDaemon* connect_to_daemon( const char* host, int &err )
	{
	err = 0;

	if ( ! create_keyfile() )
		{
		err = 1;
		error->Report("Daemon creation failed, couldn't create key file.");
		return 0;
	}

	int daemon_socket = get_tcp_socket();

	if ( remote_connection( daemon_socket, host, DAEMON_PORT ) )
		{ // Connected.
		mark_close_on_exec( daemon_socket );

		Channel* daemon_channel =
			new Channel( daemon_socket, daemon_socket );

		RemoteDaemon* r = new RemoteDaemon( host, daemon_channel );

		if ( ! authenticate_to_server( daemon_socket ) )
			{
			delete daemon_channel;
			close( daemon_socket );
			err = 1;
			error->Report("Daemon creation failed, not authorized.");
			return 0;
			}

		// Read and discard daemon's "establish" event.
		GlishEvent* e = recv_event( daemon_channel->Source() );
		Unref( e );

		// Tell the daemon which directory we want to work out of.
		char work_dir[MAXPATHLEN];

		if ( ! getcwd( work_dir, sizeof( work_dir ) ) )
			fatal->Report( "problems getting cwd:", work_dir );

		Value work_dir_value( work_dir );
		send_event( daemon_channel->Sink(), "setwd",
				&work_dir_value );

		return r;
		}

	else
		{
		close( daemon_socket );
		return 0;
		}
	}

void RemoteDaemon::UpdatePath( const Value *path )
	{
	send_event( chan->Sink(), "setpath", path );
	}
