// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>

#include <string.h>

#include "Glish/Client.h"
#include "Glish/List.h"
#include "Reporter.h"
#include "Socket.h"
#include "LocalExec.h"
#include "Npd/npd.h"
#include "ports.h"


#undef DAEMON_PORT
#define DAEMON_PORT 4077

inline int streq( const char* a, const char* b ) { return ! strcmp( a, b ); }
const char* get_prog_name( const char* full_path );
void glishd_sighup();

extern "C" {
	extern char* sys_errlist[];
	extern int chdir( const char* path );
}

//  --   --   --   --   --   --   --   --   --   --   --   --   --   --   --
//		SIGHUP can be used to shut down glishd  (DOESN'T WORK YET)
//  --   --   --   --   --   --   --   --   --   --   --   --   --   --   --
//
//  In this file, there are two daemons, "dServer" and "dUser", and there are
//  two types of clients, "cInterp" and "cClient". Each of these represents
//  processes.
//
//  "dServer" is only started by root. It listens to the published port,
//  authenticates those who connect, and starts "dUser" for each unique
//  user (in the unix sense) who connects.
//
//  "dUser" handles requests from the authenticated user, and chats with
//  "dServer", its parent. "dUser" deals with the clients, "cInterp" and
//  "cClient", maintaining lists of both.
//
//  "cInterp" represents the glish command interpreters who use "glishd" to
//  start and control clients. "cClient" represents the clients which are
//  shared (AKA multi-threaded). These clients are either share between
//  multiple glish intrepreters run by an individual user, or shared by all
//  users.
//  --   --   --   --   --   --   --   --   --   --   --   --   --   --   --

//
//  Base class for daemons
//
class GlishDaemon {
    public:
	friend void glishd_sighup();
	GlishDaemon( int &argc, char **&argv );
	GlishDaemon( );
	virtual ~GlishDaemon();
	virtual void loop() = 0;
	int IsValid() const { return valid; }
	virtual int AddInputMask( fd_set* mask );
	void InitInterruptPipe( );
    protected:
	void close_on_fork( int fd );
	int fork();
	void Invalidate();
	static char *hostname;
	static char *name;
	int interrupt_pipe[2];
	int valid;
	static List(int) *close_fds;
};

List(int) *GlishDaemon::close_fds = 0;
static GlishDaemon *current_daemon;
class dUser;
declare(PDict,LocalExec);

//
//  These are the intrepreter connections which talk only
//  to 'dUser'.
//
class cInterp {
    public:
	cInterp( Client *c ) : interpreter( c ), work_dir(0) {  }
	~cInterp();

	// Read and act on the next interpreter request.  Returns 0 to
	// indicate that the interpreter exited, non-zero otherwise.
	//
	// ~mask" corresponds to a select() mask previously constructed
	// using Interpreter()->AddInputMask().
	//
	// Upon return, "internal_event" will be non-zero if the received
	// event was directed to glishd itself; otherwise, "internal_event"
	// will be set to zero upon return.
	int NextRequest( GlishEvent* e, dUser *hub, GlishEvent*& internal_event );

	Client* Interpreter()	{ return interpreter; }

	int AddInputMask( fd_set* mask )
		{ return interpreter->AddInputMask(mask); }
	int HasClientInput( fd_set* mask )
		{ return interpreter->HasClientInput(mask); }
	GlishEvent* NextEvent( fd_set* mask )
		{ return interpreter->NextEvent( mask ); }

    protected:

	void SetWD( Value* wd );
	void PingClient( Value* client_id );
	void CreateClient( Value* argv, dUser *hub );
	void ClientRunning( Value* client, dUser *hub );
	void ShellCommand( Value* cmd );
	void KillClient( Value* client_id );
	void Probe( Value* probe_val );

	void ChangeDir();	// Change to our working-directory.

	Client* interpreter;	// Client used for connection to interpreter.
	char* work_dir;		// working-directory for this interpreter.

	// Whether we've generated an error message for a bad work_dir.
	int did_wd_msg;

	// Clients created on behalf of interpreter.
	PDict(LocalExec) clients;
};

