// $Header$

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <stream.h>
#include <string.h>
#include <stdlib.h>

#include "Reporter.h"
#include "Sequencer.h"
#include "Expr.h"
#include "Agent.h"
#include "Func.h"
#include "config.h"

int ParseNode::canDelete() const
	{
	return 1;
	}

Expr::~Expr() { }

void Expr::SideEffectsEval()
	{
	const IValue* v = ReadOnlyEval();

	if ( v )
		{
		glish_type t = v->Type();
		ReadOnlyDone( v );

		if ( t != TYPE_FUNC )
			warn->Report( "expression value ignored:", this );
		}
	}

IValue* Expr::RefEval( value_type val_type )
	{
	IValue* value = CopyEval();
	IValue* result;

	if ( val_type == VAL_VAL || val_type == VAL_CONST )
		{
		result = value;
		Ref( value );
		}

	else
		result = new IValue( value, val_type );

	if ( val_type == VAL_REF && value->IsConst() )
		warn->Report( "\"const\" reference converted to \"ref\" in",
				this );

	Unref( value );	// result is now the only pointer to value

	return result;
	}

void Expr::Assign( IValue* /* new_value */ )
	{
	error->Report( this, "is not a valid target for assignment" );
	}

int Expr::Invisible() const
	{
	return 0;
	}

Expr *Expr::BuildFrameInfo( scope_modifier m )
	{
	expr_list dl;
	Expr *ret = DoBuildFrameInfo( m, dl );

	loop_over_list( dl, i )
		Unref( dl[i] );

	return ret;
	}


Expr *Expr::DoBuildFrameInfo( scope_modifier, expr_list & )
	{
	return this;
	}

