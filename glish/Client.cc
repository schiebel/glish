// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include <stdlib.h>
#include <osfcn.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#if defined(HAVE_SHMGET)
#include <sys/types.h>
#include <sys/ipc.h>
#if HAVE_SYS_SHM_H
#include <sys/shm.h>
#endif
#if defined(SHMGET_NOT_DECLARED)
extern "C" {
int shmget(key_t, size_t, int);
int shmctl(int, int, struct shmid_ds *);
void *shmat(int, const void *, int);
int shmdt(const void *); }
#endif
#endif

#include "Channel.h"
#include <iostream.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

extern "C" {
#include <netinet/in.h>
#ifndef HAVE_STRDUP
char* strdup( const char* );
#endif
}

#include "sos/io.h"
#include "Npd/npd.h"
#include "Glish/Client.h"

#include "Daemon.h"
#include "Reporter.h"
#include "Socket.h"
#include "glish_event.h"
#include "system.h"
#include "ports.h"

typedef RETSIGTYPE (*SigHandler)( );

static Client* current_client;
static const char* prog_name = "glish-interpreter";

static int did_init = 0;

static int suspend = 0;

inline streq( const char* s1, const char* s2 )
	{
	return ! strcmp( s1, s2 );
	}


void Client_signal_handler( )
	{
	current_client->HandlePing();
	}


unsigned int EventContext::client_count = 1;

EventContext::~EventContext() 
	{ 
	if (context) free_memory( context );
	if (client_name) free_memory( client_name );
	}

EventContext &EventContext::operator=(const EventContext &c) { 
		if ( &c != this ) {
			if ( context ) free_memory( context );
			context = c.id() ? strdup(c.id()) : 0;
			if ( client_name ) free_memory( client_name );
			client_name = c.name() ? strdup(c.name()) : 0;
		}
		return *this;
	}

EventContext::EventContext( const char *client_name_, const char *context_ )
	{
	if ( client_name_ )
		client_name = strdup(client_name_);
	else
		{
		const char *unknown_str = "unknown-client";
		const int len = strlen(unknown_str);
		client_name = (char*) alloc_memory( sizeof(char)*(len + 22) );
		sprintf(client_name,"%s#%u", unknown_str, client_count);
		}

	if ( context_ )
		context = strdup(context_);
	else
		{
		const char *unknown_str = "<unknown>";
		const int len = strlen(unknown_str);
		context = (char*) alloc_memory( sizeof(char)*(len + 22) );
		sprintf(context,"%s#%u", unknown_str, client_count);
		}

	if ( ! context_ || ! client_name_ )
		client_count++;
	}

EventContext::EventContext(const EventContext &c) : 
			context( c.id() ? strdup(c.id()) : 0), 
			client_name( c.name() ? strdup(c.name()) : 0)
	{ }


GlishEvent::GlishEvent( const char* arg_name, const Value* arg_value )
	{
	name = arg_name;
	value = (Value*) arg_value;
	flags = 0;
	delete_name = delete_value = 0;
	}

GlishEvent::GlishEvent( const char* arg_name, Value* arg_value )
	{
	name = arg_name;
	value = arg_value;
	flags = 0;
	delete_name = 0;
	delete_value = 1;
	}

GlishEvent::GlishEvent( char* arg_name, Value* arg_value )
	{
	name = arg_name;
	value = arg_value;
	flags = 0;
	delete_name = delete_value = 1;
	}

GlishEvent::~GlishEvent()
	{
	if ( delete_name )
		free_memory( (void*) name );

	if ( delete_value )
		Unref( (Value*) value );
	}


int GlishEvent::IsRequest() const
	{
	return flags & GLISH_REQUEST_EVENT;
	}

int GlishEvent::IsReply() const
	{
	return flags & GLISH_REPLY_EVENT;
	}

void GlishEvent::SetIsRequest()
	{
	flags |= GLISH_REQUEST_EVENT;
	}

void GlishEvent::SetIsReply()
	{
	flags |= GLISH_REPLY_EVENT;
	}


class EventLink {
public:
	EventLink( char* task_id, char* new_name, int initial_activity )
		{
		id = task_id;
		name = new_name;
		fd = -1;
		active = initial_activity;
		path = 0;
		}

	~EventLink()
		{
		free_memory( id );
		free_memory( name );
		free_memory( path );
		}

	const char* ID() const		{ return id; }
	int FD() const			{ return fd; }
	int Active() const		{ return active; }
	const char* Name() const	{ return name; }
	const char* Path() const	{ return path; }

	void SetActive( int activity )	{ active = activity; }
	void SetFD( int new_fd )	{ fd = new_fd; }
	void SetPath( const char* p )	{ path = strdup( p ); }

private:
	char* id;
	char* name;
	int fd;
	int active;
	char* path;	// if nil, indicates this is a non-local socket
	};


