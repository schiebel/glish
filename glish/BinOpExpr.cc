// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <string.h>
#include <math.h>
#include "Reporter.h"
#include "BinOpExpr.h"
#include "IValue.h"
#include "Glish/Complex.h"

#include "glishlib.h"

static char *add_fpe_errmsg = "addition FPE occurred";
static char *sub_fpe_errmsg = "subtraction FPE occurred";
static char *mul_fpe_errmsg = "multiplication FPE occurred";
static char *div_fpe_errmsg = "division FPE occurred";
static char *mod_fpe_errmsg = "modulo FPE occurred";
static char *pow_fpe_errmsg = "power FPE occurred";

BinOpExpr::BinOpExpr( binop bin_op, Expr* op1, Expr* op2, const char* desc )
    : BinaryExpr(op1, op2, desc)
	{
	op = bin_op;
	}

IValue *BinOpExpr::TypeCheck( const IValue* lhs, const IValue* rhs,
				int& element_by_element ) const
	{
	element_by_element = 1;

	if ( ! lhs->IsNumeric() || ! rhs->IsNumeric() )
		return (IValue*) Fail( "non-numeric operand in expression:", this );

	else
		return 0;
	}

glish_type BinOpExpr::OperandsType( const IValue* lhs, const IValue* rhs ) const
	{
	glish_type t1 = lhs->Type();
	glish_type t2 = rhs->Type();

	if ( t1 == TYPE_DCOMPLEX || t2 == TYPE_DCOMPLEX )
		return TYPE_DCOMPLEX;

	else if ( t1 == TYPE_COMPLEX || t2 == TYPE_COMPLEX )
		{
		if ( t1 == TYPE_DOUBLE || t2 == TYPE_DOUBLE )
			return TYPE_DCOMPLEX;
		else
			return TYPE_COMPLEX;
		}

	else if ( t1 == TYPE_DOUBLE || t2 == TYPE_DOUBLE )
		return TYPE_DOUBLE;

	else if ( t1 == TYPE_FLOAT || t2 == TYPE_FLOAT )
		return TYPE_FLOAT;

	else if ( t1 == TYPE_INT || t2 == TYPE_INT )
		return TYPE_INT;

	else if ( t1 == TYPE_SHORT || t2 == TYPE_SHORT )
		return TYPE_SHORT;

	else if ( t1 == TYPE_BYTE || t2 == TYPE_BYTE )
		return TYPE_BYTE;

	else if ( t1 == TYPE_BOOL || t2 == TYPE_BOOL )
		// Promote bool's to int's.
		return TYPE_INT;

	else if ( t1 == TYPE_STRING || t2 == TYPE_STRING )
		return TYPE_STRING;

	else
		// Hmmmm, not a numeric type.  Just pick one of the two.
		return t1;
	}

IValue *BinOpExpr::Compute( const IValue* lhs, const IValue* rhs, int& lhs_len )
    const
	{
	int lhs_scalar = lhs->Length() == 1;
	int rhs_scalar = rhs->Length() == 1;

	if ( lhs_scalar && rhs_scalar )
		lhs_scalar = 0;	// Treat lhs as 1-element array

	lhs_len = lhs->Length();
	int rhs_len = rhs->Length();

	if ( lhs_len != rhs_len && ! lhs_scalar && ! rhs_scalar )
		{
		return (IValue*) Fail( "different-length operands in expression (",
				lhs_len, " vs. ", rhs_len, "):\n\t",
				this );
		}

	if ( lhs_scalar )
		// We need to replicate the left-hand-side so that it's
		// the same length as the right-hand-side.
		lhs_len = rhs_len;

	return 0;
	}


IValue* ArithExpr::Eval( eval_type /* etype */ )
	{
	IValue* result = left->CopyEval();
	const IValue* rhs = right->ReadOnlyEval();

	const char *err_str = 0;
	int lhs_len;
	int element_by_element;
	IValue *err = 0;
	if ( ! ( err = TypeCheck( result, rhs, element_by_element )) &&
	     (! element_by_element ||
	      ! ( err = Compute( result, rhs, lhs_len ) )))
		result = OpCompute( result, rhs, lhs_len, err_str );

	else
		{
		Unref( result );
		result = err ? err : (IValue*) Fail("ArithExpr::Eval()");
		}

	right->ReadOnlyDone( rhs );

	if ( err_str )
		{
		Unref( result );
		return (IValue*) Fail( err_str );
		}

	return result;
	}