IValue* Expr::CopyOrRefValue( const IValue* value, eval_type etype )
	{
	if ( etype == EVAL_COPY )
		return copy_value( value );

	else if ( etype == EVAL_READ_ONLY )
		{
		IValue* result = (IValue*) value;
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

VarExpr::~VarExpr()
	{
	//
	// if scope_offset is less than 0, then this VarExpr is
	// simply a reference to a global variable.
	//
	if ( scope_offset >= 0 )
		delete id;
	}

VarExpr::VarExpr( char* var_id, scope_type var_scope, int var_scope_offset,
			int var_frame_offset,
			Sequencer* var_sequencer ) : Expr(var_id), access(PARSE_ACCESS)
	{
	id = var_id;
	scope = var_scope;
	frame_offset = var_frame_offset;
	scope_offset = var_scope_offset;
	sequencer = var_sequencer;
	}

VarExpr::VarExpr( char* var_id, Sequencer* var_sequencer ) :
			Expr(var_id), access(PARSE_ACCESS)
	{
	id = strdup(var_id);
	sequencer = var_sequencer;
	scope = ANY_SCOPE;
	scope_offset = 0;
	}

void VarExpr::set( scope_type var_scope, int var_scope_offset,
			int var_frame_offset )
	{
	scope = var_scope;
	frame_offset = var_frame_offset;
	scope_offset = var_scope_offset;
	}

IValue* VarExpr::Eval( eval_type etype )
	{
	access = USE_ACCESS;
	IValue* value = sequencer->FrameElement( scope, scope_offset,
						frame_offset );
	if ( ! value )
		{
		warn->Report( "uninitialized ",
				scope == GLOBAL_SCOPE ? "global" : "local",
				" variable", this, "used" );
		value = error_ivalue();
		sequencer->SetFrameElement( scope, scope_offset, 
						frame_offset, value );
		}

	value = (IValue*) value->Deref();

	return CopyOrRefValue( value, etype );
	}

IValue* VarExpr::RefEval( value_type val_type )
	{
	access = USE_ACCESS;
	IValue* var = sequencer->FrameElement( scope, scope_offset,
						frame_offset );
	if ( ! var )
		{
		// Presumably we're going to be assigning to a subelement.
		var = new IValue( glish_false );
		sequencer->SetFrameElement( scope, scope_offset,
						frame_offset, var );
		}

	if ( val_type == VAL_VAL || val_type == VAL_CONST )
		return copy_value( var );

	return new IValue( var, val_type );
	}

void VarExpr::Assign( IValue* new_value )
	{
	access = USE_ACCESS;
	sequencer->SetFrameElement( scope, scope_offset, frame_offset,
					new_value );
	}

Expr *VarExpr::DoBuildFrameInfo( scope_modifier m, expr_list &dl )
	{

	if ( scope != ANY_SCOPE )
		return this;

	Expr *ret = 0;

	switch ( m )
		{
		case SCOPE_LHS:
			ret = sequencer->LookupVar( strdup(id), LOCAL_SCOPE,
							this );
			break;
		case SCOPE_UNKNOWN:
		case SCOPE_RHS:
			ret = sequencer->LookupVar( strdup(id), ANY_SCOPE,
							this );
			break;
		default:
			fatal->Report("bad scope modifier tag in VarExpr::DoBuildFrameInfo()");
		}

	if ( ret && ret != this )
		{
		if ( ! dl.is_member(this) )
			dl.append( this );

		Ref(ret);

		if ( RefCount() > 1 )
			Unref(this);
		}
	else
		dl.remove( this );

	return ret;
	}


ScriptVarExpr::~ScriptVarExpr() { }

Expr *ScriptVarExpr::DoBuildFrameInfo( scope_modifier m, expr_list &dl )
	{
	Expr *ret = 0;
	if ( sequencer->ScriptCreated() )
		return VarExpr::DoBuildFrameInfo( m, dl );
	else
		{
		sequencer->ScriptCreated( 1 );
		ret = VarExpr::DoBuildFrameInfo( m, dl );
		sequencer->ScriptCreated( 0 );
		}

	if ( ! ret )
		return ret;

	if ( ((VarExpr*)ret)->Scope() == GLOBAL_SCOPE )
		{
		const IValue *v = ret->ReadOnlyEval();
		if ( v->Type() == TYPE_BOOL )
			sequencer->InitScriptClient();
		ret->ReadOnlyDone( v );
		}

	return ret;
	}

VarExpr *CreateVarExpr( char *id, Sequencer *seq )
	{
	if ( seq->DoingInit() && ! seq->ScriptCreated() &&
			! strcmp( id, "script" ) )
		return new ScriptVarExpr( id, seq );
	return new VarExpr( id, seq );
	}

VarExpr *CreateVarExpr( char *id, scope_type sc, int soff, int foff, 
			Sequencer *seq )
	{
	if ( seq->DoingInit() && ! seq->ScriptCreated() &&
			! strcmp( id, "script" ) )
		return new ScriptVarExpr( id, sc, soff, foff, seq );
	return new VarExpr( id, sc, soff, foff, seq );
	}


ValExpr::~ValExpr()
	{
	Unref(val);
	}

IValue* ValExpr::Eval( eval_type etype )
	{
	return CopyOrRefValue( val, etype );
	}

IValue* ValExpr::RefEval( value_type val_type )
	{
	if ( val_type == VAL_REF && val->IsConst() )
		warn->Report( this, " is a \"const\" reference" );

	return new IValue( val, val_type );
	}


ConstExpr::~ConstExpr()
	{
	Unref((GlishObject*)const_value);
	}

ConstExpr::ConstExpr( const IValue* value ) : Expr("constant")
	{
	const_value = value;
	}

IValue* ConstExpr::Eval( eval_type etype )
	{
	return CopyOrRefValue( const_value, etype );
	}

void ConstExpr::DescribeSelf( ostream& s ) const
	{
	const_value->DescribeSelf( s );
	}

UnaryExpr::~UnaryExpr()
	{
	NodeUnref( op );
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

Expr *UnaryExpr::DoBuildFrameInfo( scope_modifier m, expr_list &dl )
	{
	op = op->DoBuildFrameInfo( m, dl );
	return this;
	}

BinaryExpr::~BinaryExpr()
	{
	NodeUnref( left );
	NodeUnref( right );
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

Expr *BinaryExpr::DoBuildFrameInfo( scope_modifier m, expr_list &dl )
	{
	left = left->DoBuildFrameInfo( m, dl );
	right = right->DoBuildFrameInfo( m, dl );
	return this;
	}


NegExpr::NegExpr( Expr* operand ) : UnaryExpr( operand, "-" )
	{
	}

IValue* NegExpr::Eval( eval_type /* etype */ )
	{
	IValue* result = op->CopyEval();
	result->Negate();
	return result;
	}

NotExpr::NotExpr( Expr* operand ) : UnaryExpr( operand, "!" )
	{
	}

IValue* NotExpr::Eval( eval_type /* etype */ )
	{
	IValue* result = op->CopyEval();
	result->Not();
	return result;
	}


AssignExpr::AssignExpr( Expr* op1, Expr* op2 ) : BinaryExpr(op1, op2, ":=")
	{
	}

IValue* AssignExpr::Eval( eval_type etype )
	{
	left->Assign( right->CopyEval() );

	if ( etype == EVAL_COPY )
		return left->CopyEval();

	else if ( etype == EVAL_READ_ONLY )
		return (IValue*) left->ReadOnlyEval();

	else
		return 0;
	}

void AssignExpr::SideEffectsEval()
	{
	if ( Eval( EVAL_SIDE_EFFECTS ) )
		fatal->Report(
		"value unexpected returnedly in AssignExpr::SideEffectsEval" );
	}

int AssignExpr::Invisible() const
	{
	return 1;
	}

Expr *AssignExpr::DoBuildFrameInfo( scope_modifier, expr_list &dl )
	{
	right = right->DoBuildFrameInfo( SCOPE_RHS, dl );
	left = left->DoBuildFrameInfo( SCOPE_LHS, dl );
	return this;
	}


OrExpr::OrExpr( Expr* op1, Expr* op2 ) : BinaryExpr(op1, op2, "||")
	{
	}

IValue* OrExpr::Eval( eval_type etype )
	{
	const IValue* left_value = left->ReadOnlyEval();

	if ( left_value->BoolVal() )
		{
		if ( etype == EVAL_COPY )
			{
			IValue* result = copy_value( left_value );
			left->ReadOnlyDone( left_value );
			return result;
			}

		else
			return (IValue*) left_value;
		}

	else
		{
		left->ReadOnlyDone( left_value );
		return right->Eval( etype );
		}
	}


AndExpr::AndExpr( Expr* op1, Expr* op2 ) : BinaryExpr(op1, op2, "&&")
	{
	}

IValue* AndExpr::Eval( eval_type etype )
	{
	const IValue* left_value = left->ReadOnlyEval();
	int left_is_true = left_value->BoolVal();
	left->ReadOnlyDone( left_value );

	if ( etype == EVAL_COPY )
		{
		if ( left_is_true )
			return right->CopyEval();
		else
			return new IValue( glish_false );
		}

	else
		{
		if ( left_is_true )
			return (IValue*) right->ReadOnlyEval();
		else
			return new IValue( glish_false );
		}
	}

ConstructExpr::~ConstructExpr()
	{
	if ( args )
		{
		loop_over_list( *args, i )
			NodeUnref( (*args)[i] );
		delete args;
		}
	}

ConstructExpr::ConstructExpr( parameter_list* arg_args ) : Expr("[construct]")
	{
	is_array_constructor = 1;

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

				is_array_constructor = 0;
				}

			else if ( ! is_array_constructor )
				{
				error->Report(
					"mixed array/record constructor: ",
						this );
				is_array_constructor = 1;
				break;
				}
			}
		}
	}

IValue* ConstructExpr::Eval( eval_type /* etype */ )
	{
	if ( ! args )
		return create_irecord();

	else if ( args->length() == 0 )
		return empty_ivalue();

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

IValue* ConstructExpr::BuildArray()
	{
	IValue* result;

	typedef const IValue* const_value_ptr;

	int num_values = 0;

	loop_over_list( *args, i )
		{
		Parameter* arg = (*args)[i];

		if ( arg->IsEllipsis() )
			num_values += (*args)[i]->NumEllipsisVals();
		else
			++num_values;
		}

	const_value_ptr* values = new const_value_ptr[num_values];

	int total_length = 0;
	for ( LOOPDECL i = 0; i < args->length(); ++i )
		{
		Parameter* arg = (*args)[i];

		if ( arg->IsEllipsis() )
			{
			int len = arg->NumEllipsisVals();

			for ( int j = 0; j < len; ++j )
				{
				values[i+j] = (const IValue*)(arg->NthEllipsisVal(j)->Deref());
				total_length += values[i+j]->Length();
				}
			}
		else
			{
			values[i] = arg->Arg()->ReadOnlyEval();
			total_length += values[i]->Length();
			}
		}

	glish_type max_type;
	if ( TypeCheck( values, num_values, max_type ) )
		result = ConstructArray( values, num_values, total_length,
					max_type );
	else
		result = error_ivalue();

	for ( LOOPDECL i = 0; i < args->length(); ++i )
		if ( ! (*args)[i]->IsEllipsis() )
			(*args)[i]->Arg()->ReadOnlyDone( values[i] );

	delete values;

	return result;
	}

int ConstructExpr::TypeCheck( const IValue* values[], int num_values,
					glish_type& max_type )
	{
	if ( num_values == 0 )
		{
		max_type = TYPE_BOOL;	// Compatible with the constant F
		return 1;
		}

	for ( int i = 0; i < num_values; ++i )
		{
		const IValue* v = values[i];

		if ( v->Length() > 0 && v->IsNumeric() )
			return MaxNumeric( values, num_values, max_type );
		}

	int result = AllEquivalent( values, num_values, max_type );

	if ( max_type == TYPE_RECORD )
		{
		error->Report( "arrays of records are not supported" );
		return 0;
		}

	return result;
	}

int ConstructExpr::MaxNumeric( const IValue* values[], int num_values,
					glish_type& max_type )
	{
	const IValue* v = (const IValue*) (values[0]->VecRefDeref());
	if ( ! v->IsNumeric() )
		{
		error->Report( "non-numeric type in array constructor",
				this );
		return 0;
		}

	max_type = v->Type();

	for ( int i = 1; i < num_values; ++i )
		{
		v = (const IValue*)(values[i]->VecRefDeref());
		if ( ! v->IsNumeric() )
			{
			error->Report( "non-numeric type in array constructor",
					this );
			return 0;
			}

		max_type = max_numeric_type( v->Type(), max_type );
		}

	return 1;
	}

int ConstructExpr::AllEquivalent( const IValue* values[], int num_values,
					glish_type& max_type )
	{
	max_type = TYPE_BOOL;

	// First pick a candidate type.
	for ( int i = 0; i < num_values; ++i )
		// Ignore empty arrays, as they can be any type.
		if ( values[i]->Length() > 0 )
			{
			max_type = values[i]->VecRefDeref()->Type();
			break;
			}

	// Now check whether all non-empty arrays conform to that type.
	for ( LOOPDECL i = 0; i < num_values; ++i )
		{
		const IValue* v = (const IValue*)(values[i]->VecRefDeref());

		if ( v->Length() > 0 && v->Type() != max_type )
			{
			error->Report(
				"incompatible types in array constructor",
					this );
			return 0;
			}
		}

	return 1;
	}

IValue* ConstructExpr::ConstructArray( const IValue* values[],
					int num_values, int total_length,
					glish_type max_type )
	{
	IValue* result;

	int is_copy;
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
		result = new IValue( array, total_length );		\
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
		result = new IValue( array, total_length, storage );	\
									\
		if ( storage == COPY_ARRAY )				\
			delete array;					\
									\
		break;							\
		}

BUILD_WITH_COERCE_TYPE(TYPE_BOOL, glish_bool, CoerceToBoolArray)
BUILD_WITH_COERCE_TYPE(TYPE_BYTE, byte, CoerceToByteArray)
BUILD_WITH_COERCE_TYPE(TYPE_SHORT, short, CoerceToShortArray)
BUILD_WITH_COERCE_TYPE(TYPE_INT, int, CoerceToIntArray)
BUILD_WITH_COERCE_TYPE(TYPE_FLOAT, float, CoerceToFloatArray)
BUILD_WITH_COERCE_TYPE(TYPE_DOUBLE, double, CoerceToDoubleArray)
BUILD_WITH_COERCE_TYPE(TYPE_COMPLEX, complex, CoerceToComplexArray)
BUILD_WITH_COERCE_TYPE(TYPE_DCOMPLEX, dcomplex, CoerceToDcomplexArray)

// For strings, copy the result so that each string in the array gets
// copied, too.
BUILD_WITH_NON_COERCE_TYPE(TYPE_STRING, charptr, StringPtr(), COPY_ARRAY)
BUILD_WITH_NON_COERCE_TYPE(TYPE_FUNC, funcptr, FuncPtr(), TAKE_OVER_ARRAY)

		case TYPE_AGENT:
			error->Report( "can't construct array of agents" );
			result = error_ivalue();
			break;

		case TYPE_OPAQUE:
			error->Report( "can't construct array opaque values" );
			result = error_ivalue();
			break;

		default:
			fatal->Report(
		    "bad type tag in ConstructExpr::ConstructArray()" );
		}

	return result;
	}

