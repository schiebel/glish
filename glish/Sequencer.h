// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef sequencer_h
#define sequencer_h
#include <stdio.h>
#include "Glish/Client.h"
#include "Stmt.h"
#include "Queue.h"
#include "Agent.h"

class UserAgent;
class GlishEvent;
class Frame;
class Notifiee;
class ScriptClient;
class Channel;

// Searches "system.include.path" for the given file; returns a malloc()'d copy
// of the path to the executable, which the caller should delete when
// done with.
extern char* which_include( const char* file_name );

// Attempt to retrieve the value associated with id. Returns 0 if the
// value is not found.
extern int lookup_print_precision( );
extern int lookup_print_limit( );

// This is used for notification of when an event has been handled, i.e.
// completion of a notification.
class NotifyTrigger : public GlishObject {
    public:
	virtual void NotifyDone( );
	NotifyTrigger() { }
	virtual ~NotifyTrigger();
};

class Notification : public GlishObject {
public:
	Notification( Agent* notifier, const char* field, IValue* value,
			Notifiee* notifiee, NotifyTrigger *t=0 );
	~Notification();

	void Describe( OStream& s ) const;

	Agent* notifier;
	char* field;
	IValue* value;
	Notifiee* notifiee;
	NotifyTrigger *trigger;

#ifdef GGC
	void TagGC( ) { if ( value ) value->TagGC(); }
#endif
	};


class Task;
class BuiltIn;
class AcceptSocket;
class AcceptSelectee;
class Selector;
class RemoteDaemon;
class awaitinfo;

glish_declare(PDict,Task);
glish_declare(PDict,RemoteDaemon);
glish_declare(PDict,char);
glish_declare(PQueue,Notification);
glish_declare(PList, awaitinfo);
glish_declare(PList, WheneverStmtCtor);

typedef PDict(Expr) expr_dict;
typedef PList(awaitinfo) awaitinfo_list;

class Scope : public expr_dict {
public:
	Scope( scope_type s = LOCAL_SCOPE ) : scope(s), expr_dict() {}
	~Scope();
	scope_type GetScopeType() const { return scope; }
	int WasGlobalRef(const char *c) const
		{ return global_refs.Lookup(c) ? 1 : 0; }
	void MarkGlobalRef(const char *c);
	void ClearGlobalRef(const char *c);
private:
	scope_type scope;
	PDict(char) global_refs;
};

glish_declare(PList,Scope);
typedef PList(Scope) scope_list;
typedef List(int) offset_list;

struct stack_type : GlishRef {
	stack_type( const stack_type &, int clip = 0 );
	stack_type( );
	~stack_type( );
	frame_list *frames;
	int flen;
	offset_list *offsets;
	int olen;
	int delete_on_spot;

#ifdef GGC
	void TagGC( );
#endif
};

glish_declare(PList,stack_type);
typedef PList(stack_type) stack_list;

extern void system_change_function(IValue *, IValue *);
class Sequencer;

class SystemInfo {
public:
	inline unsigned int TRACE( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<0; }
	inline unsigned int PRINTLIMIT( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<1; }
	inline unsigned int PRINTPRECISION( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<2; }
	inline unsigned int PATH( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<3; }
	inline unsigned int ILOGX( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<4; }
	inline unsigned int OLOGX( unsigned int mask=~((unsigned int) 0) ) const { return mask & 1<<4; }