//
//  These are the "shared" (AKA multi-threaded) clients. These
//  can be managed by either 'dServer' (clients shared with
//  everyone [GROUP or OTHER]) or 'dUser' (clients shared with
//  multiple interpreters running for a single user [USER].
//
class cClient {
    public:
	cClient( Client *c ) : client( c ) {  }
	~cClient() { if ( client ) delete client; }
	GlishEvent* NextEvent( fd_set* mask )
		{ return client->NextEvent( mask ); }
	int HasClientInput( fd_set* mask )
		{ return client->HasClientInput(mask); }
	int AddInputMask( fd_set* mask )
		{ return client->AddInputMask(mask); }
	void PostEvent( const char* event_name, const Value* event_value )
		{ client->PostEvent( event_name, event_value ); }
    protected:
	Client *client;
};

declare(PList,Client);
declare(PList,cInterp);
declare(PDict,cClient);

//
//  Daemon which operates on an individual user's behalf.
//  There is one of these daemons running for each glish
//  user on a given machine.
//
class dUser : public GlishDaemon {
    public:
	dUser( int &argc, char **&argv ) : GlishDaemon(argc, argv), count(0),
						fd_pipe(0), master(0) { valid = 0; }
	dUser( int sock, const char *user, const char *host );
	~dUser();

	cClient *LookupClient( const char *s ) { return clients[s]; }

	int AddInputMask( fd_set* mask );
	int HasClientInput( fd_set* mask );
	GlishEvent* NextEvent( fd_set* mask );

	int SendFd( int fd ) { return ! fd_pipe ? -1 : send_fd( fd_pipe, fd ); }
	int RecvFd( ) { return ! fd_pipe ? -1 : recv_fd( fd_pipe ); }

	// process interpreter connection, via dServer
	void ProcessConnect();

	// process events from master
	void ProcessMaster( fd_set * );

	// process initial commands from clients (persistence
	// requests only) or interpreters
	void ProcessTransitions( fd_set * );

	// process events from interpreters
	void ProcessInterps( fd_set * );

	// process items from shared (AKA multi-threaded) clients
	void ProcessClients( fd_set * );

	// process internal requests for the daemon
	void ProcessInternalReq( GlishEvent * );

	void loop();

    protected:
	char *new_name();
	char *id;
	PList(Client) transition;
	PDict(cClient) clients;
	PList(cInterp) interps;
  
	Client *master;
	int count;
	int fd_pipe;
	
};

declare(PDict,dUser);

//
//  Master daemon. There is one master daemon per machine,
//  and it must be started by root.
//
class dServer : public GlishDaemon {
    public:
	dServer( int &argc, char **&argv );
	~dServer();

	// select on our fds
	void loop();

	// process interpreter connection, on published port
	void ProcessConnect();

	// process events from our minions
	void ProcessUsers( fd_set* mask );

	int AddInputMask( fd_set* mask );

    protected:
	char *id;
	AcceptSocket accept_sock;
	PDict(dUser) users;
};

char *GlishDaemon::hostname = 0;
char *GlishDaemon::name = 0;

main( int argc, char **argv )
	{
	GlishDaemon *dmon;
	if ( getuid() == 0 )
		dmon = new dServer( argc, argv );
	else
		dmon = new dUser( argc, argv );

	dmon->loop( );

	delete dmon;

	return 0;
	}

void GlishDaemon::InitInterruptPipe( )
	{
	if ( pipe( interrupt_pipe ) < 0 )
		{
		syslog( LOG_ERR, "problem creating interrupt pipe" );
		interrupt_pipe[0] = interrupt_pipe[1] = 0;
		}
	else
		{
		close_on_fork( interrupt_pipe[0] );
		close_on_fork( interrupt_pipe[1] );
		}
	}

GlishDaemon::GlishDaemon( ) : valid(1)
	{
	interrupt_pipe[0] = interrupt_pipe[1] = 0;
	if ( close_fds == 0 ) close_fds = new List(int);
	}