IValue* ConstructExpr::BuildRecord()
	{
	recordptr rec = create_record_dict();

	loop_over_list( *args, i )
		{
		Parameter* p = (*args)[i];
		IValue *arg = p->Arg()->CopyEval();
		if ( p->ParamType() == VAL_CONST )
			arg->MakeConst();
		rec->Insert( strdup( p->Name() ), arg );
		}

	return new IValue( rec );
	}


ArrayRefExpr::~ArrayRefExpr()
	{
	if ( args )
		{
		loop_over_list( *args, i )
			NodeUnref( (*args)[i] );
		delete args;
		}
	}

ArrayRefExpr::ArrayRefExpr( Expr* op1, expr_list* a ) : UnaryExpr(op1, "[]")
	{
	args = a;
	}

IValue* ArrayRefExpr::Eval( eval_type etype )
	{
	const IValue* array = op->ReadOnlyEval();
	IValue* result;

	const attributeptr ptr = array->AttributePtr();
	if ( ptr )
		{
		const IValue* func = (const IValue*)((*ptr)["op[]"]);
		Func* func_val = (func && func->Type() == TYPE_FUNC) ?
					func->FuncVal() : 0;

		if ( func_val && ! func_val->Mark() )
			{ // Subscript operator functions.
			parameter_list pl;

			pl.append( new ActualParameter( VAL_VAL, op ) );

			for ( int i = 0; i < args->length(); ++i )
				{
				Expr* arg = (*args)[i];
				if ( arg )
					pl.append( new ActualParameter(
							VAL_VAL, (*args)[i] ) );
				else
					pl.append( new ActualParameter() );
				}

			result = CallFunc( func_val, EVAL_COPY, &pl );
			if ( ! result )
				{
				if ( etype != EVAL_SIDE_EFFECTS )
					result = error_ivalue();
				}
			else
				{
				IValue* tmp = (IValue*)(result->Deref());
				if ( tmp->IsVecRef() )
					{
					tmp->VecRefPolymorph(
						tmp->VecRefPtr()->Type() );
					if ( tmp != result )
						Unref( result );
					result = tmp;
					}
				}

			op->ReadOnlyDone( array );
			return result;
			}

		// Multi-element subscript operation.
		if ( (*ptr)["shape"] && args->length() > 1 )
			{
			const_value_list val_list;
			Expr* arg;

			for ( int i = 0; i < args->length(); ++i )
				{
				arg = (*args)[i];
				val_list.append( arg ?
						arg->ReadOnlyEval() : 0 );
				}

			result = (IValue*)((*array)[&val_list]);
			for ( LOOPDECL i = 0; i < args->length(); ++i )
				if ( (arg = (*args)[i]) )
					arg->ReadOnlyDone( (const IValue*) val_list[i] );

			op->ReadOnlyDone( array );
			return result;
			}
		}

	if ( args->length() != 1 )
		{
		warn->Report( this, "invalid array addressing" );
		op->ReadOnlyDone( array );
		return error_ivalue();
		}

	Expr* arg = (*args)[0];
	if ( ! arg )
		{
		error->Report( "invalid missing parameter" );
		op->ReadOnlyDone( array );
		return error_ivalue();
		}

	const IValue* index_val = arg->ReadOnlyEval();
	const attributeptr indx_attr = index_val->AttributePtr();
	const IValue* indx_shape;

	if ( index_val->VecRefDeref()->Type() == TYPE_RECORD )
		{ // Single record element slice operation.
		const_value_list val_list;
		const IValue* val;
		int n = index_val->Length();
		for ( int x = 1; x <= n; ++x )
			if ( (val = (const IValue*)(index_val->NthField( x ))) &&
			     val->Length() > 0 )
				val_list.append( val );
			else
				val_list.append( 0 );
		result = (IValue*)((*array)[&val_list]);
		}

	else if ( indx_attr && (indx_shape = (const IValue*)((*indx_attr)["shape"])) &&
		  indx_shape->IsNumeric() && index_val->Type() != TYPE_BOOL )
		{ // Single element pick operation.
		result = (IValue*)(array->Pick( index_val ));
		if ( result->IsRef() )
			{
			IValue* orig_result = result;
			result = copy_value( (const IValue*) result->Deref() );
			Unref( orig_result );
			}
		}

	else
		{ // Record or array subscripting.
		if ( index_val && index_val->Type() == TYPE_STRING &&
		     index_val->Length() == 1 )
			{ // Return single element belonging to record.
			const IValue* const_result = (const IValue *)
				array->ExistingRecordElement( index_val );

			const_result = (const IValue *) (const_result->Deref());
			result = CopyOrRefValue( const_result, etype );
			}
		else
			{ // Array subscripting.
			result = (IValue *)(*array)[index_val];

			if ( result->IsRef() )
				{
				IValue* orig_result = result;
				result = copy_value( (const IValue*) (result->Deref()) );
				Unref( orig_result );
				}
			}
		}

	arg->ReadOnlyDone( index_val );

	op->ReadOnlyDone( array );
	return result;
	}

