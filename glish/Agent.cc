// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997,1998 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include <iostream.h>
#include <stdio.h>
#include <string.h>

#include "Agent.h"
#include "Frame.h"
#include "IValue.h"
#include "Reporter.h"
#include "Sequencer.h"


#define INTERESTED_IN_ALL "*"


agent_list *agents = 0;


Notifiee::Notifiee( Stmt* arg_stmt, Sequencer *s ) :
			frame_(0), stack_(0), sequencer(s)
	{
	stmt_ = arg_stmt;
	Ref( stmt_ );
	}

Notifiee::Notifiee( Stmt* arg_stmt, Frame* arg_frame, Sequencer *s ) :
			stack_(0), sequencer(s)
	{
	stmt_ = arg_stmt;
	frame_ = arg_frame;

	Ref( stmt_ );

	if ( frame_ )
		Ref( frame_ );
	}

Notifiee::Notifiee( Stmt* arg_stmt, stack_type *arg_stack, Sequencer *s ) :
			frame_(0), sequencer(s)
	{
	stmt_ = arg_stmt;
	stack_ = arg_stack;

	if ( stack_ ) Ref( stack_ );
	Ref( stmt_ );
	}

Notifiee::~Notifiee()
	{
	sequencer->NotifieeDone( this );
	Unref( stmt_ );
	Unref( frame_ );
	Unref( stack_ );
	}

Agent::Agent( Sequencer* s )
	{
	sequencer = s;
	agent_ID = 0;
	agent_value = 0;
	string_copies = 0;

	agent_value = new IValue( create_record_dict(), this );
	(*agents).append( this );
	}

void Agent::Done()
	{
	IterCookie* c = interested_parties.InitForIteration();

	notifiee_list* list;
	const char* key;
	while ( (list = interested_parties.NextEntry( key, c )) )
		{
		loop_over_list( *list, i )
			Unref( (*list)[i] );
		delete list;
		interested_parties.Remove( key );
		}

	for ( int i=unref_stmts.length()-1; i >= 0; --i )
		NodeUnref( unref_stmts.remove_nth(i) );
	}

Agent::~Agent()
	{
	(void) (*agents).remove( this );

	Done();

	if ( string_copies )
		{
		loop_over_list( *string_copies, i )
			{
			char* str = (char*) (*string_copies)[i];
			free_memory( str );
			}

		delete string_copies;
		}	
	}

void Agent::SendSingleValueEvent( const char* event_name, const IValue* value,
					int log )
	{
	ConstExpr c( value ); Ref( (IValue*) value );
	Parameter p((const char *) "event_in", VAL_VAL, &c, 0 ); Ref( &c );
	parameter_list plist;
	plist.append( &p );

	SendEvent( event_name, &plist, 0, log );
	}

int Agent::CreateEvent( const char* event_name, IValue* event_value,
			NotifyTrigger *t )
	{
	if ( ! agent_value )
		return 0;

	return NotifyInterestedParties( event_name, event_value, t );
	}

void Agent::RegisterInterest( Notifiee* notifiee, const char* field,
					int is_copy )
	{
	if ( ! field )
		field = INTERESTED_IN_ALL;

	notifiee_list* interest_list = interested_parties[field];

	if ( ! interest_list )
		{
		interest_list = new notifiee_list;
		interested_parties.Insert( field, interest_list );
		}

	Ref( notifiee );
	interest_list->append( notifiee );

	if ( is_copy )
		{
		// We need to remember the field since we're responsible
		// for garbage-collecting it when we're destructed.

		if ( ! string_copies )
			string_copies = new string_list;

		string_copies->append( field );
		}
	}

void Agent::UnRegisterInterest( Stmt* s, const char* field )
	{
	if ( ! field )
		field = INTERESTED_IN_ALL;

	notifiee_list* list = interested_parties[field];

	if ( ! list )
		return;

	int element = -1;
	loop_over_list( *list, i )
		if ( (*list)[i]->stmt() == s )
			{
			element = i;
			break;
			}

	if ( element >= 0 )
		Unref( list->remove_nth(element) );

	}

int Agent::HasRegisteredInterest( Stmt* stmt, const char* field )
	{
	notifiee_list* interest = interested_parties[field];

	if ( interest && SearchNotificationList( interest, stmt ) )
		return 1;

	interest = interested_parties[INTERESTED_IN_ALL];

	if ( interest && SearchNotificationList( interest, stmt ) )
		return 1;

	return 0;
	}

IValue* Agent::AssociatedStatements()
	{
	int num_stmts = 0;

	IterCookie* c = interested_parties.InitForIteration();
	const char* key;
	notifiee_list* interest;

	while ( (interest = interested_parties.NextEntry( key, c )) )
		num_stmts += interest->length();

	char** event = (char**) alloc_memory( sizeof(char*)*num_stmts );
	int* stmt = (int*) alloc_memory( sizeof(int)*num_stmts );
	glish_bool* active = (glish_bool*) alloc_memory( sizeof(glish_bool)*num_stmts );
	int count = 0;

	c = interested_parties.InitForIteration();
	while ( (interest = interested_parties.NextEntry( key, c )) )
		{
		loop_over_list( *interest, j )
			{
			event[count] = strdup( key );
			stmt[count] = (*interest)[j]->stmt()->Index();
			active[count] = (*interest)[j]->stmt()->GetActivity() ?
							glish_true : glish_false;
			++count;
			}
		}

	if ( count != num_stmts )
		fatal->Report(
		"internal inconsistency in Agent::AssociatedStatements" );

	IValue* r = create_irecord();
	r->SetField( "event", (charptr*) event, num_stmts );
	r->SetField( "stmt", stmt, num_stmts );
	r->SetField( "active", active, num_stmts );

	return r;
	}