IValue* ArithExpr::OpCompute( IValue* lhs, const IValue* rhs, int lhs_len,
			      const char *&err )
	{
	switch ( OperandsType( lhs, rhs ) )
		{
		case TYPE_BYTE:
			lhs->ByteOpCompute( rhs, lhs_len, this, err );
			break;

		case TYPE_SHORT:
			lhs->ShortOpCompute( rhs, lhs_len, this, err );
			break;

		case TYPE_INT:
			lhs->IntOpCompute( rhs, lhs_len, this, err );
			break;

		case TYPE_FLOAT:
			lhs->FloatOpCompute( rhs, lhs_len, this, err );
			break;

		case TYPE_DOUBLE:
			lhs->DoubleOpCompute( rhs, lhs_len, this, err );
			break;

		case TYPE_COMPLEX:
			lhs->ComplexOpCompute( rhs, lhs_len, this, err );
			break;

		case TYPE_DCOMPLEX:
			lhs->DcomplexOpCompute( rhs, lhs_len, this, err );
			break;

		default:
			fatal->Report(
				"bad operands type in ArithExpr::OpCompute()" );
		}

	return lhs;
	}


#define COMPUTE_OP(name,op,type,errmsg)					\
void name::Compute( type lhs[], type rhs[], int lhs_len,		\
		    int rhs_incr, const char *&err )			\
	{								\
	err = 0;							\
	glish_fpe_enter( ); /* reset FPE trap */			\
	for ( int i = 0, j = 0; i < lhs_len; ++i, j += rhs_incr )	\
		lhs[i] op rhs[j];					\
	if ( glish_fpe_exit( ) ) err = errmsg;				\
	}

#define COMPLEX_COMPUTE_OP(name,op,type,errmsg)				\
void name::Compute( type lhs[], type rhs[], int lhs_len,		\
		    int rhs_incr, const char *&err  )			\
	{								\
	err = 0;							\
	glish_fpe_enter( ); /* reset FPE trap */			\
	for ( int i = 0, j = 0; i < lhs_len; ++i, j += rhs_incr )	\
		{							\
		lhs[i].r op rhs[j].r;					\
		lhs[i].i op rhs[j].i;					\
		}							\
	if ( glish_fpe_exit( ) ) err = errmsg;				\
	}

#define COMPLEX_COMPUTE_MUL_OP(type,errmsg)				\
void MultiplyExpr::Compute( type lhs[], type rhs[], int lhs_len,	\
			    int rhs_incr, const char *&err )		\
	{								\
	err = 0;							\
	glish_fpe_enter( ); /* reset FPE trap */			\
	for ( int i = 0, j = 0; i < lhs_len; ++i, j += rhs_incr )	\
		lhs[i] = mul( lhs[i], rhs[j] );				\
	if ( glish_fpe_exit( ) ) err = errmsg;				\
	}

#define COMPLEX_COMPUTE_DIV_OP(type,errmsg)				\
void DivideExpr::Compute( type lhs[], type rhs[], int lhs_len,		\
			  int rhs_incr, const char *&err )		\
	{								\
	err = 0;							\
	glish_fpe_enter( ); /* reset FPE trap */			\
	for ( int i = 0, j = 0; i < lhs_len; ++i, j += rhs_incr )	\
		lhs[i] = div( lhs[i], rhs[j] );				\
	if ( glish_fpe_exit( ) ) err = errmsg;				\
	}

#define DEFINE_ARITH_EXPR(name, op, errmsg)	\
COMPUTE_OP(name,op,byte,errmsg)			\
COMPUTE_OP(name,op,short,errmsg)		\
COMPUTE_OP(name,op,int,errmsg)			\
COMPUTE_OP(name,op,float,errmsg)		\
COMPUTE_OP(name,op,double,errmsg)		\

#define DEFINE_SIMPLE_ARITH_EXPR(name, op, errmsg)	\
DEFINE_ARITH_EXPR(name,op,errmsg)			\
COMPLEX_COMPUTE_OP(name,op,complex,errmsg)		\
COMPLEX_COMPUTE_OP(name,op,dcomplex,errmsg)		\