IValue* ArrayRefExpr::RefEval( value_type val_type )
	{
	IValue* array_ref = op->RefEval( val_type );
	IValue* array = (IValue*) array_ref->Deref();

	IValue* result = 0;

	const attributeptr ptr = array->AttributePtr();
	Expr* arg;

	if ( ptr )
		{
		const IValue* func = (const IValue*)(*ptr)["op[]"];
		Func* func_val = (func && func->Type() == TYPE_FUNC) ?
					func->FuncVal() : 0;

		if ( func_val && ! func_val->Mark() )
			{ // Subscript operator functions.
			parameter_list pl;
			pl.append( new ActualParameter( VAL_VAL, op ) );

			for ( int i = 0; i < args->length(); ++i )
				{
				if ( (arg = (*args)[i]) )
					pl.append( new ActualParameter(
							VAL_VAL, arg ) );
				else
					pl.append( new ActualParameter() );
				}

			Unref( array_ref );
			return CallFunc( func_val, EVAL_COPY, &pl );
			}

		if ( (*ptr)["shape"] && args->length() > 1 )
			{ // Multi-element subscript operation.
			const_value_list val_list;
			for ( int i = 0; i < args->length(); ++i )
				{
				arg = (*args)[i];
				val_list.append( arg ?
						arg->ReadOnlyEval() : 0 );
				}

			result = (IValue*)(array->SubRef( &val_list ));
			for ( LOOPDECL i = 0; i < args->length(); ++i )
				if ( (arg = (*args)[i]) )
					arg->ReadOnlyDone( (const IValue*)(val_list[i]) );

			Unref( array_ref );
			return result;
			}
		}

	if ( args->length() != 1 )
		{
		warn->Report( this, ": invalid array addressing" );
		Unref( array_ref );
		return error_ivalue();
		}

	if ( ! (arg = (*args)[0]) )
		{
		error->Report( this, ": invalid missing parameter" );
		Unref( array_ref );
		return error_ivalue();
		}

	const IValue* index_val = arg->ReadOnlyEval();
	const attributeptr indx_attr = index_val->AttributePtr();
	const IValue* indx_shape;

	if ( index_val->VecRefDeref()->Type() == TYPE_RECORD )
		{ // Single record element slice operation.
		const_value_list val_list;
		int n = index_val->Length();
		for ( int x = 1; x <= n; ++x )
			{
			const IValue* val = (const IValue*)(index_val->NthField( x ));
			val_list.append( (val && val->Length() > 0) ? val : 0 );
			}

		result = (IValue*)(array->SubRef( &val_list ));
		}

	else if ( indx_attr && (indx_shape = (const IValue*)((*indx_attr)["shape"])) &&
	          indx_shape->IsNumeric() && index_val->Type() != TYPE_BOOL )
		// Single element pick operation
		result = (IValue*)array->PickRef( index_val );
	else
		result = (IValue*)array->SubRef( index_val );

	arg->ReadOnlyDone( index_val );

	if ( val_type == VAL_REF && result->IsConst() )
		warn->Report( this, " is a \"const\" reference" );

	result = new IValue( result, val_type );

	Unref( array_ref );

	return result;
	}