GlishDaemon::GlishDaemon( int &argc, char **&argv ) : valid(0)
	{
	if ( close_fds == 0 ) close_fds = new List(int);

	// fork() to:
	//     o  allows the parent to exit signaling to any shell
	//        that it's process has finished
	//     o  get a new processed id so we're not a process
	//        group leader
	int pid;
	if ( (pid = fork()) < 0 )
		{
		syslog( LOG_ERR, "problem forking server daemon" );
		return;
		}
	else if ( pid != 0 )		// parent
		exit(0);

	// setsid() to:
	//     o  become session leader of a new session
	//     o  become process group leader
	//     o  have no controlling terminal
	setsid();

	// change to the root directory, by default, to avoid keeping
	// directories mounted unnecessarily
	chdir("/");

	// clear umask to prevent unexpected permission changes
	umask(0);

	// add bullet-proofing for common signals.
	install_signal_handler( SIGINT, (signal_handler) SIG_IGN );
	install_signal_handler( SIGTERM, (signal_handler) SIG_IGN );
	install_signal_handler( SIGPIPE, (signal_handler) SIG_IGN );
	// SIGHUP can be used to shutdown the daemon. "current_daemon"
	// is the hook through which shutdown is accomplished.
	install_signal_handler( SIGHUP, (signal_handler) glishd_sighup );
	InitInterruptPipe();
	current_daemon = this;

	// store away our name
	if ( ! name ) name = strdup(argv[0]);

	// signals to children that all is OK
	valid = 1;
	}

int GlishDaemon::AddInputMask( fd_set* mask )
	{
	if ( *interrupt_pipe > 0 && ! FD_ISSET( interrupt_pipe[0], mask ) )
		{
		FD_SET( interrupt_pipe[0], mask );
		return 1;
		}

	return 0;
	}

void GlishDaemon::close_on_fork( int fd )
	{
	mark_close_on_exec( fd );
	close_fds->append(fd);
	}

int GlishDaemon::fork()
	{
	int pid = ::fork();

	if ( pid == 0 )		// child
		{
		for ( int len = close_fds->length(); len > 0; --len )
			{
			close( close_fds->remove_nth( len - 1 ) );
			}
		}

	return pid;
	}

void GlishDaemon::Invalidate()
	{
	valid = 0;
	if ( *interrupt_pipe )
		close(interrupt_pipe[1]);
	interrupt_pipe[0] = interrupt_pipe[1] = 0;
	}

GlishDaemon::~GlishDaemon()
	{
	if ( close_fds ) delete close_fds;
	}

dUser::dUser( int sock, const char *user, const char *host ) : count(0), master(0), fd_pipe(0)
	{
	// it is assumed that GlishDaemon will set valid if all is OK
	if ( ! valid ) return;

	if ( ! hostname ) hostname = strdup(local_host_name());
	id = (char*) alloc_memory( strlen(hostname) + strlen(name) + strlen(user) + strlen(host) + 35 );
	sprintf( id, "%s @ %s [%d] (%s@%s)", name, hostname, int( getpid() ), user, host );

	int read_pipe[2], write_pipe[2], fd_pipe_[2];

	//	parent:	writes to write_pipe[1]
	//		reads from read_pipe[0]
	//		writes to fd_pipe_[1]
	//	child:	reads from write_pipe[0]
	//		writes to read_pipe[1]
	//		reads from fd_pipe_[0]
	if ( pipe( read_pipe ) < 0 || pipe( write_pipe ) < 0 || stream_pipe(fd_pipe_) < 0 )
		{
		valid = 0;
		syslog( LOG_ERR, "problem creating pipe for user daemon" );
		return;
		}

	// we now fork, the child will be owned by "user", and it will be
	// responsible for forking, pinging, etc. for the clients "user"
	// creates on this machine. this new "glishd" will create with
	// the "root" "glishd" via the pipes.
	int pid;
	if ( (pid = fork()) < 0 )
		{
		valid = 0;
		syslog( LOG_ERR, "problem forking user daemon" );
		return;
		}
	else if ( pid == 0 )		// child
		{
		setuid(get_userid( user ));
		setgid(get_user_group( user ));

		close( read_pipe[0] );
		close( write_pipe[1] );
		close( fd_pipe_[1] );

		InitInterruptPipe();

		master = new Client( write_pipe[0], read_pipe[1], new_name() );
		transition.append( new Client( sock, sock, new_name() ) );
		fd_pipe = fd_pipe_[0];

		loop();

		exit (0);
		}
	else				// parent
		{
		close( read_pipe[1] );
		close( write_pipe[0] );
		close( fd_pipe_[0] );

		transition.append( new Client( read_pipe[0], write_pipe[1], id ) );
		fd_pipe = fd_pipe_[1];
		}
	}