Client::Client( int& argc, char** argv, int arg_multithreaded ) :
	last_context( ), useshm(0)
	{
	int usingpipes = 0;
	char** orig_argv = argv;

	multithreaded = arg_multithreaded;
	int useshm_ = 0;

	prog_name = argv[0];
	--argc, ++argv;	// remove program name from argument list

	ClientInit();

	int read_fd;
	int write_fd;

	no_glish = 0;
	interpreter_tag = strdup("*no-interpreter*");

	if ( argc < 2 || ! streq( argv[0], "-id" ) )
		{
		// We weren't invoked by the Glish interpreter.  Set things up
		// so that we receive "events" from stdin and send them
		// to stdout.
		have_interpreter_connection = 0;
		initial_client_name = prog_name;

		if ( argc == 0 || (argc >= 1 && !streq( argv[0], "-glish" )) )
			{
			no_glish = 1;
			}

		if ( argc >= 1 && streq( argv[0], "-noglish" ) )
			{
			no_glish = 1;
			--argc, ++argv;
			}

		else
			{
			read_fd = fileno( stdin );
			write_fd = fileno( stdout );

			interpreter_tag = strdup("*stdio*");

			last_context = EventContext(initial_client_name,interpreter_tag);

			EventSource* es = new EventSource( read_fd,
				write_fd, STDIO, last_context );

			event_sources.append( es );

			if ( argc >= 1 && streq( argv[0], "-glish" ) )
				// Eat up -glish argument.
				--argc, ++argv;
			}
		}

	else
		{
		have_interpreter_connection = 1;
		initial_client_name = argv[1];
		argc -= 2, argv += 2;

		if ( argc < 2 ||
		     (! streq( argv[0], "-host" ) &&
		      ! streq( argv[0], "-pipes" )) )
			fatal->Report(
		"invalid Client argument list - missing connection arguments" );

		if ( streq( argv[0], "-host" ) )
			{ // Socket connection.
			char* host = argv[1];
			argc -= 2, argv += 2;

			if ( argc < 2 || ! streq( argv[0], "-port" ) )
				fatal->Report(
			"invalid Client argument list - missing port number" );

			int port = atoi( argv[1] );
			argc -= 2, argv += 2;

			int socket = get_tcp_socket();

			if ( ! remote_connection( socket, host, port ) )
				fatal->Report(
		"could not establish Client connection to interpreter" );

			read_fd = write_fd = socket;
			}

		else
			{ // Pipe connection.
			++argv, --argc;	// skip -pipes argument

			if ( argc < 2 )
				fatal->Report(
			"invalid Client argument list - missing pipe fds" );

			read_fd = atoi( argv[0] );
			write_fd = atoi( argv[1] );

			argc -= 2, argv += 2;
			usingpipes = 1;
			}

		interpreter_tag = strdup(last_context.id());

		if ( argc > 1 && streq( argv[0], "-interpreter" ) )
			{
			interpreter_tag = strdup(argv[1]);
			argc -= 2, argv += 2;
			}

		if ( argc > 0 && streq( argv[0], "-suspend" ) )
			{
			suspend = 1;
			--argc, ++argv;
			}

		if ( argc > 0 && streq( argv[0], "-useshm" ) )
			{
			if ( usingpipes ) useshm_ = 1;
			--argc, ++argv;
			}

		last_context = EventContext(initial_client_name,interpreter_tag);

		EventSource* es = new EventSource( read_fd, write_fd,
					INTERP, last_context );

		event_sources.append( es );
		}

	if ( argc > 0 && streq( argv[0], "-+-" ) )
		--argc, ++argv;

	if ( suspend )
		{
		int pid = int( getpid() );

		fprintf( stderr, "%s @ %s, pid %d: suspending ...\n",
			prog_name, local_host, pid );

		while ( suspend )
			sleep( 1 );	// allow debugger to attach
		}


	if ( multithreaded && ReRegister() )
		{
		fatal->Report("multithreaded client with no glishd present");
		}

	if ( have_interpreter_connection )
		SendEstablishedEvent( last_context );

	CreateSignalHandler();

	int i = 1;
	for ( ; i <= argc; ++i )
		orig_argv[i] = *(argv++);

	orig_argv[i] = 0;
	argv = orig_argv;

	// Put argv[0] back into the argument count - it was removed
	// near the top of this routine.
	++argc;

	// set useshm last so that all handshaking happens
	// outside of shared memory.
	useshm = useshm_;
	}


Client::Client( int client_read_fd, int client_write_fd, const char* name ) :
	last_context( name ), useshm(0)
	{
	initial_client_name = prog_name = name;

	multithreaded = 0;

	ClientInit();

	no_glish = 0;
	have_interpreter_connection = 1;

	interpreter_tag = strdup( last_context.id() );

	EventSource* es = new EventSource( client_read_fd, client_write_fd,
		INTERP, last_context );

	event_sources.append( es );

	if ( multithreaded && ReRegister() )
		{
		fatal->Report("multithreaded client with no glishd present");
		}

	SendEstablishedEvent( last_context );

	CreateSignalHandler();
	}