IValue* ArrayRefExpr::CallFunc( Func *fv, eval_type etype,
				parameter_list *f_args )
	{
	// Mark the function so that a user-function definition for
	// "op[]" that needs to apply array referencing doesn't endlessly
	// loop.
	fv->Mark( 1 );
	IValue* ret = fv->Call( f_args, etype );
	fv->Mark( 0 );

	return ret;
	}

void ArrayRefExpr::Assign( IValue* new_value )
	{
	IValue* lhs_value_ref = op->RefEval( VAL_REF );
	IValue* lhs_value = (IValue*)(lhs_value_ref->Deref());

	if ( lhs_value_ref->IsConst() || lhs_value->VecRefDeref()->IsConst() ||
	     lhs_value_ref->VecRefDeref()->Type() != TYPE_RECORD &&
	     (lhs_value_ref->IsModConst() || lhs_value->IsModConst()) )
		{
		error->Report( "'const' values cannot be modified." );
		Unref( lhs_value_ref );
		return;
		}

	const attributeptr ptr = lhs_value->AttributePtr();

	if ( ptr )
		{
		const IValue* func = (const IValue*)((*ptr)["op[]:="]);
		int do_assign = 1;

		if ( ! func || func->Type() != TYPE_FUNC )
			{
			func = (const IValue*)((*ptr)["op[]"]);
			do_assign = glish_false;
			}

		Func* func_val = (func && func->Type() == TYPE_FUNC) ?
					func->FuncVal() : 0;

		if ( func_val && ! func_val->Mark() )
			{ // Subscript assign operator functions.
			parameter_list pl;

			if ( do_assign )
				pl.append( new ActualParameter( VAL_VAL,
						new ValExpr( new_value ) ) );

			pl.append( new ActualParameter( VAL_VAL, op ) );

			for ( int i = 0; i < args->length(); ++i )
				{
				Expr* arg = (*args)[i];
				if ( arg )
					pl.append( new ActualParameter(
							VAL_VAL, arg ) );
				else
					pl.append( new ActualParameter() );
				}

			IValue* vecref = CallFunc( func_val, EVAL_COPY, &pl );

			if ( ! do_assign )
				vecref->Deref()->AssignElements( new_value );
			else
				Unref( new_value );

			Unref( vecref );
			Unref( lhs_value_ref );

			return;
			}

		if ( (*ptr)["shape"] && args->length() > 1 )
			{ // Multi-element subscript assign operation.
			const_value_list val_list;
			Expr* arg;
			for ( int i = 0; i < args->length(); ++i )
				{
				arg = (*args)[i];
				val_list.append( arg ?
						arg->ReadOnlyEval() : 0 );
				}

			lhs_value->AssignElements( &val_list, new_value );
			for ( LOOPDECL i = 0; i < args->length(); ++i )
				if ( (arg = (*args)[i]) )
					arg->ReadOnlyDone( (const IValue*)(val_list[i]) );

			Unref( lhs_value_ref );
			return;
			}
		}

	if ( args->length() != 1 )
		{
		warn->Report( this, " invalid array addressing" );
		Unref( new_value );
		Unref( lhs_value_ref );
		return;
		}

	const IValue* index = (*args)[0]->ReadOnlyEval();
	const attributeptr indx_attr = index->AttributePtr();
	const IValue* indx_shape;

	if ( index->VecRefDeref()->Type() == TYPE_RECORD )
		{ // Single record element slice assign operation.
		const_value_list val_list;
		int n = index->Length();
		for ( int x = 1; x <= n; ++x )
			{
			const IValue* val = (const IValue*)(index->NthField( x ));
			val_list.append( (val && val->Length() > 0) ? val : 0 );
			}

		lhs_value->AssignElements( &val_list, new_value );
		}

	else if ( index->Type() != TYPE_BOOL && indx_attr && 
		  (indx_shape = (const IValue*)((*indx_attr)["shape"])) &&
		  indx_shape->IsNumeric() )
		// Single element pick assign operation.
		lhs_value->PickAssign( index, new_value );

	else
		{
		if ( index->Type() == TYPE_STRING &&
		     lhs_value->Type() == TYPE_BOOL )
			// ### assume uninitialized variable
			lhs_value->Polymorph( TYPE_RECORD );

		lhs_value->AssignElements( index, new_value );
		}

	(*args)[0]->ReadOnlyDone( index );

	Unref( lhs_value_ref );
	}