	int Trace() { if ( TRACE(update) ) update_output( ); return trace; }
	int ILog() { if ( ILOGX(update) ) update_output( ); return ilog || log; }
	int OLog() { if ( OLOGX(update) ) update_output( ); return olog || log; }
	void DoILog( const char *s, int len=-1 ) { DoLog( 1, s, len ); }
	void DoILog( const Value *v) { DoLog( 1, v ); }
	void DoOLog( const char *s, int len=-1 ) { DoLog( 0, s, len ); }
	void DoOLog( const Value *v) { DoLog( 0, v ); }
	int PrintLimit() { if ( PRINTLIMIT(update) ) update_print( ); return printlimit; }
	int PrintPrecision() { if ( PRINTPRECISION(update) ) update_print( ); return printprecision; }
	charptr KeyDir() { if ( PATH(update) ) update_path( ); return keydir; }
	charptr *Include() { if ( PATH(update) ) update_path( ); return include; }
	int IncludeLen() { if ( PATH(update) ) update_path( ); return includelen; }
	const IValue *BinPath() { if ( PATH(update) ) update_path( ); return binpath; }
	SystemInfo( Sequencer *s ) : val(0), update( ~((unsigned int) 0) ), 
			log(0), log_val(0), log_file(0), log_name(0),
			ilog(0), ilog_val(0), ilog_file(0), ilog_name(0),
			olog(0), olog_val(0), olog_file(0), olog_name(0),
			printlimit(0), printprecision(-1), include(0), includelen(0),
			keydir(0), binpath(0), sequencer(s) { }
	void SetVal(IValue *v);
	~SystemInfo();
	void AbortOccurred();
private:
	void DoLog( int input, const char *s, int len=-1 );
	void DoLog( int input, const Value * );
	const char *prefix_buf(const char *prefix, const char *buf);
	void update_output( );
	void update_print( );
	void update_path( );
	IValue *val;
	int trace;

	int log;
	IValue *log_val;
	FILE *log_file;
	char *log_name;
	int olog;
	IValue *olog_val;
	FILE *olog_file;
	char *olog_name;
	int ilog;
	IValue *ilog_val;
	FILE *ilog_file;
	char *ilog_name;

	int printlimit;
	int printprecision;

	charptr *include;
	int includelen;
	const IValue *binpath;
	charptr keydir;

	unsigned int update;
	Sequencer *sequencer;

};
	

extern int glish_dummy_int;

class Sequencer {
public:
	friend class SystemInfo;
	Sequencer( int& argc, char**& argv );
	~Sequencer();

	const char* Name()		{ return name; }
	const char* Path()		{ return interpreter_path; }

	void AddBuiltIn( BuiltIn* built_in );

	void QueueNotification( Notification* n );
	int NotificationQueueLength ( ) { return notification_queue.length(); }

	void PushScope( scope_type s = LOCAL_SCOPE );
	int PopScope();		// returns size of frame corresponding to scope
	scope_type GetScopeType( ) const;
	// For now returns the "global" scope. Later this may be modified
	// to take a "scope_type" parameter.
	Scope *GetScope( );
	int ScopeDepth() const { return scopes.length(); }

	Expr* InstallID( char* id, scope_type scope, int do_warn = 1,
					int GlobalRef = 0, int FrameOffset = 0,
					change_var_notice f=0 );
	// "local_search_all" is used to indicate if all local scopes should be
	// searched or if *only* the "most local" scope should be searched. This
	// is only used if "scope==LOCAL_SCOPE".
	Expr* LookupID( char* id, scope_type scope, int do_install = 1, int do_warn = 1,
			int local_search_all=1 );

	Expr* InstallVar( char* id, scope_type scope, VarExpr *var );
	Expr* LookupVar( char* id, scope_type scope, VarExpr *var,
			 int &created = glish_dummy_int );

	static Sequencer *CurSeq( );
	static const AwaitStmt *ActiveAwait( );

	SystemInfo &System() { return system; }

	// In the integration of Tk, the Tk event loop is called in the
	// process of handling glish events to the Tk widgets. It is
	// necessary to prevent the Sequencer::RunQueue() from running.
	static void HoldQueue( );
	static void ReleaseQueue( );

	// This function attempts to look up a value in the current sequencer.
	// If the value doesn't exist, null is returned.
	static const IValue *LookupVal( const char *id );
	// Deletes a given global value. This is used by 'symbol_delete()' which
	// is the only way to get rid of a 'const' value.
	void DeleteVal( const char* id );

	void DescribeFrames( OStream& s ) const;
	void PushFrame( Frame* new_frame );
	void PushFrames( stack_type *new_stack );
	// Note that only the last frame popped is returned (are the others leaked?).
	Frame* PopFrame( );
	void PopFrames( );

	// This trio of functions supports keeping track of the
	// function call stack. This is used in error reporting.
	void PushFuncName( char *name );
	void PopFuncName( );
	static IValue *FuncNameStack( );

