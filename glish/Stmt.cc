// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997,1998 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream.h>
#include <string.h>

#include "Reporter.h"
#include "Pager.h"
#include "Sequencer.h"
#include "Stmt.h"
#include "BuiltIn.h"
#include "Task.h"
#include "Frame.h"

IValue *FailStmt::last_fail = 0;
Stmt* null_stmt;
unsigned int WheneverStmt::notify_count = 0;

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
	unsigned short prev_line_num = line_num;

	line_num = Line();
	flow = FLOW_NEXT;

	if ( Sequencer::CurSeq()->System().Trace() )
		if ( ! DoesTrace() && Describe(message->Stream(), ioOpt(ioOpt::SHORT(),"\t|-> ")) )
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

int Stmt::GetActivity( ) const
	{
	return 1;
	}

#ifdef GGC
void Stmt::TagGC( )
	{
	}
#endif

const char *SeqStmt::Description() const
	{
	return "sequence";
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

int SeqStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.flags(ioOpt::SHORT()) ) return 0;
	s << "{\n";
	lhs->Describe( s, opt );
	s << "\n";
	rhs->Describe( s, opt );
	s << "}\n";
	return 1;
	}

const char *WheneverStmtCtor::Description() const
	{
	return "whenever";
	}

WheneverStmtCtor::WheneverStmtCtor( event_list* arg_trigger, Sequencer* arg_sequencer )
	{
	trigger = arg_trigger;
	stmt = 0;
	misc = 0;
	cur = 0;
	sequencer = arg_sequencer;
	sequencer->RegisterWhenever(this);
	}

int WheneverStmtCtor::Index( )
	{
	if ( ! cur ) cur = new WheneverStmt(sequencer);
	return cur->Index();
	}

void WheneverStmtCtor::SetStmt( Stmt* arg_stmt, ivalue_list *arg_misc )
	{
	sequencer->UnregisterWhenever( );
	index = 0;
	stmt = arg_stmt;
	misc = arg_misc;
	}

WheneverStmtCtor::~WheneverStmtCtor()
	{
	NodeUnref( stmt );
	Unref( misc );

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
		new WheneverStmt( trigger, stmt, sequencer, misc );
	else
		{
		cur->Init( trigger, stmt, misc );
		cur = 0;
		}

	return 0;
	}

int WheneverStmtCtor::Describe( OStream& s, const ioOpt &opt ) const
	{
	GlishObject::Describe( s, opt );
	if ( opt.flags(ioOpt::SHORT()) ) return 1;
	s << " ";
	describe_event_list( trigger, s );
	s << " do ";
	stmt->Describe( s, ioOpt(opt.flags(),opt.sep()) );
	return 1;
	}


const char *WheneverStmt::Description() const
	{
	return "whenever";
	}

unsigned int WheneverStmt::NotifyCount()
	{
	return notify_count;
	}

WheneverStmt::WheneverStmt(Sequencer *arg_seq) : trigger(0), sequencer(arg_seq),
						 active(0), stack(0), misc(0)
	{
	index = sequencer->RegisterStmt( this );
	}

WheneverStmt::WheneverStmt( event_list* arg_trigger, Stmt *arg_stmt, Sequencer* arg_seq,
			    ivalue_list *arg_misc ) : trigger(0), sequencer(arg_seq),
						      active(0), stack(0), misc(0)
	{
	index = sequencer->RegisterStmt( this );

	Init( arg_trigger, arg_stmt, arg_misc );
	}

void WheneverStmt::Init( event_list* arg_trigger, Stmt *arg_stmt, ivalue_list *arg_misc )
	{
	trigger = arg_trigger; Ref(trigger);
	stmt = arg_stmt; Ref(stmt);
	misc = arg_misc; if ( misc ) Ref(misc);

	stack = sequencer->LocalFrames();

	Notifiee *note = new Notifiee( this, stack, sequencer );
	loop_over_list( *trigger, i )
		{
		(*trigger)[i]->Register( note );
		Agent *ag = (*trigger)[i]->EventAgent( VAL_CONST );
		if ( ag )
			{
			ag->RegisterUnref( this );
			(*trigger)[i]->EventAgentDone( );
			}
		}

	Unref( note );

	active = 1;

	if ( RefCount() > 1 )
		Unref(this);
	else
		{
		Str err = strFail( "WheneverStmt ref count" );
		cerr << err.Chars() << endl;
		}

	sequencer->WheneverExecuted( this );
	}

int WheneverStmt::canDelete() const
	{
// 	return shutting_glish_down;
	return 1;
	}