DEFINE_SIMPLE_ARITH_EXPR(AddExpr,+=,add_fpe_errmsg)
DEFINE_SIMPLE_ARITH_EXPR(SubtractExpr,-=,sub_fpe_errmsg)

DEFINE_ARITH_EXPR(MultiplyExpr,*=,mul_fpe_errmsg)
COMPLEX_COMPUTE_MUL_OP(complex,mul_fpe_errmsg)
COMPLEX_COMPUTE_MUL_OP(dcomplex,mul_fpe_errmsg)

COMPUTE_OP(DivideExpr,/=,byte,div_fpe_errmsg)
COMPUTE_OP(DivideExpr,/=,short,div_fpe_errmsg)
COMPUTE_OP(DivideExpr,/=,int,div_fpe_errmsg)
//
// Some OSes generate a SIGFPE (OSF1), and others don't (SunOS)
// assume that on divide all is OK (for now) with floating point
//
COMPUTE_OP(DivideExpr,/=,float,0)
COMPUTE_OP(DivideExpr,/=,double,0)
COMPLEX_COMPUTE_DIV_OP(complex,div_fpe_errmsg)
COMPLEX_COMPUTE_DIV_OP(dcomplex,div_fpe_errmsg)


glish_type DivideExpr::OperandsType( const IValue* lhs, const IValue* rhs ) const
	{
	glish_type ltype = lhs->Type();
	glish_type rtype = rhs->Type();

	if ( ltype == TYPE_DCOMPLEX || rtype == TYPE_DCOMPLEX )
		return TYPE_DCOMPLEX;

	else if ( ltype == TYPE_COMPLEX || rtype == TYPE_COMPLEX )
		return TYPE_COMPLEX;

	else
		return TYPE_DOUBLE;
	}


glish_type ModuloExpr::OperandsType( const IValue* /* lhs */,
					const IValue* /* rhs */ ) const
	{
	return TYPE_INT;
	}

COMPUTE_OP(ModuloExpr,%=,byte,mod_fpe_errmsg)
COMPUTE_OP(ModuloExpr,%=,short,mod_fpe_errmsg)
COMPUTE_OP(ModuloExpr,%=,int,mod_fpe_errmsg)

void ModuloExpr::Compute( float*, float*, int, int, const char *& )
	{
	fatal->Report( "ModuloExpr::Compute() called with float operands" );
	}

void ModuloExpr::Compute( double*, double*, int, int, const char *& )
	{
	fatal->Report( "ModuloExpr::Compute() called with double operands" );
	}

void ModuloExpr::Compute( complex*, complex*, int, int, const char *& )
	{
	fatal->Report( "ModuloExpr::Compute() called with complex operands" );
	}

void ModuloExpr::Compute( dcomplex*, dcomplex*, int, int, const char *& )
	{
	fatal->Report( "ModuloExpr::Compute() called with dcomplex operands" );
	}


glish_type PowerExpr::OperandsType( const IValue* lhs, const IValue* rhs ) const
	{
	glish_type t1 = lhs->Type();
	glish_type t2 = rhs->Type();

	if ( t1 == TYPE_DCOMPLEX || t2 == TYPE_DCOMPLEX )
		return TYPE_DCOMPLEX;

	else if ( t1 == TYPE_COMPLEX || t2 == TYPE_COMPLEX )
		return TYPE_DCOMPLEX;

	else return TYPE_DOUBLE;
	}

void PowerExpr::Compute( byte*, byte*, int, int, const char *& )
	{
	fatal->Report( "PowerExpr::Compute() called with byte operands" );
	}

void PowerExpr::Compute( short*, short*, int, int, const char *& )
	{
	fatal->Report( "PowerExpr::Compute() called with short operands" );
	}

void PowerExpr::Compute( int*, int*, int, int, const char *& )
	{
	fatal->Report( "PowerExpr::Compute() called with integer operands" );
	}

void PowerExpr::Compute( float*, float*, int, int, const char *& )
	{
	fatal->Report( "PowerExpr::Compute() called with float operands" );
	}

void PowerExpr::Compute( double lhs[], double rhs[], int lhs_len,
			 int rhs_incr, const char *&err )
	{
	err = 0;
	glish_fpe_enter( ); /* reset FPE trap */
	for ( int i = 0, j = 0; i < lhs_len; ++i, j += rhs_incr )
		lhs[i] = pow( lhs[i], rhs[j] );
	if ( glish_fpe_exit( ) ) err = pow_fpe_errmsg;
	}