int dUser::AddInputMask( fd_set* mask )
	{
	int count = GlishDaemon::AddInputMask( mask );
	loop_over_list( transition, i)
		count += transition[i]->AddInputMask( mask );

	loop_over_list( interps, j)
		count += interps[j]->AddInputMask( mask );

	const char *key = 0;
	cClient *client = 0;
	IterCookie* c = clients.InitForIteration();
	while ( client = clients.NextEntry( key, c ) )
		count += client->AddInputMask( mask );

	if ( master )
		{
		count += master->AddInputMask( mask );
		if ( fd_pipe > 0 && ! FD_ISSET( fd_pipe, mask ) )
			{
			FD_SET( fd_pipe, mask );
			++count;
			}
		}

	return count;
	}

int dUser::HasClientInput( fd_set* mask )
	{
	loop_over_list( transition, i)
		if ( transition[i]->HasClientInput( mask ) )
			return 1;

	loop_over_list( interps, j)
		if ( interps[j]->HasClientInput( mask ) )
			return 1;

	const char *key = 0;
	cClient *client = 0;
	IterCookie* c = clients.InitForIteration();
	while ( client = clients.NextEntry( key, c ) )
		if ( client->HasClientInput( mask ) )
			return 1;

	return count;
	}

GlishEvent *dUser::NextEvent( fd_set* mask )
	{
	loop_over_list( transition, i)
		if ( transition[i]->HasClientInput( mask ) )
			return transition[i]->NextEvent( mask );

	loop_over_list( interps, j)
		if ( interps[j]->HasClientInput( mask ) )
			return interps[j]->NextEvent( mask );

	const char *key = 0;
	cClient *client = 0;
	IterCookie* c = clients.InitForIteration();
	while ( client = clients.NextEntry( key, c ) )
		if ( client->HasClientInput( mask ) )
			return client->NextEvent( mask );

	return 0;
	}

void dUser::loop( )
	{
	fd_set input_fds;
	fd_set* mask = &input_fds;

	// must do this for SIGHUP to work properly
	current_daemon = this;

	Client *c;
	while ( valid )
		{
		FD_ZERO( mask );
		AddInputMask( mask );

		while ( select( FD_SETSIZE, (SELECT_MASK_TYPE *) mask, 0, 0, 0 ) < 0 )
			{
			if ( errno != EINTR )
				{
				syslog( LOG_ERR,"error during select(), exiting" );
				exit( 1 );
				}
			}

		if ( ! valid ) continue;

		if ( master && master->HasClientInput( mask ) )
			ProcessMaster( mask );

		// Accept requests from our intrepreters
		ProcessInterps( mask );

		// See if anything is up with our shared clients
		ProcessClients( mask );

		// Now look for any new interpreters contacting us, via dServer.
		if ( FD_ISSET( fd_pipe, mask ) )
			ProcessConnect();

		ProcessTransitions( mask );
		}
	}

void dUser::ProcessTransitions( fd_set *mask )
	{
	Client *c = 0;
	GlishEvent *internal = 0;

	for ( int i=0; i < transition.length(); ++i )
		{
		c = transition[i];
		if ( c->HasClientInput( mask ) )
			{
			GlishEvent *e = c->NextEvent( mask );

			// transient exiting
			if ( ! e )
				{
				delete transition.remove_nth( i-- );
				continue;
				}

			// perisitent client registering itself
			if ( ! strcmp( e->name, "*register-persistent*" ) )
				{
				char *name = e->value->StringVal();
				clients.Insert( name, new cClient(transition.remove_nth( i-- )) );
				continue;
				}

			//      ???
			if ( ! strcmp( e->name, "established" ) )
				continue;

			// process interpreter request
			cInterp *itp = new cInterp( transition.remove_nth(i--) );
			interps.append( itp );
			if ( ! itp->NextRequest( e, this, internal ) )
				{
				delete interps.remove_nth( interps.length() );
				continue;
				}

			if ( internal )
				ProcessInternalReq( internal );
			}
		}
	}

void dUser::ProcessInterps( fd_set *mask )
	{
	Client *c = 0;
	GlishEvent *e = 0;

	for ( int i = 0; i < interps.length(); ++i )
		{
		GlishEvent *internal = 0;
		cInterp *itp = interps[i];
		e = itp->Interpreter()->NextEvent( mask );
		itp->NextRequest( e, this, internal );

		if ( ! e )		// remove interpreter
			{
			delete interps.remove_nth( i-- );
			continue;
			}

		if ( internal )
			ProcessInternalReq( internal );
		}
	}