void WheneverStmt::Notify( Agent* /* agent */ )
	{
	stmt_flow_type flow;

	notify_count += 1;
	Unref( stmt->Exec( 0, flow ) );
	notify_count -= 1;

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

int WheneverStmt::GetActivity( ) const
	{
	return active;
	}

int WheneverStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	GlishObject::Describe( s, opt );
	if ( opt.flags(ioOpt::SHORT()) ) return 1;
	s << " ";
	describe_event_list( trigger, s );
	s << " do ";
	stmt->Describe( s, ioOpt(opt.flags(),opt.sep()) );
	return 1;
	}

IValue* WheneverStmt::DoExec( int, stmt_flow_type& )
	{
	return 0;
	}

#ifdef GGC
void WheneverStmt::TagGC( )
	{
	if ( stack ) stack->TagGC( );
	if ( misc )
		loop_over_list( *misc, i )
			if ( (*misc)[i] )
				(*misc)[i]->TagGC( );
	}
#endif

WheneverStmt::~WheneverStmt()
	{
	sequencer->UnregisterStmt( this );

	NodeUnref( stmt );

	if ( trigger && trigger->RefCount() == 1 )
		{
		loop_over_list( *trigger, i )
			Unref( (*trigger)[i] );
		}

	Unref( stack );
	Unref( trigger );
	}


const char *LinkStmt::Description() const
	{
	return "link";
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

		PList(char) &name_list = src->EventNames();

		if ( name_list.length() == 0 )
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

			PList(char) &sink_list = snk->EventNames();

			if ( sink_list.length() > 1 )
				err = (IValue*) Fail(
				"multiple event names not allowed in \"to\":",
						this );
			else
				{
				loop_over_list( name_list, k )
					{
					const char* name = (name_list)[k];

					MakeLink( src_task, name, snk_task,
						  sink_list.length() ? sink_list[0] : name );
					}
				}

			snk->EventAgentDone();
			}

		src->EventAgentDone();
		}

	return err;
	}

int LinkStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	GlishObject::Describe( s, opt );
	s << " ";
	describe_event_list( source, s );
	s << " to ";
	describe_event_list( sink, s );
	return 1;
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


const char *UnLinkStmt::Description() const
	{
	return "unlink";
	}

UnLinkStmt::~UnLinkStmt() { }

UnLinkStmt::UnLinkStmt( event_list* arg_source, event_list* arg_sink,
			Sequencer* arg_sequencer )
: LinkStmt( arg_source, arg_sink, arg_sequencer )
	{
	}

void UnLinkStmt::LinkAction( Task* src, IValue* v )
	{
	src->SendSingleValueEvent( "*unlink-sink*", v, 1 );
	}


const char *AwaitStmt::Description() const
	{
	return "await";
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
	}

IValue* AwaitStmt::DoExec( int /* value_needed */, stmt_flow_type& /* flow */ )
	{
	loop_over_list( *await_list, i )
		(*await_list)[i]->Register( new Notifiee( this, sequencer ) );

	if ( except_list )
		loop_over_list( *except_list, j )
			(*except_list)[j]->Register(
					new Notifiee( except_stmt, sequencer ) );

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
	if ( file && glish_files )
		sos << " (file \"" << (*glish_files)[file] << "\", line " << line << ")";
	sos << ":" << endl;
	Describe(sos);
	sos << "" << endl;
	return sos.str();
	}

int AwaitStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	s << "await ";

	loop_over_list( *await_list, i )
		{
		(*await_list)[i]->Describe( s, ioOpt(opt.flags(),opt.sep()) );
		s << " ";
		}

	if ( except_list )
		{
		s << " except ";

		loop_over_list( *except_list, j )
			{
			(*except_list)[j]->Describe( s, ioOpt(opt.flags(),opt.sep()) );
			s << " ";
			}
		}
	return 1;
	}

const char *ActivateStmt::Description() const
	{
	return "activate";
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

	whenever_index = sequencer->CurWheneverIndex();
	}

IValue* ActivateStmt::DoExec( int /* value_needed */,
				stmt_flow_type& /* flow */ )
	{
	if ( expr )
		{
		IValue* index_value = expr->CopyEval();

		if ( ! index_value->IsNumeric() )
			{
			IValue *err = (IValue*) Fail( "non-numeric index, ", index_value );
			Unref( index_value );
			return err;
			}

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

int ActivateStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( activate )
		s << "activate";
	else
		s << "deactivate";

	if ( expr )
		{
		s << " ";
		expr->Describe( s, ioOpt(opt.flags(),opt.sep()) );
		}
	return 1;
	}

const char *IfStmt::Description() const
	{
	return "if";
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

int IfStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.prefix() ) s << opt.prefix();
	s << "if ";
	expr->Describe( s, ioOpt(opt.flags(),opt.sep()) );
	if ( opt.flags(ioOpt::SHORT()) ) return 1;
	s << " ";

	if ( true_branch )
		true_branch->Describe( s, ioOpt(opt.flags(),opt.sep()) );
	else
		s << " { } ";

	if ( false_branch )
		{
		s << "\nelse ";
		false_branch->Describe( s, ioOpt(opt.flags(),opt.sep()) );
		}
	return 1;
	}

