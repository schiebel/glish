// $Header$

#ifndef agent_h
#define agent_h

#include "Stmt.h"

class Value;
class Sequencer;
class Frame;
class Task;
class Agent;

declare(List,string);
typedef List(string) string_list;

declare(PList,Agent);
typedef PList(Agent) agent_list;


class Notifiee {
    public:
	Notifiee( Stmt* arg_stmt, Frame* arg_frame );
	~Notifiee();

	Stmt* stmt;
	Frame* frame;
	};

declare(PList,Notifiee);
typedef PList(Notifiee) notification_list;

declare(PDict,notification_list);
typedef PDict(notification_list) notification_dict;


class Agent : public GlishObject {
    public:
	Agent( Sequencer* s );
	~Agent();

	// Send the given event/value list or pair to the event agent.
	// If is_request is true then this is a request/response event,
	// and the response value is returned; otherwise nil is returned.
	// If log is true and the sequencer is logging events, then
	// the event is logged.
	virtual Value* SendEvent( const char* event_name, parameter_list* args,
				bool is_request, bool log ) = 0;

	// Same for an event with just one value.
	void SendSingleValueEvent( const char* event_name, const Value* value,
					bool log );

	// Creates a event/value pair associated with this agent (the
	// event is not sent to the associated agent).  Returns true
	// if there was some interest in the event, false otherwise.
	virtual bool CreateEvent( const char* event_name, Value* event_value );

	// Register the given statement as being interested in events
	// generated by this event agent.  If field is non-null then only
	// the event .field is of interest; if is_copy is true then
	// the string "field" is the event agent's own copy (and thus
	// should be deleted when the event agent is).
	void RegisterInterest( Notifiee* notifiee, const char* field = 0,
				bool is_copy = false );


	// True if the given statement has expressed interested in
	// this Agent for the given field (or for all fields).
	bool HasRegisteredInterest( Stmt* stmt, const char* field );

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
	Value* AssociatedStatements();

	// Return the Task version of this Agent, or nil if this
	// agent is not a task.
	virtual Task* AgentTask();

	const char* AgentID()	{ return agent_ID; }
	Value* AgentRecord()	{ return agent_value; }

	void DescribeSelf( ostream& s ) const;

    protected:
	Value* BuildEventValue( parameter_list* args, bool use_refs );

	// Queues notification of those parties interested in the given
	// .field event, where "value" is the value associated with the
	// event.
	//
	// Returns true if there were some such parties, false otherwise.
	bool NotifyInterestedParties( const char* field, Value* value );

	// Queues notification of the given Notifiee, if it's active.  Returns
	// true if the Notifiee was active, false otherwise.
	bool DoNotification( Notifiee* n, const char* field, Value* value );

	// Returns true if the given statement is on the given notification
	// list, false otherwise.
	bool SearchNotificationList( notification_list* list, Stmt* stmt );

	Sequencer* sequencer;

	notification_dict interested_parties;
	string_list* string_copies;

	const char* agent_ID;
	Value* agent_value;
	};


class UserAgent : public Agent {
    public:
	UserAgent( Sequencer* s ) : Agent(s)		{ }

	// Send an event with the given name and associated values
	// to the associated user agent.
	Value* SendEvent( const char* event_name, parameter_list* args,
			bool is_request, bool log );
	};


// Agents currently in existence.
extern agent_list agents;

#endif	/* agent_h */
