// $Header$

#include <string.h>
#include <stream.h>
#include <stdlib.h>
#include <math.h>
#include "Sds/sdsgen.h"
#include "glish_event.h"
#include "BuiltIn.h"
#include "Reporter.h"
#include "Task.h"
#include "Sequencer.h"


Value* BuiltIn::Call( parameter_list* args, eval_type etype )
	{
	if ( num_args != NUM_ARGS_VARIES )
		{
		int num_args_present = 0;

		loop_over_list( *args, i )
			{
			if ( (*args)[i]->IsEllipsis() )
				num_args_present +=
					(*args)[i]->NumEllipsisVals();
			else
				++num_args_present;
			}

		if ( num_args_present != num_args )
			{
			error->Report( this, " takes", num_args, " argument",
					num_args == 1 ? ";" : "s;",
					num_args_present, " given" );

			return new Value( false );
			}
		}

	loop_over_list( *args, j )
		{
		if ( (*args)[j]->Name() )
			{
			error->Report( this,
					" does not have a parameter named \"",
					(*args)[j]->Name(), "\"" );
			return new Value( false );
			}
		}

	const_args_list* args_vals = new const_args_list;

	bool do_call = true;

	loop_over_list( *args, i )
		{
		Parameter* arg = (*args)[i];
		const Value* arg_val;

		if ( arg->IsEllipsis() )
			{
			int len = arg->NumEllipsisVals();

			for ( int j = 0; j < len; ++j )
				{
				arg_val = arg->NthEllipsisVal( j );
				if ( do_deref )
					arg_val = arg_val->Deref();

				args_vals->append( arg_val );
				}
			}

		else
			{
			arg_val = arg->Arg()->ReadOnlyEval();
			if ( do_deref )
				arg_val = arg_val->Deref();

			args_vals->append( arg_val );
			}
		}

	Value* result;

	if ( do_call )
		{
		if ( etype == EVAL_SIDE_EFFECTS )
			{
			bool side_effects_okay = false;
			DoSideEffectsCall( args_vals, side_effects_okay );

			if ( ! side_effects_okay )
				warn->Report( "function return value ignored:",
						this );

			result = 0;
			}

		else
			result = DoCall( args_vals );
		}
	else
		result = new Value( false );

	loop_over_list( *args, k )
		{
		if ( ! (*args)[k]->IsEllipsis() )
			(*args)[k]->Arg()->ReadOnlyDone( (*args_vals)[k] );
		}

	delete args_vals;

	return result;
	}

void BuiltIn::DoSideEffectsCall( const_args_list* args_vals,
				bool& side_effects_okay )
	{
	side_effects_okay = side_effects_call_okay;
	Unref( DoCall( args_vals ) );
	}

void BuiltIn::DescribeSelf( ostream& s ) const
	{
	s << description << "()";
	}

bool BuiltIn::AllNumeric( const_args_list* args_vals, bool strings_okay )
	{
	loop_over_list( *args_vals, i )
		{
		const Value* arg = (*args_vals)[i];

		if ( arg->IsNumeric() )
			continue;

		if ( strings_okay && arg->Type() == TYPE_STRING )
			continue;

		error->Report( "argument #", i + 1, "to", this,
			"is not numeric", strings_okay ? " or a string" : "" );
		return false;
		}

	return true;
	}


Value* OneValueArgBuiltIn::DoCall( const_args_list* args_val )
	{
	return (*func)( (*args_val)[0] );
	}


Value* DoubleVectorBuiltIn::DoCall( const_args_list* args_val )
	{
	const Value* arg = (*args_val)[0];

	if ( ! arg->IsNumeric() )
		{
		error->Report( this, " requires a numeric argument" );
		return new Value( false );
		}

	int len = arg->Length();

	bool is_copy;
	double* args_vec = arg->CoerceToDoubleArray( is_copy, len );
	double* result = new double[len];

	for ( int i = 0; i < len; ++i )
		result[i] = (*func)( args_vec[i] );

	if ( is_copy )
		delete args_vec;

	return new Value( result, len );
	}