	// The current evaluation frame, or 0 if there are no local frames.
	Frame* CurrentFrame();
	// Returns a list of the frames that are currently local, i.e. up to and
	// and including the first non LOCAL_SCOPE frame. Returns 0 if there are
	// no frames between the current frame and the GLOBAL_SCOPE frame. If this
	// list is to be kept, the frames must be Ref()ed. The returned list is
	// dynamically allocated, though.
	stack_type* LocalFrames();

	IValue* FrameElement( scope_type scope, int scope_offset, int frame_offset );
	// returns error message
	const char *SetFrameElement( scope_type scope, int scope_offset,
				     int frame_offset, IValue* value, change_var_notice f=0 );

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
	void ClearStmt( );
	Stmt *GetStmt( );

	// Register a statement; this assigns it a unique index that
	// can be used to retrieve the statement using LookupStmt().
	// Indexes are small positive integers.
	int RegisterStmt( Stmt* stmt );
	void UnregisterStmt( Stmt* stmt );

	// Returns 0 if the index is invalid.
	Stmt* LookupStmt( int index );

	// Returns a non-zero value if the hostname is the local
	// host, and zero otherwise
	int LocalHost( const char* hostname );

	// Return a channel to a daemon for managing clients on the given host.
	// sets "err" to a non-zero value if an error occurs
	Channel* GetHostDaemon( const char* host, int &err );

	IValue *Exec( int startup_script = 0, int value_needed = 0 );
	IValue *Eval( const char* strings[] );
	IValue *Include( const char* file );
	// called when "pragma include once" is used
	void IncludeOnce( );

	static void SetErrorResult( IValue *err );
	void ClearErrorResult()
		{ Unref(error_result); error_result = 0; }
	const IValue *ErrorResult() const { return error_result; }

	// Wait for an event in which await_stmt has expressed interest,
	// though while waiting process any of the events in which
	// except_stmt has expressed interest (or all other events,
	// if "only_flag" is false).
	void Await( AwaitStmt* await_list, int only_flag, Stmt* except_list );

	// Wait for a reply to the given event to arrive on the given
	// channel and return its value.
	IValue* AwaitReply( Task* task, const char* event_name,
				const char* reply_name );

	// In sending an event, the sending was discontinued. Most likely
	// because the pipe filled up. The fd being written to should be
	// monitored, and sending resumed when possible. Value is just a
	// copy (reference counted) and is deleted when the send completes.
	void SendSuspended( sos_status *, Value * );

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

	int NewEvent( Task* task, GlishEvent* event, int complain_if_no_interest = 0,
		      NotifyTrigger *t=0 );
	int NewEvent( Agent* agent, GlishEvent* event, int complain_if_no_interest = 0,
		      NotifyTrigger *t=0 );
	int NewEvent( Agent* agent, const char* event_name, IValue* value,
		      int complain_if_no_interest = 0, NotifyTrigger *t=0 );

	// Returns true if tasks associated with the given nam should
	// have an implicit "<suspend>" attribute, false otherwise.
	int ShouldSuspend( const char* task_var_ID );

	// Used to inform the sequencer that an event belonging to the
	// agent with the given id has been generated.  If is_inbound
	// is true then the event was generated by the agent and has
	// come into the sequence; otherwise, the event is being sent
	// to the agent.
	void LogEvent( const char* gid, const char* id, const char* event_name,
			const IValue* event_value, int is_inbound );
	void LogEvent( const char* gid, const char* id, const GlishEvent* e,
			int is_inbound );

	// With UserAgents, the event handling never goes through
	// Sequencer::NewEvent(), so Agent need to be able
	void CheckAwait( Agent* agent, const char* event_name );

	// Report a "system" event; one that's reflected by the "system"
	// global variable.
	void SystemEvent( const char* name, const IValue* val );

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
	void EventLoop( int in_await = 0 );

	const char* ConnectionHost()	{ return connection_host; }
	const char* ConnectionPort()	{ return connection_port; }
	const char* InterpreterTag()	{ return interpreter_tag; }

	// Returns a non-zero value if there are existing clients.
	int ActiveClients() const	{ return num_active_processes > 0; }

	Client::ShareType MultiClientScript() { return multi_script; }
	Client::ShareType MultiClientScript( Client::ShareType set_to )
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
	void InitScriptClient( );

