// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef client_h
#define client_h

#include <generic.h>
#include <sys/types.h>
#include <unistd.h>

#include "Glish/Value.h"

extern "C" {
#ifndef HAVE_STRDUP
char* strdup( const char* );
#endif
}

struct fd_set;
class EventContext {
public:
	EventContext(const char *client_name_ = 0, const char *context_ = 0 );
	EventContext(const EventContext &c);
	EventContext &operator=(const EventContext &c);
	const char *id() const { return context; }
	const char *name() const { return client_name; }
	~EventContext();
private:
	char *context;
	char *client_name;
	static unsigned int client_count;
};

class GlishEvent : public GlishObject {
public:
	GlishEvent( const char* arg_name, const Value* arg_value );
	GlishEvent( const char* arg_name, Value* arg_value );
	GlishEvent( char* arg_name, Value* arg_value );

	~GlishEvent();

	const char* Name() const	{ return name; }
	Value* Val() const		{ return value; }

	int IsRequest() const;
	int IsReply() const;
	unsigned char Flags() const	{ return flags; }

	void SetIsRequest();
	void SetIsReply();
	void SetFlags( unsigned char new_flags ) { flags = new_flags; }

	// These are public for historical reasons.
	const char* name;
	Value* value;

protected:
	unsigned char flags;
	int delete_name;
	int delete_value;
	};

typedef enum event_src_type { INTERP, I_LINK, STDIO, GLISHD } event_src_type;

class EventSource : public GlishObject {
    public:
	EventSource( int arg_read_fd, int arg_write_fd,
	    event_src_type arg_type = INTERP ) : context( )
		{
		read_fd = arg_read_fd;
		write_fd = arg_write_fd;
		type = arg_type;
		}

	EventSource( int arg_read_fd, int arg_write_fd,
	    event_src_type arg_type,
	    const EventContext &arg_context ) : context(arg_context)
		{
		read_fd = arg_read_fd;
		write_fd = arg_write_fd;
		type = arg_type;
		}

	EventSource( int arg_fd, event_src_type arg_type = INTERP ) : 
	    context( )
		{
		read_fd = write_fd = arg_fd;
		type = arg_type;
		}

	EventSource( int arg_fd, event_src_type arg_type,
	    const EventContext &arg_context ) : context( arg_context )
		{
		read_fd = write_fd = arg_fd;
		type = arg_type;
		}

	// destructor closes the fds
	~EventSource()
		{
		close ( read_fd );
		close ( write_fd );
		}

	int Read_FD() { return read_fd; }
	int Write_FD() { return write_fd; }
	const EventContext &Context() const { return context; }
	event_src_type Type() { return type; }

    protected:
	int read_fd;
	int write_fd;
	EventContext context;
	event_src_type type;
	};

declare(List,int);

// Holds information regarding outbound "link" commands.
class EventLink;

declare(PList,EventLink);
declare(PDict,EventLink);

declare(PList,EventSource);
declare(PDict,EventSource);

typedef PList(EventLink) event_link_list;
declare(PDict,event_link_list);

typedef PDict(event_link_list) event_link_context_list;
declare(PDict,event_link_context_list);

declare(Dict,int);
typedef Dict(int) sink_id_list;
declare(PDict,sink_id_list);

class AcceptSocket;

class Client {
    public:
	// Client's are constructed by giving them the program's
	// argc and argv.  Any client-specific arguments are read
	// and stripped off.
	Client( int& argc, char** argv, int arg_multithreaded = 0 );

	// Alternatively, a Client can be constructed from fd's for
	// reading and writing events and a client name.  This version
	// of the constructor does not generate an initial "established"
	// event.
	Client( int client_read_fd, int client_write_fd, const char* name );
	Client( int client_read_fd, int client_write_fd, const char* name,
		const EventContext &arg_context, int arg_multithreaded = 0 );

	virtual ~Client();

	// Wait for the next event to arrive and return a pointer to 
	// it.  The GlishEvent will be Unref()'d on the next call to
	// NextEvent(), so if the caller wishes to keep the GlishEvent
	// (or its Value) they must Ref() the GlishEvent (or its Value).
	//
	// If the interpreter connection has been broken then 0 is returned
	// (and the caller should terminate).
	GlishEvent* NextEvent();

