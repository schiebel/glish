// $Header$

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <stdio.h>
#include <stdlib.h>
#include <stream.h>
#include <string.h>

#include "Reporter.h"
#include "Sequencer.h"
#include "Stmt.h"
#include "BuiltIn.h"
#include "Task.h"
#include "Frame.h"


Stmt* null_stmt;


Stmt::~Stmt() { }

IValue* Stmt::Exec( int value_needed, stmt_flow_type& flow )
	{
	int prev_line_num = line_num;

	line_num = Line();
	flow = FLOW_NEXT;

	IValue* result = DoExec( value_needed, flow );

	line_num = prev_line_num;

	return result;
	}

void Stmt::Notify( Agent* /* agent */ )
	{
	}

int Stmt::IsActiveFor( Agent* /* agent */, const char* /* field */,
			IValue* /* value */ ) const
	{
	return 1;
	}

void Stmt::SetActivity( int /* activate */ )
	{
	}

SeqStmt::~SeqStmt()
	{
	NodeUnref( lhs );
	NodeUnref( rhs );
	}

SeqStmt::SeqStmt( Stmt* arg_lhs, Stmt* arg_rhs )
	{
	lhs = arg_lhs;
	rhs = arg_rhs;
	description = "sequence";
	}

IValue* SeqStmt::DoExec( int value_needed, stmt_flow_type& flow )
	{
	IValue* result = lhs->Exec( 0, flow );

	if ( flow == FLOW_NEXT )
		{
		Unref( result );
		result = rhs->Exec( value_needed, flow );
		}

	return result;
	}

void SeqStmt::Describe( ostream& s ) const
	{
	s << "{\n";
	lhs->Describe( s );
	s << "\n";
	rhs->Describe( s );
	s << "}\n";
	}


WheneverStmt::WheneverStmt( event_list* arg_trigger, Stmt* arg_stmt,
			Sequencer* arg_sequencer )
	{
	trigger = arg_trigger;
	stmt = arg_stmt;
	sequencer = arg_sequencer;
	active = 0;

	index = sequencer->RegisterStmt( this );

	description = "whenever";
	}

WheneverStmt::~WheneverStmt()
	{
	NodeUnref( stmt );
	loop_over_list( *trigger, i )
		Unref( (*trigger)[i] );
	}

int WheneverStmt::canDelete() const
	{
	return 0;
	}

IValue* WheneverStmt::DoExec( int /* value_needed */,
				stmt_flow_type& /* flow */ )
	{
	Frame* frame = sequencer->CurrentFrame();

	loop_over_list( *trigger, i )
		(*trigger)[i]->Register( new Notifiee( this, frame ) );

	active = 1;

	sequencer->WheneverExecuted( this );

	return 0;
	}

void WheneverStmt::Notify( Agent* /* agent */ )
	{
	stmt_flow_type flow;
	Unref( stmt->Exec( 0, flow ) );

	if ( flow != FLOW_NEXT )
		warn->Report( "loop/break/return does not make sense inside",
				this );
	}

int WheneverStmt::IsActiveFor( Agent* /* agent */, const char* /* field */,
				IValue* /* value */ ) const
	{
	return active;
	}

void WheneverStmt::SetActivity( int activate )
	{
	active = activate;
	}

void WheneverStmt::Describe( ostream& s ) const
	{
	DescribeSelf( s );
	s << " ";
	describe_event_list( trigger, s );
	s << " do ";
	stmt->Describe( s );
	}


LinkStmt::~LinkStmt() { }

LinkStmt::LinkStmt( event_list* arg_source, event_list* arg_sink,
			Sequencer* arg_sequencer )
	{
	source = arg_source;
	sink = arg_sink;
	sequencer = arg_sequencer;

	description = "link";
	}