Client::Client( int client_read_fd, int client_write_fd, const char* name,
	const EventContext &arg_context, int arg_multithreaded ) : 
	last_context( arg_context ), useshm(0)
	{
	// BUG HERE -- name (argument) could go away...
	initial_client_name = prog_name = name;

	multithreaded = arg_multithreaded;

	ClientInit();

	no_glish = 0;
	have_interpreter_connection = 1;

	interpreter_tag = strdup( last_context.id() );

	EventSource* es = new EventSource( client_read_fd, client_write_fd,
		INTERP, last_context );

	event_sources.append( es );

	if ( multithreaded && ReRegister() )
		{
		fatal->Report("multithreaded client with no glishd present");
		}

	SendEstablishedEvent( last_context );

	CreateSignalHandler();
	}

Client::~Client()
	{
	(void) install_signal_handler( SIGIO, former_handler );

	if ( have_interpreter_connection )
		{
		loop_over_list( event_sources, i )
			{
			if ( event_sources[i]->Type() == INTERP )
				{
				PostEvent( "done", event_sources[i]->Context().name(),
					event_sources[i]->Context() );
				RemoveInterpreter( event_sources[i] );

				// restart at beginning since links may be
				// gone too!
				i = 0;
				}
			}
		}

	// clean up extraneous event_sources, if any...
	// links were cleaned with RemoveInterpreter() calls
	for ( int j = event_sources.length()-1 ; j >= 0 ; j-- )
		delete event_sources[j];

	if ( interpreter_tag )
		free_memory( interpreter_tag );

	}

GlishEvent* Client::NextEvent()
	{
	if ( no_glish )
		return 0;

	fd_set input_fds;

	FD_ZERO( &input_fds );
	AddInputMask( &input_fds );

	while ( select( FD_SETSIZE, (SELECT_MASK_TYPE *) &input_fds, 0, 0, 0 ) < 0 )
		{
		if ( errno != EINTR )
			{
			fprintf( stderr, "%s: ", prog_name );
			perror( "error during select()" );
			return 0;
			}
		}

	return NextEvent( &input_fds );
	}

GlishEvent* Client::NextEvent( fd_set* mask )
	{
	if ( no_glish )
		return 0;

	loop_over_list( event_sources, i )
		{
		int fd = event_sources[i]->Source().fd();

		if ( FD_ISSET( fd, mask ) )
			return GetEvent( event_sources[i] );
		}

	PostEvent( "error", "bad call to Client::NextEvent" );

	return 0;
	}

void Client::Unrecognized()
	{
	if ( ! last_event )
		return;

	if ( last_event->name[0] == '*' )
		// Internal event - ignore.
		return;

	PostEvent( "unrecognized", last_event->name );
	}

void Client::Error( const char* msg )
	{
	if ( last_event )
		PostEvent( "error", "bad \"%s\" event: %s",
				last_event->name, msg );
	else
		PostEvent( "error", msg );
	}

void Client::Error( const char* fmt, const char* arg )
	{
	char buf[8192];
	sprintf( buf, fmt, arg );
	Error( buf );
	}

void Client::PostEvent( const GlishEvent* event, const EventContext &context )
	{
	SendEvent( event, -1, context );
	}

void Client::PostEvent( const char* event_name, const Value* event_value,
    const EventContext &context )
	{
	GlishEvent e( event_name, event_value );
	SendEvent( &e, -1, context );
	}

void Client::PostEvent( const char* event_name, const char* event_value,
    const EventContext &context )
	{
	Value val( event_value );
	PostEvent( event_name, &val, context );
	}

void Client::PostEvent( const char* event_name, const char* event_fmt,
    const char* event_arg, const EventContext &context )
	{
	char buf[8192];
	sprintf( buf, event_fmt, event_arg );
	PostEvent( event_name, buf, context );
	}

void Client::PostEvent( const char* event_name, const char* event_fmt,
    const char* arg1, const char* arg2, const EventContext &context )
	{
	char buf[8192];
	sprintf( buf, event_fmt, arg1, arg2 );
	PostEvent( event_name, buf, context );
	}

//*** TJS - Client::Reply() might need some work to avoid race.  Probably ok tho'
void Client::Reply( const Value* event_value )
	{
	if ( ! ReplyPending() )
		error->Report( prog_name,
	" issued Reply without having received a corresponding request" );

	else
		{
		GlishEvent e( (const char*) pending_reply, event_value );
		e.SetIsReply();
		PostEvent( &e );
		delete pending_reply;
		pending_reply = 0;
		}
	}

void Client::PostOpaqueSDS_Event( const char* event_name, int sds,
    const EventContext &context )
	{
	GlishEvent e( event_name, (const Value*) 0 );
	SendEvent( &e, sds, context );
	}

int Client::AddInputMask( fd_set* mask )
	{
	if ( no_glish )
		return 0;

	int num_added = 0;

	// Now add in any fd's due to event sources
	loop_over_list( event_sources, i )
		{
		int this_fd = event_sources[i]->Source().fd();

		if ( (this_fd >=0 ) && ! FD_ISSET( this_fd, mask ) )
			{
			FD_SET( this_fd, mask );
			++num_added;
			}
		}

	return num_added;
	}

int Client::HasClientInput( fd_set* mask )
	{
	if ( no_glish )
		return 0;

	loop_over_list( event_sources, i )
		{
		int this_fd = event_sources[i]->Source().fd();
		if ( (this_fd >= 0 ) && FD_ISSET( this_fd, mask ) )
			return 1;
		}

	return 0;
	}


