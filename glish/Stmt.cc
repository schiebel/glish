// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

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
#include "system.h"

IValue *FailStmt::last_fail = 0;
Stmt* null_stmt;
extern int current_whenever_index;


Stmt::~Stmt() { }

void Stmt::CollectUnref( stmt_list &del_list )
	{
	if ( RefCount() > 1 )
		NodeUnref( this );
	else if ( ! del_list.is_member( this ) )
		del_list.append( this );
	}

IValue* Stmt::Exec( int value_needed, stmt_flow_type& flow )
	{
	int prev_line_num = line_num;

	line_num = Line();
	flow = FLOW_NEXT;

	if ( Sequencer::CurSeq()->System().Trace() )
		if ( ! DoesTrace() && DescribeSelf(message->Stream(), "\t|-> ") )
			message->Stream() << endl;

	IValue* result = DoExec( value_needed, flow );

	line_num = prev_line_num;

	return result;
	}

int Stmt::DoesTrace( ) const
	{
	return 0;
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

void SeqStmt::CollectUnref( stmt_list &del_list )
	{
	if ( RefCount() > 1 )
		NodeUnref( this );
	else if ( ! del_list.is_member( this ) )
		{
		del_list.append( this );
		if ( lhs ) lhs->CollectUnref( del_list );
		if ( rhs ) rhs->CollectUnref( del_list );
		lhs = rhs = 0;
		}
	}

static int cnt = 0;
void SeqStmt::Describe( OStream& s ) const
	{
	s << "{\n";
	lhs->Describe( s );
	s << "\n";
	rhs->Describe( s );
	s << "}\n";
	}

int SeqStmt::DescribeSelf( OStream &, charptr ) const
	{
	return 0;
	}



WheneverStmtCtor::WheneverStmtCtor( event_list* arg_trigger, Sequencer* arg_sequencer )
	{
	trigger = arg_trigger;
	stmt = 0;
	sequencer = arg_sequencer;

	//
	// Change the "current_whenever_index" temporarily so that parameterless
	// "enable"/"disable" stmts within the whenever have the right index.
	// "current_whenever_index" is reset in SetStmt().
	//
	index = current_whenever_index;
	cur = new WheneverStmt(sequencer);
	current_whenever_index = cur->Index();

	description = "whenever";
	}

void WheneverStmtCtor::SetStmt( Stmt* arg_stmt )
	{
	current_whenever_index = index;
	index = 0;
	stmt = arg_stmt;
	}

WheneverStmtCtor::~WheneverStmtCtor()
	{
	NodeUnref( stmt );

	if ( trigger && trigger->RefCount() == 1 )
		{
		loop_over_list( *trigger, i )
			Unref( (*trigger)[i] );
		}

	Unref( trigger );
	}

void WheneverStmtCtor::CollectUnref( stmt_list &del_list )
	{
	if ( RefCount() > 1 )
		NodeUnref( this );
	else if ( ! del_list.is_member( this ) )
		{
		del_list.append( this );
		if ( stmt ) stmt->CollectUnref( del_list );
		stmt = 0;
		}
	}

IValue* WheneverStmtCtor::DoExec( int /* value_needed */,
				stmt_flow_type& /* flow */ )
	{
	if ( ! cur )
		new WheneverStmt( trigger, stmt, sequencer );
	else
		{
		cur->Init( trigger, stmt );
		cur = 0;
		}

	return 0;
	}

void WheneverStmtCtor::Describe( OStream& s ) const
	{
	DescribeSelf( s );
	s << " ";
	describe_event_list( trigger, s );
	s << " do ";
	stmt->Describe( s );
	}



WheneverStmt::WheneverStmt(Sequencer *arg_seq) : sequencer(arg_seq), active(0)
	{
	index = sequencer->RegisterStmt( this );
	description = "whenever";
	}

WheneverStmt::WheneverStmt( event_list* arg_trigger, Stmt *arg_stmt,
			    Sequencer* arg_seq ) : sequencer(arg_seq), active(0)
	{
	index = sequencer->RegisterStmt( this );

	Init( arg_trigger, arg_stmt );
	description = "whenever";
	}

void WheneverStmt::Init( event_list* arg_trigger, Stmt *arg_stmt )
	{
	trigger = arg_trigger; Ref(trigger);
	stmt = arg_stmt; Ref(stmt);

	stack_type *stack = sequencer->LocalFrames();

	loop_over_list( *trigger, i )
		(*trigger)[i]->Register( new Notifiee( this, stack ) );

	Unref( stack );
	active = 1;

	sequencer->WheneverExecuted( this );
	}

int WheneverStmt::canDelete() const
	{
	return shutting_glish_down;
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

void WheneverStmt::Describe( OStream& s ) const
	{
	DescribeSelf( s );
	s << " ";
	describe_event_list( trigger, s );
	s << " do ";
	stmt->Describe( s );
	}

IValue* WheneverStmt::DoExec( int value_needed, stmt_flow_type& flow )
	{
	return 0;
	}

WheneverStmt::~WheneverStmt()
	{
	NodeUnref( stmt );

	if ( trigger && trigger->RefCount() == 1 )
		{
		loop_over_list( *trigger, i )
			Unref( (*trigger)[i] );
		}

	Unref( trigger );
	}



LinkStmt::~LinkStmt()
	{
	if ( source )
		{
		loop_over_list( *source, i )
			Unref( (*source)[i] );

		delete source;
		}

	if ( sink )
		{
		loop_over_list( *sink, i )
			Unref( (*sink)[i] );

		delete sink;
		}
	}

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
	IValue *err = 0;
	loop_over_list( *source, i )
		{
		if ( err ) break;

		EventDesignator* src = (*source)[i];
		Agent* src_agent = src->EventAgent( VAL_CONST );

		if ( ! src_agent )
			{
			err = (IValue*) Fail( src->EventAgentExpr(),
					      "is not an agent" );
			continue;
			}

		Task* src_task = src_agent->AgentTask();

		if ( ! src_task )
			{
			err = (IValue*) Fail( src->EventAgentExpr(),
				"is not a client" );
			continue;
			}

		PList(char)* name_list = src->EventNames();

		if ( ! name_list )
			{
			err = (IValue*) Fail( this,
				"linking of all events not yet supported" );
			continue;
			}

		loop_over_list( *sink, j )
			{
			if ( err ) break;

			EventDesignator* snk = (*sink)[j];
			Agent* snk_agent = snk->EventAgent( VAL_REF );

			if ( ! snk_agent )
				{
				err = (IValue*) Fail( snk->EventAgentExpr(),
					"is not an agent" );
				continue;
				}

			Task* snk_task = snk_agent->AgentTask();

			if ( ! snk_task )
				{
				err = (IValue*) Fail( snk->EventAgentExpr(),
					"is not a client" );
				continue;
				}

			PList(char)* sink_list = snk->EventNames();

			if ( sink_list && sink_list->length() > 1 )
				err = (IValue*) Fail(
				"multiple event names not allowed in \"to\":",
						this );
			else
				{
				loop_over_list( *name_list, k )
					{
					const char* name = (*name_list)[k];

					MakeLink( src_task, name, snk_task,
						sink_list ? (*sink_list)[0] : name );
					}
				}

			delete_name_list( sink_list );
			snk->EventAgentDone();
			}

		delete_name_list( name_list );
		src->EventAgentDone();
		}

	return err;
	}

void LinkStmt::Describe( OStream& s ) const
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
	if ( await_list )
		{
		loop_over_list( *await_list, i )
			Unref( (*await_list)[i] );

		delete await_list;
		}

	if ( except_list )
		{
		loop_over_list( *except_list, i )
			Unref( (*except_list)[i] );

		delete except_list;
		}

	NodeUnref( except_stmt );
	}