void ArrayRefExpr::Describe( ostream& s ) const
	{
	op->Describe( s );
	s << "[";
	if ( args )
		describe_expr_list( args, s );
	s << "]";
	}

RecordRefExpr::~RecordRefExpr()
	{
	if ( field )
		delete field;
	}

RecordRefExpr::RecordRefExpr( Expr* op_, char* record_field )
    : UnaryExpr(op_, ".")
	{
	field = record_field;
	}

IValue* RecordRefExpr::Eval( eval_type etype )
	{
	const IValue* record = op->ReadOnlyEval();
	const IValue* const_result = (const IValue*)(record->ExistingRecordElement( field ));

	const_result = (const IValue*)(const_result->Deref());

	IValue* result;

	result = CopyOrRefValue( const_result, etype );

	op->ReadOnlyDone( record );

	return result;
	}

IValue* RecordRefExpr::RefEval( value_type val_type )
	{
	IValue* value_ref = op->RefEval( val_type );
	IValue* value = (IValue*)(value_ref->Deref());

	value = (IValue*)(value->GetOrCreateRecordElement( field ));

	if ( val_type == VAL_REF && value->IsConst() )
		warn->Report( "record field", this,
				" is a \"const\" reference" );

	value = new IValue( value, val_type );

	Unref( value_ref );

	return value;
	}