Value* SumBuiltIn::DoCall( const_args_list* args_val )
	{
	if ( ! AllNumeric( args_val ) )
		return new Value( false );

	double sum = 0.0;

	loop_over_list( *args_val, i )
		{
		const Value* val = (*args_val)[i];
		int len = val->Length();
		bool is_copy;
		double* val_array = val->CoerceToDoubleArray( is_copy, len );

		for ( int j = 0; j < len; ++j )
			sum += val_array[j];

		if ( is_copy )
			delete val_array;
		}

	return new Value( sum );
	}

Value* RangeBuiltIn::DoCall( const_args_list* args_val )
	{
	if ( ! AllNumeric( args_val ) )
		return new Value( false );

	double min_val = HUGE;
	double max_val = -HUGE;

	loop_over_list( *args_val, i )
		{
		const Value* val = (*args_val)[i];
		int len = val->Length();
		bool is_copy;
		double* val_array = val->CoerceToDoubleArray( is_copy, len );

		for ( int j = 0; j < len; ++j )
			{
			if ( val_array[j] < min_val )
				min_val = val_array[j];

			if ( val_array[j] > max_val )
				max_val = val_array[j];
			}

		if ( is_copy )
			delete val_array;
		}

	double* range = new double[2];
	range[0] = min_val;
	range[1] = max_val;

	return new Value( range, 2 );
	}

Value* SeqBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();

	if ( len == 0 || len > 3 )
		{
		error->Report( this, " takes from one to three arguments" );
		return new Value( false );
		}

	double starting_point = 1.0;
	double stopping_point;
	double stride = 1.0;

	const Value* arg;

	if ( len == 1 )
		{
		arg = (*args_val)[0];

		if ( arg->Length() != 1 )
			stopping_point = double( arg->Length() );
		else
			stopping_point = double( arg->IntVal() );
		}

	else
		{
		starting_point = (*args_val)[0]->DoubleVal();
		stopping_point = (*args_val)[1]->DoubleVal();

		if ( len == 3 )
			stride = (*args_val)[2]->DoubleVal();

		else if ( starting_point > stopping_point )
			stride = -1;
		}

	if ( stride == 0 )
		{
		error->Report( "in call to ", this, ", stride = 0" );
		return new Value( false );
		}

	if ( (starting_point < stopping_point && stride < 0) ||
	     (starting_point > stopping_point && stride > 0) )
		{
		error->Report( "in call to ", this,
				", stride has incorrect sign" );
		return new Value( false );
		}

	double range = stopping_point - starting_point;
	int num_vals = int( range / stride ) + 1;

	if ( num_vals > 1e6 )
		{
		error->Report( "ridiculously large sequence in call to ",
				this );
		return new Value( false );
		}

	double* result = new double[num_vals];

	double val = starting_point;
	for ( int i = 0; i < num_vals; ++i )
		{
		result[i] = val;
		val += stride;
		}

	Value* result_val = new Value( result, num_vals );

	if ( starting_point == double( int( starting_point ) ) &&
	     stopping_point == double( int( stopping_point ) ) &&
	     stride == double( int( stride ) )  )
		result_val->Polymorph( TYPE_INT );

	return result_val;
	}

Value* NumArgsBuiltIn::DoCall( const_args_list* args_val )
	{
	return new Value( args_val->length() );
	}

Value* NthArgBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();

	if ( len <= 0 )
		{
		error->Report( "first argument missing in call to", this );
		return new Value( false );
		}

	int n = (*args_val)[0]->IntVal();

	if ( n < 0 || n >= len )
		{
		error->Report( "first argument (=", n, ") to", this,
				" out of range: ", len - 1,
				"additional arguments supplied" );
		return new Value( false );
		}

	return copy_value( (*args_val)[n] );
	}


