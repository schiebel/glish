// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"

#include <string.h>
#include <stream.h>
#include <math.h>
#include <stdlib.h>

#include "Agent.h"
#include "Func.h"
#include "Stmt.h"
#include "Frame.h"
#include "Sequencer.h"
#include "Reporter.h"

Parameter::~Parameter()
	{
	NodeUnref( arg );
	NodeUnref( default_value );
	if ( name && can_delete )
		free_memory( name );
	}

Parameter::Parameter( const char* arg_name, value_type arg_parm_type,
			Expr* arg_arg, int arg_is_ellipsis,
			Expr* arg_default_value, int arg_is_empty )
	{
	name = (char*) arg_name;
	parm_type = arg_parm_type;
	arg = arg_arg;
	is_ellipsis = arg_is_ellipsis;
	default_value = arg_default_value;
	is_empty = arg_is_empty;
	can_delete = 0;
	}

Parameter::Parameter( char* arg_name, value_type arg_parm_type,
			Expr* arg_arg, int arg_is_ellipsis,
			Expr* arg_default_value, int arg_is_empty )
	{
	name = arg_name;
	parm_type = arg_parm_type;
	arg = arg_arg;
	is_ellipsis = arg_is_ellipsis;
	default_value = arg_default_value;
	is_empty = arg_is_empty;
	can_delete = 1;
	}

Parameter::Parameter( value_type arg_parm_type,
			Expr* arg_arg, int arg_is_ellipsis,
			Expr* arg_default_value, int arg_is_empty )
	{
	name = 0;
	parm_type = arg_parm_type;
	arg = arg_arg;
	is_ellipsis = arg_is_ellipsis;
	default_value = arg_default_value;
	is_empty = arg_is_empty;
	can_delete = 0;
	}

int Parameter::NumEllipsisVals() const
	{
	if ( ! is_ellipsis )
		fatal->Report(
			"Parameter::NumEllipsisVals called but not ellipsis" );

	const IValue* values = Arg()->ReadOnlyEval();

	int n = values->RecordPtr(0)->Length();

	Arg()->ReadOnlyDone( values );

	return n;
	}

const IValue* Parameter::NthEllipsisVal( int n ) const
	{
	if ( ! is_ellipsis )
		fatal->Report(
			"Parameter::NthEllipsisVal called but not ellipsis" );

	const IValue* values = Arg()->ReadOnlyEval();

	if ( n < 0 || n >= values->RecordPtr(0)->Length() )
		fatal->Report( "bad value of n in Parameter::NthEllipsisVal" );

	char field_name[256];
	sprintf( field_name, "%d", n );

	const IValue* result = (const IValue*) values->ExistingRecordElement( field_name );

	Arg()->ReadOnlyDone( values );

	return result;
	}

void Parameter::Describe( OStream& s ) const
	{
	if ( name )
		s << name;
	else if ( arg )
		arg->Describe( s );

	if ( default_value )
		{
		s << " = ";
		default_value->Describe( s );
		}
	}


FormalParameter::FormalParameter( const char* name_, value_type parm_type_,
			Expr* arg_, int is_ellipsis_, Expr* default_value_ )
    : Parameter(name_, parm_type_, arg_, is_ellipsis_, default_value_, 0)
	{
	}

FormalParameter::FormalParameter( char* name_, value_type parm_type_,
			Expr* arg_, int is_ellipsis_, Expr* default_value_ )
    : Parameter(name_, parm_type_, arg_, is_ellipsis_, default_value_, 0)
	{
	}

FormalParameter::FormalParameter( value_type parm_type_,
			Expr* arg_, int is_ellipsis_, Expr* default_value_ )
    : Parameter(parm_type_, arg_, is_ellipsis_, default_value_, 0)
	{
	}

void FormalParameter::Describe( OStream& s ) const
	{
	if ( parm_type == VAL_CONST )
		s << "const";
	else if ( parm_type == VAL_REF )
		s << "ref";
	else
		s << "val";

	s << " ";

	Parameter::Describe( s );
	}


UserFunc::UserFunc( parameter_list* arg_formals, Stmt* arg_body, int arg_size,
			Sequencer* arg_sequencer, Expr* arg_subsequence_expr, IValue *&err )
	{
	kernel = new UserFuncKernel(arg_formals, arg_body, arg_size,
				    arg_sequencer, arg_subsequence_expr, err);
	sequencer = arg_sequencer;
	scope_established = 0;
	stack = 0;
	}

UserFunc::UserFunc( const UserFunc *f ) : kernel(f->kernel), stack(0),
			scope_established(0), sequencer(f->sequencer)
	{
	Ref(kernel);
	}

