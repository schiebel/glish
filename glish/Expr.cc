// $Header$

#include <stream.h>
#include <string.h>

#if ! defined (__GNUC__) & ! defined (SABER)
#include <stdlib.h>
#endif

#include "Glish/Value.h"

#include "Expr.h"
#include "Sequencer.h"
#include "Reporter.h"
#include "Agent.h"
#include "Func.h"


void Expr::SideEffectsEval()
	{
	const Value* v = ReadOnlyEval();

	if ( v )
		{
		glish_type t = v->Type();
		ReadOnlyDone( v );

		if ( t != TYPE_FUNC )
			warn->Report( "expression value ignored:", this );
		}
	}

Value* Expr::RefEval( value_type val_type )
	{
	Value* value = CopyEval();
	Value* result;

	if ( val_type == VAL_VAL )
		{
		result = value;
		Ref( value );
		}

	else
		result = new Value( value, val_type );

	if ( val_type == VAL_REF && value->IsConst() )
		warn->Report( "\"const\" reference converted to \"ref\" in",
				this );

	Unref( value );	// result is now the only pointer to value

	return result;
	}

void Expr::Assign( Value* /* new_value */ )
	{
	error->Report( this, "is not a valid target for assignment" );
	}

bool Expr::Invisible() const
	{
	return false;
	}

Value* Expr::CopyOrRefValue( const Value* value, eval_type etype )
	{
	if ( etype == EVAL_COPY )
		return copy_value( value );
	
	else if ( etype == EVAL_READ_ONLY )
		{
		Value* result = (Value*) value;
		Ref( result );
		return result;
		}

	else
		{
		// EVAL_SIDE_EFFECTS should've been caught earlier; making it
		// this far indicates that the function erroneously overrode
		// SideEffectsEval().
		fatal->Report( "bad eval_type in Expr::CopyOrRefValue" );
		return 0;
		}
	}


VarExpr::VarExpr( char* var_id, scope_type var_scope, int var_offset,
			Sequencer* var_sequencer ) : Expr(var_id)
	{
	id = var_id;
	scope = var_scope;
	frame_offset = var_offset;
	sequencer = var_sequencer;
	}

VarExpr::~VarExpr()
	{
	delete id;
	}

Value* VarExpr::Eval( eval_type etype )
	{
	Value* value = sequencer->FrameElement( scope, frame_offset );

	if ( ! value )
		{
		warn->Report( "uninitialized variable", this, "used" );
		value = new Value( false );
		sequencer->SetFrameElement( scope, frame_offset, value );
		}

	value = value->Deref();

	return CopyOrRefValue( value, etype );
	}

Value* VarExpr::RefEval( value_type val_type )
	{
	Value* var = sequencer->FrameElement( scope, frame_offset );

	if ( ! var )
		{
		// Presumably we're going to be assigning to a subelement.
		var = new Value( false );
		sequencer->SetFrameElement( scope, frame_offset, var );
		}

	if ( val_type == VAL_REF && var->IsConst() )
		warn->Report( this, " is a \"const\" reference" );

	return new Value( var, val_type );
	}

void VarExpr::Assign( Value* new_value )
	{
	sequencer->SetFrameElement( scope, frame_offset, new_value );
	}


ConstExpr::ConstExpr( const Value* value ) : Expr("constant")
	{
	const_value = value;
	}

Value* ConstExpr::Eval( eval_type etype )
	{
	return CopyOrRefValue( const_value, etype );
	}

void ConstExpr::DescribeSelf( ostream& s ) const
	{
	const_value->DescribeSelf( s );
	}


UnaryExpr::UnaryExpr( Expr* operand, const char* desc ) : Expr(desc)
	{
	op = operand;
	}

void UnaryExpr::Describe( ostream& s ) const
	{
	DescribeSelf( s );
	op->Describe( s );
	}


BinaryExpr::BinaryExpr( Expr* op1, Expr* op2, const char* desc )
    : Expr(desc)
	{
	left = op1;
	right = op2;
	}


