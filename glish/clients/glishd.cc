// $Header$

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <osfcn.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "Glish/Dict.h"
#include "Npd/npd.h"
#include "Glish/Client.h"

#include "LocalExec.h"
#include "Channel.h"
#include "Reporter.h"
#include "Socket.h"
#include "ports.h"
#include "config.h"


extern "C" {
	extern char* sys_errlist[];
	extern int chdir( const char* path );
}


inline int streq( const char* a, const char* b )
	{
	return ! strcmp( a, b );
	}


declare(PDict,LocalExec);


// A GlishDaemon can be thought of as a "thread" inside the Glish
// daemon which works on behalf of a particular Glish interpreter.

class GlishDaemon {
public:
	GlishDaemon( Client* client );
	~GlishDaemon();

	// Read and act on the next interpreter request.  Returns 0 to
	// indicate that the interpreter exited, non-zero otherwise.
	//
	// ~mask" corresponds to a select() mask previously constructed
	// using Interpreter()->AddInputMask().
	//
	// Upon return, "internal_event" will be non-zero if the received
	// event was directed to glishd itself; otherwise, "internal_event"
	// will be set to zero upon return.
	int NextRequest( GlishEvent* e, GlishEvent*& internal_event );

	Client* Interpreter() const	{ return interpreter; }