Value* PasteBuiltIn::DoCall( const_args_list* args_val )
	{
	if ( args_val->length() == 0 )
		{
		error->Report( "paste() invoked with no arguments" );
		return new Value( false );
		}

	// First argument gives separator string.
	char* separator = (*args_val)[0]->StringVal();

	charptr* string_vals = new charptr[args_val->length()];

	int len = 1;	// Room for end-of-string.
	int sep_len = strlen( separator );

	for ( int i = 1; i < args_val->length(); ++i )
		{
		string_vals[i] = (*args_val)[i]->StringVal();
		len += strlen( string_vals[i] ) + sep_len;
		}

	char* paste_val = new char[len];
	paste_val[0] = '\0';

	for ( int j = 1; j < i; ++j )
		{
		strcat( paste_val, string_vals[j] );

		if ( j < i - 1 )
			strcat( paste_val, separator );

		delete (char*) string_vals[j];
		}

	delete string_vals;
	delete separator;

	charptr* result = new charptr[1];
	result[0] = paste_val;

	return new Value( result, 1 );
	}

Value* SplitBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();

	if ( len < 1 || len > 2 )
		{
		error->Report( this, " takes 1 or 2 arguments" );
		return new Value( false );
		}

	char* source = (*args_val)[0]->StringVal();

	char* split_chars = " \t\n";
	if ( len == 2 )
		split_chars = (*args_val)[1]->StringVal();

	Value* result = split( source, split_chars );

	delete source;
	if ( len == 2 )
		delete split_chars;

	return result;
	}


Value* ReadValueBuiltIn::DoCall( const_args_list* args_val )
	{
	char* file_name = (*args_val)[0]->StringVal();

	int sds = (int) sds_use( file_name, SDS_FILE, SDS_READ );

	Value* result;

	if ( sds < 0 )
		{
		error->Report( "could not read value from \"", file_name,
				"\"" );
		result = new Value( false );
		}

	else
		result = read_value_from_SDS( sds, SDS_EVENT );

	delete file_name;

	return result;
	}


Value* WriteValueBuiltIn::DoCall( const_args_list* args_val )
	{
	char* file_name = (*args_val)[1]->StringVal();
	const Value* v = (*args_val)[0];

	bool result = true;

	if ( v->Type() == TYPE_OPAQUE )
		{
		int sds = v->SDS_IndexVal();

		if ( sds_assemble( sds, file_name, SDS_FILE ) != sds )
			{
			error->Report( "could not save opaque value to \"",
					file_name, "\"" );
			result = false;
			}
		}

	else
		{
		int sds = (int) sds_new_index( (char*) "" );

		if ( sds < 0 )
			{
			error->Report( "problem saving value to \"", file_name,
					"\", SDS error code = ", sds );
			result = false;
			}

		else
			{
			del_list d;

			(*args_val)[0]->AddToSds( sds, &d );

			if ( sds_assemble( sds, file_name, SDS_FILE ) != sds )
				{
				error->Report( "could not save value to \"",
						file_name, "\"" );
				result = false;
				}

			sds_destroy( sds );

			delete_list( &d );
			}
		}

	delete file_name;

	return new Value( result );
	}


Value* WheneverStmtsBuiltIn::DoCall( const_args_list* args_val )
	{
	Agent* agent = (*args_val)[0]->AgentVal();

	if ( ! agent )
		return new Value( false );

	else
		return agent->AssociatedStatements();
	}


Value* ActiveAgentsBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	Value* r = create_record();

	loop_over_list( agents, i )
		{
		Value* a = agents[i]->AgentRecord();
		r->SetField( r->NewFieldName(), new Value( a, VAL_REF ) );
		}

	return r;
	}


Value* CreateAgentBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	Agent* user_agent = new UserAgent( sequencer );
	return user_agent->AgentRecord();
	}


Value* CurrentWheneverBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	Notification* n = sequencer->LastNotification();

	if ( ! n )
		{
		error->Report( "no active whenever, in call to", this );
		return new Value( 0 );
		}

	return new Value( n->notifiee->stmt->Index() );
	}

Value* LastWheneverExecutedBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	Stmt* s = sequencer->LastWheneverExecuted();

	if ( ! s )
		{
		error->Report( "no whenever's executed, in call to", this );
		return new Value( 0 );
		}

	return new Value( s->Index() );
	}