void PowerExpr::Compute( complex*, complex*, int, int, const char *& )
	{
	fatal->Report( "PowerExpr::Compute() called with complex operands" );
	}

void PowerExpr::Compute( dcomplex lhs[], dcomplex rhs[],
			int lhs_len, int rhs_incr, const char *&err )
	{
	err = 0;
	glish_fpe_enter( ); /* reset FPE trap */
	for ( int i = 0, j = 0; i < lhs_len; ++i, j += rhs_incr )
		lhs[i] = pow( lhs[i], rhs[j] );
	if ( glish_fpe_exit( ) ) err = pow_fpe_errmsg;
	}



IValue* RelExpr::Eval( eval_type /* etype */ )
	{
	const IValue* lhs = left->ReadOnlyEval();
	const IValue* rhs = right->ReadOnlyEval();

	IValue* result;
	int lhs_len;
	int element_by_element;
	IValue *err = 0;
	if ( ! ( err = TypeCheck( lhs, rhs, element_by_element )) &&
	     (! element_by_element ||
	     ! ( err = Compute( lhs, rhs, lhs_len )) ))
		result = OpCompute( lhs, rhs, lhs_len );
	else
		result = err ? err : (IValue*) Fail("RelExpr::Eval()");

	left->ReadOnlyDone( lhs );
	right->ReadOnlyDone( rhs );

	return result;
	}

IValue *RelExpr::TypeCheck( const IValue* lhs, const IValue* rhs,
				int& element_by_element ) const
	{
	element_by_element = 1;

	if ( lhs->Type() == TYPE_STRING && rhs->Type() == TYPE_STRING )
		return 0;

	else if ( lhs->IsNumeric() || rhs->IsNumeric() )
		return BinOpExpr::TypeCheck( lhs, rhs, element_by_element );

	else
		{
		// Equality comparisons are allowed between all types,
		// but not other operations.
		if ( op == OP_EQ || op == OP_NE )
			{
			element_by_element = 0;
			return 0;
			}

		else
			return (IValue*) Fail("bad types for ", description);
		}
	}

glish_type RelExpr::OperandsType( const IValue* lhs, const IValue* rhs ) const
	{
	glish_type t1 = lhs->Type();
	glish_type t2 = rhs->Type();

	if ( t1 == TYPE_STRING && t2 == TYPE_STRING )
		return TYPE_STRING;

	else if ( (op == OP_AND || op == OP_OR) &&
		  (t1 == TYPE_BOOL && t2 == TYPE_BOOL) )
		return TYPE_BOOL;

	else
		return BinOpExpr::OperandsType( lhs, rhs );
	}

IValue* RelExpr::OpCompute( const IValue* lhs, const IValue* rhs, int lhs_len )
	{
	IValue* result;

	switch ( OperandsType( lhs, rhs ) )
		{
		case TYPE_BOOL:
			result = bool_rel_op_compute( lhs, rhs, lhs_len, this );
			break;

		case TYPE_BYTE:
			result = byte_rel_op_compute( lhs, rhs, lhs_len, this );
			break;

		case TYPE_SHORT:
			result = short_rel_op_compute( lhs, rhs, lhs_len, this );
			break;

		case TYPE_INT:
			result = int_rel_op_compute( lhs, rhs, lhs_len, this );
			break;

		case TYPE_FLOAT:
			result = float_rel_op_compute( lhs, rhs,
							lhs_len, this );
			break;

		case TYPE_DOUBLE:
			result = double_rel_op_compute( lhs, rhs,
							lhs_len, this );
			break;

		case TYPE_COMPLEX:
			result = complex_rel_op_compute( lhs, rhs,
							lhs_len, this );
			break;

		case TYPE_DCOMPLEX:
			result = dcomplex_rel_op_compute( lhs, rhs,
							lhs_len, this );
			break;

		case TYPE_STRING:
			result = string_rel_op_compute( lhs, rhs,
							lhs_len, this );
			break;

		case TYPE_AGENT:
		case TYPE_FUNC:
		case TYPE_RECORD:
		case TYPE_OPAQUE:
			if ( op == OP_EQ )
				return new IValue( glish_bool( lhs == rhs ) );

			else if ( op == OP_NE )
				return new IValue( glish_bool( lhs != rhs ) );

			else
				fatal->Report(
				"bad operands type in RelExpr::OpCompute()" );
			break;

		default:
			fatal->Report(
				"bad operands type in RelExpr::OpCompute()" );
		}

	return result;
	}