IValue* LinkStmt::DoExec( int /* value_needed */, stmt_flow_type& /* flow */ )
	{
	loop_over_list( *source, i )
		{
		EventDesignator* src = (*source)[i];
		Agent* src_agent = src->EventAgent( VAL_CONST );

		if ( ! src_agent )
			{
			error->Report( src->EventAgentExpr(),
					"is not an agent" );
			continue;
			}

		Task* src_task = src_agent->AgentTask();

		if ( ! src_task )
			{
			error->Report( src->EventAgentExpr(),
				"is not a client" );
			continue;
			}

		PList(char)* name_list = src->EventNames();

		if ( ! name_list )
			{
			error->Report( this,
				"linking of all events not yet supported" );
			continue;
			}

		loop_over_list( *sink, j )
			{
			EventDesignator* snk = (*sink)[j];
			Agent* snk_agent = snk->EventAgent( VAL_REF );

			if ( ! snk_agent )
				{
				error->Report( snk->EventAgentExpr(),
					"is not an agent" );
				continue;
				}

			Task* snk_task = snk_agent->AgentTask();

			if ( ! snk_task )
				{
				error->Report( snk->EventAgentExpr(),
					"is not a client" );
				continue;
				}

			PList(char)* sink_list = snk->EventNames();

			if ( sink_list && sink_list->length() > 1 )
				error->Report(
				"multiple event names not allowed in \"to\":",
						this );

			loop_over_list( *name_list, k )
				{
				const char* name = (*name_list)[k];

				MakeLink( src_task, name, snk_task,
					sink_list ? (*sink_list)[0] : name );
				}

			delete_name_list( sink_list );
			snk->EventAgentDone();
			}

		delete_name_list( name_list );
		src->EventAgentDone();
		}

	return 0;
	}

void LinkStmt::Describe( ostream& s ) const
	{
	DescribeSelf( s );
	s << " ";
	describe_event_list( source, s );
	s << " to ";
	describe_event_list( sink, s );
	}

void LinkStmt::MakeLink( Task* src, const char* source_event,
			 Task* snk, const char* sink_event )
	{
	IValue* v = create_irecord();

	v->SetField( "event", source_event );
	v->SetField( "new_name", sink_event );
	v->SetField( "source_id", src->TaskID() );
	v->SetField( "sink_id", snk->TaskID() );
	v->SetField( "is_local", same_host( src, snk ) );

	LinkAction( src, v );

	Unref( v );
	}

void LinkStmt::LinkAction( Task* src, IValue* v )
	{
	src->SendSingleValueEvent( "*link-sink*", v, 1 );
	}


UnLinkStmt::~UnLinkStmt() { }

UnLinkStmt::UnLinkStmt( event_list* arg_source, event_list* arg_sink,
			Sequencer* arg_sequencer )
: LinkStmt( arg_source, arg_sink, arg_sequencer )
	{
	description = "unlink";
	}

void UnLinkStmt::LinkAction( Task* src, IValue* v )
	{
	src->SendSingleValueEvent( "*unlink-sink*", v, 1 );
	}


AwaitStmt::~AwaitStmt()
	{
	NodeUnref( except_stmt );
	}

AwaitStmt::AwaitStmt( event_list* arg_await_list, int arg_only_flag,
			event_list* arg_except_list, Sequencer* arg_sequencer )
	{
	await_list = arg_await_list;
	only_flag = arg_only_flag;
	except_list = arg_except_list;
	sequencer = arg_sequencer;
//	except_stmt = null_stmt;
	except_stmt = this;

	description = "await";
	}

IValue* AwaitStmt::DoExec( int /* value_needed */, stmt_flow_type& /* flow */ )
	{
	loop_over_list( *await_list, i )
		(*await_list)[i]->Register( new Notifiee( this, 0 ) );

	if ( except_list )
		loop_over_list( *except_list, j )
			(*except_list)[j]->Register(
					new Notifiee( except_stmt, 0 ) );

	sequencer->Await( this, only_flag, except_stmt );

	loop_over_list( *await_list, k )
		(*await_list)[k]->UnRegister( this );

	if ( except_list )
		loop_over_list( *except_list, l )
			(*except_list)[l]->UnRegister( except_stmt );

	return 0;
	}

void AwaitStmt::Describe( ostream& s ) const
	{
	s << "await ";

	loop_over_list( *await_list, i )
		{
		(*await_list)[i]->Describe( s );
		s << " ";
		}

	if ( except_list )
		{
		s << " except ";

		loop_over_list( *except_list, j )
			{
			(*except_list)[j]->Describe( s );
			s << " ";
			}
		}
	}


ActivateStmt::~ActivateStmt()
	{
	NodeUnref( expr );
	}

ActivateStmt::ActivateStmt( int arg_activate, Expr* e,
				Sequencer* arg_sequencer )
	{
	activate = arg_activate;
	expr = e;
	sequencer = arg_sequencer;

	description = "activate";
	}