	// Informs the GlishDaemon that the given client has terminated.
	// Returns non-zero if that client belonged to the GlishDaemon,
	// zero if it didn't.
	int ClientHasTerminated( int pid );

protected:
	void SetWD( Value* wd );
	void PingClient( Value* client_id );
	void CreateClient( Value* argv );
	void ClientRunning( Value* client );
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


declare(PList,GlishDaemon);
declare(PList,Client);


void internal_request( GlishEvent* event );
const char* get_prog_name( const char* full_path );
const char* prog_name;

PList(Client) mpcs;
PList(GlishDaemon) threads;
PList(Client) transition;
name_list mpc_names;
extern Client *CreateIncomingConnection( int, const char * );

int main( int /* argc */, char** argv )
	{
	static char prog_name_buf[1024];
	sprintf( prog_name_buf, "%s @ %s [%d]", argv[0],
			local_host_name(), int( getpid() ) );

	prog_name = prog_name_buf;

#ifdef AUTHENTICATE
	init_log(argv[0]);
#endif

	// First, try to grab the glishd daemon port; possession indicates
	// we're the sole daemon for this host.
	AcceptSocket a( 0, DAEMON_PORT, 0 );

	if ( a.Port() == 0 )
		{
		// Didn't get it.
		fprintf( stderr, "%s: daemon apparently already running\n",
			prog_name );
		exit( 1 );
		}

	// Add bullet-proofing for common signals.
	(void) signal( SIGINT, SIG_IGN );
	(void) signal( SIGTERM, SIG_IGN );
	(void) signal( SIGPIPE, SIG_IGN );

	// Don't let our children inherit the accept socket fd; we want
	// it to go away when we do.
	mark_close_on_exec( a.FD() );

//	int suspend = 1;
//	while ( suspend ) sleep( 1 );

	for ( ; ; )
		{
		fd_set input_fds;
		fd_set* mask = &input_fds;

		FD_ZERO( mask );
		FD_SET( a.FD(), mask );

		loop_over_list( threads, i )
			{
			threads[i]->Interpreter()->AddInputMask( mask );
			}

		for ( LOOPDECL i=0 ; i < transition.length() ; ++i )
			{
			transition[i]->AddInputMask( mask );
			}

		for ( LOOPDECL i=0 ; i < mpcs.length() ; ++i )
			{
			mpcs[i]->AddInputMask( mask );
			}

		while ( select( FD_SETSIZE, mask, 0, 0, 0 ) < 0 )
			{
			if ( errno != EINTR )
				{
				fprintf( stderr, "%s: ", prog_name );
				perror( "error during select()" );
				exit( 1 );
				}
			}

		Client* c;
		GlishDaemon* d;
		GlishEvent* e;

		// Look for any threads that have activity.
		for ( LOOPDECL i = 0; i < threads.length(); ++i )
			{
			d = threads[i];

			if ( d->Interpreter()->HasClientInput( mask ) )
				{
				GlishEvent* internal = 0;

				e = d->Interpreter()->NextEvent( mask );
				d->NextRequest( e, internal );

				if ( ! e )
					{ // Delete this thread.
					delete threads.remove_nth( i-- );
					continue;
					}

				if ( internal )
					internal_request( internal );
				}
			}

		for ( LOOPDECL i=0 ; i<mpcs.length() ; ++i )
			{
			c = mpcs[i];

			if ( c->HasClientInput( mask ) ) // read and drop
				{
				e = c->NextEvent( mask );
				if ( ! e ) // mpc has exited
					{
					delete mpcs.remove_nth( i );
					delete mpc_names.remove_nth( i-- );
					}
				else
					{
					// ignore non-null mpc events for now
					}
				}
			}

		// Now look for any new interpreters contacting us.
		if ( FD_ISSET( a.FD(), mask ) )
			{
			int s = accept_connection( a.FD() );

			if ( s < 0 )
				{
				fprintf( stderr, "%s: ", prog_name );
				perror( "error when accepting connection" );
				exit( 1 );
				}

			// Don't let our children inherit this socket fd; if
			// they do, then when we exit our remote Glish-
			// interpreter peers won't see select() activity
			// and detect out exit.
			mark_close_on_exec( s );

			if ( c = CreateIncomingConnection( s, prog_name ) )
				transition.append( c );
			}

		for ( LOOPDECL i=0 ; i<transition.length() ; ++i )
			{
			c = transition[i];

			if ( c->HasClientInput( mask ) )
				{
				e = c->NextEvent( mask );

				if ( e )
					{
					if ( streq( e->name, "*register-persistent*" ) )
						{ // transition to mpc

						char* mpc_name = e->value->StringVal();
						mpcs.append( transition.remove_nth( i-- ) );
						mpc_names.append( mpc_name );
						continue;
						}

					if ( streq( e->name, "established" ) )
						{
						continue;
						}

					d = new GlishDaemon( transition.remove_nth( i-- ) );
					threads.append( d );

					GlishEvent* internal;
					if ( ! d->NextRequest( e, internal ) )
						{
						delete threads.remove_nth( threads.length() );
						continue;
						}
					}
				else // transient exited
					{
					delete transition.remove_nth( i-- );
					}
				}
			}

		// Pick up any clients that have exited.
		int pid;

		while ( (pid = reap_terminated_process()) )
			{ // Remove terminated process from list of clients.
			loop_over_list( threads, i )
				if ( threads[i]->ClientHasTerminated( pid ) )
					break;	// no need to look further
			}
		}
	}


Client *CreateIncomingConnection( int sock, const char *name )
	{

#ifdef AUTHENTICATE
	if ( authenticate_client( sock ) )
		return new Client( sock, sock, name );
	else
		close( sock );

	return 0;
#else
	return new Client(sock, sock, name);
#endif
	}
	
void internal_request( GlishEvent* event )
	{
	const char* name = event->name;

	if ( streq( name, "*terminate-daemon*" ) )
		exit( 0 );

	else
		{
		fprintf( stderr, "%s: bad internal event \"%s\"\n",
				prog_name, name );
		exit( 1 );
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

GlishDaemon::GlishDaemon( Client* client )
	{
	interpreter = client;
	work_dir = 0;
	did_wd_msg = 0;
	}

GlishDaemon::~GlishDaemon()
	{
	delete interpreter;
	delete work_dir;
	}

int GlishDaemon::NextRequest( GlishEvent* e, GlishEvent*& internal_event )
	{
	internal_event = 0;

	if ( ! e )
		return 0;

	if ( streq( e->name, "setwd" ) )
		SetWD( e->value );

	else if ( streq( e->name, "ping" ) )
		PingClient( e->value );

	else if ( streq( e->name, "client" ) )
		CreateClient( e->value );

	else if ( streq( e->name, "client-up" ) )
		ClientRunning( e->value );

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

int GlishDaemon::ClientHasTerminated( int pid )
	{
	IterCookie* c = clients.InitForIteration();

	const char* key;
	LocalExec* exec;
	while ( (exec = clients.NextEntry( key, c )) )
		if ( exec->PID() == pid )
			{
			char* client_key = clients.Remove( key );
			delete client_key;
			return 1;
			}

	return 0;
	}

void GlishDaemon::SetWD( Value* v )
	{
	delete work_dir;
	did_wd_msg = 0;
	work_dir = v->StringVal();

	ChangeDir();	// try it out to see if it's okay
	}

void GlishDaemon::PingClient( Value* client_id )
	{
	char* id = client_id->StringVal();
	LocalExec* client = clients[id];

	if ( ! client )
		error->Report( "no such client ", id );
	else
		client->Ping();

	delete id;
	}


void GlishDaemon::CreateClient( Value* argv )
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

	loop_over_list( mpcs, i )
		{
		if ( streq( prog_str, mpc_names[i] ) ||
		     streq( name_str, mpc_names[i] ) )
			{
			mpcs[i]->PostEvent( "client", argv );
			delete client_str;
			return;
			}
		}
	delete client_str;

	// First strip off the id.
	charptr* argv_ptr = argv->StringPtr();
	char* client_id = strdup( argv_ptr[0] );

	--argc;
	++argv_ptr;

	charptr* client_argv = new charptr[argc + 1];

	for ( i = 0; i < argc; ++i )
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

	delete client_argv;
	}


void GlishDaemon::ClientRunning( Value* client )
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

	loop_over_list( mpcs, i )
		{
		if ( streq( prog_str, mpc_names[i] ) ||
		     streq( name_str, mpc_names[i] ) )
			{
			Value true_value( glish_true );
			interpreter->PostEvent( "client-up-reply", &true_value );
			return;
			}
		}
	
	interpreter->PostEvent( "client-up-reply", false_value );
	}

void GlishDaemon::ShellCommand( Value* cmd )
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

	delete command;
	delete input;
	}


void GlishDaemon::KillClient( Value* client_id )
	{
	char* id = client_id->StringVal();
	LocalExec* client = clients[id];

	if ( ! client )
		error->Report( "no such client ", id );

	else
		{
		delete client;
		char* client_key = clients.Remove( id );
		delete client_key;
		}

	delete id;
	}


void GlishDaemon::Probe( Value* /* probe_val */ )
	{
	interpreter->PostEvent( "probe-reply", false_value );
	}

void GlishDaemon::ChangeDir()
	{
	if ( chdir( work_dir ) < 0 && ! did_wd_msg )
		{
		error->Report( "couldn't change to directory ", work_dir, ": ",
					sys_errlist[errno] );
		did_wd_msg = 1;
		}
	}