int Client::ReRegister( char* registration_name )
	{
	if ( ! multithreaded || no_glish )
		return 0;

	loop_over_list( event_sources, i )
		{
		if ( event_sources[i]->Type() == GLISHD )
			// already got a glishd event source
			return 0;
		}

	Socket* mysock = new AcceptSocket( 0, DAEMON_PORT, 0 );
	int glishd_running = ! mysock->Port(); // Port()==0 if glishd running
	delete mysock;

	if ( ! glishd_running )
		{
		message->Report( "starting local daemon ..." );
		start_local_daemon();
		const int tries = 15;
		int count = tries;
		for ( ; count && ! glishd_running; count--)
			{
			if ( count != tries )
				{
				message->Report( "waiting for daemon ..." );
				sleep( 1 );
				}
			mysock = new AcceptSocket( 0, DAEMON_PORT, 0 );
			glishd_running = ! mysock->Port(); // Port()==0 if glishd running
			delete mysock;
			}
		if ( ! count )
			return 1;
		}

	// connect and register with glishd

	int socket = get_tcp_socket();

	if ( ! remote_connection( socket, local_host, DAEMON_PORT ) )
		return 1; // connect failed

#ifdef AUTHENTICATE
	if ( ! authenticate_to_server( socket ) )
		{
		close( socket );
		return 1;
		}
#endif

	EventSource* es = new EventSource( socket, GLISHD,
		EventContext(( registration_name == 0 ) ?
			       prog_name : registration_name,
			       "*glishd*"));
	event_sources.append( es );

	GlishEvent e( (const char *) "*register-persistent*",
		create_value( ( registration_name == 0 ) ? prog_name :
		registration_name ) );
	send_event( es->Sink(), &e );

	return 0;
	}


void Client::ClientInit()
	{
	if ( ! did_init )
		{
// 		sds_init();
		init_reporters();
		init_values();

		did_init = 1;

#ifdef AUTHENTICATE
		init_log( prog_name );
#endif
		}

	last_event = 0;
	pending_reply = 0;

	local_host = local_host_name();
	}

void Client::CreateSignalHandler()
	{
	current_client = this;
	former_handler = install_signal_handler( SIGIO, Client_signal_handler );
	}

void Client::SendEstablishedEvent( const EventContext &context )
	{
	Value* r = create_record();

	r->SetField( "name", context.name() ? context.name() : initial_client_name );
	r->SetField( "protocol", GLISH_CLIENT_PROTO_VERSION );

	// Leave some time to allow client to setup
	sleep(1);
	PostEvent( "established", r, context );

	Unref( r );
	}

GlishEvent* Client::GetEvent( EventSource* source )
	{
	Unref( last_event );

	sos_fd_source &fd_src = source->Source();
	EventContext context = source->Context();
	event_src_type type = source->Type();

	if ( type == STDIO )
		{
		char buf[512];
		if ( ! fgets( buf, sizeof( buf ), stdin ) )
			{
			// stdio context exited
			if ( ! multithreaded )
				return 0;
			else
				{
				RemoveInterpreter( source );
				FD_Change( fd_src.fd(), 0 );
				return new GlishEvent( (const char *) "*end-context*",
					create_value("*stdio*") );
				}
			}

		// Nuke final newline, if present.
		char* delim = strchr( buf, '\n' );

		if ( delim )
			*delim = '\0';

		// Find separation between event name and value.
		delim = strchr( buf, ' ' );
		Value* result;

		if ( ! delim )
			result = error_value();

		else
			{
			*delim++ = '\0';
			result = create_value( delim );
			}

		last_event = new GlishEvent( strdup( buf ), result );

		last_context = context;
		}

	else if ( have_interpreter_connection )
		{
		last_context = context;

		last_event = recv_event( fd_src );

		if ( last_event && multithreaded && type == GLISHD )
			{
			// Caught glishd request in multithreaded context
			//
			// Should only be forwarded interpreter client request
			//
			// Create new event source, create back-connection to
			//	interpreter, and create "*new-context*" event
			//	to pass back to client.
			//
			// Change last_context to the context of the entering
			//	interpreter.

			char* info = last_event->value->StringVal();

			if ( streq( last_event->name, "client" ) )
				{
				int port;
				char c_name[256];
				char host[256];
				char interp_tag[256];

				sscanf(info, "remote task %*d %*s -id %s -host %s -port %d -interpreter %s",
						c_name, host, &port, interp_tag);

				int my_socket = get_tcp_socket();
				if ( ! remote_connection( my_socket, host, port ) )
					{
					fatal->Report( "could not establish Client"
						"connection to interpreter" );
					}

				static Dict(int) mult_connect;
				int count = mult_connect[interp_tag];
				char context_str[256];
				sprintf(context_str,"%s#%d",interp_tag,++count);
				mult_connect.Insert(interp_tag,count);
				last_context = EventContext(c_name, context_str);

				EventSource* es = new EventSource( my_socket,
					INTERP,	last_context );

				event_sources.append( es );

				SendEstablishedEvent( last_context );

				last_event = new GlishEvent( (const char *) "*new-context*",
					create_value( es->Context().id() ) );

				return last_event;
				}
			else
				{
				// ignore
				}
			}

		if ( ! last_event )
			{
			// event source has gone away.  Can't be *stdio*.
			// Could be *glishd*, link or interpreter.
			// Only worry about links here.

			loop_over_list( event_sources, i )
				{
				if ( event_sources[i]->Source().fd() == fd_src.fd() &&
				    type == I_LINK )
					{
					RemoveIncomingLink( i );
					last_event = new GlishEvent( (const char *) "*dummy*",
						error_value() );
					FD_Change( fd_src.fd(), 0 );
					// context_sources,sinks?
					}
				}
			}
		else
			{
			Value* v = last_event->value;

			if ( v->FieldVal( "*reply*", pending_reply ) )
				{ // Request/reply event; unwrap the request.
				Value* request = v->Field( "*request*" );

				if ( ! request )
					fatal->Report(
						"bad request/reply event" );

				Ref( request );
				Unref( v );
				last_event->value = request;
				}
			}
		}

	if ( last_event )
		{
		const char* name = last_event->name;

		if ( streq( name, "terminate" ) )
			{
			Unref(last_event);
			last_event = 0;
			}

		else if ( name[0] == '*' )
			{
			Value* v = last_event->value;

			if ( streq( name, "*sync*" ) )
				Reply( false_value );

			else if ( streq( name, "*link-sink*" ) )
				BuildLink( v );

			else if ( streq( name, "*rendezvous-orig*" ) )
				RendezvousAsOriginator( v );

			else if ( streq( name, "*rendezvous-resp*" ) )
				RendezvousAsResponder( v );

			else if ( streq( name, "*unlink-sink*" ) )
				UnlinkSink( v );

			}
		}

	if ( ! last_event && multithreaded && type != I_LINK )
		{
		// Interpreter context has exited for multithreaded client
		//
		// Don't pass back null events; instead return an
		// *end-context* event for the interpreter context that
		// exited.  Do this event for *stdio* and *glishd* contexts -
		// the client can then do whatever they like such as exiting
		// or restarting the glishd with a call to ReRegister().

		RemoveInterpreter( source );
		FD_Change( fd_src.fd(), 0 );

		last_event = new GlishEvent( (const char *) "*end-context*",
			create_value( last_context.id() ) );		
		}

	return last_event;
	}