void dUser::ProcessClients( fd_set *mask )
	{
	const char *key = 0;
	cClient *client = 0;
	IterCookie* c = clients.InitForIteration();

	while ( client = clients.NextEntry( key, c ) )
		if ( client->HasClientInput( mask ) )
			{
			GlishEvent *e = client->NextEvent( mask );

			if ( ! e )	// "shared" client exited
				{
				// free key
				free_memory( clients.Remove( key ) );
				delete client;
				}
			else
				{
				// ignore non-null events for now
				syslog( LOG_ERR, "received non-null event from \"%s\", ignoring", key );
				}
			}
	}


void dUser::ProcessConnect( )
	{
	int fd = RecvFd();

	if ( fd > 0 )
		transition.append( new Client( fd, fd, new_name() ) );
	else
		syslog( LOG_ERR, "attempted connect failed (or master exited), ignoring" );

	}

void dUser::ProcessMaster( fd_set *mask )
	{
	GlishEvent *e = master->NextEvent( mask );

	if ( ! e )
		{
		syslog( LOG_ERR, "problems, master exited" );
		delete master;
		close( fd_pipe );
		fd_pipe = 0;
		master = 0;
		}
	else
		{
		// ignore non-null events for now
		}
	}


void dUser::ProcessInternalReq( GlishEvent* event )
	{
	const char* name = event->name;

	if ( streq( name, "*terminate-daemon*" ) )
		exit( 0 );
	else
		syslog( LOG_ERR,"bad internal event, \"%s\"", name );
	}


dUser::~dUser( )
	{
	if ( master )
		delete master;

	if ( fd_pipe )
		close( fd_pipe );

	loop_over_list( transition, i )
		delete transition[i];

	loop_over_list( interps, j )
		delete interps[j];

	const char *key = 0;
	cClient *client = 0;
	IterCookie* c = clients.InitForIteration();
	while ( client = clients.NextEntry( key, c ) )
		{
		// free key
		free_memory( clients.Remove( key ) );
		delete client;
		}

	if ( id ) free_memory( id );
	}

char *dUser::new_name( )
	{
	char *name = (char*) alloc_memory(strlen(id)+20);
	sprintf(name, "%s #%d", id, count++);
	return name;
	}

dServer::dServer( int &argc, char **&argv ) : GlishDaemon( argc, argv ), id(0),
						        accept_sock( 0, DAEMON_PORT, 0 )
	{
	// it is assumed that GlishDaemon will set valid if all is OK
	if ( ! valid ) return;

	if ( ! hostname ) hostname = strdup(local_host_name());
	id = (char*) alloc_memory( strlen(hostname) + strlen(name) + 30 );
	sprintf( id, "%s @ %s [%d]", name, hostname, int( getpid() ) );

	// setup syslog facility
	openlog(id,LOG_CONS,LOG_DAEMON);

	if ( accept_sock.Port() == 0 )		// didn't get it.
		{
		syslog( LOG_ERR, "daemon apparently already running, exiting" );
		exit( 1 );
		}

	// don't let our children inherit the accept socket fd; we want
	// it to go away when we do.
	mark_close_on_exec( accept_sock.FD() );

	// init for npd and the md5 authentication code
	init_log(argv[0]);
	}

void dServer::loop()
	{
	fd_set input_fds;
	fd_set* mask = &input_fds;

	// must do this for SIGHUP to work properly
	current_daemon = this;

	while ( valid )
		{
		FD_ZERO( mask );
		AddInputMask( mask );

		while ( select( FD_SETSIZE, (SELECT_MASK_TYPE *) mask, 0, 0, 0 ) < 0 )
			{
			if ( errno != EINTR )
				{
				syslog( LOG_ERR,"error during select(), exiting" );
				exit( 1 );
				}
			}

		if ( ! valid ) continue;

		// check on our minions
		ProcessUsers( mask );

		// Now look for any new interpreters contacting us.
		if ( FD_ISSET( accept_sock.FD(), mask ) )
			ProcessConnect();

		}
	}

void dServer::ProcessConnect()
	{
	int s = accept_connection( accept_sock.FD() );

	if ( s < 0 )
		{
		syslog( LOG_ERR, "error when accepting connection, exiting" );
		exit( 1 );
		}

	// Don't let our children inherit this socket fd; if
	// they do, then when we exit our remote Glish-
	// interpreter peers won't see select() activity
	// and detect out exit.
	mark_close_on_exec( s );

	char **peer = 0;
	if ( peer = authenticate_client( s ) )
		{
		dUser *user = users[peer[0]];
		if ( user )
			{
			if ( user->SendFd( s ) < 0 )
				syslog( LOG_ERR, "sendfd failed, %m" );
			}
		else
			{
			dUser *user = new dUser( s, peer[0], peer[1] );
			if ( user && user->IsValid() )
				users.Insert( strdup(peer[0]), user );
			else
				delete user;
			}
		}

	close( s );
	}