void BinaryExpr::Describe( ostream& s ) const
	{
	s << "(";
	left->Describe( s );
	s << " ";
	DescribeSelf( s );
	s << " ";
	right->Describe( s );
	s << ")";
	}


NegExpr::NegExpr( Expr* operand ) : UnaryExpr( operand, "-" )
	{
	}

Value* NegExpr::Eval( eval_type /* etype */ )
	{
	Value* result = op->CopyEval();
	result->Negate();
	return result;
	}


NotExpr::NotExpr( Expr* operand ) : UnaryExpr( operand, "!" )
	{
	}

Value* NotExpr::Eval( eval_type /* etype */ )
	{
	Value* result = op->CopyEval();
	result->Not();
	return result;
	}


AssignExpr::AssignExpr( Expr* op1, Expr* op2 ) : BinaryExpr(op1, op2, ":=")
	{
	}

Value* AssignExpr::Eval( eval_type etype )
	{
	left->Assign( right->CopyEval() );

	if ( etype == EVAL_COPY )
		return left->CopyEval();

	else if ( etype == EVAL_READ_ONLY )
		return (Value*) left->ReadOnlyEval();

	else
		return 0;
	}

void AssignExpr::SideEffectsEval()
	{
	if ( Eval( EVAL_SIDE_EFFECTS ) )
		fatal->Report(
		"value unexpected returnedly in AssignExpr::SideEffectsEval" );
	}

bool AssignExpr::Invisible() const
	{
	return true;
	}


OrExpr::OrExpr( Expr* op1, Expr* op2 ) : BinaryExpr(op1, op2, "||")
	{
	}

Value* OrExpr::Eval( eval_type etype )
	{
	const Value* left_value = left->ReadOnlyEval();

	if ( left_value->BoolVal() )
		{
		if ( etype == EVAL_COPY )
			{
			Value* result = copy_value( left_value );
			left->ReadOnlyDone( left_value );
			return result;
			}

		else
			return (Value*) left_value;
		}

	else
		return right->Eval( etype );
	}


AndExpr::AndExpr( Expr* op1, Expr* op2 ) : BinaryExpr(op1, op2, "&&")
	{
	}

Value* AndExpr::Eval( eval_type etype )
	{
	const Value* left_value = left->ReadOnlyEval();
	bool left_is_true = left_value->BoolVal();
	left->ReadOnlyDone( left_value );

	if ( etype == EVAL_COPY )
		{
		if ( left_is_true )
			return right->CopyEval();
		else
			return new Value( false );
		}

	else
		{
		if ( left_is_true )
			return (Value*) right->ReadOnlyEval();
		else
			return new Value( false );
		}
	}


ConstructExpr::ConstructExpr( parameter_list* arg_args ) : Expr("[construct]")
	{
	is_array_constructor = true;

	args = arg_args;

	if ( args )
		{
		loop_over_list( *args, i )
			{
			if ( (*args)[i]->Name() )
				{
				if ( i > 0 && is_array_constructor )
					{
					error->Report(
					"mixed array/record constructor: ",
							this );
					break;
					}

				is_array_constructor = false;
				}

			else if ( ! is_array_constructor )
				{
				error->Report(
					"mixed array/record constructor: ",
						this );
				is_array_constructor = true;
				break;
				}
			}
		}
	}

Value* ConstructExpr::Eval( eval_type /* etype */ )
	{
	if ( ! args )
		return create_record();

	else if ( args->length() == 0 )
		{ // Create an empty array.
		bool empty;
		return new Value( &empty, 0, COPY_ARRAY );
		}

	else if ( is_array_constructor )
		return BuildArray();

	else
		return BuildRecord();
	}

void ConstructExpr::Describe( ostream& s ) const
	{
	s << "[";

	if ( args )
		describe_parameter_list( args, s );
	else
		s << "=";

	s << "]";
	}