Task* Agent::AgentTask()
	{
	return 0;
	}

int Agent::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.prefix() ) s << opt.prefix();
	if ( agent_ID )
		s << agent_ID;
	else
		s << "<agent>";
	return 1;
	}

void Agent::WrapperGone( const IValue *v )
	{
	if ( agent_value == v ) agent_value = 0;
	}

int Agent::StickyNotes( ) const
	{
	return 0;
	}

IValue* Agent::BuildEventValue( parameter_list* args, int use_refs )
	{
	if ( args->length() == 0 )
		return false_ivalue();

	if ( args->length() == 1 && ! (*args)[0]->Name() )
		{
		Expr* arg_expr = (*args)[0]->Arg();

		return use_refs ? arg_expr->RefEval( VAL_CONST ) :
					arg_expr->CopyEval();
		}

	// Build up a record.
	IValue* event_val = create_irecord();

	loop_over_list( *args, i )
		{
		Parameter* p = (*args)[i];
		const char* index;
		char buf[64];
		
		if ( p->Name() )
			index = p->Name();
		else
			{
			sprintf( buf, "arg%d", i + 1 );
			index = buf;
			}

		Expr* arg_expr = p->Arg();
		IValue* arg_val = use_refs ?
			arg_expr->RefEval( VAL_CONST ) : arg_expr->CopyEval();

		event_val->AssignRecordElement( index, arg_val );

		Unref( arg_val );
		}

	return event_val;
	}

int Agent::NotifyInterestedParties( const char* field, IValue* value,
				    NotifyTrigger *t )
	{
	notifiee_list* interested = interested_parties[field];
	int there_is_interest = 0;

	if ( interested )
		{
		loop_over_list( *interested, i )
			{
			// We ignore DoNotification's return value, for now;
			// we consider that the Notifiee exists, even if not
			// active, sufficient to consider that there was
			// interest in this event.
			(void) DoNotification( (*interested)[i], field, value, t );
			}

		there_is_interest = 1;
		}

	interested = interested_parties[INTERESTED_IN_ALL];

	if ( interested )
		{
		loop_over_list( *interested, i )
			(void) DoNotification( (*interested)[i], field, value, t );

		there_is_interest = 1;
		}

	if ( agent_value && ! there_is_interest && agent_value->Type() == TYPE_RECORD )
		{
		// We have to assign the corresponding field in the agent
		// record right here, ourselves, since the sequencer isn't
		// going to do it for us in Sequencer::RunQueue.
		agent_value->AssignRecordElement( field, value );
		}

	Unref( value );
	Unref( t );

	return there_is_interest;
	}

int Agent::DoNotification( Notifiee* n, const char* field, IValue* value,
			   NotifyTrigger *t )
	{
	Stmt* s = n->stmt();

	if ( s->IsActiveFor( this, field, value ) )
		{
		if ( t ) Ref(t);

		Notification* note = new Notification( this, field, value, n, t,
			StickyNotes() ? Notification::STICKY : Notification::WHENEVER );

		sequencer->QueueNotification( note );

		return 1;
		}

	else
		return 0;
	}

int Agent::SearchNotificationList( notifiee_list* list, Stmt* stmt )
	{
	loop_over_list( *list, i )
		if ( (*list)[i]->stmt() == stmt )
			return 1;

	return 0;
	}

void Agent::RegisterUnref( Stmt *s )
	{
	if ( ! unref_stmts.is_member(s) )
		{
		Ref( s );
		unref_stmts.append( s );
		}
	}

void Agent::UnRegisterUnref( Stmt *s )
	{
	unref_stmts.remove(s);
	}

int Agent::IsProxy( ) const
	{
	return 0;
	}

int Agent::IsPseudo( ) const
	{
	return 0;
	}

IValue* UserAgent::SendEvent( const char* event_name, parameter_list* args,
				int /* is_request */, int log )
	{
	IValue* event_val = BuildEventValue( args, 0 );

	if ( log )
		sequencer->LogEvent( "<agent>", "<agent>",
					event_name, event_val, 0 );

	sequencer->CheckAwait( this, event_name );

	CreateEvent( event_name, event_val );

	return 0;
	}

int UserAgent::StickyNotes( ) const
	{
	return 1;
	}


void delete_agent_dict( agent_dict *ad )
	{
	if ( ad )
		{
		IterCookie* c = ad->InitForIteration();
		agent_list* member;
		const char* key;
		while ( (member = ad->NextEntry( key, c )) )
			{
			free_memory( (void*) key );
			delete member;
			}

		delete ad;
		}
	}