#define COMPUTE_BOOL_REL_RESULT(name,op,type)				\
void name::Compute( type lhs[], type rhs[], glish_bool result[],	\
    int lhs_len, int rhs_incr )						\
	{								\
	for ( int i = 0, j = 0; i < lhs_len; ++i, j += rhs_incr )	\
		result[i] = glish_bool( int( lhs[i] ) op int( rhs[j] ) );\
	}

#define COMPUTE_NUMERIC_REL_RESULT(name,op,type)			\
void name::Compute( type lhs[], type rhs[], glish_bool result[],	\
    int lhs_len, int rhs_incr )						\
	{								\
	for ( int i = 0, j = 0; i < lhs_len; ++i, j += rhs_incr )	\
		result[i] = glish_bool( lhs[i] op rhs[j] );		\
	}

#define COMPUTE_STRING_REL_RESULT(name,op,type)				\
void name::Compute( type lhs[], type rhs[], glish_bool result[],	\
    int lhs_len, int rhs_incr )						\
	{								\
	for ( int i = 0, j = 0; i < lhs_len; ++i, j += rhs_incr )	\
		result[i] = glish_bool( strcmp( lhs[i], rhs[j] ) op 0 );\
	}

#define DEFINE_REL_EXPR(name, op)					\
COMPUTE_BOOL_REL_RESULT(name,op,glish_bool)				\
COMPUTE_NUMERIC_REL_RESULT(name,op,byte)				\
COMPUTE_NUMERIC_REL_RESULT(name,op,short)				\
COMPUTE_NUMERIC_REL_RESULT(name,op,int)					\
COMPUTE_NUMERIC_REL_RESULT(name,op,float)				\
COMPUTE_NUMERIC_REL_RESULT(name,op,double)				\
COMPUTE_NUMERIC_REL_RESULT(name,op,complex)				\
COMPUTE_NUMERIC_REL_RESULT(name,op,dcomplex)				\
COMPUTE_STRING_REL_RESULT(name,op,charptr)

DEFINE_REL_EXPR(EQ_Expr, ==)
DEFINE_REL_EXPR(NE_Expr, !=)
DEFINE_REL_EXPR(LE_Expr, <=)
DEFINE_REL_EXPR(GE_Expr, >=)
DEFINE_REL_EXPR(LT_Expr, <)
DEFINE_REL_EXPR(GT_Expr, >)


#define DEFINE_LOG_EXPR_COMPUTE(type, typename)				\
void LogExpr::Compute( type*, type*, glish_bool*, int, int )		\
	{								\
	fatal->Report( "LogExpr::Compute called with", typename, "operands" );\
	}
DEFINE_LOG_EXPR_COMPUTE(glish_bool, "boolean")
DEFINE_LOG_EXPR_COMPUTE(byte, "byte")
DEFINE_LOG_EXPR_COMPUTE(short, "short")
DEFINE_LOG_EXPR_COMPUTE(int, "integer")
DEFINE_LOG_EXPR_COMPUTE(float, "float")
DEFINE_LOG_EXPR_COMPUTE(double, "double")
DEFINE_LOG_EXPR_COMPUTE(complex, "complex")
DEFINE_LOG_EXPR_COMPUTE(dcomplex, "dcomplex")
DEFINE_LOG_EXPR_COMPUTE(charptr, "string")

IValue *LogExpr::TypeCheck( const IValue* lhs, const IValue* rhs,
				int& element_by_element ) const
	{
	element_by_element = 1;

	if ( lhs->Type() == TYPE_BOOL && rhs->Type() == TYPE_BOOL )
		return 0;
	else
		return (IValue*) Fail( "non-boolean operands to", this );
	}

glish_type LogExpr::OperandsType( const IValue*, const IValue* ) const
	{
	return TYPE_BOOL;
	}

COMPUTE_BOOL_REL_RESULT(LogAndExpr,&,glish_bool)
COMPUTE_BOOL_REL_RESULT(LogOrExpr,|,glish_bool)