Value* ConstructExpr::BuildArray()
	{
	Value* result;

	typedef const Value* const_value_ptr;

	int num_values = args->length();
	const_value_ptr* values = new const_value_ptr[num_values];

	int total_length = 0;
	for ( int i = 0; i < num_values; ++i )
		{
		values[i] = (*args)[i]->Arg()->ReadOnlyEval();
		total_length += values[i]->Length();
		}

	glish_type max_type;
	if ( TypeCheck( values, num_values, max_type ) )
		result = ConstructArray( values, num_values, total_length,
					max_type );
	else
		result = new Value( false );

	for ( i = 0; i < num_values; ++i )
		(*args)[i]->Arg()->ReadOnlyDone( values[i] );

	delete values;

	return result;
	}

bool ConstructExpr::TypeCheck( const Value* values[], int num_values,
					glish_type& max_type )
	{
	if ( num_values == 0 )
		{
		max_type = TYPE_BOOL;	// Compatible with the constant F
		return true;
		}

	for ( int i = 0; i < num_values; ++i )
		{
		const Value* v = values[i];

		if ( v->Length() > 0 && v->IsNumeric() )
			return MaxNumeric( values, num_values, max_type );
		}

	bool result = AllEquivalent( values, num_values, max_type );

	if ( max_type == TYPE_RECORD )
		{
		error->Report( "arrays of records are not supported" );
		return false;
		}

	return result;
	}

bool ConstructExpr::MaxNumeric( const Value* values[], int num_values,
					glish_type& max_type )
	{
	max_type = values[0]->Type();

	for ( int i = 1; i < num_values; ++i )
		{
		if ( ! values[i]->IsNumeric() )
			{
			error->Report( "non-numeric type in array constructor",
					this );
			return false;
			}

		glish_type t = values[i]->Type();

		if ( max_type == TYPE_DOUBLE || t == TYPE_DOUBLE )
			max_type = TYPE_DOUBLE;
		else if ( max_type == TYPE_FLOAT || t == TYPE_FLOAT )
			max_type = TYPE_FLOAT;
		else if ( max_type == TYPE_INT || t == TYPE_INT )
			max_type = TYPE_INT;
		else
			max_type = TYPE_BOOL;
		}

	return true;
	}

bool ConstructExpr::AllEquivalent( const Value* values[], int num_values,
					glish_type& max_type )
	{
	max_type = TYPE_BOOL;

	// First pick a candidate type.
	for ( int i = 0; i < num_values; ++i )
		// Ignore empty arrays, as they can be any type.
		if ( values[i]->Length() > 0 )
			{
			max_type = values[i]->Type();
			break;
			}

	// Now check whether all non-empty arrays conform to that type.
	for ( i = 0; i < num_values; ++i )
		{
		const Value* v = values[i];

		if ( v->Length() > 0 && v->Type() != max_type )
			{
			error->Report(
				"incompatible types in array constructor",
					this );
			return false;
			}
		}

	return true;
	}

Value* ConstructExpr::ConstructArray( const Value* values[],
					int num_values, int total_length,
					glish_type max_type )
	{
	Value* result;

	bool is_copy;
	int i, len;

	switch ( max_type )
		{

#define BUILD_WITH_COERCE_TYPE(tag, type, coercer)			\
	case tag:							\
		{							\
		type* array = new type[total_length];			\
		type* array_ptr = array;				\
									\
		for ( i = 0; i < num_values; ++i )			\
			{						\
			len = values[i]->Length();			\
			if ( len > 0 )					\
				(void) values[i]->coercer( is_copy,	\
						len, array_ptr );	\
			array_ptr += len;				\
			}						\
									\
		result = new Value( array, total_length );		\
									\
		break;							\
		}

#define BUILD_WITH_NON_COERCE_TYPE(tag, type, accessor, storage)	\
	case tag:							\
		{							\
		type* array = new type[total_length];			\
		type* array_ptr = array;				\
									\
		for ( i = 0; i < num_values; ++i )			\
			{						\
			len = values[i]->Length();			\
			if ( len > 0 )					\
				copy_array( values[i]->accessor,	\
						array_ptr, len, type );	\
			array_ptr += len;				\
			}						\
									\
		result = new Value( array, total_length, storage );	\
									\
		if ( storage == COPY_ARRAY )				\
			delete array;					\
									\
		break;							\
		}

BUILD_WITH_COERCE_TYPE(TYPE_BOOL, bool, CoerceToBoolArray)
BUILD_WITH_COERCE_TYPE(TYPE_INT, int, CoerceToIntArray)
BUILD_WITH_COERCE_TYPE(TYPE_FLOAT, float, CoerceToFloatArray)
BUILD_WITH_COERCE_TYPE(TYPE_DOUBLE, double, CoerceToDoubleArray)

// For strings, copy the result so that each string in the array gets
// copied, too.
BUILD_WITH_NON_COERCE_TYPE(TYPE_STRING, charptr, StringPtr(), COPY_ARRAY)
BUILD_WITH_NON_COERCE_TYPE(TYPE_FUNC, funcptr, FuncPtr(), TAKE_OVER_ARRAY)

		case TYPE_AGENT:
			error->Report( "can't construct array of agents" );
			result = new Value( false );
			break;

		case TYPE_OPAQUE:
			error->Report( "can't construct array opaque values" );
			result = new Value( false );
			break;

		default:
			fatal->Report(
		    "bad type tag in ConstructExpr::ConstructArray()" );
		}

	return result;
	}