#define DEFINE_AS_XXX_BUILT_IN(name,type,tag,stringcvt,coercer,text,zero) \
Value* name( const Value* arg )						\
	{								\
	int len = arg->Length();					\
									\
	if ( arg->Type() == TYPE_STRING )				\
		{							\
		const charptr* strings = arg->StringPtr();		\
		type* result = new type[len];				\
									\
		for ( int i = 0; i < len; ++i )				\
			result[i] = stringcvt( strings[i] );		\
									\
		return new Value( result, len );			\
		}							\
									\
	if ( ! arg->IsNumeric() )					\
		{							\
		error->Report( "non-numeric argument to ", text );	\
		return new Value( zero );				\
		}							\
									\
	if ( arg->Type() == tag )					\
		return copy_value( arg );				\
									\
	bool is_copy;							\
	type* result = arg->coercer( is_copy, len );			\
									\
	return new Value( result, len );				\
	}

bool string_to_bool( const char* string )
	{
	double d;
	if ( text_to_double( string, d ) )
		return bool( int( d ) );
	else
		return false;
	}

DEFINE_AS_XXX_BUILT_IN(as_boolean_built_in, bool, TYPE_BOOL, string_to_bool,
	CoerceToBoolArray, "as_boolean", false)

DEFINE_AS_XXX_BUILT_IN(as_integer_built_in, int, TYPE_INT, atoi,
	CoerceToIntArray, "as_integer", 0)

DEFINE_AS_XXX_BUILT_IN(as_float_built_in, float, TYPE_FLOAT, atof,
	CoerceToFloatArray, "as_float", 0.0)

DEFINE_AS_XXX_BUILT_IN(as_double_built_in, double, TYPE_DOUBLE, atof,
	CoerceToDoubleArray, "as_double", 0.0)


Value* as_string_built_in( const Value* arg )
	{
	if ( arg->Type() == TYPE_STRING )
		return copy_value( arg );

	if ( ! arg->IsNumeric() )
		{
		error->Report( "non-numeric argument to as_string()" );
		return new Value( "" );
		}

	int len = arg->Length();
	charptr* result = new charptr[len];
	int i;
	char buf[256];

	switch ( arg->Type() )
		{
		case TYPE_BOOL:
			{
			bool* vals = arg->BoolPtr();
			for ( i = 0; i < len; ++i )
				result[i] = strdup( vals[i] ? "T" : "F" );
			}
			break;

#define COERCE_XXX_TO_STRING(tag,type,accessor,format)			\
	case tag:							\
		{							\
		type* vals = arg->accessor();				\
		for ( i = 0; i < len; ++i )				\
			{						\
			sprintf( buf, format, vals[i] );		\
			result[i] = strdup( buf );			\
			}						\
		}							\
		break;

		COERCE_XXX_TO_STRING(TYPE_INT,int,IntPtr,"%d")
		COERCE_XXX_TO_STRING(TYPE_FLOAT,float,FloatPtr,"%.6g")
		COERCE_XXX_TO_STRING(TYPE_DOUBLE,double,DoublePtr,"%.12g")

		default:
			fatal->Report( "bad type tag in as_string()" );
		}

	return new Value( result, len );
	}


Value* type_name_built_in( const Value* arg )
	{
	glish_type t = arg->Type();

	if ( t == TYPE_REF || t == TYPE_CONST )
		{
		Value* deref_val = type_name_built_in( arg->RefPtr() );
		char* deref_name = deref_val->StringVal();

		char buf[512];

		sprintf( buf, "%s %s", t == TYPE_REF ? "ref" : "const",
			deref_name );

		delete deref_name;
		Unref( deref_val );

		return new Value( buf );
		}

	return new Value( type_names[t] );
	}

Value* length_built_in( const Value* arg )
	{
	return new Value( int( arg->Length() ) );
	}

Value* field_names_built_in( const Value* arg )
	{
	if ( arg->Type() != TYPE_RECORD )
		{
		error->Report( "argument to field_names is not a record" );
		return new Value( false );
		}

	recordptr record_dict = arg->RecordPtr();
	IterCookie* c = record_dict->InitForIteration();

	charptr* names = new charptr[record_dict->Length()];
	const char* key;

	for ( int i = 0; record_dict->NextEntry( key, c ); ++i )
		names[i] = strdup( key );

	return new Value( names, i );
	}


