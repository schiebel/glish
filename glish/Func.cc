// $Header$

#include <string.h>
#include <stream.h>
#include <math.h>
#include "Agent.h"
#include "Func.h"
#include "Stmt.h"
#include "Frame.h"
#include "Sequencer.h"
#include "Reporter.h"


Parameter::Parameter( const char* arg_name, value_type arg_parm_type,
			Expr* arg_arg, bool arg_is_ellipsis,
			Expr* arg_default_value )
	{
	name = arg_name;
	parm_type = arg_parm_type;
	arg = arg_arg;
	is_ellipsis = arg_is_ellipsis;
	default_value = arg_default_value;
	}

int Parameter::NumEllipsisVals() const
	{
	if ( ! is_ellipsis )
		fatal->Report(
			"Parameter::NumEllipsisVals called but not ellipsis" );

	const Value* values = Arg()->ReadOnlyEval();

	int n = values->RecordPtr()->Length();

	Arg()->ReadOnlyDone( values );

	return n;
	}

const Value* Parameter::NthEllipsisVal( int n ) const
	{
	if ( ! is_ellipsis )
		fatal->Report(
			"Parameter::NthEllipsisVal called but not ellipsis" );

	const Value* values = Arg()->ReadOnlyEval();

	if ( n < 0 || n >= values->RecordPtr()->Length() )
		fatal->Report( "bad value of n in Parameter::NthEllipsisVal" );

	char field_name[256];
	sprintf( field_name, "%d", n );

	const Value* result = values->ExistingRecordElement( field_name );

	Arg()->ReadOnlyDone( values );

	return result;
	}

void Parameter::Describe( ostream& s ) const
	{
	if ( name )
		s << name;
	else
		arg->Describe( s );

	if ( default_value )
		{
		s << " = ";
		default_value->Describe( s );
		}
	}


FormalParameter::FormalParameter( const char* name, value_type parm_type,
			Expr* arg, bool is_ellipsis, Expr* default_value )
    : Parameter(name, parm_type, arg, is_ellipsis, default_value)
	{
	}

void FormalParameter::Describe( ostream& s ) const
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


ActualParameter::ActualParameter( const char* name, value_type parm_type,
			Expr* arg, bool is_ellipsis, Expr* default_value )
    : Parameter(name, parm_type, arg, is_ellipsis, default_value)
	{
	}


UserFunc::UserFunc( parameter_list* arg_formals, Stmt* arg_body, int arg_size,
			Sequencer* arg_sequencer, Expr* arg_subsequence_expr )
	{
	formals = arg_formals;
	body = arg_body;
	frame_size = arg_size;
	sequencer = arg_sequencer;
	subsequence_expr = arg_subsequence_expr;

	valid = true;

	has_ellipsis = false;
	loop_over_list( *formals, i )
		if ( (*formals)[i]->IsEllipsis() )
			{
			if ( has_ellipsis )
				{
				error->Report(
			"\"...\" appears more than once in parameter list" );
				valid = false;
				break;
				}

			has_ellipsis = true;
			ellipsis_position = i;
			}
	}

Value* UserFunc::Call( parameter_list* args, eval_type etype )
	{
	if ( ! valid )
		return new Value( false );

	args_list args_vals;
	bool do_call = true;
	Parameter* f;

	int num_args = 0;
	int num_supplied_args = args->length();
	int num_formals;

	Value* ellipsis_value;

	if ( has_ellipsis )
		{
		ellipsis_value = create_record();
		num_formals = ellipsis_position;
		}

	else
		{
		ellipsis_value = 0;
		num_formals = formals->length();
		}

	// Match until a named argument is encountered.
	loop_over_list( *args, i )
		{
		Parameter* arg = (*args)[i];

		if ( arg->Name() )
			break;

		if ( arg->IsEllipsis() )
			AddEllipsisArgs( &args_vals, arg, num_args, num_formals,
					ellipsis_value, do_call );

		else
			{
			if ( num_args >= num_formals )
				ArgOverFlow( (*args)[i]->Arg(),
						num_args, num_formals,
						ellipsis_value, do_call );

			else
				{
				f = (*formals)[num_args];
				args_vals.append(
					EvalParam( f, (*args)[i]->Arg() ) );
				++num_args;
				}
			}
		}

	int first_named_arg = i;

	// Check the named arguments to see if they're valid.
	for ( int named_arg = first_named_arg; named_arg < num_supplied_args;
	      ++named_arg )
		{
		const char* arg_name = (*args)[named_arg]->Name();

		if ( ! arg_name )
			{
			if ( do_call )
				{
				error->Report( "unnamed arg (position",
						named_arg,
					") given after named arg in call to",
						this );
				do_call = false;
				}
			}

		else
			{
			loop_over_list( *formals, j )
				{
				const char* formal_name = (*formals)[j]->Name();

				if ( formal_name &&
				     ! strcmp( formal_name, arg_name ) )
					break;
				}

			if ( j >= formals->length() )
				{
				error->Report( "named arg \"", arg_name,
				    "\" does not match any formal in call to",
						this );
				do_call = false;
				}

			else if ( j < args_vals.length() )
				{
				error->Report( "formal \"", arg_name,
		    "\" matched by both positional and named arg in call to",
						this );
				do_call = false;
				}
			}
		}

	if ( do_call )
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

			for ( int match = first_named_arg;
			      match < num_supplied_args; ++match )
				{
				const char* actual_name =
					(*args)[match]->Name();
				if ( ! strcmp( formal_name, actual_name ) )
					break;
				}

			if ( match < num_supplied_args )
				args_vals.append(
					EvalParam( f, (*args)[match]->Arg() ) );

			else
				{
				Expr* default_value = f->DefaultValue();

				if ( ! default_value )
					{
					error->Report( "parameter \"",
							f->Name(),
							"\" missing in call to",
							this );
					do_call = false;
					}

				else
					args_vals.append(
						EvalParam( f, default_value ) );
				}
			}
		}

	Value* result;

	if ( do_call )
		result = DoCall( &args_vals, etype );

	else
		{
		loop_over_list( args_vals, k )
			Unref( args_vals[k] );

		result = new Value( false );
		}

	return result;
	}

