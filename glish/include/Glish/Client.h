// $Header$

#ifndef client_h
#define client_h

#include <signal.h>
#include <generic.h>
#include <sys/types.h>

#include "Glish/Value.h"

#ifdef __GNUG__
typedef SignalHandler SigHandler;
#else
typedef SIG_PF SigHandler;
#endif


class GlishEvent : public GlishObject {
    public:
	GlishEvent( char* arg_name, Value* arg_value )
		{ name = arg_name; value = arg_value; }

	~GlishEvent()
		{ delete name; Unref( value ); }

	char* name;
	Value* value;
	};


declare(List,int);

// Holds information regarding outbound "link" commands.
class EventLink;

declare(PList,EventLink);
declare(PDict,EventLink);

typedef PList(EventLink) event_link_list;
declare(PDict,event_link_list);

declare(Dict,int);


class AcceptSocket;

class Client {
    public:
	// Client's are constructed by giving them the program's
	// argc and argv.  Any client-specific arguments are read
	// and stripped off.
	Client( int& argc, char** argv );

	// Alternatively, a Client can be constructed from fd's for
	// reading and writing events and a client name.  This version
	// of the constructor does not generate an initial "established"
	// event.
	Client( int client_read_fd, int client_write_fd, const char* name );

	virtual ~Client();

	// Wait for the next event to arrive and return a pointer to 
	// it.  The GlishEvent will be Unref()'d on the next call to
	// NextEvent(), so if the caller wishes to keep the GlishEvent
	// (or its Value) they must Ref() the GlishEvent (or its Value).
	//
	// If the sequencer connection has been broken then 0 is returned
	// (and the caller should terminate).
	GlishEvent* NextEvent();

	// Another version of NextEvent which can be passed an fd_set
	// returned by select() to aid in determining from where to
	// read the next event.
	GlishEvent* NextEvent( fd_set* mask );

	// Called by the main program (or whoever called NextEvent()) when
	// the current event is unrecognized.
	void Unrecognized();


	// Sends an event with the given name and value.
	void PostEvent( GlishEvent* event );
	void PostEvent( const char* event_name, const Value* event_value );

	// Sends an event with the given name and character string value.
	void PostEvent( const char* event_name, const char* event_value );

	// Sends an event with the given name, using a printf-style format
	// and an associated string argument.  For example,
	//
	//	client->PostEvent( "error", "couldn't open %s", file_name );
	//
	void PostEvent( const char* event_name, const char* event_fmt,
				const char* event_arg );

	// Reply to the last received event.
	void Reply( const Value* event_value );

	// Post an event with an "opaque" SDS value - one that we won't
	// try to convert to a Glish record.
	void PostOpaqueSDS_Event( const char* event_name, int sds );


	// For any file descriptors this Client might read events from,
	// sets the corresponding bits in the passed fd_set.  The caller
	// may then use the fd_set in a call to select().
	void AddInputMask( fd_set* mask );

	// Returns true if the following mask indicates that input
	// is available for the client.
	int HasClientInput( fd_set* mask );

	// Returns true if the client was invoked by a Glish sequencer,
	// false if the client is running standalone.
	int HasSequencerConnection()
		{ return int(have_sequencer_connection); }

	// Returns true if the client has *some* event source, either
	// the sequencer or stdin; false if not.
	int HasEventSource()	{ return ! int(no_glish); }

    protected:
	friend void Client_signal_handler();

	// Performs Client initialization that doesn't depend on the
	// arguments with which the client was invoked.
	void ClientInit();

	// Creates the necessary signal handler for dealing with client
	// "ping"'s.
	void CreateSignalHandler();

	// Sends out the Client's "established" event.
	void SendEstablishedEvent();

	// Returns the next event from the given fd.
	GlishEvent* GetEvent( int fd );

	// Called whenever a new fd is added (add_flag=true) or deleted
	// (add_flag=false) from those that the client listens to.  By
	// redefining this function, a Client subclass can keep track of
	// the same information that otherwise must be computed by calling
	// AddInputMask() prior to each call to select().
	virtual void FD_Change( int fd, bool add_flag );

	// Called asynchronously whenever a ping is received; the
	// sequencer sends a ping whenever a new event is ready and
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

	// Sends an event with the given name and value.  If the value
	// is nil then the value is instead taken from the SDS "sds".
	void SendEvent( const char* event_name, const Value* event_value,
			int sds );


	const char* client_name;
	int read_fd;
	int write_fd;
	bool have_sequencer_connection;
	bool no_glish;	// if true, don't have any event source whatsoever

	char* pending_reply;	// the name of the pending reply, if any

	List(int) input_links;

	// Maps event names to EventLink's describing where to send
	// those events.
	PDict(event_link_list) output_links;	// keyed on event name

	// Maps remote client id's to socket fds so we can use a single
	// socket for all links to that client.
	Dict(int) remote_sources;
	Dict(int) remote_sinks;

	GlishEvent* last_event;

	// Previous signal handler; used for <ping>'s.
	SigHandler former_handler;

	char local_host[64];
	};


extern GlishEvent* recv_event( int fd );

extern void send_event( int fd, const char* event_name,
			const Value* event_value, int sds = -1 );

#endif	/* client_h */