void RecordRefExpr::Assign( IValue* new_value )
	{
	IValue* lhs_value_ref = op->RefEval( VAL_REF );
	IValue* lhs_value = (IValue*)(lhs_value_ref->Deref());

	if ( lhs_value_ref->IsConst() || lhs_value->VecRefDeref()->IsConst() ||
	     lhs_value_ref->IsRefConst() || lhs_value->VecRefDeref()->IsRefConst() )
		{
		error->Report( "'const' values cannot be modified." );
		Unref( lhs_value_ref );
		return;
		}

	if ( lhs_value->VecRefDeref()->IsFieldConst() &&
	     ! lhs_value->VecRefDeref()->HasRecordElement(field) )
		{
		error->Report( "fields cannot be added to a 'const' record." );
		Unref( lhs_value_ref );
		return;
		}

	if ( lhs_value->Type() == TYPE_BOOL && lhs_value->Length() == 1 )
		// ### assume uninitialized variable
		lhs_value->Polymorph( TYPE_RECORD );

	if ( lhs_value->VecRefDeref()->Type() == TYPE_RECORD )
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

AttributeRefExpr::~AttributeRefExpr()
	{
	if ( field )
		delete field;
	} 

AttributeRefExpr::AttributeRefExpr( Expr *op1 ) : BinaryExpr(op1, 0, "::")
	{
	field = 0;
	}

AttributeRefExpr::AttributeRefExpr( Expr* op1, char* attribute ) :
		BinaryExpr(op1, 0, "::")
	{
	field = attribute;
	}

AttributeRefExpr::AttributeRefExpr( Expr* op1, Expr* op2 ) :
		BinaryExpr(op1, op2, "::[]")
	{
	field = 0;
	}

IValue* AttributeRefExpr::Eval( eval_type etype )
	{
	const IValue* val = left->ReadOnlyEval();
	IValue* result = 0;
	const IValue* const_result = 0;

	if ( field )
		const_result = (const IValue*)(val->ExistingAttribute( field ));

	else if ( right )
		{
		const IValue* index_val = right->ReadOnlyEval();
		if ( index_val && index_val->Type() == TYPE_STRING &&
		     index_val->Length() == 1  )
			const_result = (const IValue*)(val->ExistingAttribute( index_val ));
		else
			const_result = (const IValue*)(val->AttributeRef( index_val ));

		right->ReadOnlyDone( index_val );
		}

	else
		{
		recordptr new_record = create_record_dict();
		const attributeptr aptr = val->AttributePtr();

		if ( aptr )
			{
			IterCookie* c = aptr->InitForIteration();
			const IValue* member;
			const char* key;
			while ( (member = (const IValue*)(aptr->NextEntry( key, c))) )
				new_record->Insert( strdup( key ),
						   copy_value( member ) );
			}

		result = new IValue( new_record );
		}

	if ( ! result )
		result = CopyOrRefValue( (const IValue*)(const_result->Deref()), etype );

	left->ReadOnlyDone( val );
	return result;
	}

IValue* AttributeRefExpr::RefEval( value_type val_type )
	{
	IValue* value_ref = left->RefEval( val_type );
	IValue* value = (IValue*)(value_ref->Deref());

	if ( field )
		{
		value = (IValue*)(value->GetOrCreateAttribute( field ));

		if ( val_type == VAL_REF && value->IsConst() )
			warn->Report( "attribute field", this,
					" is a \"const\" reference" );

		value = new IValue( value, val_type );
		}

	else if ( right )
		{
		const IValue* index_val = right->ReadOnlyEval();

		if ( index_val && index_val->Type() == TYPE_STRING &&
		     index_val->Length() == 1  )
			{
			value = (IValue*)(value->GetOrCreateAttribute( index_val ));

			if ( val_type == VAL_REF && value->IsConst() )
				warn->Report( "record field", this,
						" is a \"const\" reference" );

			value = new IValue( value, val_type );
			}
		else
			{
			warn->Report( this, " invalid attribute access" );
			value = error_ivalue();
			}

		right->ReadOnlyDone( index_val );
		}

	else
		{
		warn->Report( this, " invalid attribute access" );
		value = error_ivalue();
		}

	Unref( value_ref );

	return value;
	}

void AttributeRefExpr::Assign( IValue* new_value )
	{
	IValue* lhs_value_ref = left->RefEval( VAL_REF );
	IValue* lhs_value = (IValue*)(lhs_value_ref->Deref());

	if ( field )
		lhs_value->AssignAttribute( field, new_value );

	else if ( right )
		{
		const IValue* index_val = right->ReadOnlyEval();
		if ( index_val && index_val->Type() == TYPE_STRING &&
		     index_val->Length() == 1  )
			{
			char* str = index_val->StringVal();
			lhs_value->AssignAttribute( str, new_value );
			delete str;
			}

		else
			warn->Report( this, " invalid attribute access" );

		right->ReadOnlyDone( index_val );
		}

	else
		{
		if ( new_value->Type() == TYPE_RECORD )
			{
			if ( new_value->Length() > 0 )
				lhs_value->AssignAttributes(
					copy_value( new_value ) );
			else
				lhs_value->AssignAttributes( 0 );
			}
		else
			warn->Report( this, " invalid attribute assignment" );
		}

	Unref( new_value );
	Unref( lhs_value_ref );
	}

void AttributeRefExpr::Describe( ostream& s ) const
	{
	left->Describe( s );

	if ( field )
		s << "::" << field;

	else if ( right )
		{
		s << "::[";
		right->Describe( s );
		s << "]";
		}
	}

Expr *AttributeRefExpr::DoBuildFrameInfo( scope_modifier m, expr_list &dl )
	{
	left = left->DoBuildFrameInfo( m, dl );

	if ( right )
		right = right->DoBuildFrameInfo( m, dl );

	return this;
	}

RefExpr::RefExpr( Expr* op_, value_type arg_type ) : UnaryExpr(op_, "ref")
	{
	type = arg_type;
	}

IValue* RefExpr::Eval( eval_type /* etype */ )
	{
	IValue *val = op->RefEval( type );

	if ( type == VAL_CONST )
		val->MakeModConst();

	return val;
	}

void RefExpr::Assign( IValue* new_value )
	{
	if ( type == VAL_VAL )
		{
		IValue* value = op->RefEval( VAL_REF );

		if ( value->VecRefDeref()->IsConst() )
			error->Report( "'const' values cannot be modified." );
		else if ( value->Deref()->IsVecRef() )
			value->AssignElements( new_value );
		else
			value->Deref()->TakeValue( new_value );

		Unref( value );
		}

	else if ( type == VAL_CONST )
		{
		new_value->MakeConst( );
		op->Assign( new_value );
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

IValue* RangeExpr::Eval( eval_type /* etype */ )
	{
	const IValue* left_val = left->ReadOnlyEval();
	const IValue* right_val = right->ReadOnlyEval();

	IValue* result;

	if ( ! left_val->IsNumeric() || ! right_val->IsNumeric() )
		{
		error->Report( "non-numeric value in", this );
		result = error_ivalue();
		}

	else if ( left_val->Length() > 1 || right_val->Length() > 1 )
		{
		error->Report( "non-scalar value in", this );
		result = error_ivalue();
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

		result = new IValue( range, num_values );
		}

	left->ReadOnlyDone( left_val );
	right->ReadOnlyDone( right_val );

	return result;
	}


CallExpr::~CallExpr()
	{
	if ( args )
		{
		loop_over_list( *args, i )
			NodeUnref( (*args)[i] );
		delete args;
		}
	}

CallExpr::CallExpr( Expr* func, parameter_list* args_args )
    : UnaryExpr(func, "func()")
	{
	args = args_args;
	}

IValue* CallExpr::Eval( eval_type etype )
	{
	const IValue* func = op->ReadOnlyEval();
	Func* func_val = func->FuncVal();

	IValue* result = 0;

	if ( ! func_val || ! (result = func_val->Call( args, etype )) )
		{
		if ( etype != EVAL_SIDE_EFFECTS )
			result = error_ivalue();
		}

	op->ReadOnlyDone( func );

	return result;
	}

void CallExpr::SideEffectsEval()
	{
	IValue* result = Eval( EVAL_SIDE_EFFECTS );

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


SendEventExpr::~SendEventExpr()
	{
	if ( args )
		{
		loop_over_list( *args, i )
			NodeUnref( (*args)[i] );
		delete args;
		}

	if ( sender )
		Unref( sender );
	} 

SendEventExpr::SendEventExpr( EventDesignator* arg_sender,
				parameter_list* arg_args,
				int arg_is_request_reply ) : Expr("->")
	{
	sender = arg_sender;
	args = arg_args;
	is_request_reply = arg_is_request_reply;
	}

IValue* SendEventExpr::Eval( eval_type etype )
	{
	IValue* result = sender->SendEvent( args, is_request_reply );

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

IValue* LastEventExpr::Eval( eval_type etype )
	{
	Notification* n = sequencer->LastNotification();

	if ( ! n )
		{
		warn->Report( this, ": no events have been received" );
		return error_ivalue();
		}

	IValue* result;

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
			result = new IValue( n->field );
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

IValue* LastEventExpr::RefEval( value_type val_type )
	{
	Notification* n = sequencer->LastNotification();

	if ( ! n )
		{
		warn->Report( this, ": no events have been received" );
		return error_ivalue();
		}

	IValue* result;

	if ( type == EVENT_AGENT )
		result = new IValue( n->notifier->AgentRecord(), val_type );

	else if ( type == EVENT_NAME )
		result = new IValue( n->field );

	else if ( type == EVENT_VALUE )
		{
		if ( val_type == VAL_REF && n->value->IsConst() )
			warn->Report( this, " is a \"const\" reference" );

		result = new IValue( n->value, val_type );
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

			if ( (*list)[i] )
				(*list)[i]->Describe( s );
			}
	}
