// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997,1998 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include <string.h>
#include <iostream.h>
#include <math.h>
#include <stdlib.h>

#include "Agent.h"
#include "Func.h"
#include "Stmt.h"
#include "Frame.h"
#include "Sequencer.h"
#include "Reporter.h"

#ifdef GGC
void Func::TagGC() { }
#endif

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

int Parameter::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( name )
		s << name;
	else if ( arg )
		arg->Describe( s, ioOpt(opt.flags(),opt.sep()) );

	if ( default_value )
		{
		s << " = ";
		default_value->Describe( s, ioOpt(opt.flags(),opt.sep()) );
		}
	return 1;
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

int FormalParameter::Describe( OStream& s, const ioOpt &opt ) const
	{
	if ( parm_type == VAL_CONST )
		s << "const";
	else if ( parm_type == VAL_REF )
		s << "ref";
	else
		s << "val";

	s << " ";

	Parameter::Describe( s, ioOpt(opt.flags(),opt.sep()) );
	return 1;
	}


UserFunc::UserFunc( parameter_list* arg_formals, Stmt* arg_body, int arg_size,
		    Sequencer* arg_sequencer, Expr* arg_subsequence_expr,
		    IValue *&err, const IValue *attributes, ivalue_list *misc_values )
	{
	kernel = new UserFuncKernel(arg_formals, arg_body, arg_size, arg_sequencer,
				    arg_subsequence_expr, attributes, err);
	sequencer = arg_sequencer;
	misc = misc_values;
	scope_established = 0;
	stack = 0;
	}

UserFunc::UserFunc( const UserFunc *f ) : sequencer(f->sequencer), kernel(f->kernel),
			scope_established(0), stack(0), misc(f->misc)
	{
	Ref(kernel);
	if ( misc ) Ref(misc);
	}

UserFunc::~UserFunc()
	{
	Unref(kernel);
	
	if ( stack )
		Unref( stack );
	if ( misc )
		Unref( misc );
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

int UserFunc::Describe( OStream& s, const ioOpt &opt ) const
	{
	return kernel->Describe(s, opt);
	}


#ifdef MEMFREE
unsigned int UserFunc::CountRefs( recordptr r ) const
	{
	static func_list been_there;

	if ( been_there.is_member( (UserFunc*) this ) )
		return 0;

	been_there.append( (UserFunc*) this );

	unsigned int count = 0;
	if ( stack )
		{
		frame_list *frames = stack->frames();
		if ( frames )
			{
			loop_over_list( *frames, j )
				{
				Frame *f = (*frames)[j];
				if ( f ) count += f->CountRefs(r);
				}
			}
		}

	been_there.remove( (UserFunc*) this );

	return count;
	}

int UserFunc::CountRefs( Frame *f ) const
	{
	int count = 0;
	if ( stack )
		{
		frame_list *frames = stack->frames();
		if ( frames )
			loop_over_list( *frames, j )
				if ( (*frames)[j] == f )
					count += 1;
		}
	return count;
	}
#endif

#ifdef GGC
void UserFunc::TagGC( )
	{
	if ( stack ) stack->TagGC( );
	if ( misc )
		loop_over_list( *misc, i )
			if ( (*misc)[i] )
				(*misc)[i]->TagGC( );
	}
#endif

UserFuncKernel::UserFuncKernel( parameter_list* arg_formals, Stmt* arg_body, int arg_size,
				Sequencer* arg_sequencer, Expr* arg_subsequence_expr,
				const IValue *attributes, IValue *&err )
	{
	formals = arg_formals;
	body = arg_body;
	frame_size = arg_size;
	sequencer = arg_sequencer;
	subsequence_expr = arg_subsequence_expr;

	const Value *tmp = 0;
	reflect_events = subsequence_expr && attributes && attributes->Type() == TYPE_RECORD &&
				(tmp=attributes->HasRecordElement("reflect")) &&
				tmp->IsNumeric() && tmp->IntVal();

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

	int arg_cnt = 0;
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
	IValue *missing_val = empty_ivalue();
	Frame* call_frame = new Frame( frame_size, missing_val, FUNC_SCOPE );

	loop_over_list_nodecl( *args, i )
		{
		Parameter* arg = (*args)[i];

		if ( arg->Name() )
			break;

		if ( arg->IsEllipsis() )
			{
			fail = AddEllipsisArgs( call_frame, arg_cnt, arg, num_args, num_formals,
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
						((VarExpr*)(*formals)[i]->Arg())->Assign( v, call_frame );
						arg_cnt++;
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
					((VarExpr*)(*formals)[i]->Arg())->Assign( v, call_frame );
					arg_cnt++;
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

				else if ( j < arg_cnt )
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
				((VarExpr*)(*formals)[num_args]->Arg())->Assign( ellipsis_value, call_frame );
				arg_cnt++;
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
				((VarExpr*)(*formals)[num_args]->Arg())->Assign( v, call_frame );
				arg_cnt++;
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
					((VarExpr*)(*formals)[num_args]->Arg())->Assign( v, call_frame );
					arg_cnt++;
					ADD_MISSING_INFO(glish_true)
					}
				}

			if ( fail ) break;
			}
		}

	loop_over_list_nodecl( *formals, i )
		((VarExpr*)(*formals)[i]->Arg())->PopFrame( );

	IValue* result;
	if ( do_call && ! fail )
		{
		if ( missing_len > 0 )
			{
			IValue* misx =  new IValue( missing, missing_len, PRESERVE_ARRAY );
			missing_val->TakeValue( misx );
			}

		//
		// Push on a NULL note so that when the function returns
		// any await info that accumulates can be removed...
		//
		sequencer->PushNote( 0 );

		if ( stack ) sequencer->PushFrames( stack );
		sequencer->PushFrame( call_frame );

		result = DoCall( etype, stack );
		// No need to Unref() missing_val, Sequencer::PopFrame did
		// that for us.

		loop_over_list_nodecl( *formals, i )
			((VarExpr*)(*formals)[i]->Arg())->ReleaseFrames( );

		if ( sequencer->PopFrame() != call_frame )
			fatal->Report( "frame inconsistency in UserFunc::DoCall" );

		if ( stack )
			if ( sequencer->PopFrames( ) != stack )
				fatal->Report( "stack inconsistency in UserFunc::DoCall" );

		//
		// Pop off down to the NULL we pushed on...
		//
		sequencer->PopNote( 1 );

		}

	else

		result = fail ? fail : error_ivalue();

	free_memory( (void*) missing );

	Unref( call_frame );

	return result;
	}