void Client::FD_Change( int /* fd */, int /* add_flag */ )
	{
	}

void Client::HandlePing()
	{
	}

void Client::UnlinkSink( Value* v )
	{
	int is_new;
	EventLink* el = AddOutputLink( v, 1, is_new );

	if ( ! el )
		return;

	if ( is_new )
		PostEvent( "error", "no link to %s", el->ID() );

	el->SetActive( 0 );
	}

void Client::BuildLink( Value* v )
	{
	int is_new;
	EventLink* el = AddOutputLink( v, 0, is_new );

	if ( ! el )
		return;

	if ( ! is_new )
		{ // No need to forward, we've already built the link.
		el->SetActive( 1 );
		return;
		}

	char* sink_id = 0;
	if ( ! v->FieldVal( "sink_id", sink_id ) )
		fatal->Report( "bad internal link event" );

	sink_id_list* remote_sinks = context_sinks[ last_context.id() ];

	if ( ! remote_sinks )
		{
		remote_sinks = new sink_id_list;
		context_sinks.Insert( strdup( last_context.id() ), remote_sinks );
		}

	int fd = (*remote_sinks)[sink_id];
	free_memory( sink_id );

	// Minor-league bug here.  fd=0 is legal, though it means that
	// somehow stdin got closed.  But even in that case, we'll just
	// miss an opportunity to reuse the connection, and instead create
	// a new, separate connection.
	if ( fd )
		{ // Already have a connection, piggyback on it.
		el->SetFD( fd );
		el->SetActive( 1 );
		return;
		}

	// We have to create a new AcceptSocket each time, otherwise
	// there's a race in which we send out rendezvous events to
	// B and then to C, but C gets the event first and connects
	// to the accept socket in lieu of B.
	AcceptSocket* accept_socket;

	int is_local;
	if ( ! v->FieldVal( "is_local", is_local ) )
		fatal->Report( "internal link event lacks is_local field" );

	if ( is_local )
		{
		accept_socket = new AcceptSocket( 1 );

		char* path = local_socket_name( accept_socket->FD() );
		el->SetPath( path );
		v->SetField( "path", path );
		}

	else
		{ // Use an ordinary socket.
		accept_socket = new AcceptSocket();

		// Include information in the event about how to reach us.
		v->SetField( "host", local_host );
		v->SetField( "port", accept_socket->Port() );
		}

	// So we can recover this socket when the interpreter reflects
	// this rendezvous event reflected back to us.
	v->SetField( "accept_fd", accept_socket->FD() );

	PostEvent( "*rendezvous*", v );
	}

