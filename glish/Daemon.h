// $Header$

#ifndef daemon_h
#define daemon_h

class Channel;

// Possible states a daemon can be in.
typedef enum
	{
	DAEMON_OK,	// all is okay
	DAEMON_REPLY_PENDING,	// we're waiting for reply to last probe
	DAEMON_LOST	// we've lost connectivity
	} daemon_states;

// A RemoteDaemon keeps track of a glishd running on a remote host.
// This includes the Channel used to communicate with the daemon and
// modifiable state indicating whether we're currently waiting for
// a probe response from the daemon.
class RemoteDaemon {
public:
	RemoteDaemon( const char* daemon_host, Channel* channel )
		{
		host = daemon_host;
		chan = channel;
		SetState( DAEMON_OK );
		}

	const char* Host()		{ return host; }
	Channel* DaemonChannel()	{ return chan; }
	daemon_states State()		{ return state; }
	void SetState( daemon_states s )	{ state = s; }

protected:
	const char* host;
	Channel* chan;
	daemon_states state;
	};



int start_remote_daemon( const char *host );
int start_local_daemon( );
RemoteDaemon *connect_to_daemon(const char *host, int &err);

#endif
