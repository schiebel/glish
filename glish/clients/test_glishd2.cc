// $Id$
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <iostream.h>

#include "system.h"
#include "Socket.h"
#include "Npd/npd.h"
#include "ports.h"

#undef DAEMON_PORT
#define DAEMON_PORT 4077

main( int argc, char **argv )
	{
	if ( ! create_keyfile() )
		{
		cerr << "couldn't create key file" << endl;
		return 1;
		}

	int daemon_socket = get_tcp_socket();

	if ( remote_connection( daemon_socket, "muse.cv.nrao.edu", DAEMON_PORT ) )
		{ // Connected.
		mark_close_on_exec( daemon_socket );

		if ( ! authenticate_to_server( daemon_socket ) )
			{
			close( daemon_socket );
			cerr << "authentication failed" << endl;
			return 1;
			}
		}

	sleep(90);
	return 0;
	}