char* paste( parameter_list* args )
	{
	PasteBuiltIn paste;

	// Create another parameter list with the separator at the 
	// beginning.
	parameter_list args2;
	Value sep( " " );
	ConstExpr sep_expr( &sep );
	Parameter sep_parm( 0, VAL_CONST, &sep_expr );

	args2.append( &sep_parm );

	loop_over_list( *args, i )
		args2.append( (*args)[i] );

	Value* args_value = paste.Call( &args2, EVAL_COPY );

	// ### could save on some string copies here by returning the
	// value instead, and using StringPtr() instead of StringVal()
	// to get its string value.
	char* result = args_value->StringVal();
	Unref( args_value );

	return result;
	}


char* paste( const_args_list* args )
	{
	PasteBuiltIn paste;

	// Create another args list with the separator at the beginning.
	const_args_list args2;
	Value sep( " " );
	args2.append( &sep );

	loop_over_list( *args, i )
		args2.append( (*args)[i] );

	Value* args_value = paste.DoCall( &args2 );
	char* result = args_value->StringVal();
	Unref( args_value );

	return result;
	}


Value* split( char* source, char* split_chars )
	{
	// First see how many pieces the split will result in.
	int num_pieces = 0;
	char* source_copy = strdup( source );
	charptr next_string = strtok( source_copy, split_chars );
	while ( next_string )
		{
		++num_pieces;
		next_string = strtok( 0, split_chars );
		}
	delete source_copy;

	charptr* strings = new charptr[num_pieces];
	charptr* sptr = strings;
	next_string = strtok( source, split_chars );
	while ( next_string )
		{
		*(sptr++) = strdup( next_string );
		next_string = strtok( 0, split_chars );
		}

	return new Value( strings, num_pieces );
	}


static void add_one_arg_built_in( Sequencer* s, value_func_1_value_arg func,
					const char* name, bool do_deref = true )
	{
	BuiltIn* b = new OneValueArgBuiltIn( func, name );
	b->SetDeref( do_deref );
	s->AddBuiltIn( b );
	}

void create_built_ins( Sequencer* s )
	{
	add_one_arg_built_in( s, as_boolean_built_in, "as_boolean" );
	add_one_arg_built_in( s, as_integer_built_in, "as_integer" );
	add_one_arg_built_in( s, as_float_built_in, "as_float" );
	add_one_arg_built_in( s, as_double_built_in, "as_double" );
	add_one_arg_built_in( s, as_string_built_in, "as_string" );

	add_one_arg_built_in( s, type_name_built_in, "type_name", false );
	add_one_arg_built_in( s, length_built_in, "length" );
	add_one_arg_built_in( s, field_names_built_in, "field_names" );

	s->AddBuiltIn( new DoubleVectorBuiltIn( sqrt, "sqrt" ) );
	s->AddBuiltIn( new DoubleVectorBuiltIn( exp, "exp" ) );
	s->AddBuiltIn( new DoubleVectorBuiltIn( log, "log" ) );
	s->AddBuiltIn( new DoubleVectorBuiltIn( sin, "sin" ) );
	s->AddBuiltIn( new DoubleVectorBuiltIn( cos, "cos" ) );
	s->AddBuiltIn( new DoubleVectorBuiltIn( tan, "tan" ) );

	s->AddBuiltIn( new SumBuiltIn );
	s->AddBuiltIn( new RangeBuiltIn );
	s->AddBuiltIn( new SeqBuiltIn );
	s->AddBuiltIn( new NumArgsBuiltIn );
	s->AddBuiltIn( new NthArgBuiltIn );

	s->AddBuiltIn( new PasteBuiltIn );
	s->AddBuiltIn( new SplitBuiltIn );

	s->AddBuiltIn( new ReadValueBuiltIn );
	s->AddBuiltIn( new WriteValueBuiltIn );

	s->AddBuiltIn( new WheneverStmtsBuiltIn );

	s->AddBuiltIn( new ActiveAgentsBuiltIn );

	s->AddBuiltIn( new CreateAgentBuiltIn( s ) );
	s->AddBuiltIn( new CreateTaskBuiltIn( s ) );

	s->AddBuiltIn( new LastWheneverExecutedBuiltIn( s ) );
	s->AddBuiltIn( new CurrentWheneverBuiltIn( s ) );

	sds_init();
	}