void dServer::ProcessUsers( fd_set *mask)
	{
	const char* key = 0;
	dUser *user = 0;
	IterCookie* c = users.InitForIteration();

	while ( user = users.NextEntry( key, c ) )
		if ( user->HasClientInput( mask ) )
			{
			GlishEvent *e = user->NextEvent( mask );

			if ( ! e )	// dUser has exited
				{
				// At some point, we'll need to do "shared"
				// client cleanup here too.
				free_memory( users.Remove( key ) );
				delete user;
				}
			else
				{
				// ignore non-null events for now
				}
			}
	}

int dServer::AddInputMask( fd_set *mask )
	{
	int count = GlishDaemon::AddInputMask( mask );
	const char* key = 0;
	dUser *user = 0;
	IterCookie* c = users.InitForIteration();

	while ( user = users.NextEntry( key, c ) )
		count += user->AddInputMask( mask );

	if ( accept_sock.FD() > 0 && ! FD_ISSET( accept_sock.FD(), mask ) )
		{
		FD_SET( accept_sock.FD(), mask );
		++count;
		}
	}

dServer::~dServer()
	{
	closelog();

	const char *key = 0;
	dUser *user = 0;
	IterCookie* c = users.InitForIteration();
	while ( user = users.NextEntry( key, c ) )
		{
		// free key
		free_memory( users.Remove( key ) );
		delete user;
		}

	if ( id ) free_memory(id);
	}


cInterp::~cInterp( )
	{
	delete interpreter;

	const char *key = 0;
	LocalExec *client = 0;
	IterCookie* c = clients.InitForIteration();
	while ( client = clients.NextEntry( key, c ) )
		{
		// free key
		free_memory( clients.Remove( key ) );
		delete client;
		}

	free_memory( work_dir );
	}

int cInterp::NextRequest( GlishEvent* e, dUser *hub, GlishEvent*& internal_event )
	{
	internal_event = 0;

	if ( ! e )
		return 0;

	if ( streq( e->name, "setwd" ) )
		SetWD( e->value );

	else if ( streq( e->name, "ping" ) )
		PingClient( e->value );

	else if ( streq( e->name, "client" ) )
		CreateClient( e->value, hub );

	else if ( streq( e->name, "client-up" ) )
		ClientRunning( e->value, hub );

	else if ( streq( e->name, "shell" ) )
		ShellCommand( e->value );

	else if ( streq( e->name, "kill" ) )
		KillClient( e->value );

	else if ( streq( e->name, "probe" ) )
		Probe( e->value );

	else if ( e->name[0] == '*' )
		// Internal event for glishd itself.
		internal_event = e;

	else
		interpreter->Unrecognized();

	return 1;
	}

void cInterp::SetWD( Value* v )
	{
	free_memory( work_dir );
	did_wd_msg = 0;
	work_dir = v->StringVal();

	ChangeDir();	// try it out to see if it's okay
	}

void cInterp::PingClient( Value* client_id )
	{
	char* id = client_id->StringVal();
	LocalExec* client = clients[id];

	if ( ! client )
		{
		error->Report( "no such client ", id );
		syslog( LOG_ERR, "no such client, \"%s\"", id );
		}
	else
		client->Ping();

	free_memory( id );
	}