Value* UserFunc::DoCall( args_list* args_vals, eval_type etype )
	{
	Frame* call_frame = new Frame( frame_size );
	sequencer->PushFrame( call_frame );

	if ( subsequence_expr )
		{
		UserAgent* self = new UserAgent( sequencer );
		subsequence_expr->Assign( new Value( self->AgentRecord(),
							VAL_REF ) );
		}

	loop_over_list( (*formals), i )
		(*formals)[i]->Arg()->Assign( (*args_vals)[i] );

	bool value_needed = bool(etype != EVAL_SIDE_EFFECTS);
	stmt_flow_type flow;
	Value* result = body->Exec( value_needed, flow );

	if ( subsequence_expr )
		{
		if ( result &&
		     (result->Type() != TYPE_BOOL || result->BoolVal()) )
			{
			warn->Report( "value (", result,
			") returned from subsequence replaced by ref self" );
			Unref( result );
			}

		result = subsequence_expr->RefEval( VAL_REF );

		if ( etype == EVAL_SIDE_EFFECTS )
			warn->Report( "agent returned by subsequence ignored" );
		}

	if ( sequencer->PopFrame() != call_frame )
		fatal->Report( "frame inconsistency in UserFunc::DoCall" );

	Unref( call_frame );

	return result;
	}


void UserFunc::Describe( ostream& s ) const
	{
	s << "function (";
	describe_parameter_list( formals, s );
	s << ") ";
	body->Describe( s );
	}


Value* UserFunc::EvalParam( Parameter* p, Expr* actual )
	{
	value_type param_type = p->ParamType();

	if ( param_type == VAL_VAL )
		return actual->CopyEval();
	else
		return actual->RefEval( param_type );
	}


void UserFunc::AddEllipsisArgs( args_list* args_vals,
				Parameter* actual_ellipsis, int& num_args,
				int num_formals, Value* formal_ellipsis_value,
				bool& do_call )
	{
	int len = actual_ellipsis->NumEllipsisVals();

	for ( int i = 0; i < len; ++i )
		{
		const Value* val;

		if ( num_args >= num_formals )
			{
			if ( ! formal_ellipsis_value )
				{
				error->Report( "too many arguments (> ",
						num_formals,
						") supplied in call to", this );
				do_call = false;
				return;
				}

			char field_name[256];
			sprintf( field_name, "%d",
				formal_ellipsis_value->RecordPtr()->Length() );

			val = actual_ellipsis->NthEllipsisVal( i );

			Value* val_ref = new Value( (Value*) val, VAL_CONST );
			formal_ellipsis_value->AssignRecordElement( field_name,
								val_ref );

			// See comment in Value.h regarding AssignRecordElement.
			Unref( val_ref );

			continue;
			}

		Parameter* f = (*formals)[num_args];

		if ( f->ParamType() == VAL_REF )
			{
			error->Report(
				"\"...\" is a \"const\" reference, formal",
					f->Name(), " is \"ref\"" );
			do_call = false;
			return;
			}

		val = actual_ellipsis->NthEllipsisVal( i );

		if ( f->ParamType() == VAL_VAL )
			args_vals->append( copy_value( val ) );

		else
			{
			Value* val_ref = new Value( (Value*) val, VAL_CONST );
			args_vals->append( val_ref );
			}

		++num_args;
		}
	}


void UserFunc::AddEllipsisValue( Value* ellipsis_value, Expr* arg )
	{
	char field_name[256];
	sprintf( field_name, "%d", ellipsis_value->RecordPtr()->Length() );

	Value* val = arg->RefEval( VAL_CONST );

	ellipsis_value->AssignRecordElement( field_name, val );

	Unref( val );	// see comment in Value.h regarding AssignRecordElement
	}


void UserFunc::ArgOverFlow( Expr* arg, int num_args, int num_formals,
				Value* ellipsis_value, bool& do_call )
	{
	if ( ellipsis_value )
		AddEllipsisValue( ellipsis_value, arg );

	else
		{
		if ( num_args == num_formals + 1 )
			{
			error->Report( "too many arguments (> ",
				num_formals, ") supplied in call to",
				this );
			}

		do_call = false;
		}
	}


void describe_parameter_list( parameter_list* params, ostream& s )
	{
	loop_over_list( *params, i )
		{
		if ( i > 0 )
			s << ", ";

		(*params)[i]->Describe( s );
		}
	}