IValue* ActivateStmt::DoExec( int /* value_needed */,
				stmt_flow_type& /* flow */ )
	{
	if ( expr )
		{
		IValue* index_value = expr->CopyEval();
		int* index = index_value->IntPtr(0);
		int n = index_value->Length();

		for ( int i = 0; i < n; ++i )
			{
			Stmt* s = sequencer->LookupStmt( index[i] );

			if ( ! s )
				{
				error->Report( i,
			"does not designate a valid \"whenever\" statement" );
				break;
				}

			s->SetActivity( activate );
			}

		Unref( index_value );
		}

	else
		{
		Notification* n = sequencer->LastNotification();

		if ( ! n )
			{
			error->Report(
	"\"activate\"/\"deactivate\" executed without previous \"whenever\"" );
			return 0;
			}

		n->notifiee->stmt->SetActivity( activate );
		}

	return 0;
	}

void ActivateStmt::Describe( ostream& s ) const
	{
	if ( activate )
		s << "activate";
	else
		s << "deactivate";

	if ( expr )
		{
		s << " ";
		expr->Describe( s );
		}
	}


IfStmt::~IfStmt()
	{
	NodeUnref( expr );
	NodeUnref( true_branch );
	NodeUnref( false_branch );
	}

IfStmt::IfStmt( Expr* arg_expr, Stmt* arg_true_branch,
		Stmt* arg_false_branch )
	{
	expr = arg_expr;
	true_branch = arg_true_branch;
	false_branch = arg_false_branch;
	description = "if";
	}

IValue* IfStmt::DoExec( int value_needed, stmt_flow_type& flow )
	{
	const IValue* test_value = expr->ReadOnlyEval();
	int take_true_branch = test_value->BoolVal();
	expr->ReadOnlyDone( test_value );

	IValue* result = 0;

	if ( take_true_branch )
		{
		if ( true_branch )
			result = true_branch->Exec( value_needed, flow );
		}

	else if ( false_branch )
		result = false_branch->Exec( value_needed, flow );

	return result;
	}

void IfStmt::Describe( ostream& s ) const
	{
	s << "if ";
	expr->Describe( s );
	s << " ";

	if ( true_branch )
		true_branch->Describe( s );
	else
		s << " { } ";

	if ( false_branch )
		{
		s << "\nelse ";
		false_branch->Describe( s );
		}
	}


ForStmt::~ForStmt()
	{
	NodeUnref( index );
	NodeUnref( range );
	NodeUnref( body );
	}

ForStmt::ForStmt( Expr* index_expr, Expr* range_expr,
		  Stmt* body_stmt )
	{
	index = index_expr;
	range = range_expr;
	body = body_stmt;
	description = "for";
	}

IValue* ForStmt::DoExec( int /* value_needed */, stmt_flow_type& flow )
	{
	IValue* range_value = range->CopyEval();

	IValue* result = 0;

	if ( ! range_value->IsNumeric() && range_value->Type() != TYPE_STRING )
		error->Report( "range (", range,
				") in for loop is not numeric or string" );

	else
		{
		int len = range_value->Length();

		for ( int i = 1; i <= len; ++i )
			{
			IValue* loop_counter = new IValue( i );
			IValue* iter_value = (IValue*)((*range_value)[loop_counter]);

			index->Assign( iter_value );

			Unref( result );
			result = body->Exec( 0, flow );

			Unref( loop_counter );

			if ( flow == FLOW_BREAK || flow == FLOW_RETURN )
				break;
			}
		}

	Unref( range_value );

	if ( flow != FLOW_RETURN )
		flow = FLOW_NEXT;

	return result;
	}

void ForStmt::Describe( ostream& s ) const
	{
	s << "for ( ";
	index->Describe( s );
	s << " in ";
	range->Describe( s );
	s << " ) ";
	body->Describe( s );
	}

WhileStmt::~WhileStmt()
	{
	NodeUnref( test );
	NodeUnref( body );
	}

WhileStmt::WhileStmt( Expr* test_expr, Stmt* body_stmt )
	{
	test = test_expr;
	body = body_stmt;
	description = "while";
	}

IValue* WhileStmt::DoExec( int /* value_needed */, stmt_flow_type& flow )
	{
	IValue* result = 0;

	while ( 1 )
		{
		const IValue* test_value = test->ReadOnlyEval();
		int do_test = test_value->BoolVal();
		test->ReadOnlyDone( test_value );

		if ( do_test )
			{
			Unref( result );
			result = body->Exec( 0, flow );
			if ( flow == FLOW_BREAK || flow == FLOW_RETURN )
				break;
			}

		else
			break;
		}

	if ( flow != FLOW_RETURN )
		flow = FLOW_NEXT;

	return result;
	}