void AwaitStmt::CollectUnref( stmt_list &del_list )
	{
	if ( RefCount() > 1 )
		NodeUnref( this );
	else if ( ! del_list.is_member( this ) )
		{
		del_list.append( this );
		if ( except_stmt ) except_stmt->CollectUnref( del_list );
		except_stmt = 0;
		}
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
		(*await_list)[i]->Register( new Notifiee( this ) );

	if ( except_list )
		loop_over_list( *except_list, j )
			(*except_list)[j]->Register(
					new Notifiee( except_stmt ) );

	sequencer->Await( this, only_flag, except_stmt );

	loop_over_list( *await_list, k )
		(*await_list)[k]->UnRegister( this );

	if ( except_list )
		loop_over_list( *except_list, l )
			(*except_list)[l]->UnRegister( except_stmt );

	return 0;
	}

const char *AwaitStmt::TerminateInfo() const
	{
	static SOStream sos;
	sos.reset();
	sos << "await terminated";
	if ( file && file->chars() )
		sos << " (file \"" << file->chars() << "\", line " << line << ")";
	sos << ":" << endl;
	Describe(sos);
	sos << "" << endl;
	return sos.str();
	}

void AwaitStmt::Describe( OStream& s ) const
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

	whenever_index = current_whenever_index;
	}