Value* ConstructExpr::BuildRecord()
	{
	recordptr rec = create_record_dict();

	loop_over_list( *args, i )
		{
		Parameter* p = (*args)[i];
		rec->Insert( strdup( p->Name() ), p->Arg()->CopyEval() );
		}

	return new Value( rec );
	}


ArrayRefExpr::ArrayRefExpr( Expr* op1, Expr* op2 ) : BinaryExpr(op1, op2, "[]")
	{
	}

Value* ArrayRefExpr::Eval( eval_type etype )
	{
	const Value* array = left->ReadOnlyEval();
	const Value* index_val = right->ReadOnlyEval();

	Value* result;

	if ( index_val->Type() == TYPE_STRING && index_val->Length() == 1 )
		{ // Return single element belonging to record.
		const Value* const_result =
			array->ExistingRecordElement( index_val );

		const_result = const_result->Deref();

		result = CopyOrRefValue( const_result, etype );
		}

	else
		{
		result = (*array)[index_val];

		if ( result->IsRef() )
			{
			Value* orig_result = result;

			result = copy_value( result->Deref() );
			Unref( orig_result );
			}
		}

	left->ReadOnlyDone( array );
	right->ReadOnlyDone( index_val );

	return result;
	}

Value* ArrayRefExpr::RefEval( value_type val_type )
	{
	Value* array_ref = left->RefEval( val_type );
	Value* array = array_ref->Deref();

	Value* result = 0;

	if ( array && array->Type() == TYPE_RECORD )
		{
		const Value* index_val = right->ReadOnlyEval();

		if ( index_val->Length() == 1 )
			{
			if ( index_val->Type() == TYPE_STRING )
				result =
					array->GetOrCreateRecordElement(
								index_val );
			else
				result = array->NthField( index_val->IntVal() );
			}

		right->ReadOnlyDone( index_val );
		}

	if ( ! result )
		result = Expr::RefEval( val_type );

	if ( val_type == VAL_REF && result->IsConst() )
		warn->Report( this, " is a \"const\" reference" );

	result = new Value( result, val_type );

	Unref( array_ref );

	return result;
	}

void ArrayRefExpr::Assign( Value* new_value )
	{
	Value* lhs_value_ref = left->RefEval( VAL_REF );
	Value* lhs_value = lhs_value_ref->Deref();

	const Value* index = right->ReadOnlyEval();

	if ( index->Type() == TYPE_STRING && lhs_value->Type() == TYPE_BOOL )
		// ### assume uninitialized variable
		lhs_value->Polymorph( TYPE_RECORD );

	lhs_value->AssignElements( index, new_value );

	right->ReadOnlyDone( index );

	Unref( lhs_value_ref );
	}

void ArrayRefExpr::Describe( ostream& s ) const
	{
	left->Describe( s );
	s << "[";
	right->Describe( s );
	s << "]";
	}


