// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include <iostream.h>
#include <string.h>

#include "IValue.h"
#include "Expr.h"
#include "Agent.h"
#include "Reporter.h"


EventDesignator::EventDesignator( Expr* arg_agent, Expr* arg_event_name )
	{
	agent = arg_agent;
	event_name_expr = arg_event_name;
	event_name_str = 0;
	event_agent_ref = 0;
	}

EventDesignator::EventDesignator( Expr* arg_agent, const char* arg_event_name )
	{
	agent = arg_agent;
	event_name_expr = 0;
	event_name_str = arg_event_name;
	event_agent_ref = 0;
	}

EventDesignator::~EventDesignator( )
	{
	NodeUnref( agent );
	NodeUnref( event_name_expr );
	}

Agent* EventDesignator::EventAgent( value_type val_type )
	{
	event_agent_ref = agent->RefEval( val_type );
	IValue* event_agent_val = (IValue*)(event_agent_ref->Deref());

	if ( ! event_agent_val->IsAgentRecord() )
		{
		EventAgentDone();
		return 0;
		}

	return event_agent_val->AgentVal();
	}

void EventDesignator::EventAgentDone()
	{
	Unref( event_agent_ref );
	event_agent_ref = 0;
	}

IValue* EventDesignator::SendEvent( parameter_list* arguments, int is_request )
	{
	name_list* nl = EventNames();

	if ( ! nl )
		{
		error->Report( "->* illegal for sending an event" );
		return is_request ? error_ivalue() : 0;
		}

	Agent* a = EventAgent( VAL_REF );
	IValue* result = 0;

	if ( a )
		{
		if ( nl->length() > 1 )
			error->Report( this,
					"must designate exactly one event" );

		result = a->SendEvent( (*nl)[0], arguments, is_request, 1 );
		}

	else
		error->Report( EventAgentExpr(), "is not an agent" );

	delete_name_list( nl );

	EventAgentDone();

	return result;
	}

void EventDesignator::Register( Notifiee* notifiee )
	{
	name_list* nl = EventNames();

	Agent* a = EventAgent( VAL_CONST );

	if ( a )
		{
		if ( ! nl )
			// Register for all events.
			a->RegisterInterest( notifiee );

		else
			loop_over_list( *nl, i )
				a->RegisterInterest( notifiee, (*nl)[i], 1 );

		// We don't delete the elements of nl because they've
		// been given over to the agent.

		delete nl;
		}

	else
		{
		error->Report( EventAgentExpr(), "is not an agent" );

		delete_name_list( nl );
		}

	EventAgentDone();
	}

void EventDesignator::UnRegister( Stmt* s )
	{
	name_list* nl = EventNames();

	Agent* a = EventAgent( VAL_CONST );

	if ( a )
		{
		if ( ! nl )
			// Register for all events.
			a->UnRegisterInterest( s );

		else
			loop_over_list( *nl, i )
				a->UnRegisterInterest( s, (*nl)[i] );

		delete_name_list( nl );
		}

	else
		{
		error->Report( EventAgentExpr(), "is not an agent" );

		delete_name_list( nl );
		}

	EventAgentDone();
	}

name_list* EventDesignator::EventNames()
	{
	name_list* result = new name_list;

	if ( event_name_str )
		{
		result->append( strdup( event_name_str ) );
		return result;
		}

	if ( ! event_name_expr )
		return 0;

	const IValue* index_val = event_name_expr->ReadOnlyEval();

	if ( index_val->Type() == TYPE_STRING )
		{
		int n = index_val->Length();
		const char** s = index_val->StringPtr(0);
		for ( int i = 0; i < n; ++i )
			result->append( strdup( s[i] ) );
		}

	else
		error->Report( this, "does not have a string-valued index" );

	event_name_expr->ReadOnlyDone( index_val );

	return result;
	}

int EventDesignator::DescribeSelf( OStream& s, charptr prefix ) const
	{
	if ( prefix ) s << prefix;
	EventAgentExpr()->DescribeSelf( s );
	s << "->";

	if ( event_name_expr )
		{
		s << "[";
		event_name_expr->Describe( s );
		s << "]";
	}

	else if ( event_name_str )
		s << "." << event_name_str;

	else
		s << "*";
	return 1;
	}


void delete_name_list( name_list* nl )
	{
	if ( nl )
		{
		loop_over_list( *nl, i )
			free_memory( (*nl)[i] );

		delete nl;
		}
	}


void describe_event_list( const event_list* list, OStream& s )
	{
	if ( list )
		loop_over_list( *list, i )
			{
			if ( i > 0 )
				s << ", ";
			(*list)[i]->Describe( s );
			}
	}