UserFunc::~UserFunc()
	{
	Unref(kernel);
	
	if ( stack )
		Unref( stack );
	}

IValue* UserFunc::Call( parameter_list* args, eval_type etype )
	{
	IValue *last = FailStmt::SwapFail(0);

	IValue *ret = kernel->Call(args, etype,stack);

	if ( ret && ret->Type() != TYPE_FAIL )
		FailStmt::SetFail(last);

	Unref(last);

	return ret;
	}

void UserFunc::EstablishScope()
	{
	if ( ! scope_established && ! stack )
		{
		scope_established = 1;
		stack = sequencer->LocalFrames();
		}
	}

void UserFunc::Describe( OStream& s ) const
	{
	kernel->Describe(s);
	}

UserFuncKernel::UserFuncKernel( parameter_list* arg_formals, Stmt* arg_body, int arg_size,
			Sequencer* arg_sequencer, Expr* arg_subsequence_expr, IValue *&err )
	{
	formals = arg_formals;
	body = arg_body;
	frame_size = arg_size;
	sequencer = arg_sequencer;
	subsequence_expr = arg_subsequence_expr;

	valid = 1;
	err = 0;

	has_ellipsis = 0;
	loop_over_list( *formals, i )
		if ( (*formals)[i]->IsEllipsis() )
			{
			if ( has_ellipsis )
				{
				err = (IValue*) Fail(
			"\"...\" appears more than once in parameter list" );
				valid = 0;
				break;
				}

			has_ellipsis = 1;
			ellipsis_position = i;
			}
	}

UserFuncKernel::~UserFuncKernel()
	{
	loop_over_list( *formals, i )
		Unref((*formals)[i]);

	delete formals;	      

	NodeUnref(subsequence_expr);
	NodeUnref( body );
	}