void WhileStmt::Describe( ostream& s ) const
	{
	s << "while ( ";
	test->Describe( s );
	s << " ) ";
	body->Describe( s );
	}

PrintStmt::~PrintStmt()
	{
	if ( args )
		loop_over_list( *args, i )
			NodeUnref( (*args)[i] );
	}

IValue* PrintStmt::DoExec( int /* value_needed */, stmt_flow_type& /* flow */ )
	{
	if ( args )
		{
		char* args_string = paste( args );
		message->Report( args_string );
		delete args_string;
		}

	else
		message->Report( "" );

	return 0;
	}

void PrintStmt::Describe( ostream& s ) const
	{
	s << "print ";

	describe_parameter_list( args, s );

	s << ";";
	}


ExprStmt::~ExprStmt()
	{
	NodeUnref( expr );
	}

IValue* ExprStmt::DoExec( int value_needed, stmt_flow_type& /* flow */ )
	{
	if ( value_needed && ! expr->Invisible() )
		return expr->CopyEval();
	else
		{
		expr->SideEffectsEval();
		return 0;
		}
	}

void ExprStmt::Describe( ostream& s ) const
	{
	expr->Describe( s );
	s << ";";
	}

void ExprStmt::DescribeSelf( ostream& s ) const
	{
	expr->DescribeSelf( s );
	}

ExitStmt::~ExitStmt() { /**** should never be called ****/ }

IValue* ExitStmt::DoExec( int /* value_needed */, stmt_flow_type& /* flow */ )
	{
	int exit_val = status ? status->CopyEval()->IntVal() : 0;

	delete sequencer;

	glish_cleanup();

	exit( exit_val );

	return 0;
	}

void ExitStmt::Describe( ostream& s ) const
	{
	s << "exit";

	if ( status )
		{
		s << " ";
		status->Describe( s );
		}
	}

LoopStmt::~LoopStmt() { }

IValue* LoopStmt::DoExec( int /* value_needed */, stmt_flow_type& flow )
	{
	flow = FLOW_LOOP;
	return 0;
	}


BreakStmt::~BreakStmt() { }

IValue* BreakStmt::DoExec( int /* value_needed */, stmt_flow_type& flow )
	{
	flow = FLOW_BREAK;
	return 0;
	}


ReturnStmt::~ReturnStmt()
	{
	NodeUnref( retval );
	}

IValue* ReturnStmt::DoExec( int /* value_needed */, stmt_flow_type& flow )
	{
	flow = FLOW_RETURN;

	if ( retval )
		return retval->CopyEval();

	else
		return 0;
	}

void ReturnStmt::Describe( ostream& s ) const
	{
	s << "return";

	if ( retval )
		{
		s << " ";
		retval->Describe( s );
		}
	}

StmtBlock::~StmtBlock()
	{
	NodeUnref( stmt );
	}

StmtBlock::StmtBlock( int fsize, Stmt *arg_stmt,
			Sequencer *arg_sequencer )
	{
	stmt = arg_stmt;
	frame_size = fsize;
	sequencer = arg_sequencer;
	}

IValue* StmtBlock::DoExec( int value_needed, stmt_flow_type& flow )
	{
	Frame* call_frame = new Frame( frame_size, 0, LOCAL_SCOPE );
	sequencer->PushFrame( call_frame );

	IValue* result = 0;

	result = stmt->Exec( value_needed, flow );

	if ( sequencer->PopFrame() != call_frame )
		fatal->Report( "frame inconsistency in StmtBlock::DoExec" );

	Unref( call_frame );

	return result;
	}

void StmtBlock::Describe( ostream& s ) const
	{
	s << "{{ ";
	stmt->Describe( s );
	s << " }}";
	}


NullStmt::~NullStmt() { }

int NullStmt::canDelete() const
	{
	return 0;
	}

IValue* NullStmt::DoExec( int /* value_needed */, stmt_flow_type& /* flow */ )
	{
	return 0;
	}


Stmt* merge_stmts( Stmt* stmt1, Stmt* stmt2 )
	{
	if ( stmt1 == null_stmt )
		return stmt2;

	else if ( stmt2 == null_stmt )
		return stmt1;

	else
		return new SeqStmt( stmt1, stmt2 );
	}