void cInterp::CreateClient( Value* argv, dUser *hub )
	{
	argv->Polymorph( TYPE_STRING );

	int argc = argv->Length();

	if ( argc <= 1 )
		{
		error->Report(
			"no arguments given for creating remote client" );
		return;
		}

	char* client_str = argv->StringVal(); // copy here for good reason
	char* name_str;

	if ( !strtok( client_str, " " ) ||
	     !strtok( NULL, " " ) ||
	     !strtok( NULL, " " ) ||
	     !(name_str = strtok( NULL, " " )) )
		{
		error->Report( "bad arguments for creating remote client" );
		return;
		}

	const char *prog_str = get_prog_name( name_str );

	cClient *persistent = hub->LookupClient(name_str);
	if ( persistent || (persistent = hub->LookupClient(prog_str)) )
		{
		persistent->PostEvent( "client", argv );
		free_memory( client_str );
		return;
		}

	free_memory( client_str );

	// First strip off the id.
	charptr* argv_ptr = argv->StringPtr();
	char* client_id = strdup( argv_ptr[0] );

	--argc;
	++argv_ptr;

	charptr* client_argv = (charptr*) alloc_memory(sizeof(charptr) * (argc + 1));

	for ( LOOPDECL i = 0; i < argc; ++i )
		client_argv[i] = argv_ptr[i];

	client_argv[argc] = 0;

	const char* exec_name = which_executable( client_argv[0] );
	if ( ! exec_name )
		error->Report( "no such executable ", client_argv[0] );

	else
		{
		ChangeDir();

		LocalExec* exec = new LocalExec( exec_name, client_argv );

		if ( exec->ExecError() )
			error->Report( "problem exec'ing client ", 
				client_argv[0], ": ", sys_errlist[errno] );

		clients.Insert( client_id, exec );
		}

	free_memory( client_argv );
	}

void cInterp::ClientRunning( Value* client, dUser *hub )
	{
	client->Polymorph( TYPE_STRING );

	int argc = client->Length();

	if ( argc < 1 )
		{
		error->Report(
			"no client name given" );
		interpreter->PostEvent( "client-up-reply", false_value );
		return;
		}

	charptr *strs = client->StringPtr();
	const char *name_str = strs[0];
	const char *prog_str = get_prog_name( name_str );

	cClient *persistent = hub->LookupClient(name_str);
	if ( persistent || (persistent = hub->LookupClient(prog_str)) )
		{
		Value true_value( glish_true );
		interpreter->PostEvent( "client-up-reply", &true_value );
		return;
		}
	
	interpreter->PostEvent( "client-up-reply", false_value );
	}


void cInterp::ShellCommand( Value* cmd )
	{
	char* command;
	if ( ! cmd->FieldVal( "command", command ) )
		error->Report( "remote glishd received bad shell command:",
				cmd );

	char* input;
	if ( ! cmd->FieldVal( "input", input ) )
		input = 0;

	ChangeDir();

	FILE* shell = popen_with_input( command, input );

	if ( ! shell )
		{
		Value F( glish_false );
		interpreter->PostEvent( "fail", &F );
		}
	else
		{
		// ### This is an awful lot of events; much simpler would
		// be to slurp up the entire output into an array of strings
		// and send that back.  Value::AssignElements makes this
		// easy since it will grow the array as needed.  The
		// entire result could then be stuffed into a record.

		Value T( glish_true );

		interpreter->PostEvent( "okay", &T );
		char line_buf[8192];

		while ( fgets( line_buf, sizeof( line_buf ), shell ) )
			interpreter->PostEvent( "shell_out", line_buf );

		interpreter->PostEvent( "done", &T );

		// Cfront, in its infinite bugginess, complains about
		// "int assigned to enum glish_bool" if we use
		//
		//	Value status_val( pclose( shell ) );
		//
		// here, so instead we create status_val dynamically.

		Value* status_val = new Value( pclose_with_input( shell ) );

		interpreter->PostEvent( "status", status_val );

		Unref( status_val );
		}

	free_memory( command );
	free_memory( input );
	}


void cInterp::KillClient( Value* client_id )
	{
	char* id = client_id->StringVal();
	LocalExec* client = clients[id];

	if ( ! client )
		error->Report( "no such client ", id );

	else
		{
		// free key
		free_memory(clients.Remove( id ));
		delete client;
		}

	free_memory( id );
	}


void cInterp::Probe( Value* /* probe_val */ )
	{
	interpreter->PostEvent( "probe-reply", false_value );
	}

void cInterp::ChangeDir()
	{
	if ( chdir( work_dir ) < 0 && ! did_wd_msg )
		{
		error->Report( "couldn't change to directory ", work_dir, ": ",
					sys_errlist[errno] );
		did_wd_msg = 1;
		}
	}

const char *get_prog_name(const char* full_path )
{
	if ( strchr( full_path, '/' ) == (char*) NULL )
		return full_path;

	int i = 0;
	while ( full_path[i] != '\0' ) ++i;
	while ( full_path[i] != '/'  ) --i;
	return full_path + (i+1) * sizeof( char );
}

void glishd_sighup()
	{
	if ( current_daemon )
		current_daemon->Invalidate();
	unblock_signal(SIGHUP);
	}
