#include "system.h"
#include "Glish/Client.h"
#include "Daemon.h"
#include "Npd/npd.h"
#include "Reporter.h"
#include "Channel.h"
#include "LocalExec.h"
#include "ports.h"
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

int start_remote_daemon( const char *host )
	{
	char daemon_cmd[1024];
	sprintf( daemon_cmd, "%s %s -n glishd &", RSH, host );
	system( daemon_cmd );
	return 1;
	}

int start_local_daemon( )
	{
	new LocalExec("/net/kochab/kochab_3/dschieb/glish/glish/glish/clients/sun4sol/glishd");
	return 1;
	}

RemoteDaemon* connect_to_daemon( const char* host, int &err )
	{
	err = 0;

#ifdef AUTHENTICATE
	if ( ! create_keyfile() )
		{
		err = 1;
		error->Report("Daemon creation failed, couldn't create key file.");
		return 0;
	}
#endif

	int daemon_socket = get_tcp_socket();

	if ( remote_connection( daemon_socket, host, DAEMON_PORT ) )
		{ // Connected.
		mark_close_on_exec( daemon_socket );

		Channel* daemon_channel =
			new Channel( daemon_socket, daemon_socket );

		RemoteDaemon* r = new RemoteDaemon( host, daemon_channel );

#ifdef AUTHENTICATE
		if ( ! authenticate_to_server( daemon_socket ) )
			{
			delete daemon_channel;
			close( daemon_socket );
			err = 1;
			error->Report("Daemon creation failed, not authorized.");
			return 0;
			}
#endif

		// Read and discard daemon's "establish" event.
		GlishEvent* e = recv_event( daemon_channel->ReadFD() );
		Unref( e );

		// Tell the daemon which directory we want to work out of.
		char work_dir[MAXPATHLEN];

		if ( ! getcwd( work_dir, sizeof( work_dir ) ) )
			fatal->Report( "problems getting cwd:", work_dir );

		Value work_dir_value( work_dir );
		send_event( daemon_channel->WriteFD(), "setwd",
				&work_dir_value );

		return r;
		}

	else
		{
		close( daemon_socket );
		return 0;
		}
	}