EventLink* Client::AddOutputLink( Value* v, int want_active, int& is_new )
	{
	char* event_name = 0;
	char* new_name = 0;
	char* task_id = 0;

	if ( ! v->FieldVal( "event", event_name ) ||
	     ! v->FieldVal( "new_name", new_name ) ||
	     ! v->FieldVal( "sink_id", task_id ) )
		fatal->Report( "bad internal link event" );

	event_link_context_list* cl = context_links[ last_context.id() ];

	if ( ! cl )
		{
		cl = new event_link_context_list;
		context_links.Insert( strdup( last_context.id() ), cl );
		}

	event_link_list* l = (*cl)[event_name];

	if ( ! l )
		{ // Event link doesn't exist yet.
		l = new event_link_list;
		(*cl).Insert( event_name, l );
		}

	else
		{ // Look for whether this event link already exists.
		free_memory( event_name );

		loop_over_list( *l, i )
			{
			EventLink* el = (*l)[i];

			if ( el->Active() == want_active &&
			     streq( el->Name(), new_name ) &&
			     streq( el->ID(), task_id ) )
				{ // This one fits the bill.
				free_memory( new_name );
				free_memory( task_id );

				is_new = 0;

				return el;
				}
			}
		}

	EventLink* result = new EventLink( task_id, new_name, want_active );
	l->append( result );

	is_new = 1;

	return result;
	}

void Client::RendezvousAsOriginator( Value* v )
	{
	int is_new;
	EventLink* el = AddOutputLink( v, 0, is_new );

	if ( ! el || is_new )
		fatal->Report( "bad internal link event" );

	int output_fd;

	int accept_fd;
	if ( ! v->FieldVal( "accept_fd", accept_fd ) )
		fatal->Report( "bad internal link event" );

	char* sink_id;
	if ( ! v->FieldVal( "sink_id", sink_id ) )
		fatal->Report( "bad internal link event" );

	sink_id_list* remote_sinks = context_sinks[ last_context.id() ];

	if ( ! remote_sinks )
		{
		remote_sinks = new sink_id_list;
		context_sinks.Insert( strdup( last_context.id() ), remote_sinks );
		}

	int sink_fd = (*remote_sinks)[sink_id];

	if ( sink_fd )
		{
		// Already have a socket connection to this remote sink.
		// Even though BuildLink endeavors to avoid sending out
		// a *rendezvous* if such a connection already exists,
		// it's possible for a second *link-sink* to come in before
		// we've finished processing the first, in which case the
		// second call to BuildLink won't see that we already have
		// a socket connection.  So we check here, too.
		output_fd = sink_fd;
		free_memory( sink_id );

		// If this was a local socket, get rid of the corresponding
		// file.
		char* path;
		if ( v->FieldVal( "path", path ) )
			{
			if ( unlink( path ) < 0 )
				PostEvent( "error",
					"can't unlink local socket sink %s",
						path );

			free_memory( path );
			}
		}

	else
		{ // Create a new socket connection.
		int is_local;
		if ( ! v->FieldVal( "is_local", is_local ) )
			fatal->Report( "bad internal link event" );

		output_fd = is_local ?
			accept_local_connection( accept_fd ) :
			accept_connection( accept_fd );

		if ( output_fd < 0 )
			fatal->Report( "socket accept for link failed" );

		(*remote_sinks).Insert( sink_id, output_fd );
		}

	close( accept_fd );

	el->SetFD( output_fd );
	el->SetActive( 1 );
	}

void Client::RendezvousAsResponder( Value* v )
	{
	char* source_id;
	if ( ! v->FieldVal( "source_id", source_id ) )
		fatal->Report( "bad internal link event" );

	sink_id_list* remote_sources = context_sources[ last_context.id() ];

	if ( ! remote_sources )
		{
		remote_sources = new sink_id_list;
		context_sources.Insert( strdup(last_context.id() ), remote_sources );
		}

	if ( (*remote_sources)[source_id] )
		{
		// We're all set, we already select on this input source.
		free_memory( source_id );
		return;
		}

	int input_fd;

	int is_local;
	if ( ! v->FieldVal( "is_local", is_local ) )
		fatal->Report( "bad internal link event" );

	if ( is_local )
		{
		input_fd = get_local_socket();

		char* path;
		if ( ! v->FieldVal( "path", path ) )
			fatal->Report( "bad internal link event" );

		if ( ! local_connection( input_fd, path ) )
			fatal->Report( "couldn't connect to local socket",
					path );

		// Now that we've rendezvous'd, remove the Unix-domain
		// socket from the file system namespace.

		if ( unlink( path ) < 0 )
			PostEvent( "error", "can't unlink local socket sink %s",
					path );

		free_memory( path );
		}

	else
		{
		char* host;
		int port;

		if ( ! v->FieldVal( "host", host ) ||
		     ! v->FieldVal( "port", port ) )
			fatal->Report( "bad internal link event" );

		input_fd = get_tcp_socket();

		if ( ! remote_connection( input_fd, host, port ) )
			{
			PostEvent( "error", "can't open socket sink %s", host );
			return;
			}

		free_memory( host );
		}

	event_sources.append( new EventSource( input_fd, I_LINK, last_context ) );
	FD_Change( input_fd, 1 );

	(*remote_sources).Insert( strdup(source_id), input_fd );
	}

