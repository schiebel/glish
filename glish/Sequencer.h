// $Header$

#ifndef sequencer_h
#define sequencer_h

#include <stdio.h>

#include "Glish/Client.h"

#include "Stmt.h"
#include "Queue.h"


class Agent;
class UserAgent;
class GlishEvent;
class Frame;
class Notifiee;
class ScriptClient;
class Channel;

// Searches "system.include.path" for the given file; returns a malloc()'d copy
// of the path to the executable, which the caller should delete when
// done with.
char* which_include( const char* file_name );

class Notification : public GlishObject {
public:
	Notification( Agent* notifier, const char* field, Value* value,
			Notifiee* notifiee );
	~Notification();

	void Describe( ostream& s ) const;

	Agent* notifier;
	char* field;
	Value* value;
	Notifiee* notifiee;
	};


class Task;
class BuiltIn;
class AcceptSocket;
class AcceptSelectee;
class Selector;
class RemoteDaemon;

declare(PList,Frame);
declare(PDict,Task);
declare(PDict,RemoteDaemon);
declare(PDict,char);
declare(PQueue,Notification);

typedef PDict(Expr) expr_dict;

class Scope : public expr_dict {
public:
	Scope( scope_type s = LOCAL_SCOPE ) : scope(s), expr_dict() {}
	scope_type GetScopeType() const { return scope; }
	int WasGlobalRef(const char *c) const
		{ return global_refs.Lookup(c) ? 1 : 0; }
	void MarkGlobalRef(const char *c);
	void ClearGlobalRef(const char *c);
private:
	scope_type scope;
	Dict(int) global_refs;
};

declare(PList,Scope);
typedef PList(Scope) scope_list;
typedef PList(Frame) frame_list;
typedef List(int) offset_list;


class Sequencer {
public:
	Sequencer( int& argc, char**& argv );
	~Sequencer();

	char* Name()			{ return name; }

	void AddBuiltIn( BuiltIn* built_in );

	void QueueNotification( Notification* n );

	void PushScope( scope_type s = LOCAL_SCOPE );
	int PopScope();		// returns size of frame corresponding to scope
	scope_type GetScopeType( ) const;
	// For now returns the "global" scope. Later this may be modified
	// to take a "scope_type" parameter.
	Scope *GetScope( );

	Expr* InstallID( char* id, scope_type scope, int do_warn = 1,
					int GlobalRef = 0, int FrameOffset = 0 );
	Expr* LookupID( char* id, scope_type scope, int do_install = 1, int do_warn = 1 );

	Expr* InstallVar( char* id, scope_type scope, VarExpr *var );
	Expr* LookupVar( char* id, scope_type scope, VarExpr *var );

	// This function attempts to look up a value in the current sequencer.
	// If the value doesn't exist, null is returned.
	static const Value *LookupVal( const char *id );

	void PushFrame( Frame* new_frame );
	Frame* PopFrame();

	// The current evaluation frame, or 0 if there are no local frames.
	Frame* CurrentFrame();

	Value* FrameElement( scope_type scope, int scope_offset, int frame_offset );
	void SetFrameElement( scope_type scope, int scope_offset, int frame_offset,
				Value* value );

	// The last notification processed, or 0 if none received yet.
	Notification* LastNotification()	{ return last_notification; }

	// The last "whenever" statement executed, or 0 if none yet.
	Stmt* LastWheneverExecuted()	{ return last_whenever_executed; }

	// Register the given statement as the most recently-executed
	// whenever statement.
	void WheneverExecuted( Stmt* s )	{ last_whenever_executed = s; }

	// Registers a new task with the sequencer and returns its
	// task ID.
	char* RegisterTask( Task* new_task );

	// Unregister a task.
	void DeleteTask( Task* task );

	void AddStmt( Stmt* addl_stmt );

	// Register a statement; this assigns it a unique index that
	// can be used to retrieve the statement using LookupStmt().
	// Indexes are small positive integers.
	int RegisterStmt( Stmt* stmt );

	// Returns 0 if the index is invalid.
	Stmt* LookupStmt( int index );

	// Returns a non-zero value if the hostname is the local
	// host, and zero otherwise
	int LocalHost( const char* hostname );

	// Return a channel to a daemon for managing clients on the given host.
	// sets "err" to a non-zero value if an error occurs
	Channel* GetHostDaemon( const char* host, int &err );

	void Exec( int startup_script = 0 );

	// Wait for an event in which await_stmt has expressed interest,
	// though while waiting process any of the events in which
	// except_stmt has expressed interest (or all other events,
	// if "only_flag" is false).
	void Await( Stmt* await_list, int only_flag, Stmt* except_list );

	// Wait for a reply to the given event to arrive on the given
	// channel and return its value.
	Value* AwaitReply( Task* task, const char* event_name,
				const char* reply_name );

	// Inform the sequencer to expect a new, local (i.e., pipe-based)
	// client communicating via the given fd's.  Returns the channel
	// used from now on to communicate with the client.
	Channel* AddLocalClient( int read_fd, int write_fd );