IValue* ActivateStmt::DoExec( int /* value_needed */,
				stmt_flow_type& /* flow */ )
	{
	IValue *result = 0;
	if ( expr )
		{
		IValue* index_value = expr->CopyEval();
		int* index_ = index_value->IntPtr(0);
		int n = index_value->Length();

		for ( int i = 0; i < n; ++i )
			{
			Stmt* s = sequencer->LookupStmt( index_[i] );

			if ( ! s )
				{
				Unref( index_value );
				return (IValue*) Fail(i, 
				"does not designate a valid \"whenever\" statement" );
				}

			s->SetActivity( activate );
			}

		Unref( index_value );
		}

	else
		{
		if ( whenever_index < 0 )
			return (IValue*) Fail(
			"\"activate\"/\"deactivate\" executed without previous \"whenever\"" );
		Stmt *s = sequencer->LookupStmt( whenever_index );

		if ( ! s )
			return (IValue*) Fail( whenever_index,
			"does not designate a valid \"whenever\" statement" );

		s->SetActivity( activate );
		}

	return 0;
	}

void ActivateStmt::Describe( OStream& s ) const
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

void IfStmt::CollectUnref( stmt_list &del_list )
	{
	if ( RefCount() > 1 )
		NodeUnref( this );
	else if ( ! del_list.is_member( this ) )
		{
		del_list.append( this );
		if ( true_branch ) true_branch->CollectUnref( del_list );
		if ( false_branch ) false_branch->CollectUnref( del_list );
		true_branch = false_branch = 0;
		}
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
	Str err;
	int take_true_branch = test_value->BoolVal(1,err);
	expr->ReadOnlyDone( test_value );

	if ( err.chars() )
		return (IValue*) Fail(err.chars());

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

void IfStmt::Describe( OStream& s ) const
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

int IfStmt::DescribeSelf( OStream &s, charptr prefix ) const
	{
	if ( prefix ) s << prefix;
	s << "if ";
	expr->Describe( s );
	return 1;
	}


ForStmt::~ForStmt()
	{
	NodeUnref( index );
	NodeUnref( range );
	NodeUnref( body );
	}

void ForStmt::CollectUnref( stmt_list &del_list )
	{
	if ( RefCount() > 1 )
		NodeUnref( this );
	else if ( ! del_list.is_member( this ) )
		{
		del_list.append( this );
		if ( body ) body->CollectUnref( del_list );
		body = 0;
		}
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

	if ( ! range_value ) return 0;

	IValue* result = 0;

	if ( ! range_value->IsNumeric() && range_value->Type() != TYPE_STRING )
		result = (IValue*) Fail( "range (", range,
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

void ForStmt::Describe( OStream& s ) const
	{
	s << "for ( ";
	index->Describe( s );
	s << " in ";
	range->Describe( s );
	s << " ) ";
	body->Describe( s );
	}

int ForStmt::DescribeSelf( OStream& s, charptr prefix ) const
	{
	if ( prefix ) s << prefix;
	s << "for ( ";
	index->Describe( s );
	s << " in ";
	range->Describe( s );
	s << " )";
	return 1;
	}

WhileStmt::~WhileStmt()
	{
	NodeUnref( test );
	NodeUnref( body );
	}

void WhileStmt::CollectUnref( stmt_list &del_list )
	{
	if ( RefCount() > 1 )
		NodeUnref( this );
	else if ( ! del_list.is_member( this ) )
		{
		del_list.append( this );
		if ( body ) body->CollectUnref( del_list );
		body = 0;
		}
	}

WhileStmt::WhileStmt( Expr* test_expr, Stmt* body_stmt )
	{
	test = test_expr;
	body = body_stmt;
	description = "while";
	}

IValue* WhileStmt::DoExec( int /* value_needed */, stmt_flow_type& flow )
	{
	Str err;
	IValue* result = 0;

	while ( 1 )
		{
		const IValue* test_value = test->ReadOnlyEval();
		int do_test = test_value->BoolVal(1,err);
		test->ReadOnlyDone( test_value );

		if ( err.chars() )
			{
			result = (IValue*) Fail(err.chars());
			break;
			}

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

void WhileStmt::Describe( OStream& s ) const
	{
	s << "while ( ";
	test->Describe( s );
	s << " ) ";
	body->Describe( s );
	}

PrintStmt::~PrintStmt()
	{
	if ( args )
		{
		loop_over_list( *args, i )
			NodeUnref( (*args)[i] );
		delete args;
		}
	}

IValue* PrintStmt::DoExec( int /* value_needed */, stmt_flow_type& /* flow */ )
	{
	if ( args )
		{
		char* args_string = paste( args );
		message->Report( args_string );
		free_memory( args_string );
		}

	else
		message->Report( "" );

	return 0;
	}

void PrintStmt::Describe( OStream& s ) const
	{
	s << "print ";

	describe_parameter_list( args, s );

	s << ";";
	}


int PrintStmt::DescribeSelf( OStream &s, charptr prefix ) const
	{
	if ( prefix ) s << prefix;
	Describe(s);
	return 1;
	}

FailStmt::~FailStmt()
	{
	if ( arg ) NodeUnref( arg );
	}

IValue* FailStmt::DoExec( int /* value_needed */, stmt_flow_type& flow )
	{
	flow = FLOW_RETURN;

	if ( arg )
		ClearFail();

	IValue *ret = (IValue*) Fail( );

	//
	// Assign message separately so that the message is preserved.
	//
	if ( arg )
		ret->AssignAttribute( "message", arg->CopyEval() );

	return ret;
	}

void FailStmt::Describe( OStream& s ) const
	{
	s << "fail ";

	if ( arg )
		arg->Describe( s );

	s << ";";
	}


void FailStmt::ClearFail()
	{
	Unref(last_fail);
	last_fail = 0;
	}

const IValue *FailStmt::GetFail()
	{
	return last_fail;
	}

IValue *FailStmt::SwapFail( IValue *err )
	{
	IValue *t = last_fail;
	if ( err ) Ref( err );
	last_fail = err;
	return t;
	}

void FailStmt::SetFail( IValue *err )
	{
	if ( err ) Ref( err );
	Unref(last_fail);
	last_fail = err;
	}


IncludeStmt::~IncludeStmt()
	{
	if ( arg ) NodeUnref( arg );
	}

IValue* IncludeStmt::DoExec( int /* value_needed */, stmt_flow_type& /* flow */ )
	{
	const IValue *str_val = arg->ReadOnlyEval();
	char *str = str_val->StringVal();
	arg->ReadOnlyDone( str_val );

	IValue *ret = sequencer->Include( str );

	free_memory( str );
	return ret;
	}

void IncludeStmt::Describe( OStream& s ) const
	{
	s << "include ";

	if ( arg )
		arg->Describe( s );

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
		return expr->SideEffectsEval();
	}

int ExprStmt::DoesTrace( ) const
	{
	return expr->DoesTrace();
	}

void ExprStmt::Describe( OStream& s ) const
	{
	expr->Describe( s );
	s << ";";
	}

int ExprStmt::DescribeSelf( OStream& s, charptr prefix ) const
	{
	if ( prefix ) s << prefix;
	expr->DescribeSelf( s );
	return 1;
	}

ExitStmt::~ExitStmt() { }

int ExitStmt::canDelete() const
	{
	return can_delete;
	}

IValue* ExitStmt::DoExec( int /* value_needed */, stmt_flow_type& /* flow */ )
	{
	can_delete = 0;

	glish_cleanup();

	int exit_val = status ? status->CopyEval()->IntVal() : 0;

	delete sequencer;

	exit( exit_val );

	return 0;
	}

void ExitStmt::Describe( OStream& s ) const
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

void ReturnStmt::Describe( OStream& s ) const
	{
	s << "return";

	if ( retval )
		{
		s << " ";
		retval->Describe( s );
		}
	}

int ReturnStmt::DescribeSelf( OStream &s, charptr prefix ) const
	{
	if ( prefix ) s << prefix;
	Describe(s);
	return 1;
	}


StmtBlock::~StmtBlock()
	{
	NodeUnref( stmt );
	}

void StmtBlock::CollectUnref( stmt_list &del_list )
	{
	if ( RefCount() > 1 )
		NodeUnref( this );
	else if ( ! del_list.is_member( this ) )
		{
		del_list.append( this );
		if ( stmt ) stmt->CollectUnref( del_list );
		stmt = 0;
		}
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
	IValue* result = 0;

	if ( frame_size )
		{
		Frame* call_frame = new Frame( frame_size, 0, LOCAL_SCOPE );

		sequencer->PushFrame( call_frame );

		result = stmt->Exec( value_needed, flow );

		if ( sequencer->PopFrame() != call_frame )
			fatal->Report( "frame inconsistency in StmtBlock::DoExec" );

		Unref( call_frame );
		}
	else
		{
		sequencer->PushFrame( 0 );
		result = stmt->Exec( value_needed, flow );
		sequencer->PopFrame();
		}

	return result;
	}

void StmtBlock::Describe( OStream& s ) const
	{
	s << "{{ ";
	stmt->Describe( s );
	s << " }}";
	}

int StmtBlock::DescribeSelf( OStream &, charptr ) const
	{
	return 0;
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