void Client::SendEvent( const GlishEvent* e, int sds, const EventContext &context )
	{
	if ( no_glish )
		return;

	const char* name = e->name;
	const Value* value = e->value;

	if ( ! have_interpreter_connection || streq( context.id(), "*stdio*" ) )
		{
		if ( value )
			message->Report( name, " ", value );
		else
			message->Report( name, " <no value>" );
		return;
		}

	int did_send = 0;

	event_link_context_list* cl = context_links[ context.id() ];
	event_link_list* l;
	EventLink* el;

	if ( cl && ( l = (*cl)[name] ) )
		{
		loop_over_list( *l, i )
			{
			el = (*l)[i];

			if ( el->Active() )
				{
 /**!!LOOK!!**/			static sos_fd_sink sink;
				sink.setFd(el->FD());
				GlishEvent e( el->Name(), value );
				send_event( sink, &e );
				did_send = 1;
				}
			}

		// if linked and sent, can return now
		if ( did_send )
			return;
		}

	// If we didn't send any events, all of the appropriate existing links,
	// if any, are inactive.  Forward to the appropriate interpreter.

	loop_over_list( event_sources, j )
		{
		if ( streq( event_sources[j]->Context().id(), context.id() ) &&
		    event_sources[j]->Sink().fd() >= 0 )
			{
			GlishEvent e( name, value );

			send_event( event_sources[j]->Sink(), &e );
			return; // should only be one match
			}
		}
	}

void Client::RemoveIncomingLink( int dead_event_source )
	{
	int read_fd = event_sources[dead_event_source]->Source().fd();
	const char* context = event_sources[dead_event_source]->Context().id();

	sink_id_list* remote_sources = context_sources[ context ];

	if ( read_fd && remote_sources ) // don't worry 'bout stdin
		{
		// go through and remote this read_fd from the FD list
		IterCookie* c = remote_sources->InitForIteration();
		const char* key;
		int this_fd;

		while ( this_fd = remote_sources->NextEntry( key, c ) )
			{
			if ( this_fd == read_fd )
				{
				remote_sources->Remove( key );
				return;
				}
			}
		}

	delete event_sources.remove_nth( dead_event_source );
	}

void Client::RemoveInterpreter( EventSource* source )
	{
	const char* context = source->Context().id();

	// delete all event sources for links associated with this interpreter
	loop_over_list( event_sources, i )
		{
		if ( streq( event_sources[i]->Context().id(), context ) &&
		    event_sources[i]->Type() == I_LINK )
			{
			delete event_sources.remove_nth( i-- );
			}
		}

	// delete all links associated with this interpreter
	// how wonderful to be so profound!
	delete context_links[ context ];
	delete context_sinks[ context ];
	delete context_sources[ context ];

	delete event_sources.remove( source );
	}

static Value *read_value( sos_in &, char *&, unsigned char & );
static Value *read_record( sos_in &sos, unsigned int len, int is_fail = 0 )
	{
	sos_code type;
	unsigned int xlen;

	charptr *fields = (charptr*) sos.get( xlen, type );
	if ( type != SOS_STRING )
		fatal->Report("field names expected first when reading record");

	recordptr nr = create_record_dict();

	for ( int cnt = 0; cnt < len; cnt++ )
		{
		char *dummy;
		unsigned char f;
		Value *val = read_value( sos, dummy, f );
		nr->Insert( (char*) fields[cnt], val );
		}

	free_memory( fields );

	Value *ret = create_value( nr );

	if ( is_fail )
		{
		Value *tmp = create_value();
		tmp->AssignAttributes( ret );
		ret = tmp;
		}

	return ret;
	}


static Value *read_value( sos_in &sos, char *&name, unsigned char &flags )
	{
	sos_code type;
	unsigned int len;
	static sos_header head((char*) alloc_memory(SOS_HEADER_SIZE), 0, SOS_UNKNOWN, 1);
	Value *val = 0;

	void *ary = sos.get( len, type, head );

	if ( ! ary ) return 0;

	unsigned int name_len = head.ugeti();

	flags = head.ugetc(1);

	switch ( type )
		{
		case SOS_INT: 
			if ( head.ugetc(0) == TYPE_BOOL )
				val = create_value( (glish_bool*) ary, len );
			else
				val = create_value( (int*) ary, len );
			break;
		case SOS_BYTE: val = create_value( (byte*) ary, len ); break;
		case SOS_SHORT: val = create_value( (short*) ary, len ); break;
		case SOS_FLOAT:
			if ( head.ugetc(0) == TYPE_COMPLEX )
				val = create_value( (complex*) ary, len / 2 );
			else
				val = create_value( (float*) ary, len );
			break;
		case SOS_DOUBLE:
			if ( head.ugetc(0) == TYPE_DCOMPLEX )
				val = create_value( (dcomplex*) ary, len / 2 );
			else
				val = create_value( (double*) ary, len );
			break;
		case SOS_STRING: val = create_value( (charptr*) ary, len ); break;
		case SOS_RECORD: val = read_record( sos, len, head.ugetc(0) == TYPE_FAIL ); break;
		default:
			cerr << "header: " << head << endl;
			fatal->Report( "bad type in 'read_value( sos_in &, char *&, unsigned char &)'" );
		}

	if ( name_len )
		{
		name = (char*) alloc_memory( name_len + 1 );
		sos.read(name,name_len);
		name[name_len] = '\0';
		}
	else
		name = 0;

	if ( flags & GLISH_HAS_ATTRIBUTE )
		{
		char *dummy = 0;
		unsigned char f;
		Value *attr = read_value( sos, dummy, f );
		val->AssignAttributes( attr );
		}

	return val;
	}