IValue* UserFuncKernel::Call( parameter_list* args, eval_type etype, stack_type *stack )
	{
	if ( ! valid )
		return error_ivalue( "function not valid" );

	args_list args_vals;
	int do_call = 1;
	Parameter* f;

	int num_args = 0;
	int num_supplied_args = args->length();
	int num_formals;

	IValue* ellipsis_value;
	if ( has_ellipsis )
		{
		ellipsis_value = create_irecord();
		num_formals = ellipsis_position;
		}

	else
		{
		ellipsis_value = 0;
		num_formals = formals->length();
		}

	int missing_size = num_supplied_args + formals->length();
	int missing_len = 0;
	glish_bool* missing =
		(glish_bool*) alloc_memory( missing_size * sizeof(glish_bool) );
	if ( ! missing )
		fatal->Report( "out of memory in UserFunc::Call" );

// Macro to note which arguments were missing and which were present.  Use
// is: spin through the arguments sequentially and call the macro with either
// "true" or "false" to indicate that the next argument was missing/present.
#define ADD_MISSING_INFO(value)						\
	{								\
	if ( missing_len >= missing_size )				\
		{							\
		missing_size *= 2;					\
		missing = (glish_bool*) realloc_memory( (void*) missing,\
				missing_size * sizeof(glish_bool) );	\
		if ( ! missing )					\
			fatal->Report( "out of memory in UserFunc::Call" );\
		}							\
	missing[missing_len++] = value;					\
	}

	// Match until a named argument is encountered.
	int i;
	IValue *fail = 0;
	loop_over_list_nodecl( *args, i )
		{
		Parameter* arg = (*args)[i];

		if ( arg->Name() )
			break;

		if ( arg->IsEllipsis() )
			{
			fail = AddEllipsisArgs( &args_vals, arg, num_args, num_formals,
						ellipsis_value );
			for ( int j = 0; j < arg->NumEllipsisVals(); ++j )
				ADD_MISSING_INFO(glish_false)
			}

		else
			{
			if ( num_args >= num_formals )
				{
				Parameter* p = (*args)[i];
				if ( p->IsEmpty() )
					{
					f = (*formals)[ellipsis_position];
					Expr* dflt = f ? f->DefaultValue() : 0;
					if ( dflt )
						{
						fail = ArgOverFlow( dflt, num_args,
								    num_formals,
								    ellipsis_value );
						ADD_MISSING_INFO(glish_true)
						}
					else
						fail = (IValue*) Fail(
						"Missing parameter ", i+1,
						", no default available" );
					}
				else
					{
					fail = ArgOverFlow( p->Arg(), num_args,
							    num_formals,
							    ellipsis_value );
					ADD_MISSING_INFO(glish_false)
					}
				}

			else
				{
				f = (*formals)[num_args];
				Parameter* p = (*args)[i];
				if ( p->IsEmpty() )
					{
					Expr* dflt = f ? f->DefaultValue() : 0;
					if ( dflt )
						{
						IValue *v = EvalParam( f, dflt, fail );
						if ( fail ) break;
						args_vals.append( v );
						ADD_MISSING_INFO(glish_true)
						}
					else
						fail = (IValue*) Fail(
						"Missing parameter ", i+1, 
						", no default available." );
					}
				else
					{
					IValue *v = EvalParam( f, (*args)[i]->Arg(), fail );
					if ( fail ) break;
					args_vals.append( v );
					ADD_MISSING_INFO(glish_false)
					}

				++num_args;
				}
			}

		if ( fail ) break;
		}

	int first_named_arg = i;

	if ( ! fail && do_call )
		{
		// Check the named arguments to see if they're valid.
		for ( int named_arg = first_named_arg; named_arg < num_supplied_args ;
		      ++named_arg )
			{
			const char* arg_name = (*args)[named_arg]->Name();

			if ( ! arg_name )
				{
				if ( do_call )
					fail = (IValue*) Fail( "unnamed arg (position",
							named_arg,
						") given after named arg in call to",
							this );
				}

			else
				{
				int j;
				loop_over_list_nodecl( *formals, j )
					{
					const char* formal_name = (*formals)[j]->Name();

					if ( formal_name &&
					     ! strcmp( formal_name, arg_name ) )
						break;
					}

				if ( j >= formals->length() )
					fail = (IValue*) Fail( "named arg \"", arg_name,
					    "\" does not match any formal in call to",
							this );

				else if ( j < args_vals.length() )
					fail = (IValue*) Fail( "formal \"", arg_name,
				    "\" matched by both positional and named arg in call to",
							this );
				}

			if ( fail ) break;
			}
		}

	if ( do_call && ! fail )
		{
		// Fill in remaining formals, looking for matching named
		// arguments or else using the formal's default value (if
		// any).  Also put the ellipsis value (if any) into args_vals.

		// Be sure to match formals beyond the ellipsis, too.
		num_formals = formals->length();

		for ( ; num_args < num_formals; ++num_args )
			{
			f = (*formals)[num_args];

			if ( f->IsEllipsis() )
				{
				args_vals.append( ellipsis_value );
				continue;
				}

			const char* formal_name = f->Name();

			int match = first_named_arg;
			for ( ; match < num_supplied_args; ++match )
				{
				const char* actual_name =
					(*args)[match]->Name();
				if ( ! strcmp( formal_name, actual_name ) )
					break;
				}

			if ( match < num_supplied_args )
				{
				IValue *v = EvalParam( f, (*args)[match]->Arg(), fail );
				if ( fail ) break;
				args_vals.append( v );
				ADD_MISSING_INFO(glish_false)
				}
			else
				{
				Expr* default_value = f->DefaultValue();

				if ( ! default_value )
					fail = (IValue*) Fail( "parameter \"",
							f->Name(),
							"\" missing in call to",
							this );
				else
					{
					IValue *v = EvalParam( f, default_value, fail );
					if ( fail ) break;
					args_vals.append( v );
					ADD_MISSING_INFO(glish_true)
					}
				}

			if ( fail ) break;
			}
		}

	IValue* result;

	if ( do_call && ! fail )
		{
		IValue* missing_val = missing_len > 0 ?
			new IValue( missing, missing_len, PRESERVE_ARRAY ) : 0;
		result = DoCall( &args_vals, etype, missing_val, stack );
		// No need to Unref() missing_val, Sequencer::PopFrame did
		// that for us.
		}

	else
		{
		loop_over_list( args_vals, k )
			Unref( args_vals[k] );

		result = fail ? fail : error_ivalue();
		}

	free_memory( (void*) missing );

	return result;
	}

