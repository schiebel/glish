// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef task_h
#define task_h

#include "Agent.h"
#include "BuiltIn.h"
#include "Executable.h"
#include "Glish/Client.h"

class Channel;
class Selector;
class GlishEvent;

declare(PList,GlishEvent);
typedef PList(GlishEvent) glish_event_list;


class TaskAttr {
    public:
	TaskAttr( char* arg_ID, char* hostname, Channel* daemon_channel,
		  int async_flag, int ping_flag, int suspend_flag, const char *name_ = 0 );

	~TaskAttr();

	char* task_var_ID;
	char* hostname;
	Channel* daemon_channel;		// null for local invocation
	int async_flag;
	int ping_flag;
	int suspend_flag;
	int useshm;
	char *name;
	};


class Task : public Agent {
    public:
	Task( TaskAttr* task_attrs, Sequencer* s );
	~Task();

	void SendTerminate( );

	const char* Name() const	{ return name; }
	const char* TaskID() const	{ return id; }
	const char* Host() const	{ return attrs->hostname; }
	int NoSuchProgram() const	{ return no_such_program; }
	Executable* Exec() const	{ return executable; }
	int TaskError() const		{ return task_error; }

	// True if a .established has been seen; different from
	// Exec()->Active(), which is true if the executable has
	// be fired up and hasn't terminated yet
	int Active() const		{ return active; }


	// Send an event with the given name and associated values
	// to the associated task.
	IValue* SendEvent( const char* event_name, parameter_list* args,
			int is_request, int log );

	void SetActive()	{ SetActivity( 1 ); }
	void SetDone()		{ SetActivity( 0 );
				  if ( executable ) executable->DoneReceived(); }

	void SetChannel( Channel* c, Selector* s );
	void CloseChannel();
	Channel* GetChannel() const	{ return channel; }

	int Protocol() const	{ return protocol; }
	void SetProtocol( int arg_protocol )	{ protocol = arg_protocol; }

	Task* AgentTask();

	int DescribeSelf( OStream &s, charptr prefix = 0 ) const;

	virtual void sendEvent( sos_sink &fd, const char* event_name,
			const GlishEvent* e, int sds = -1 );

	void sendEvent( sos_sink &fd, const GlishEvent* e, int sds = -1 )
		{ sendEvent( fd, e->name, e, sds ); }

	void sendEvent( sos_sink &fd, const char* nme, const Value* value )
		{
		GlishEvent e( nme, value );
		sendEvent( fd, &e );
		}

    protected:
	// Creates and returns an argv vector with room for num_args arguments.
	// The program name and any additional, connection-related arguments
	// are added to the beginning and upon return "argc" holds the
	// total number of arguments.  Thus the non-connection arguments
	// can be put in positions argc-num_args through argc-1.
	//
	// The return value should be deleted when done with.
	const char** CreateArgs( const char* prog, int num_args, int& argc );

	void Exec( const char** argv );
	void SetActivity( int is_active );

	char* name;
	char* id;
	TaskAttr* attrs;
	Channel* channel;
	Channel* local_channel;	// used for local tasks
	int no_such_program;
	Selector* selector;

	// Events that we haven't been able to send out yet because
	// our associated task hasn't yet connected to the interpreter.
	glish_event_list* pending_events;

	Executable* executable;
	int task_error;	// true if any problems occurred
	int active;

	int protocol;	// which protocol the client speaks, or 0 if not known

	// Pipes used for local connections.
	int pipes_used;
	int read_pipe[2], write_pipe[2];
	char* read_pipe_str;
	char* write_pipe_str;
	};


class ShellTask : public Task {
    public:
	ShellTask( const_args_list* args, TaskAttr* task_attrs, Sequencer* s );
	};

class ClientTask : public Task {
    public:
	ClientTask( const_args_list* args, TaskAttr* task_attrs, Sequencer* s, int shm_flag=0 );
	void sendEvent( sos_sink &fd, const char* event_name, const GlishEvent* e, int sds = -1 );
    protected:
	int useshm;
	void CreateAsyncClient( const char** argv );
	};


class CreateTaskBuiltIn : public BuiltIn {
    public:
	CreateTaskBuiltIn( Sequencer* arg_sequencer )
	    : BuiltIn("create_task", NUM_ARGS_VARIES)
		{ sequencer = arg_sequencer; }

	IValue* DoCall( const_args_list* args_val );
	void DoSideEffectsCall( const_args_list* args_val,
				int& side_effects_okay );

    protected:
	// Extract a string from the given value, or if it indicates
	// "default" (by being equal to F) return 0.
	char* GetString( const IValue* val );

	// 'argv' represents the parameters to start a "script client",
	// this function goes through these arguments and expands the
	// path to the glish script.
	const char* ExpandScript( int start, const_args_list &args_val );

	// Execute the given command locally and synchronously, with the
	// given string as input, and return a Value holding the result.
	IValue* SynchronousShell( const char* command, const char* input );

	// Same except execute the command remotely.
	IValue* RemoteSynchronousShell( const char* command,
					IValue* input );

	// Read the shell output appearing on "shell" due to executing
	// "command" and return a Value representing the result.  Used
	// by SynchronousShell() and RemoteSynchronousShell().  "is_remote"
	// is a flag that if true indicates that the shell is executing
	// remotely.
	IValue* GetShellCmdOutput( const char* command, FILE* shell,
					int is_remote );

	// Read the next line from a local or remote (respectively)
	// synchronous shell command.
	char* NextLocalShellCmdLine( FILE* shell, char* line_buf );
	char* NextRemoteShellCmdLine( char* line_buf );

	// Create an asynchronous shell or client.
	IValue* CreateAsyncShell( const_args_list* args );
	IValue* CreateClient( const_args_list* args, int shm_flag );

	// Check to see whether the creation of the given task was
	// successful. Returns 0 if creation was successful, or an
	// error (non-zero pointer) if it failed.
	IValue *CheckTaskStatus( Task* task );

	Sequencer* sequencer;
	TaskAttr* attrs;	// attributes for task currently being created
	};


// True if both tasks are executing on the same host.
extern int same_host( Task* t1, Task* t2 );

#endif	/* task_h */