	// Another version of NextEvent which can be passed an fd_set
	// returned by select() to aid in determining from where to
	// read the next event.
	GlishEvent* NextEvent( fd_set* mask );

	// Called by the main program (or whoever called NextEvent()) when
	// the current event is unrecognized.
	void Unrecognized();

	// Called to report that the current event has an error.
	void Error( const char* msg );
	void Error( const char* fmt, const char* arg );


	// Sends an event with the given name and value.
	void PostEvent( const GlishEvent* event, const EventContext &context );
	void PostEvent( const GlishEvent* event ) 
		{ PostEvent( event, EventContext( initial_client_name, interpreter_tag ) ); }
	void PostEvent( const char* event_name, const Value* event_value,
		const EventContext &context );
	void PostEvent( const char* event_name, const Value* event_value )
		{ PostEvent( event_name, event_value, 
			     EventContext(initial_client_name, interpreter_tag) ); }

	// Sends an event with the given name and character string value.
	void PostEvent( const char* event_name, const char* event_value,
		const EventContext &context );
	void PostEvent( const char* event_name, const char* event_value )
		{ PostEvent( event_name, event_value,
			     EventContext(initial_client_name, interpreter_tag) ); }

	// Sends an event with the given name, using a printf-style format
	// and an associated string argument.  For example,
	//
	//	client->PostEvent( "error", "couldn't open %s", file_name );
	//
	void PostEvent( const char* event_name, const char* event_fmt,
				const char* event_arg, const EventContext &context );
	void PostEvent( const char* event_name, const char* event_fmt,
				const char* event_arg )
		{ PostEvent( event_name, event_fmt, event_arg, 
			     EventContext(initial_client_name, interpreter_tag) ); }
	void PostEvent( const char* event_name, const char* event_fmt,
				const char* arg1, const char* arg2,
				const EventContext &context );
	void PostEvent( const char* event_name, const char* event_fmt,
				const char* arg1, const char* arg2 )
		{ PostEvent( event_name, event_fmt, arg1, arg2,
			     EventContext(initial_client_name, interpreter_tag) ); }

	// Reply to the last received event.
	void Reply( const Value* event_value );

	// True if the Client is expecting a reply, false otherwise.
	int ReplyPending() const	{ return pending_reply != 0; }

	// Post an event with an "opaque" SDS value - one that we won't
	// try to convert to a Glish record.
	void PostOpaqueSDS_Event( const char* event_name, int sds,
		const EventContext &context );
	void PostOpaqueSDS_Event( const char* event_name, int sds )
		{ PostOpaqueSDS_Event( event_name, sds, 
				       EventContext(initial_client_name, interpreter_tag) ); }


	// For any file descriptors this Client might read events from,
	// sets the corresponding bits in the passed fd_set.  The caller
	// may then use the fd_set in a call to select().  Returns the
	// number of fd's added to the mask.
	int AddInputMask( fd_set* mask );

	// Returns true if the following mask indicates that input
	// is available for the client.
	int HasClientInput( fd_set* mask );

	// Returns true if the client was invoked by a Glish interpreter,
	// false if the client is running standalone.
	int HasInterpreterConnection()
		{ return int(have_interpreter_connection); }

	int HasSequencerConnection()	// deprecated
		{ return HasInterpreterConnection(); }

	// Returns true if the client has *some* event source, either
	// the interpreter or stdin; false if not.
	int HasEventSource()	{ return ! int(no_glish); }

	// Register with glishd if multithreaded, adding event_source
	// that corresponds to the glishd.  Returns nonzero if connection
	// failed for any reason (eg no glishd).
	int ReRegister( char* registration_name = 0 );

	// 1 if multithreaded, 0 if not
	int Multithreaded() { return multithreaded; }

	// return context of last event received
	const EventContext &LastContext() { return last_context; }

    protected:
	friend void Client_signal_handler( );

	// Performs Client initialization that doesn't depend on the
	// arguments with which the client was invoked.
	void ClientInit();

	// Creates the necessary signal handler for dealing with client
	// "ping"'s.
	void CreateSignalHandler();