GlishEvent* recv_event( sos_source &in )
	{
	static sos_in sos;
	sos.set(&in);

	char *name = 0;
	unsigned char flags;
	Value *result = read_value( sos, name, flags );

	if ( ! result ) return 0;

	if ( ! name )
		fatal->Report("no event name found");

	GlishEvent *e = new GlishEvent( name, result );
	e->SetFlags( flags );
	return e;
	}


void write_value( sos_out &sos, Value *val, const char *label, char *name, unsigned char flags )
	{
	static sos_header head( (char*) alloc_memory(SOS_HEADER_SIZE), 0, SOS_UNKNOWN, 1 );

	if ( val->IsVecRef() )
		{
		Value* copy = copy_value( val );
//		dlist->append( new DelObj( copy ) );	/*!!! LEAK !!!*/
		write_value( sos, copy, label, name, flags );
		return;
		}

	if ( val->IsRef() )
		{
		write_value( sos, val->Deref(), label, name, flags );
		return;
		}

	glish_type type = val->Type();

	head.useti( name ? strlen(name) : 0 );
	head.usetc( type, 0 );
	head.usetc( (val->AttributePtr() ? GLISH_HAS_ATTRIBUTE : 0) | flags, 1 );

	switch( type )
		{
#define WRITE_ACTION(TAG,ACCESSOR,CAST,MUL)					\
	case TAG:								\
		sos.put( CAST val->ACCESSOR(0), val->Length() MUL, head );	\
		break;

		WRITE_ACTION(TYPE_BOOL,BoolPtr, (int*), )
		WRITE_ACTION(TYPE_BYTE,BytePtr,,)
		WRITE_ACTION(TYPE_SHORT,ShortPtr,,)
		WRITE_ACTION(TYPE_INT,IntPtr,,)
		WRITE_ACTION(TYPE_FLOAT,FloatPtr,,)
		WRITE_ACTION(TYPE_DOUBLE,DoublePtr,,)
		WRITE_ACTION(TYPE_COMPLEX,ComplexPtr, (float*), *2 )
		WRITE_ACTION(TYPE_DCOMPLEX,DcomplexPtr, (double*), *2 )
		WRITE_ACTION(TYPE_STRING,StringPtr,,)
		case TYPE_FAIL:
			val = (Value*) val->GetAttributes();
			head.usetc( val->AttributePtr() ? 1 : 0, 1 );
		case TYPE_RECORD:
			{
			unsigned int len = val->Length();
			recordptr rec = val->RecordPtr( 0 );
			IterCookie* c = rec->InitForIteration();
			const Value* member;
			const char* key;

			sos.put_record_start( len, head );

			char **fields = (char**) alloc_memory( sizeof(char*) * len );
			unsigned int i = 0;
			while ( (member = rec->NextEntry( key, c )) )
				fields[i++] = (char*) key;
			sos.put( fields, len );
			free_memory( fields );

			c = rec->InitForIteration();
			while ( (member = rec->NextEntry( key, c )) )
				write_value( sos, (Value*) member, key );
			}
			break;
		case TYPE_AGENT:
			warn->Report( "skipping agent value \"", (label ? label : "?"),
					"\" in creation of dataset" );
			write_value( sos, (Value*) false_value, label, name );
			return;
			break;
		case TYPE_FUNC:
			warn->Report( "skipping function value \"", (label ? label : "?"),
					"\" in creation of dataset" );
			write_value( sos, (Value*) false_value, label, name );
			return;
			break;
                default:
                        fatal->Report( "bad type in write_value()" );
 		}

	if ( name ) sos.write( name, strlen(name), sos_sink::COPY );

	if ( val->AttributePtr() )
		write_value( sos, (Value*) val->GetAttributes(), name );
	}

sos_status *send_event( sos_sink &out, const char* name, const GlishEvent* e, int can_suspend )
	{
	static sos_out sos;
	sos.set(&out);

	write_value( sos, e->value, name, (char*) name, e->Flags() );
	sos_status *ss = sos.flush();
	if ( ! ss || can_suspend ) return ss;
	while ( ss = ss->resume( ) );
	return 0;
	}

//
// Routines for reading and writing values to and from files.
// Used by:
//		WriteValueBuiltIn::DoCall( const_args_list* )
//		ReadValueBuiltIn::DoCall( const_args_list* )
//
Value *read_value( sos_in &sos )
	{
	char *name = 0;
	unsigned char flags;
	Value *result = read_value( sos, name, flags );

	cerr << "read_value: " << name << " (" << (void*) flags << ")" << endl;

	return result;
	}

void write_value( sos_out &sos, const Value *v )
	{
	write_value( sos, (Value*) v, 0, 0, 0 );
	sos.flush();
	}