IValue* UserFuncKernel::DoCall( eval_type etype, stack_type * )
	{
	if ( subsequence_expr )
		{
		UserAgent* self = new UserAgent( sequencer, 1 );
		subsequence_expr->Assign( new IValue( self->AgentRecord(),
							VAL_REF ) );
		if ( reflect_events )
			self->SetReflect( );
		}

	int value_needed = etype != EVAL_SIDE_EFFECTS;
	stmt_flow_type flow;

	//
	// need to set "file_name" for errors during execution
	//
	unsigned short old_file_name = file_name;
	file_name = file;
	IValue* result = body->Exec( value_needed, flow );
	file_name = old_file_name;

	if ( subsequence_expr )
		{
		Str err;
		if ( result )
			{
			if ( result->Type() == TYPE_FAIL )
				return result;

			if ( result->Type() != TYPE_BOOL || result->BoolVal(1,err) )
				{
				if ( err.chars() )
					{
					Unref( result );
					return (IValue*) Fail(err.chars());
					}

				warn->Report( "value (", result,
					      ") returned from subsequence replaced by ref self" );
				}

			Unref( result );
			}

		file_name = file;
		result = subsequence_expr->RefEval( VAL_REF );
		file_name = old_file_name;

		if ( etype == EVAL_SIDE_EFFECTS )
			warn->Report( "agent returned by subsequence ignored" );
		}

	return result;
	}


int UserFuncKernel::Describe( OStream& s, const ioOpt &opt ) const
	{
	s << "function (";
	describe_parameter_list( formals, s );
	s << ") ";
	if ( opt.flags(ioOpt::SHORT()) ) return 1;
	body->Describe( s );
	return 1;
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


IValue *UserFuncKernel::AddEllipsisArgs( Frame *call_frame, int &arg_cnt,
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
			{
			((VarExpr*)(*formals)[num_args]->Arg())->Assign( copy_value(val), call_frame );
			arg_cnt++;
			}

		else
			{
			IValue* val_ref = new IValue( (IValue*) val, VAL_CONST );
			((VarExpr*)(*formals)[num_args]->Arg())->Assign( val_ref, call_frame );
			arg_cnt++;
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