const char *ForStmt::Description() const
	{
	return "for";
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
	}

IValue* ForStmt::DoExec( int /* value_needed */, stmt_flow_type& flow )
	{
	IValue* range_value = range->CopyEval();

	if ( ! range_value ) return 0;

	IValue* result = 0;
	glish_type type = range_value->Type();

	if ( ! range_value->IsNumeric() && type != TYPE_STRING && type != TYPE_RECORD )
		result = (IValue*) Fail( "range (", range,
				") in for loop is not numeric or record or string" );
	else
		{
		int len = range_value->Length();

		for ( int i = 1; i <= len; ++i )
			{
			IValue *iter_value = 0;
			IValue *loop_counter = 0;

			if ( type == TYPE_RECORD )
				iter_value = new IValue( * (IValue*) range_value->NthField( i ) );
			else
				{
				loop_counter = new IValue( i );
				iter_value = (IValue*)((*range_value)[loop_counter]);
				}

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

int ForStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.prefix() ) s << opt.prefix();
	s << "for ( ";
	index->Describe( s, ioOpt(opt.flags(),opt.sep()) );
	s << " in ";
	range->Describe( s, ioOpt(opt.flags(),opt.sep()) );
	s << " ) ";
	if ( opt.flags(ioOpt::SHORT()) ) return 1;
	body->Describe( s, ioOpt(opt.flags(),opt.sep()) );
	return 1;
	}

const char *WhileStmt::Description() const
	{
	return "while";
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

int WhileStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.prefix() ) s << opt.prefix();
	s << "while ( ";
	test->Describe( s, ioOpt(opt.flags(),opt.sep()) );
	s << " ) ";
	if ( opt.flags(ioOpt::SHORT()) ) return 1;
	body->Describe( s, ioOpt(opt.flags(),opt.sep()) );
	return 1;
	}

const char *PrintStmt::Description() const
	{
	return "print";
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
		pager->Report( args_string );
		free_memory( args_string );
		}

	else
		message->Report( "" );

	return 0;
	}

int PrintStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.prefix() ) s << opt.prefix();
	s << "print ";

	describe_parameter_list( args, s );

	s << ";";
	return 1;
	}


const char *FailStmt::Description() const
	{
	return "fail";
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
	if ( arg ) ret->SetFailMessage( arg->CopyEval() );

	return ret;
	}

int FailStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.prefix() ) s << opt.prefix();
	s << "fail ";

	if ( arg )
		arg->Describe( s, ioOpt(opt.flags(),opt.sep()) );

	s << ";";
	return 1;
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


const char *IncludeStmt::Description() const
	{
	return "include";
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

int IncludeStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.prefix() ) s << opt.prefix();
	s << "include ";

	if ( arg )
		arg->Describe( s, ioOpt(opt.flags(),opt.sep()) );

	s << ";";
	return 1;
	}


const char *ExprStmt::Description() const
	{
	return "expression";
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

int ExprStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.prefix() ) s << opt.prefix();
	expr->Describe( s, ioOpt(opt.flags(),opt.sep()) );
	s << ";";
	return 1;
	}

const char *ExitStmt::Description() const
	{
	return "exit";
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

int ExitStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.prefix() ) s << opt.prefix();
	s << "exit";

	if ( status )
		{
		s << " ";
		status->Describe( s, ioOpt(opt.flags(),opt.sep()) );
		}
	return 1;
	}

const char *LoopStmt::Description() const
	{
	return "next";
	}

LoopStmt::~LoopStmt() { }

IValue* LoopStmt::DoExec( int /* value_needed */, stmt_flow_type& flow )
	{
	flow = FLOW_LOOP;
	return 0;
	}


const char *BreakStmt::Description() const
	{
	return "break";
	}

BreakStmt::~BreakStmt() { }

IValue* BreakStmt::DoExec( int /* value_needed */, stmt_flow_type& flow )
	{
	flow = FLOW_BREAK;
	return 0;
	}

const char *ReturnStmt::Description() const
	{
	return "return";
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

int ReturnStmt::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.prefix() ) s << opt.prefix();
	s << "return";

	if ( retval )
		{
		s << " ";
		retval->Describe( s, ioOpt(opt.flags(),opt.sep()) );
		}
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

int StmtBlock::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( opt.flags(ioOpt::SHORT()) ) return 0;
	if ( opt.prefix() ) s << opt.prefix();
	s << "{{ ";
	stmt->Describe( s, opt );
	s << " }}";
	return 1;
	}

const char *NullStmt::Description() const
	{
	return ";";
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