RecordRefExpr::RecordRefExpr( Expr* op, char* record_field )
    : UnaryExpr(op, ".")
	{
	field = record_field;
	}

Value* RecordRefExpr::Eval( eval_type etype )
	{
	const Value* record = op->ReadOnlyEval();
	const Value* const_result = record->ExistingRecordElement( field );

	const_result = const_result->Deref();

	Value* result;

	result = CopyOrRefValue( const_result, etype );

	op->ReadOnlyDone( record );

	return result;
	}

Value* RecordRefExpr::RefEval( value_type val_type )
	{
	Value* value_ref = op->RefEval( val_type );
	Value* value = value_ref->Deref();

	value = value->GetOrCreateRecordElement( field );

	if ( val_type == VAL_REF && value->IsConst() )
		warn->Report( "record field", this,
				" is a \"const\" reference" );

	value = new Value( value, val_type );

	Unref( value_ref );

	return value;
	}

void RecordRefExpr::Assign( Value* new_value )
	{
	Value* lhs_value_ref = op->RefEval( VAL_REF );
	Value* lhs_value = lhs_value_ref->Deref();

	if ( lhs_value->Type() == TYPE_BOOL && lhs_value->Length() == 1 )
		// ### assume uninitialized variable
		lhs_value->Polymorph( TYPE_RECORD );

	if ( lhs_value->Type() == TYPE_RECORD )
		lhs_value->AssignRecordElement( field, new_value );
	else
		error->Report( op, "is not a record" );

	Unref( new_value );
	Unref( lhs_value_ref );
	}

void RecordRefExpr::Describe( ostream& s ) const
	{
	op->Describe( s );
	s << "." << field;
	}


RefExpr::RefExpr( Expr* op, value_type arg_type ) : UnaryExpr(op, "ref")
	{
	type = arg_type;
	}

Value* RefExpr::Eval( eval_type /* etype */ )
	{
	return op->RefEval( type );
	}

void RefExpr::Assign( Value* new_value )
	{
	if ( type == VAL_VAL )
		{
		Value* value = op->RefEval( VAL_REF );
		value->Deref()->TakeValue( new_value ); 
		Unref( value );
		}

	else
		Expr::Assign( new_value );
	}

void RefExpr::Describe( ostream& s ) const
	{
	if ( type == VAL_CONST )
		s << "const ";
	else if ( type == VAL_REF )
		s << "ref ";
	else
		s << "val ";

	op->Describe( s );
	}


RangeExpr::RangeExpr( Expr* op1, Expr* op2 ) : BinaryExpr(op1, op2, ":")
	{
	}

Value* RangeExpr::Eval( eval_type /* etype */ )
	{
	const Value* left_val = left->ReadOnlyEval();
	const Value* right_val = right->ReadOnlyEval();

	Value* result;

	if ( ! left_val->IsNumeric() || ! right_val->IsNumeric() )
		{
		error->Report( "non-numeric value in", this );
		result = new Value( false );
		}

	else if ( left_val->Length() > 1 || right_val->Length() > 1 )
		{
		error->Report( "non-scalar value in", this );
		result = new Value( false );
		}

	else
		{
		int start = left_val->IntVal();
		int stop = right_val->IntVal();

		int direction = (start > stop) ? -1 : 1;
		int num_values = (stop - start) * direction + 1;
		int* range = new int[num_values];

		int i;
		int index = 0;

		if ( direction < 0 )
			for ( i = start; i >= stop; --i )
				range[index++] = i;

		else
			for ( i = start; i <= stop; ++i )
				range[index++] = i;

		result = new Value( range, num_values );
		}

	left->ReadOnlyDone( left_val );
	right->ReadOnlyDone( right_val );

	return result;
	}


CallExpr::CallExpr( Expr* func, parameter_list* args_args )
    : UnaryExpr(func, "func()")
	{
	args = args_args;
	}