IValue* UserFuncKernel::DoCall( args_list* args_vals, eval_type etype, IValue* missing, stack_type *stack )
	{
	Frame* call_frame = new Frame( frame_size, missing, FUNC_SCOPE );
	int local_pushed = 0;

	if ( stack )
		sequencer->PushFrames( stack );

	sequencer->PushFrame( call_frame );

	if ( subsequence_expr )
		{
		UserAgent* self = new UserAgent( sequencer );
		subsequence_expr->Assign( new IValue( self->AgentRecord(),
							VAL_REF ) );
		}

	loop_over_list( (*formals), i )
		(*formals)[i]->Arg()->Assign( (*args_vals)[i] );

	int value_needed = etype != EVAL_SIDE_EFFECTS;
	stmt_flow_type flow;
	IValue* result = body->Exec( value_needed, flow );

	if ( sequencer->PopFrame() != call_frame )
		fatal->Report( "frame inconsistency in UserFunc::DoCall" );

	if ( stack )
		sequencer->PopFrames( );

	Unref( call_frame );

	if ( subsequence_expr )
		{
		Str err;
		if ( result &&
		     (result->Type() != TYPE_BOOL || result->BoolVal(1,err)) )
			{
			if ( err.chars() )
				{
				Unref( result );
				return (IValue*) Fail(err.chars());
				}

			warn->Report( "value (", result,
			") returned from subsequence replaced by ref self" );
			Unref( result );
			}

		result = subsequence_expr->RefEval( VAL_REF );

		if ( etype == EVAL_SIDE_EFFECTS )
			warn->Report( "agent returned by subsequence ignored" );
		}

	return result;
	}


void UserFuncKernel::Describe( OStream& s ) const
	{
	s << "function (";
	describe_parameter_list( formals, s );
	s << ") ";
	body->Describe( s );
	}


IValue* UserFuncKernel::EvalParam( Parameter* p, Expr* actual, IValue *&fail )
	{
	fail = 0;
	value_type param_type = p->ParamType();

	if ( param_type == VAL_CONST )
		{
		IValue *ret = actual->CopyEval();
		if ( ret->Type() == TYPE_FAIL )
			{
			fail = ret;
			return 0;
			}
		ret->MakeConst( );
		return ret;
		}
	else if ( param_type == VAL_VAL )
		{
		IValue *ret = actual->CopyEval();
		if ( ret->Type() == TYPE_FAIL )
			{
			fail = ret;
			return 0;
			}
		return ret;
		}
	else
		{
		IValue *ret = actual->RefEval( param_type );
		if ( ret->Type() == TYPE_FAIL )
			{
			fail = ret;
			return 0;
			}
		return ret;
		}
	}


IValue *UserFuncKernel::AddEllipsisArgs( args_list* args_vals,
				Parameter* actual_ellipsis, int& num_args,
				int num_formals, IValue* formal_ellipsis_value )
	{
	int len = actual_ellipsis->NumEllipsisVals();

	for ( int i = 0; i < len; ++i )
		{
		const IValue* val;

		if ( num_args >= num_formals )
			{
			if ( ! formal_ellipsis_value )
				return (IValue*) Fail( "too many arguments (> ",
						num_formals,
						") supplied in call to", this );

			char field_name[256];
			sprintf( field_name, "%d",
				formal_ellipsis_value->RecordPtr(0)->Length() );

			val = actual_ellipsis->NthEllipsisVal( i );

			IValue* val_ref = new IValue( (IValue*) val, VAL_CONST );
			formal_ellipsis_value->AssignRecordElement( field_name,
								val_ref );

			// See comment in Value.h regarding AssignRecordElement.
			Unref( val_ref );

			continue;
			}

		Parameter* f = (*formals)[num_args];

		if ( f->ParamType() == VAL_REF )
			return (IValue*) Fail(
				"\"...\" is a \"const\" reference, formal",
					f->Name(), " is \"ref\"" );

		val = actual_ellipsis->NthEllipsisVal( i );

		if ( f->ParamType() == VAL_VAL )
			args_vals->append( copy_value( val ) );

		else
			{
			IValue* val_ref = new IValue( (IValue*) val, VAL_CONST );
			args_vals->append( val_ref );
			}

		++num_args;
		}

	return 0;
	}


void UserFuncKernel::AddEllipsisValue( IValue* ellipsis_value, Expr* arg )
	{
	char field_name[256];
	sprintf( field_name, "%d", ellipsis_value->RecordPtr(0)->Length() );

	IValue* val = arg->RefEval( VAL_CONST );

	ellipsis_value->AssignRecordElement( field_name, val );

	Unref( val );	// see comment in Value.h regarding AssignRecordElement
	}


IValue *UserFuncKernel::ArgOverFlow( Expr* arg, int num_args, int num_formals,
				IValue* ellipsis_value )
	{
	if ( ellipsis_value )
		AddEllipsisValue( ellipsis_value, arg );

	else
		if ( num_args == num_formals )
			return (IValue*) Fail( "too many arguments (> ",
				num_formals, ") supplied in call to",
				this );

	return 0;
	}


void describe_parameter_list( parameter_list* params, OStream& s )
	{
	loop_over_list( *params, i )
		{
		if ( i > 0 )
			s << ", ";

		(*params)[i]->Describe( s );
		}
	}
