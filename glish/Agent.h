// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997,1998 Associated Universities Inc.
#ifndef agent_h
#define agent_h

#include "Stmt.h"

class IValue;
class Sequencer;
class Frame;
class Task;
class Agent;
class NotifyTrigger;
class stack_type;

#define string_to_void void_ptr
#define void_to_string string
glish_declare(List,string);
typedef List(string) string_list;

glish_declare(PList,Agent);
typedef PList(Agent) agent_list;

class Notifiee : public GlishRef {
    public:
	Notifiee( Stmt* arg_stmt, Sequencer* );
	Notifiee( Stmt* arg_stmt, Frame* arg_frame, Sequencer* );
	Notifiee( Stmt* arg_stmt, stack_type *arg_stack, Sequencer* );
	~Notifiee();
	Stmt *stmt() { return stmt_; }
	Frame *frame() { return frame_; }
	stack_type *stack() { return stack_; }
    protected:
	Notifiee( const Notifiee & );
	Notifiee &operator=( const Notifiee & );
	Stmt* stmt_;
	Frame* frame_;
	stack_type *stack_;
	Sequencer *sequencer;
	};

glish_declare(PList,Notifiee);
typedef PList(Notifiee) notifiee_list;

glish_declare(PDict,notifiee_list);
typedef PDict(notifiee_list) notification_dict;

glish_declare(PDict,agent_list);
typedef PDict(agent_list) agent_dict;
extern void delete_agent_dict( agent_dict * );

class Agent : public GlishObject {
    public:
	Agent( Sequencer* s );
	virtual ~Agent();

	virtual const char* Name() const { return agent_ID ? agent_ID : "<agent>"; }

	// Send the given event/value list or pair to the event agent.
	// If is_request is true then this is a request/response event,
	// and the response value is returned; otherwise nil is returned.
	// If log is true and the sequencer is logging events, then
	// the event is logged.
	virtual IValue* SendEvent( const char* event_name, parameter_list* args,
				int is_request, int log ) = 0;

	// Same for an event with just one value.
	void SendSingleValueEvent( const char* event_name, const IValue* value,
					int log );

	// Creates a event/value pair associated with this agent (the
	// event is not sent to the associated agent).  Returns true
	// if there was some interest in the event, false otherwise.
	virtual int CreateEvent( const char* event_name, IValue* event_value,
				 NotifyTrigger *t=0 );

	// Register the given statement as being interested in events
	// generated by this event agent.  If field is non-null then only
	// the event .field is of interest; if is_copy is true then
	// the string "field" is the event agent's own copy (and thus
	// should be deleted when the event agent is).
	void RegisterInterest( Notifiee* notifiee, const char* field = 0,
				int is_copy = 0 );

	// Undo RegisterInterest()
	void UnRegisterInterest( Stmt* s, const char* field = 0 );

	//
	// Statements to be Unref'ed when this agent goes away...
	//
	void RegisterUnref( Stmt *s );
	//
	// Avoid Unref'ing, but caller must Unref 's' if it is no
	// longer needed...
	//
	void UnRegisterUnref( Stmt *s );

	// True if the given statement has expressed interested in
	// this Agent for the given field (or for all fields).
	int HasRegisteredInterest( Stmt* stmt, const char* field );

	// Returns a Value object listing this agent's "whenever"
	// statements.  The value consists of a record with two fields,
	// "event", which is a string array of event names, and
	// "stmt", the indices of the corresponding "whenever" statements.
	// Note that "event" might include more than one entry with the
	// same name; the pairs <event, stmt> are unique, though.
	//
	// The special event name "*" corresponds to statements interested
	// in all events this agent generates (e.g., "whenever foo->* do ...").
	//
	// The return Value should be Unref()'d when done with it.
	IValue* AssociatedStatements();

	// Return the Task version of this Agent, or nil if this
	// agent is not a task.
	virtual Task* AgentTask();

	const char* AgentID() const { return agent_ID ? agent_ID : "<agent>"; }
	IValue* AgentRecord()	    { return agent_value; }

	int Describe( OStream &s, const ioOpt &opt ) const;
	int Describe( OStream &s ) const
		{ return Describe( s, ioOpt() ); }

	// Sometimes the wrapper of a Task (subclass of Agent) can go
	// out-of-scope and be deleted, but the Task sticks around
	// until the client terminates (right?)
	virtual void WrapperGone( const IValue *v );

	// Called when the agent is finished. It unrefs dangling
	// statements etc.
	virtual void Done( );

	// This returns true for proxy agents...
	virtual int IsProxy( ) const;


	// This is a hack for the Tk widgets
	virtual int IsPseudo( ) const;

    protected:
	// Should the notifications generated by this agent be sticky, i.e.
	// persist through function returns. Useful for subsequences...
	virtual int StickyNotes( ) const;

	IValue* BuildEventValue( parameter_list* args, int use_refs );

	// Queues notification of those parties interested in the given
	// .field event, where "value" is the value associated with the
	// event.
	//
	// Returns true if there were some such parties, false otherwise.
	int NotifyInterestedParties( const char* field, IValue* value,
				 NotifyTrigger *t=0 );

	// Queues notification of the given Notifiee, if it's active.  Returns
	// true if the Notifiee was active, false otherwise.
	int DoNotification( Notifiee* n, const char* field, IValue* value,
				 NotifyTrigger *t=0 );

	// Returns true if the given statement is on the given notification
	// list, false otherwise.
	int SearchNotificationList( notifiee_list* list, Stmt* stmt );

	Sequencer* sequencer;

	notification_dict interested_parties;
	string_list* string_copies;

	stmt_list unref_stmts;

	const char* agent_ID;
	IValue* agent_value;
	};


class UserAgent : public Agent {
    public:
	UserAgent( Sequencer* s ) : Agent(s)		{ }

	// Send an event with the given name and associated values
	// to the associated user agent.
	IValue* SendEvent( const char* event_name, parameter_list* args,
			int is_request, int log );
    protected:
	int StickyNotes( ) const;
	};


// Agents currently in existence.
extern agent_list *agents;

#endif	/* agent_h */