Value* CallExpr::Eval( eval_type etype )
	{
	const Value* func = op->ReadOnlyEval();
	Func* func_val = func->FuncVal();

	Value* result = 0;

	if ( ! func_val || ! (result = func_val->Call( args, etype )) )
		{
		if ( etype != EVAL_SIDE_EFFECTS )
			result = new Value( false );
		}

	op->ReadOnlyDone( func );

	return result;
	}

void CallExpr::SideEffectsEval()
	{
	Value* result = Eval( EVAL_SIDE_EFFECTS );

	if ( result )
		{
		warn->Report( "function return value ignored" );
		Unref( result );
		}
	}

void CallExpr::Describe( ostream& s ) const
	{
	op->Describe( s );
	s << "(";
	loop_over_list( *args, i )
		{
		if ( i > 0 )
			s << ", ";

		(*args)[i]->Describe( s );
		}
	s << ")";
	}


SendEventExpr::SendEventExpr( EventDesignator* arg_sender,
				parameter_list* arg_args,
				bool arg_is_request_reply ) : Expr("->")
	{
	sender = arg_sender;
	args = arg_args;
	is_request_reply = arg_is_request_reply;
	}

Value* SendEventExpr::Eval( eval_type etype )
	{
	Value* result = sender->SendEvent( args, is_request_reply );

	if ( etype == EVAL_SIDE_EFFECTS )
		{
		Unref( result );
		return 0;
		}

	else
		return result;
	}

void SendEventExpr::SideEffectsEval()
	{
	if ( Eval( EVAL_SIDE_EFFECTS ) )
		fatal->Report(
	"value unexpectedly returned in SendEventExpr::SideEffectsEval" );
	}

void SendEventExpr::Describe( ostream& s ) const
	{
	if ( is_request_reply )
		s << "request ";

	sender->Describe( s );
	s << "->(";
	describe_parameter_list( args, s );
	s << ")";
	}


LastEventExpr::LastEventExpr( Sequencer* arg_sequencer,
				last_event_type arg_type ) : Expr("$last_event")
	{
	sequencer = arg_sequencer;
	type = arg_type;
	}

Value* LastEventExpr::Eval( eval_type etype )
	{
	Notification* n = sequencer->LastNotification();

	if ( ! n )
		{
		warn->Report( this, ": no events have been received" );
		return new Value( false );
		}

	Value* result;

	switch ( type )
		{
		case EVENT_AGENT:
			result = n->notifier->AgentRecord();

			if ( etype == EVAL_COPY )
				result = copy_value( result );
			else
				Ref( result );
			break;

		case EVENT_NAME:
			result = new Value( n->field );
			break;

		case EVENT_VALUE:
			result = n->value;

			if ( etype == EVAL_COPY )
				result = copy_value( result );
			else
				Ref( result );
			break;

		default:
			fatal->Report( "bad type in LastEventExpr::Eval" );
		}

	return result;
	}

Value* LastEventExpr::RefEval( value_type val_type )
	{
	Notification* n = sequencer->LastNotification();

	if ( ! n )
		{
		warn->Report( this, ": no events have been received" );
		return new Value( false );
		}

	Value* result;

	if ( type == EVENT_AGENT )
		result = new Value( n->notifier->AgentRecord(), val_type );

	else if ( type == EVENT_NAME )
		result = new Value( n->field );

	else if ( type == EVENT_VALUE )
		{
		if ( val_type == VAL_REF && n->value->IsConst() )
			warn->Report( this, " is a \"const\" reference" );

		result = new Value( n->value, val_type );
		}

	else
		fatal->Report( "bad type in LastEventExpr::RefEval" );

	return result;
	}

void LastEventExpr::Describe( ostream& s ) const
	{
	if ( type == EVENT_AGENT )
		s << "$agent";

	else if ( type == EVENT_NAME )
		s << "$name";

	else if ( type == EVENT_VALUE )
		s << "$value";

	else
		s << "$weird";
	}


void describe_expr_list( const expr_list* list, ostream& s )
	{
	if ( list )
		loop_over_list( *list, i )
			{
			if ( i > 0 )
				s << ", ";
			(*list)[i]->Describe( s );
			}
	}