	// Sends out the Client's "established" event.
	void SendEstablishedEvent( const EventContext &context );

	// Returns the next event from the given event source.
	GlishEvent* GetEvent( EventSource* source );

	// Called whenever a new fd is added (add_flag=1) or deleted
	// (add_flag=0) from those that the client listens to.  By
	// redefining this function, a Client subclass can keep track of
	// the same information that otherwise must be computed by calling
	// AddInputMask() prior to each call to select().
	virtual void FD_Change( int fd, int add_flag );

	// Called asynchronously whenever a ping is received; the
	// interpreter sends a ping whenever a new event is ready and
	// the client was created with the <ping> attribute.
	virtual void HandlePing();

	// Removes a link.
	void UnlinkSink( Value* v );

	// Decodes an event value describing an output link and returns
	// a pointer to a (perhaps preexisting) corresponding EventLink.
	// "want_active" states whether the EventLink must be active
	// or inactive.  "is_new" is true upon return if a new EventLink
	// was created.  Nil is returned upon an error.
	EventLink* AddOutputLink( Value* v, int want_active, int& is_new );

	// Builds a link to the peer specified in "v", the originating
	// link event.  If the link already exists, just activates it.
	// If not, forwards an event telling the link peer how to connect
	// to us.
	void BuildLink( Value* v );

	// Accept a link request from the given socket.  Returns an
	// internal Glish event that can be handed to a caller of
	// NextEvent, which they (should) will then pass along to
	// Unrecognized.
	GlishEvent* AcceptLink( int sock );

	// Rendezvous with a link peer.  The first method is for when
	// we're the originator of the link, the second for when we're
	// responding.
	void RendezvousAsOriginator( Value* v );
	void RendezvousAsResponder( Value* v );

	// Sends the given event.  If sds is non-negative, the event's value
	// is taken from the SDS "sds".
	void SendEvent( const GlishEvent* e, int sds, const EventContext &context );
	void SendEvent( const GlishEvent* e, int sds = -1 )
		{ SendEvent( e, sds, last_context ); }


	void RemoveIncomingLink( int dead_event_source );
	void RemoveInterpreter( EventSource* source );

	const char* initial_client_name;
	int have_interpreter_connection;
	int no_glish;	// if true, no event source whatsoever

	char* pending_reply;	// the name of the pending reply, if any

	// All EventSources, keyed by context and typed
	PList(EventSource) event_sources;

	// Maps interpreter contexts to output link lists
	//
	// context_links is a PDICT of event_link_contexts_lists,
	//	keyed by context.
	//
	// event_link_context_list is a PDICT of event_link_lists,
	//	keyed by event name.
	//
	// event_link_list is a PLIST of EventLinks.

	PDict(event_link_context_list) context_links;

	// context_sinks and context_sources are PDICTs of sink_id_lists,
	//	keyed by context.
	PDict(sink_id_list) context_sinks;
	PDict(sink_id_list) context_sources;

	GlishEvent* last_event;

	// Previous signal handler; used for <ping>'s.
	glish_signal_handler former_handler;

	const char *local_host;
	char *interpreter_tag;

	// Context of last received event
	EventContext last_context;

	// Multithreaded or not?
	int multithreaded;

	// Use shared memory
	int useshm;
	};


extern GlishEvent* recv_event( int fd );

extern void send_event( int fd, const char* event_name,
			const GlishEvent* e, int sds = -1 );

inline void send_event( int fd, const GlishEvent* e, int sds = -1 )
	{
	send_event( fd, e->name, e, sds );
	}

inline void send_event( int fd, const char* name, const Value* value )
	{
	GlishEvent e( name, value );
	send_event( fd, &e );
	}

extern void send_shm_event( int write_fd, const char* event_name,
			const GlishEvent* e, int sds = -1 );

inline void send_shm_event( int write_fd, const GlishEvent* e, int sds = -1 )
	{
	send_shm_event( write_fd, e->name, e, sds );
	}

inline void send_shm_event( int write_fd, const char* name, const Value* value )
	{
	GlishEvent e( name, value );
	send_shm_event( write_fd, &e );
	}

extern GlishEvent* recv_shm_event( int shmid );

extern void clear_shared_memory();

#endif	/* client_h */
