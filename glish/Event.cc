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
	names = 0;
	}

EventDesignator::EventDesignator( Expr* arg_agent, const char* arg_event_name )
	{
	agent = arg_agent;
	event_name_expr = 0;
	event_name_str = arg_event_name;
	event_agent_ref = 0;
	names = 0;
	}

EventDesignator::~EventDesignator( )
	{
	NodeUnref( agent );
	NodeUnref( event_name_expr );
	if ( names ) delete_name_list( names );
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
	name_list &nl = EventNames();

	if ( nl.length() == 0 )
		{
		error->Report( "->* illegal for sending an event" );
		return is_request ? error_ivalue() : 0;
		}

	Agent* a = EventAgent( VAL_REF );
	IValue* result = 0;

	if ( a )
		{
		if ( nl.length() > 1 )
			error->Report( this,
				       "must designate exactly one event" );

		result = a->SendEvent( nl[0], arguments, is_request, 1 );
		}

	else
		result = (IValue*) Fail( EventAgentExpr(), "is not an agent" );

	EventAgentDone();

	return result;
	}

void EventDesignator::Register( Notifiee* notifiee )
	{
	if ( names ) delete_name_list( names );
	names = 0;

	name_list &nl = EventNames();

	Agent* a = EventAgent( VAL_CONST );

	if ( a )
		{
		if ( nl.length() == 0 )
			// Register for all events.
			a->RegisterInterest( notifiee );
		else
			loop_over_list( nl, i )
				a->RegisterInterest( notifiee, strdup(nl[i]), 1 );
		}

	else
		error->Report( EventAgentExpr(), "is not an agent" );

	EventAgentDone();
	}

void EventDesignator::UnRegister( Stmt* s )
	{
	name_list &nl = EventNames();

	Agent* a = EventAgent( VAL_CONST );

	if ( a )
		{
		if ( nl.length() == 0 )
			// Register for all events.
			a->UnRegisterInterest( s );

		else
			loop_over_list( nl, i )
				a->UnRegisterInterest( s, nl[i] );
		}

	else
		error->Report( EventAgentExpr(), "is not an agent" );

	EventAgentDone();
	}

name_list &EventDesignator::EventNames()
	{

	if ( names ) return *names;

	names = new name_list;

	if ( event_name_str )
		{
		names->append( strdup( event_name_str ) );
		return *names;
		}

	if ( ! event_name_expr )
		return *names;

	const IValue* index_val = event_name_expr->ReadOnlyEval();

	if ( index_val->Type() == TYPE_STRING )
		{
		int n = index_val->Length();
		const char** s = index_val->StringPtr(0);
		for ( int i = 0; i < n; ++i )
			names->append( strdup( s[i] ) );
		}

	else
		error->Report( this, "does not have a string-valued index" );

	event_name_expr->ReadOnlyDone( index_val );

	return *names;
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