	// Tells the sequencer that a new client has been forked.  This
	// call *must* be made before the sequencer returns to its event
	// loop, since it keeps track of the number of active clients,
	// and if the number falls to zero the sequencer exits.
	void NewClientStarted();

	// Waits for the given task to connect to the interpreter,
	// and returns the corresponding channel.
	Channel* WaitForTaskConnection( Task* task );

	// Open a new connection and return the task it now corresponds to.
	Task* NewConnection( Channel* connection_channel );

	void AssociateTaskWithChannel( Task* task, Channel *chan );

	int NewEvent( Task* task, GlishEvent* event );

	// Returns true if tasks associated with the given nam should
	// have an implicit "<suspend>" attribute, false otherwise.
	int ShouldSuspend( const char* task_var_ID );

	// Used to inform the sequencer that an event belonging to the
	// agent with the given id has been generated.  If is_inbound
	// is true then the event was generated by the agent and has
	// come into the sequence; otherwise, the event is being sent
	// to the agent.
	void LogEvent( const char* gid, const char* id, const char* event_name,
			const Value* event_value, int is_inbound );
	void LogEvent( const char* gid, const char* id, const GlishEvent* e,
			int is_inbound );

	// Report a "system" event; one that's reflected by the "system"
	// global variable.
	void SystemEvent( const char* name, const Value* val );

	// Read all of the events pending in a given task's channel input
	// buffer, being careful to stop (and delete the channel) if the
	// channel becomes invalid (this can happen when one of the events
	// causes the task associated with the channel to be deleted).
	// The final flag, if true, specifies that we should force a read
	// on the channel.  Otherwise we just consume what's already in the
	// channel's buffer.
	int EmptyTaskChannel( Task* task, int force_read = 0 );

	// Reads and responds to incoming events until either one of
	// the responses is to terminate (because an "exit" statement
	// was encountered, or because there are no longer any active
	// clients) or we detect keyboard activity (when running
	// interactively).
	void EventLoop();

	const char* ConnectionHost()	{ return connection_host; }
	const char* ConnectionPort()	{ return connection_port; }
	const char* InterpreterTag()	{ return interpreter_tag; }

	// Returns a non-zero value if there are existing clients.
	int ActiveClients() const	{ return num_active_processes > 0; }

	int MultiClientScript() { return multi_script; }
	int MultiClientScript( int set_to )
		{
		multi_script = set_to;
		return multi_script;
		}
	int DoingInit( ) { return doing_init; }
	int ScriptCreated( ) { return script_created; }
	int ScriptCreated( int set_to ) 
		{
		script_created = set_to;
		return script_created;
		}
	void InitScriptClient();

protected:
	void MakeEnvGlobal();
	void MakeArgvGlobal( char** argv, int argc );
	void BuildSuspendList();
	void Parse( FILE* file, const char* filename = 0 );
	void Parse( const char file[] );
	void Parse( const char* strings[] );
	RemoteDaemon* CreateDaemon( const char* host );
	// Sets err to a non-zero value if an error occurred
	RemoteDaemon* OpenDaemonConnection( const char* host, int &err );
	void ActivateMonitor( char* monitor_client );
	void Rendezvous( const char* event_name, Value* value );
	void ForwardEvent( const char* event_name, Value* value );
	void RunQueue();
	void RemoveSelectee( Channel* chan );

	char* name;
	int verbose;
	int my_id;

	UserAgent* system_agent;

	Expr *script_expr;
	ScriptClient* script_client;

	scope_list scopes;
	offset_list global_scopes;

	frame_list frames;
	offset_list global_frames;

	value_list global_frame;

	int last_task_id;
	PDict(Task) ids_to_tasks;
	PDict(RemoteDaemon) daemons;
	Dict(int) suspend_list;	// which variables' tasks to suspend
	PQueue(Notification) notification_queue;
	Notification* last_notification;
	Stmt* last_whenever_executed;
	Stmt* stmts;
	PList(Stmt) registered_stmts;

	Stmt* await_stmt;
	int await_only_flag;
	Stmt* except_stmt;

	// Task that we interrupted processing because we came across
	// an "await"-ed upon event; if non-null, should be Empty()'d
	// next time we process events.
	Task* pending_task;

	Task* monitor_task;

	AcceptSocket* connection_socket;
	Selector* selector;
	const char* connection_host;
	char* connection_port;
	char* interpreter_tag;

	int num_active_processes;

	// Used to indicate that the current script client should be
	// started as a multi-threaded client.
	int multi_script;
	// Used to indicate that the sequencer is in the initialization
	// phase of startup.
	int doing_init;
	int script_created;

	// These three values are used in the process of initializing
	// "script" value. This was complicated by "multi-threaded"
	// clients.
	int argc_;
	char **argv_;
	Value *sys_val;

	// Keeps track of the current sequencer...
	// Later this may have to be a stack...
	static Sequencer *cur_sequencer;
	};

#endif /* sequencer_h */