	static void TopLevelReset();

	void UpdateLocalBinPath( );
	void UpdateRemoteBinPath( );
	//
	// host=0 implies local bin path
	//
	void UpdateBinPath( const char *host = 0 );

#ifdef GGC
	//
	// go through all values and delete any which aren't
	// referenced via the globals
	//
	void CollectGarbage( );

	//
	// register ivalues to be preserved
	//
	void RegisterValue( IValue *v )
		{ registered_values.append(v); }
	void UnregisterValue( IValue *v )
		{ registered_values.remove(v); }
#endif

	//
	// register current whenever ctor, for use in
	// activate/deactivate stmts local to the whenever
	//
	void RegisterWhenever( WheneverStmtCtor *ctor ) { cur_whenever.append(ctor); }
	void UnregisterWhenever( ) { int len = cur_whenever.length();
				     if ( len > 0 ) cur_whenever.remove_nth(len-1); }
	void ClearWhenevers( );

	int CurWheneverIndex( );

protected:
	void MakeEnvGlobal();
	void MakeArgvGlobal( char** argv, int argc, int append_name=0 );
	void BuildSuspendList();
	IValue *Parse( FILE* file, const char* filename = 0, int value_needed=0 );
	IValue *Parse( const char file[], int value_needed=0 );
	IValue *Parse( const char* strings[], int value_needed=0 );

	RemoteDaemon* CreateDaemon( const char* host );
	// Sets err to a non-zero value if an error occurred
	RemoteDaemon* OpenDaemonConnection( const char* host, int &err );
	void ActivateMonitor( char* monitor_client );
	void Rendezvous( const char* event_name, IValue* value );
	void ForwardEvent( const char* event_name, IValue* value );
	void RunQueue();
	void RemoveSelectee( Channel* chan );

	void SetupSysValue( IValue * );

	void PushAwait( );
	void PopAwait();
	void CurrentAwaitDone();

	char *name;
	char *interpreter_path;
	int verbose;
	int my_id;

	void SystemChanged( );
	unsigned int system_change_count;
	SystemInfo system;

	UserAgent* system_agent;

	Expr *script_expr;
	ScriptClient* script_client;
	int script_client_active;

	scope_list scopes;
	offset_list global_scopes;

	stack_list stack;
	const frame_list &frames() const { return *(stack[stack.length()-1]->frames); }
	frame_list &frames() { return *(stack[stack.length()-1]->frames); }
	const offset_list &global_frames() const { return *(stack[stack.length()-1]->offsets); }
	offset_list &global_frames() { return *(stack[stack.length()-1]->offsets); }

	ivalue_list global_frame;

	name_list func_names;

	int last_task_id;
	PDict(Task) ids_to_tasks;
	PDict(RemoteDaemon) daemons;
	Dict(int) suspend_list;	// which variables' tasks to suspend
	PQueue(Notification) notification_queue;
	Notification* last_notification;
	Stmt* last_whenever_executed;
	Stmt* stmts;
	PList(Stmt) registered_stmts;
	Dict(int) include_once;
	char *expanded_name;

#ifdef GGC
	ivalue_list registered_values;
#endif

	Stmt* await_stmt;
	agent_dict* await_dict;
	int await_only_flag;
	Stmt* except_stmt;
	awaitinfo_list await_list;
	awaitinfo *last_await_info;
	int current_await_done;

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
	Client::ShareType multi_script;
	// Used to indicate that the sequencer is in the initialization
	// phase of startup.
	int doing_init;
	int script_created;

	// These three values are used in the process of initializing
	// "script" value. This was complicated by "multi-threaded"
	// clients.
	int argc_;
	char **argv_;
	IValue *sys_val;

	char *run_file;

	IValue *error_result;

	// Keeps track of the current sequencer...
	// Later this may have to be a stack...
	static Sequencer *cur_sequencer;
	static int hold_queue;

	//
	// handling for the current whenever stmt
	//
	PList(WheneverStmtCtor) cur_whenever;

	// Called from Sequencer::TopLevelReset()
	void toplevelreset();
	};

extern IValue *glish_parser( Stmt *&stmt );

#endif /* sequencer_h */
