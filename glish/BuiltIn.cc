// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// For MAXINT, MAXFLOAT, HUGE.
#include <values.h>

#include "sos/io.h"
#include "Npd/npd.h"
#include "glish_event.h"
#include "BuiltIn.h"
#include "Reporter.h"
#include "Task.h"
#include "Sequencer.h"
#include "Frame.h"

#include "Glish/Stream.h"
#include "glishlib.h"

#ifdef GLISHTK
#include "TkAgent.h"
#include "TkCanvas.h"
#ifdef TKPGPLOT
#include "TkPgplot.h"
#endif
#endif

#if !defined(HUGE) /* this because it's not defined in the vxworks includes */
#if defined(HUGE_VAL)
#define HUGE HUGE_VAL
#else
#define HUGE (infinity())
#endif

#if !defined(MAXINT)
#define MAXINT 0x7fffffff
#endif
#if !defined(MAXFLOAT)
// Half-assed guess.
#define MAXFLOAT 1e38
#endif
#endif

const char *BuiltIn::Description() const
	{
	return description;
	}

IValue* BuiltIn::Call( parameter_list* args, eval_type etype )
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
			return (IValue*) Fail( this, " takes", num_args, " argument",
					num_args == 1 ? ";" : "s;",
					num_args_present, " given" );
		}

	IValue *fail = 0;
	loop_over_list( *args, j )
		{
		Parameter* arg = (*args)[j];
		if ( ! arg->Arg() )
			return (IValue*) Fail( "missing parameter invalid for", this );

		if ( arg->Name() )
			return (IValue*) Fail( this,
					" does not have a parameter named \"",
					arg->Name(), "\"" );
		}

	const_args_list* args_vals = new const_args_list;

	int do_call = 1;

	loop_over_list( *args, i )
		{
		Parameter* arg = (*args)[i];
		const IValue* arg_val;

		if ( arg->IsEllipsis() )
			{
			int len = arg->NumEllipsisVals();

			for ( int j = 0; j < len; ++j )
				{
				arg_val = arg->NthEllipsisVal( j );
				if ( arg_val->Type() == TYPE_FAIL &&
				     ! handle_fail )
					{
					fail = copy_value( arg_val );
					break;
					}
				if ( do_deref )
					arg_val = (const IValue*) (arg_val->Deref());

				args_vals->append( arg_val );
				}
			}

		else
			{
			if ( do_ref_eval )
				arg_val = arg->Arg()->RefEval( VAL_REF );
			else
				arg_val = arg->Arg()->ReadOnlyEval();

			if ( arg_val->Type() == TYPE_FAIL &&
			     ! handle_fail )
				{
				fail = copy_value( arg_val );
				break;
				}
			if ( do_deref )
				arg_val = (const IValue*) (arg_val->Deref());

			args_vals->append( arg_val );
			}
		}

	IValue* result;

	if ( do_call && ! fail )
		{
		if ( etype == EVAL_SIDE_EFFECTS )
			{
			int side_effects_okay = 0;
			DoSideEffectsCall( args_vals, side_effects_okay );

			if ( ! side_effects_okay )
				warn->Report( "function return value ignored:",
						this );

			result = 0;
			}

		else
			{
			IValue *last = handle_fail ? FailStmt::SwapFail(0) : 0;

			result = DoCall( args_vals );

			if ( handle_fail && result && result->Type() != TYPE_FAIL )
				FailStmt::SetFail(last);

			Unref(last);
			}
		}
	else
		result = fail ? fail : error_ivalue();

	if ( do_ref_eval )
		{
		loop_over_list( *args_vals, k )
			if ( ! (*args)[k]->IsEllipsis() )
				Unref((IValue*)(*args_vals)[k]);
		}
	else
		{
		loop_over_list( *args, k )
			if ( ! (*args)[k]->IsEllipsis() )
				(*args)[k]->Arg()->ReadOnlyDone( (const IValue*) ((*args_vals)[k]));
		}

	delete args_vals;

	return result;
	}

void BuiltIn::DoSideEffectsCall( const_args_list* args_vals,
				int& side_effects_okay )
	{
	side_effects_okay = side_effects_call_okay;
	Unref( DoCall( args_vals ) );
	}

int BuiltIn::DescribeSelf( OStream& s, charptr prefix ) const
	{
	if ( prefix ) s << prefix;
	s << description << "()";
	return 1;
	}

IValue *BuiltIn::AllNumeric( const_args_list* args_vals, glish_type& max_type,
	int strings_okay )
	{
	max_type = TYPE_STRING;

	loop_over_list( *args_vals, i )
		{
		const IValue* arg = (const IValue*)((*args_vals)[i]->VecRefDeref());

		if ( arg->IsNumeric() )
			{
			max_type = max_numeric_type( max_type, arg->Type() );
			continue;
			}

		if ( strings_okay && arg->Type() == TYPE_STRING )
			continue;

		return (IValue*) Fail( "argument #", i + 1, "to", this,
			"is not numeric", strings_okay ? " or a string" : "" );
		}

	return 0;
	}


IValue* OneValueArgBuiltIn::DoCall( const_args_list* args_val )
	{
	return (*func)( (*args_val)[0] );
	}


IValue* NumericVectorBuiltIn::DoCall( const_args_list* args_val )
	{
	const IValue* arg = (*args_val)[0];
	IValue* result;

	if ( ! arg->IsNumeric() )
		return (IValue*) Fail( this, " requires a numeric argument" );

	int len = arg->Length();
	glish_type type = arg->Type();

#define NUMERIC_BUILTIN_ACTION_LOOP(fn)				\
	for ( int i = 0; i < len; ++i )				\
		stor[i] = (*fn)( args_vec[i] );
				
#define NUMERIC_BUILTIN_ACTION(type,accessor,fn)		\
	{							\
	int is_copy;						\
	type* args_vec = arg->accessor( is_copy, len );		\
	type* stor = (type*) alloc_memory( sizeof(type)*len );	\
								\
	NUMERIC_BUILTIN_ACTION_LOOP(fn)				\
								\
	if ( is_copy )						\
		free_memory( args_vec );			\
								\
	result = new IValue( stor, len );			\
	result->CopyAttributes( arg );				\
	}

	if ( type == TYPE_COMPLEX || type == TYPE_DCOMPLEX )
		NUMERIC_BUILTIN_ACTION(dcomplex,CoerceToDcomplexArray,cfunc)
	else
#if defined(__alpha) || defined(__alpha__)
#undef NUMERIC_BUILTIN_ACTION_LOOP
#define NUMERIC_BUILTIN_ACTION_LOOP(fn)				\
	glish_func_loop( fn, stor, args_vec, len );
#endif
		NUMERIC_BUILTIN_ACTION(double,CoerceToDoubleArray,func)

	return result;
	}

IValue* RealBuiltIn::DoCall( const_args_list* args_val )
	{
	const IValue* v = (*args_val)[0];

	if ( ! v->IsNumeric() )
		return (IValue*) Fail( this, " requires a numeric argument" );

	IValue* result;

#define RE_IM_BUILTIN_ACTION(tag,type,subtype,accessor,OFFSET,elem,XLATE)\
	case tag:							\
		{							\
		int is_copy;						\
		int len = v->Length();					\
		subtype* stor = (subtype*) alloc_memory( sizeof(subtype)*len );\
		type* from = v->accessor( is_copy, len );		\
		for ( int i = 0; i < len; i++ )				\
			{						\
			XLATE						\
			stor[i] = from[OFFSET] elem;			\
			}						\
		if ( is_copy )						\
			free_memory( from );				\
		result = new IValue( stor, len );			\
		result->CopyAttributes( v );				\
		}							\
		break;

	switch ( v->Type() )
		{
RE_IM_BUILTIN_ACTION(TYPE_COMPLEX,complex,float,CoerceToComplexArray,i,.r,)
RE_IM_BUILTIN_ACTION(TYPE_DCOMPLEX,dcomplex,double,CoerceToDcomplexArray,i,.r,)

		case TYPE_SUBVEC_REF:
			{
			VecRef* ref = v->VecRefPtr();
			IValue* theVal = (IValue*) ref->Val();

			switch ( theVal->Type() )
				{

#define RE_IM_BUILTIN_ACTION_XLATE				\
	int err;						\
	int off = ref->TranslateIndex( i, &err );		\
	if ( err )						\
		{						\
		free_memory( stor );				\
		if ( is_copy )					\
			free_memory( from );			\
		return (IValue*) Fail("invalid index (=",i+1,"),\
			sub-vector reference may be bad");	\
		}

RE_IM_BUILTIN_ACTION(TYPE_COMPLEX,complex,float,CoerceToComplexArray,off,.r,RE_IM_BUILTIN_ACTION_XLATE)
RE_IM_BUILTIN_ACTION(TYPE_DCOMPLEX,dcomplex,double,CoerceToDcomplexArray,off,.r,RE_IM_BUILTIN_ACTION_XLATE)

				default:
					result = copy_value(v);
				}
			}
			break;
		default:
			result = copy_value(v);
		}

	return result;
	}

IValue* ImagBuiltIn::DoCall( const_args_list* args_val )
	{
	const IValue* v = (*args_val)[0];

	if ( ! v->IsNumeric() )
		return (IValue*) Fail( this, " requires a numeric argument" );

	IValue* result;

	switch ( v->Type() )
		{
RE_IM_BUILTIN_ACTION(TYPE_COMPLEX,complex,float,CoerceToComplexArray,i,.i,)
RE_IM_BUILTIN_ACTION(TYPE_DCOMPLEX,dcomplex,double,CoerceToDcomplexArray,i,.i,)

		case TYPE_SUBVEC_REF:
			{
			VecRef* ref = v->VecRefPtr();
			IValue* theVal = (IValue*) ref->Val();

			switch ( theVal->Type() )
				{

RE_IM_BUILTIN_ACTION(TYPE_COMPLEX,complex,float,CoerceToComplexArray,off,.i,RE_IM_BUILTIN_ACTION_XLATE)
RE_IM_BUILTIN_ACTION(TYPE_DCOMPLEX,dcomplex,double,CoerceToDcomplexArray,off,.i,RE_IM_BUILTIN_ACTION_XLATE)

				default:
					result = copy_value(v);
				}
			}
			break;
		default:
			result = new IValue( 0.0 );
		}

	return result;
	}

IValue* StrlenBuiltIn::DoCall( const_args_list* args_val )
	{
	const IValue* v = (*args_val)[0];

	if ( ! v->Type() == TYPE_STRING )
		return (IValue*) Fail( this, " requires a string argument" );

	int len = v->Length();
	int *ret = (int*) alloc_memory( sizeof(int)*len );
	charptr *strs = v->StringPtr(0);

	for ( int i=0; i < len; ++i )
		ret[i] = strlen( strs[i] );

	return new IValue( ret, len );
	}

IValue* ComplexBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();
	IValue* result;

	if ( len < 1 || len > 2 )
		return (IValue*) Fail( this, " takes 1 or 2 arguments" );

	if ( len == 2 )
		{
		const IValue* rv = (*args_val)[0];
		const IValue* iv = (*args_val)[1];

		if ( ! rv->IsNumeric() || ! iv->IsNumeric() )
			return (IValue*) Fail( this,
				" requires one or two numeric arguments" );

		int rlen = rv->Length();
		int ilen = iv->Length();

		int rscalar = rlen == 1;
		int iscalar = ilen == 1;

		if ( rlen != ilen && ! rscalar && ! iscalar )
			return (IValue*) Fail(
				"different-length operands in expression (",
					rlen, " vs. ", ilen, "):\n\t",
					this );

		glish_type maxt = max_numeric_type( rv->Type(), iv->Type() );

#define COMPLEXBUILTIN_TWOPARM_ACTION(tag,type,rettype,accessor,coerce)	\
	case tag:							\
		{							\
		int r_is_copy;						\
		int i_is_copy;						\
		int maxlen = rlen > ilen ? rlen : ilen;			\
		rettype* r = rv->accessor( r_is_copy, rlen );		\
		rettype* i = iv->accessor( i_is_copy, ilen );		\
		type* stor = (type*) alloc_memory( sizeof(type)*maxlen );\
		for ( int cnt = 0; cnt < maxlen; ++cnt )		\
			{						\
			stor[cnt].r = coerce( r[rscalar ? 0 : cnt] );	\
			stor[cnt].i = coerce( i[iscalar ? 0 : cnt] );	\
			}						\
		if ( r_is_copy )					\
			free_memory( r );				\
		if ( i_is_copy )					\
			free_memory( i );				\
		result = new IValue( stor, maxlen );			\
		}							\
		break;

		switch ( maxt )
			{
COMPLEXBUILTIN_TWOPARM_ACTION(TYPE_BOOL,complex,glish_bool,
	CoerceToBoolArray,float)
COMPLEXBUILTIN_TWOPARM_ACTION(TYPE_BYTE,complex,byte,CoerceToByteArray,float)
COMPLEXBUILTIN_TWOPARM_ACTION(TYPE_SHORT,complex,short,CoerceToShortArray,float)
COMPLEXBUILTIN_TWOPARM_ACTION(TYPE_INT,complex,int,CoerceToIntArray,float)
COMPLEXBUILTIN_TWOPARM_ACTION(TYPE_FLOAT,complex,float,CoerceToFloatArray,)
COMPLEXBUILTIN_TWOPARM_ACTION(TYPE_DOUBLE,dcomplex,double,CoerceToDoubleArray,)

			case TYPE_COMPLEX:
			case TYPE_DCOMPLEX:
				if ( rv->Type() == TYPE_COMPLEX ||
				     rv->Type() == TYPE_DCOMPLEX )
					result = copy_value( rv );
				else
					result = copy_value( iv );
				break;

			default:
				result = error_ivalue();
			}
		}

	else
		{
		const IValue* v = (*args_val)[0];

		if ( ! v->IsNumeric() )
			return (IValue*) Fail( this,
				" requires one or two numeric arguments" );

#define COMPLEXBUILTIN_ONEPARM_ACTION(tag,type,rettype,accessor,coerce)	\
	case tag:							\
		{							\
		int is_copy;						\
		int vlen = v->Length();					\
		rettype* vp = v->accessor( is_copy, vlen );		\
		type* stor = (type*) alloc_memory( sizeof(type)*vlen );	\
		for ( int cnt = 0; cnt < vlen; ++cnt )			\
			{						\
			stor[cnt].r = coerce( vp[cnt] );		\
			stor[cnt].i = 0;				\
			}						\
		if ( is_copy )						\
			free_memory( vp );				\
		result = new IValue( stor, vlen );			\
		}							\
		break;

		switch ( v->Type() )
			{
COMPLEXBUILTIN_ONEPARM_ACTION(TYPE_BOOL,complex,glish_bool,
	CoerceToBoolArray,float)
COMPLEXBUILTIN_ONEPARM_ACTION(TYPE_BYTE,complex,byte,CoerceToByteArray,float)
COMPLEXBUILTIN_ONEPARM_ACTION(TYPE_SHORT,complex,short,CoerceToShortArray,float)
COMPLEXBUILTIN_ONEPARM_ACTION(TYPE_INT,complex,int,CoerceToIntArray,float)
COMPLEXBUILTIN_ONEPARM_ACTION(TYPE_FLOAT,complex,float,CoerceToFloatArray,)
COMPLEXBUILTIN_ONEPARM_ACTION(TYPE_DOUBLE,dcomplex,double,CoerceToDoubleArray,)

			case TYPE_COMPLEX:
			case TYPE_DCOMPLEX:
				result = copy_value( v );
				break;

			default:
				result = error_ivalue();
			}
		}

	return result;
	}

IValue* SumBuiltIn::DoCall( const_args_list* args_val )
	{
	glish_type max_type;
	IValue* result;

	if ( (result = AllNumeric( args_val, max_type )) )
		return result;

#define SUM_BUILTIN_ACTION(type,accessor)			\
	{							\
	type sum = 0.0;						\
	loop_over_list( *args_val, i )				\
		{						\
		const IValue* val = (*args_val)[i];		\
		int len = val->Length();			\
		int is_copy;					\
		type* val_array = val->accessor(is_copy,len);	\
		for ( int j = 0; j < len; ++j )			\
			sum += val_array[j];			\
		if ( is_copy )					\
			free_memory( val_array );		\
		}						\
	result = new IValue( sum );				\
	}

	if ( max_type == TYPE_COMPLEX || max_type == TYPE_DCOMPLEX )
		SUM_BUILTIN_ACTION(dcomplex,CoerceToDcomplexArray)
	else
		SUM_BUILTIN_ACTION(double,CoerceToDoubleArray)

	return result;
	}

IValue* ProdBuiltIn::DoCall( const_args_list* args_val )
	{
	glish_type max_type;
	IValue* result;

	if ( (result = AllNumeric( args_val, max_type )) )
		return result;

	switch ( max_type )
		{
#define PRODBUILTIN_ACTION(type,accessor)				\
		{							\
		type prod = 1.0;					\
		loop_over_list( *args_val, i )				\
			{						\
			const IValue* val = (*args_val)[i];		\
			int len = val->Length();			\
			int is_copy;					\
			type* val_array = val->accessor(is_copy, len);	\
			for ( int j = 0; j < len; ++j )			\
				prod *= val_array[j];			\
			if ( is_copy )					\
				free_memory( val_array );		\
			}						\
		result = new IValue( prod );				\
		break;							\
		}

		case TYPE_COMPLEX:
		case TYPE_DCOMPLEX:
			PRODBUILTIN_ACTION(dcomplex,CoerceToDcomplexArray)

		case TYPE_BOOL:
		case TYPE_BYTE:
		case TYPE_SHORT:
		case TYPE_INT:
		case TYPE_FLOAT:
		case TYPE_DOUBLE:
			PRODBUILTIN_ACTION(double,CoerceToDoubleArray)

		default:
			fatal->Report( "bad type in ProdBuiltIn::DoCall()" );
			return 0;
		}

	return result;
	}

IValue* LengthBuiltIn::DoCall( const_args_list* args_val )
	{
	int num = args_val->length();

	if ( num > 1 )
		{
		int* len = (int*) alloc_memory( sizeof(int)*args_val->length() );
		loop_over_list( *args_val, i )
			len[i] = (*args_val)[i]->Length();
		return new IValue( len, num );
		}

	else if ( num == 1 )
		return new IValue( int( (*args_val)[0]->Length() ) );

	else
		return empty_ivalue();
	}

IValue* RangeBuiltIn::DoCall( const_args_list* args_val )
	{
	glish_type max_type;
	IValue* result;

	if ( (result = AllNumeric( args_val, max_type )) )
		return result;

#define RANGEBUILTIN_ARG_ACTION(tag,src_type,tgt_type,accessor,INDEX,src_elem,XLATE) \
	case tag:							\
		{							\
		src_type* val_array = val->accessor( 0 );		\
									\
		for ( int j = 0; j < len; ++j )				\
			{						\
			XLATE						\
			if ( (tgt_type) val_array[INDEX] src_elem < min_val )	\
				min_val = (tgt_type) val_array[INDEX] src_elem;	\
									\
			if ( (tgt_type) val_array[INDEX] src_elem > max_val )	\
				max_val = (tgt_type) val_array[INDEX] src_elem;	\
			}						\
		}							\
		break;
	
#define RANGEBUILTIN_XLATE						\
	int err;							\
	int off = ref->TranslateIndex( j , &err );			\
	if ( err )							\
		return (IValue*) Fail("invalid index (=",j+1,		\
			      "), sub-vector reference may be bad");

#define RANGEBUILTIN_ACTION(tag,type,max,src_elem)			\
case tag:								\
	{								\
	type min_val = (type) max;					\
	type max_val = (type) -max;					\
									\
	loop_over_list( *args_val, i )					\
		{							\
		const IValue* val = (*args_val)[i];			\
		int len = val->Length();				\
									\
		switch ( val->Type() )					\
			{						\
RANGEBUILTIN_ARG_ACTION(TYPE_DCOMPLEX,dcomplex,type,DcomplexPtr,j,src_elem,) \
RANGEBUILTIN_ARG_ACTION(TYPE_COMPLEX,complex,type,ComplexPtr,j,src_elem,)    \
RANGEBUILTIN_ARG_ACTION(TYPE_DOUBLE,double,type,DoublePtr,j,,)		\
RANGEBUILTIN_ARG_ACTION(TYPE_FLOAT,float,type,FloatPtr,j,,)		\
RANGEBUILTIN_ARG_ACTION(TYPE_BOOL,glish_bool,type,BoolPtr,j,,)		\
RANGEBUILTIN_ARG_ACTION(TYPE_BYTE,byte,type,BytePtr,j,,)		\
RANGEBUILTIN_ARG_ACTION(TYPE_SHORT,short,type,ShortPtr,j,,)		\
RANGEBUILTIN_ARG_ACTION(TYPE_INT,int,type,IntPtr,j,,)			\
			case TYPE_SUBVEC_REF:				\
				{					\
				VecRef* ref = val->VecRefPtr();		\
				IValue* val = (IValue*) ref->Val();	\
				int len = ref->Length();		\
									\
				switch ( val->Type() )			\
					{				\
RANGEBUILTIN_ARG_ACTION(TYPE_DCOMPLEX,dcomplex,type,DcomplexPtr,off,src_elem,RANGEBUILTIN_XLATE)\
RANGEBUILTIN_ARG_ACTION(TYPE_COMPLEX,complex,type,ComplexPtr,off,src_elem,RANGEBUILTIN_XLATE)	\
RANGEBUILTIN_ARG_ACTION(TYPE_DOUBLE,double,type,DoublePtr,off,,RANGEBUILTIN_XLATE)		\
RANGEBUILTIN_ARG_ACTION(TYPE_FLOAT,float,type,FloatPtr,off,,RANGEBUILTIN_XLATE)			\
RANGEBUILTIN_ARG_ACTION(TYPE_BOOL,glish_bool,type,BoolPtr,off,,RANGEBUILTIN_XLATE)		\
RANGEBUILTIN_ARG_ACTION(TYPE_BYTE,byte,type,BytePtr,off,,RANGEBUILTIN_XLATE)			\
RANGEBUILTIN_ARG_ACTION(TYPE_SHORT,short,type,ShortPtr,off,,RANGEBUILTIN_XLATE)			\
RANGEBUILTIN_ARG_ACTION(TYPE_INT,int,type,IntPtr,off,,RANGEBUILTIN_XLATE)			\
					default:			\
						return error_ivalue();	\
					}				\
				}					\
				break;					\
			default:					\
				return error_ivalue();			\
			}						\
									\
		}							\
	type* range = (type*) alloc_memory( sizeof(type)*2 );		\
	range[0] = min_val;						\
	range[1] = max_val;						\
									\
	result = new IValue( range, 2 );				\
	}								\
	break;

	switch ( max_type )
		{
		RANGEBUILTIN_ACTION(TYPE_DCOMPLEX,dcomplex,HUGE,)
		RANGEBUILTIN_ACTION(TYPE_COMPLEX,complex,MAXFLOAT,)
		RANGEBUILTIN_ACTION(TYPE_DOUBLE,double,HUGE,.r)
		RANGEBUILTIN_ACTION(TYPE_FLOAT,float,MAXFLOAT,.r)
		case TYPE_BOOL:
		case TYPE_BYTE:
		case TYPE_SHORT:
		RANGEBUILTIN_ACTION(TYPE_INT,int,MAXINT,.r)
		default:
			result = error_ivalue();
		}

	return result;
	}

IValue* SeqBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();

	if ( len == 0 || len > 3 )
		return (IValue*) Fail( this, " takes from one to three arguments" );

	double starting_point = 1.0;
	double stopping_point;
	double stride = 1.0;

	const IValue* arg;

	if ( len == 1 )
		{
		arg = (*args_val)[0];

		if ( arg->Length() > 1 )
			stopping_point = double( arg->Length() );
		else
			{
			Str err;
			stopping_point = double( arg->IntVal(1,err) );
			if ( err.chars() )
				return (IValue*) Fail( err.chars() );
			}
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
		return (IValue*) Fail( "in call to ", this, ", stride = 0" );

	if ( (starting_point < stopping_point && stride < 0) ||
	     (starting_point > stopping_point && stride > 0) )
		return (IValue*) Fail( "in call to ", this,
				", stride has incorrect sign" );

	double range = stopping_point - starting_point;
	int num_vals = int( range / stride ) + 1;

	if ( num_vals > 1e6 )
		return (IValue*) Fail( "ridiculously large sequence in call to ",
				this );

	double* result = (double*) alloc_memory( sizeof(double)*num_vals );

	double val = starting_point;
	for ( int i = 0; i < num_vals; ++i )
		{
		result[i] = val;
		val += stride;
		}

	IValue* result_val = new IValue( result, num_vals );

	if ( starting_point == double( int( starting_point ) ) &&
	     stopping_point == double( int( stopping_point ) ) &&
	     stride == double( int( stride ) )  )
		result_val->Polymorph( TYPE_INT );

	return result_val;
	}

IValue* RepBuiltIn::DoCall( const_args_list* args_val )
	{
	const IValue* element = (*args_val)[0];
	const IValue* times = (*args_val)[1];

	if ( ! times->IsNumeric() )
		return (IValue*) Fail( "non-numeric parameters invalid for", this );

	if ( times->Length() != 1 && times->Length() != element->Length() )
		return (IValue*) Fail( this,
				": parameter vectors have unequal lengths" );

	int times_is_copy;
	int times_len = times->Length();
	int* times_vec = times->CoerceToIntArray( times_is_copy, times_len );

	for ( int x = 0; x < times_len; ++x )
		if ( times_vec[x] < 0 )
			{
			if ( times_is_copy )
				free_memory( times_vec );
			return (IValue*) Fail( "invalid replication parameter, 2nd (",
					times_vec[x], "), in ", this );
			}

	IValue* ret = 0;
	if ( times_len > 1 )
		{
		// Here we know that BOTH the length of the element and the
		// length of the multiplier are greater than zero.
		int off = 0;
		int veclen = 0;

		for ( int i = 0; i < times_len; ++i )
			veclen += times_vec[i];

		switch ( element->Type() )
			{
#define REPBUILTIN_ACTION_A(tag,type,accessor,copy_func)			\
	case tag:								\
		{								\
		type* vec = (type*) alloc_memory( sizeof(type)*veclen );	\
		type* elm = element->accessor(0);				\
		for ( LOOPDECL i=0; i < times_len; ++i )			\
			for ( int j=0; j < times_vec[i]; ++j )			\
			  vec[off++] = copy_func( elm[i] );			\
		ret = new IValue( vec, veclen );				\
		}								\
		break;

		REPBUILTIN_ACTION_A(TYPE_BOOL,glish_bool,BoolPtr,)
		REPBUILTIN_ACTION_A(TYPE_BYTE,byte,BytePtr,)
		REPBUILTIN_ACTION_A(TYPE_SHORT,short,ShortPtr,)
		REPBUILTIN_ACTION_A(TYPE_INT,int,IntPtr,)
		REPBUILTIN_ACTION_A(TYPE_FLOAT,float,FloatPtr,)
		REPBUILTIN_ACTION_A(TYPE_DOUBLE,double,DoublePtr,)
		REPBUILTIN_ACTION_A(TYPE_COMPLEX,complex,ComplexPtr,)
		REPBUILTIN_ACTION_A(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,)
		REPBUILTIN_ACTION_A(TYPE_STRING,charptr,StringPtr,strdup)
		REPBUILTIN_ACTION_A(TYPE_REGEX,regexptr,RegexPtr,new Regex)

			default:
				fatal->Report(
					"bad type in RepBuiltIn::DoCall()" );
			}
		}
	else
		{
		int len = times_vec[0];

		if ( element->Length() == 1 )
			{
			switch ( element->Type() )
				{
#define REPBUILTIN_ACTION_B(tag,type,accessor,copy_func,CLEANUP_VAL)	\
	case tag:							\
		{							\
		type val = element->accessor();				\
		type *vec = (type*) alloc_memory( sizeof(type)*len );	\
		for (int i = 0; i < len; i++)				\
			vec[i] = copy_func(val);			\
		ret = new IValue( vec, len );				\
		CLEANUP_VAL						\
		}							\
		break;

			REPBUILTIN_ACTION_B(TYPE_BOOL,glish_bool,BoolVal,,)
			REPBUILTIN_ACTION_B(TYPE_BYTE,byte,ByteVal,,)
			REPBUILTIN_ACTION_B(TYPE_SHORT,short,ShortVal,,)
			REPBUILTIN_ACTION_B(TYPE_INT,int,IntVal,,)
			REPBUILTIN_ACTION_B(TYPE_FLOAT,float,FloatVal,,)
			REPBUILTIN_ACTION_B(TYPE_DOUBLE,double,DoubleVal,,)
			REPBUILTIN_ACTION_B(TYPE_COMPLEX,complex,ComplexVal,,)
			REPBUILTIN_ACTION_B(TYPE_DCOMPLEX,dcomplex,DcomplexVal,,)
			REPBUILTIN_ACTION_B(TYPE_STRING,charptr,StringVal,strdup,free_memory((void*)val);)
			REPBUILTIN_ACTION_B(TYPE_REGEX,regexptr,RegexVal,new Regex,)

				default:
					fatal->Report(
					"bad type in RepBuiltIn::DoCall()" );
				}
			}
		else
			{
			int off = 0;
			int repl = times_vec[0];
			int e_len = element->Length();
			int veclen = e_len * repl;

			switch ( element->Type() )
				{
#define REPBUILTIN_ACTION_C(tag,type,accessor,copy_func)		\
	case tag:							\
		{							\
		type* val = element->accessor(0);			\
		type* vec = (type*) alloc_memory( sizeof(type)*veclen );\
		for ( int j = 0; j < repl; ++j )			\
			for ( int i = 0; i < e_len; ++i )		\
				vec[off++] =  copy_func(val[i]);	\
		ret = new IValue( vec, veclen );			\
		}							\
		break;

	REPBUILTIN_ACTION_C(TYPE_BOOL,glish_bool,BoolPtr,)
	REPBUILTIN_ACTION_C(TYPE_BYTE,byte,BytePtr,)
	REPBUILTIN_ACTION_C(TYPE_SHORT,short,ShortPtr,)
	REPBUILTIN_ACTION_C(TYPE_INT,int,IntPtr,)
	REPBUILTIN_ACTION_C(TYPE_FLOAT,float,FloatPtr,)
	REPBUILTIN_ACTION_C(TYPE_DOUBLE,double,DoublePtr,)
	REPBUILTIN_ACTION_C(TYPE_COMPLEX,complex,ComplexPtr,)
	REPBUILTIN_ACTION_C(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,)
	REPBUILTIN_ACTION_C(TYPE_STRING,charptr,StringPtr,strdup)
	REPBUILTIN_ACTION_C(TYPE_REGEX,regexptr,RegexPtr,new Regex)

				default:
					fatal->Report(
					"bad type in RepBuiltIn::DoCall()" );
				}
			}
		}

	if ( times_is_copy )
		free_memory( times_vec );

	return ret ? ret : error_ivalue();
	}

IValue* NumArgsBuiltIn::DoCall( const_args_list* args_val )
	{
	return new IValue( args_val->length() );
	}

IValue* NthArgBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();
	Str err;

	if ( len <= 0 )
		return (IValue*) Fail( "first argument missing in call to", this );

	int n = (*args_val)[0]->IntVal(1,err);
	if ( err.chars() )
		return (IValue*) Fail(err.chars());

	if ( n < 0 || n >= len )
		return (IValue*) Fail( "first argument (=", n, ") to", this,
				" out of range: ", len - 1,
				"additional arguments supplied" );

	return copy_value( (*args_val)[n] );
	}

IValue* RandomBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();
	const IValue *val = 0;
	int arg1 = 0;
	int arg2 = 0;
	Str err;

	if ( len > 2 )
		return (IValue*) Fail( this, " takes from zero to two arguments" );

	if ( len >= 1 )
		{
		val = (*args_val)[0];

		if ( ! val->IsNumeric() )
			return (IValue*) Fail( "non-numeric parameter invalid for",
						this );

		arg1 = val->IntVal(1,err);
		if ( err.chars() )
			return (IValue*) Fail(err.chars());
		}

	if ( len == 2 ) 
		{
		val = (*args_val)[1];

		if ( ! val->IsNumeric() )
			return (IValue*) Fail( "non-numeric parameter invalid for",
						this );

		arg2 = val->IntVal(1,err);
		if ( err.chars() )
			return (IValue*) Fail(err.chars());

		if ( arg1 > arg2 )
			{
			int tmp = arg1;
			arg1 = arg2;
			arg2 = tmp;
			}
		}

	IValue *ret = 0;
	if ( len <= 1 )
		{
		if ( arg1 < 1 )
			ret = new IValue( (int) random_long() );
		else
			{
			int *ival = (int*) alloc_memory( sizeof(int)*arg1 );
			for ( int i = arg1 - 1; i >= 0; i-- )
				ival[i] = (int) random_long();

			ret = new IValue( ival, arg1 );
			}
		}
	else
		{
		int diff = arg2 - arg1;
		if ( diff <= 0 )
			return (IValue*) Fail( "invalid range for",
						this );

		ret =  new IValue( (int)((unsigned long)random_long() % 
						(diff+1)) + arg1 );
		}

	return ret;
	}

#define XBIND_MIXTYPE_ERROR						\
	{								\
	return (IValue*) Fail( "both numeric and non-numeric arguments" );\
	return error_ivalue();						\
	}

#define XBIND_CLEANUP							\
	if ( shape_is_copy )						\
		free_memory( shape );

#define XBIND_ALLOC_PTR(tag, type)					\
	case tag:							\
		result = alloc_memory( sizeof(type)*(cols*rows) );	\
		break;

#define XBIND_PLACE_ACTION(tag,type,array,to,from,access,conv)		\
	case tag:							\
		((type*)result)[to]  = (type)( array[from] access conv );\
		break;

#define XBIND_PLACE_ELEMENT(array,to,from,access)			\
	switch ( result_type )						\
		{							\
	XBIND_PLACE_ACTION(TYPE_BOOL,glish_bool,array,to,from,access,	\
			      ? glish_true : glish_false )		\
	XBIND_PLACE_ACTION(TYPE_INT,int,array,to,from,access,)		\
	XBIND_PLACE_ACTION(TYPE_BYTE,byte,array,to,from,access,)	\
	XBIND_PLACE_ACTION(TYPE_SHORT,short,array,to,from,access,)	\
	XBIND_PLACE_ACTION(TYPE_FLOAT,float,array,to,from,access,)	\
	XBIND_PLACE_ACTION(TYPE_DOUBLE,double,array,to,from,access,)	\
	XBIND_PLACE_ACTION(TYPE_COMPLEX,complex,array,to,from,,)	\
	XBIND_PLACE_ACTION(TYPE_DCOMPLEX,dcomplex,array,to,from,,)	\
		default:						\
		 fatal->Report( "bad type in CbindBuiltIn::DoCall()" );	\
		}

#define XBIND_ACTION(tag,ptr_name,source,OFFSET,XLATE,access,stride,COLS,OFF,ADV1,ADV2)	\
	case tag:							\
		{							\
		ptr_name = arg->VecRefDeref()->source();		\
		int off = offset;					\
		if (  attr && (shape_v = (const IValue*) ((*attr)["shape"])) &&	\
		      shape_v != false_value &&	shape_v->IsNumeric() &&	\
		      (shape_len = shape_v->Length()) > 1 )		\
			{						\
			int* shape = shape_v->CoerceToIntArray( 	\
				shape_is_copy, shape_len );		\
			for (int i = 0; i < rows; i++, off += stride - OFF)\
				for (int j = 0; j < shape[COLS]; j++, off++)\
					{				\
					int vecoff = i*shape[COLS]+j;	\
					XLATE				\
			XBIND_PLACE_ELEMENT(ptr_name,off,OFFSET,access)	\
					}				\
			XBIND_CLEANUP					\
			offset = ADV1;					\
			}						\
		else							\
			{						\
			for ( int vecoff = 0; vecoff < rows;		\
					vecoff++, off += stride )	\
				{					\
				XLATE					\
			XBIND_PLACE_ELEMENT(ptr_name,off,OFFSET,access)	\
				}					\
			offset = ADV2;					\
			}						\
		}							\
		break;

#define XBIND_ACTIONS(index,xlate,stride,COLS,OFF,ADV1,ADV2)		\
XBIND_ACTION(TYPE_INT,int_ptr,IntPtr,index,xlate,,stride,COLS,OFF,ADV1,ADV2)\
XBIND_ACTION(TYPE_BYTE,byte_ptr,BytePtr,index,xlate,,stride,COLS,OFF,ADV1,ADV2)\
XBIND_ACTION(TYPE_BOOL,bool_ptr,BoolPtr,index,xlate,,stride,COLS,OFF,ADV1,ADV2)\
XBIND_ACTION(TYPE_SHORT,short_ptr,ShortPtr,index,xlate,,stride,COLS,OFF,ADV1,ADV2)\
XBIND_ACTION(TYPE_FLOAT,float_ptr,FloatPtr,index,xlate,,stride,COLS,OFF,ADV1,ADV2)\
XBIND_ACTION(TYPE_DOUBLE,double_ptr,DoublePtr,index,xlate,,stride,COLS,OFF,ADV1,ADV2)\
XBIND_ACTION(TYPE_COMPLEX,complex_ptr,ComplexPtr,index,xlate,.r,stride,COLS,OFF,ADV1,ADV2)\
XBIND_ACTION(TYPE_DCOMPLEX,dcomplex_ptr,DcomplexPtr,index,xlate,.r,stride,COLS,OFF,ADV1,ADV2)

#define XBIND_XLATE							\
	int index = ref->TranslateIndex( vecoff, &err );		\
	if ( err )							\
		return (IValue*) Fail( "invalid sub-vector" );

#define XBIND_RETURN_ACTION(tag,type)					\
	case tag:							\
		result_value = new IValue((type*)result,rows*cols);	\
		break;

#define XBINDBUILTIN(name,ROWS,COLS,stride,OFF,ADV1,ADV2)		\
IValue* name::DoCall( const_args_list* args_vals )			\
	{								\
	int numeric = -1, rows = -1, minrows = -1;			\
	int cols = 0;							\
	glish_type result_type = TYPE_BOOL;				\
									\
	if ( args_vals->length() < 2 )					\
		return (IValue*) Fail(this, " takes at least two arguments");\
									\
	loop_over_list( *args_vals, i )					\
		{							\
		const IValue *arg = (*args_vals)[i];			\
		int arg_len = arg->Length();				\
		const attributeptr attr = arg->AttributePtr();		\
		const IValue *shape_v;					\
		int shape_len;						\
		int shape_is_copy;					\
									\
		if ( arg->IsNumeric() )					\
			if ( numeric == 0 )				\
				XBIND_MIXTYPE_ERROR			\
			else						\
				{					\
				numeric = 1;				\
				result_type =				\
			max_numeric_type(result_type,			\
					 arg->VecRefDeref()->Type());	\
				}					\
		else if ( arg->VecRefDeref()->Type() == TYPE_STRING )	\
			if ( numeric == 1 )				\
				XBIND_MIXTYPE_ERROR			\
			else						\
				{					\
				numeric = 0;				\
				result_type = TYPE_STRING;		\
				}					\
		else							\
			return (IValue*) Fail("invalid type (argument ",i+1,")");\
									\
		if (  attr && (shape_v = (const IValue*)((*attr)["shape"])) &&	\
		      shape_v != false_value && shape_v->IsNumeric() &&	\
		      (shape_len = shape_v->Length()) > 1 )		\
			{						\
			if ( shape_len > 2 )				\
				return (IValue*) Fail( "argument (",i+1,\
				  ") with dimensionality greater than 2" );\
									\
			int* shape =					\
				shape_v->CoerceToIntArray( shape_is_copy,\
							   shape_len );	\
									\
			cols += shape[COLS];				\
			if ( rows >= 0 )				\
				{					\
				if ( shape[ROWS] != rows || 		\
				     (minrows >= 0 && minrows < rows ) )\
					{				\
					XBIND_CLEANUP			\
					return (IValue*) Fail( 		\
					"mismatch in number of rows" );	\
					}				\
				}					\
			else						\
				rows = shape[ROWS];			\
			XBIND_CLEANUP					\
			}						\
		else							\
			{						\
			cols += 1;					\
			if ( minrows < 0 || minrows > arg_len )		\
				minrows = arg_len;			\
			if ( rows >= 0 && minrows < rows )		\
				return (IValue*) Fail( 			\
					"mismatch in number of rows" );	\
			}						\
		}							\
									\
	if ( rows < 0 )							\
		rows = minrows;						\
									\
	void *result;							\
	IValue *result_value = 0;					\
	if ( result_type == TYPE_STRING )				\
		return (IValue*) Fail("sorry not implemented for strings yet");	\
									\
	switch ( result_type )						\
		{							\
		XBIND_ALLOC_PTR(TYPE_BOOL,glish_bool)			\
		XBIND_ALLOC_PTR(TYPE_INT,int)				\
		XBIND_ALLOC_PTR(TYPE_BYTE,byte)				\
		XBIND_ALLOC_PTR(TYPE_SHORT,short)			\
		XBIND_ALLOC_PTR(TYPE_FLOAT,float)			\
		XBIND_ALLOC_PTR(TYPE_DOUBLE,double)			\
		XBIND_ALLOC_PTR(TYPE_COMPLEX,complex)			\
		XBIND_ALLOC_PTR(TYPE_DCOMPLEX,dcomplex)			\
		default:						\
		fatal->Report( "bad type in CbindBuiltIn::DoCall" );	\
		}							\
									\
	int offset = 0;							\
	loop_over_list( *args_vals, x )					\
		{							\
		const IValue *arg = (*args_vals)[x];			\
		const attributeptr attr = arg->AttributePtr();		\
		const IValue *shape_v;					\
		int shape_len;						\
		int shape_is_copy;					\
									\
		glish_bool* bool_ptr;					\
		byte* byte_ptr;						\
		short* short_ptr;					\
		int* int_ptr;						\
		float* float_ptr;					\
		double* double_ptr;					\
		complex* complex_ptr;					\
		dcomplex* dcomplex_ptr;					\
									\
		switch ( arg->Type() )					\
			{						\
	XBIND_ACTIONS(vecoff,,stride,COLS,OFF,ADV1,ADV2)		\
			case TYPE_SUBVEC_REF:				\
				{					\
				const VecRef* ref = arg->VecRefPtr();	\
				int err;				\
				switch ( ref->Type() )			\
					{				\
	XBIND_ACTIONS(index,XBIND_XLATE,stride,COLS,OFF,ADV1,ADV2)	\
					default:			\
					fatal->Report(			\
				"bad type in CbindBuiltIn::DoCall()" );	\
						}			\
				}					\
				break;					\
									\
			default:					\
			fatal->Report("bad type in CbindBuiltIn::DoCall()");\
			}						\
		}							\
									\
	switch ( result_type )						\
		{							\
		XBIND_RETURN_ACTION(TYPE_BOOL,glish_bool)		\
		XBIND_RETURN_ACTION(TYPE_INT,int)			\
		XBIND_RETURN_ACTION(TYPE_BYTE,byte)			\
		XBIND_RETURN_ACTION(TYPE_SHORT,short)			\
		XBIND_RETURN_ACTION(TYPE_FLOAT,float)			\
		XBIND_RETURN_ACTION(TYPE_DOUBLE,double)			\
		XBIND_RETURN_ACTION(TYPE_COMPLEX,complex)		\
		XBIND_RETURN_ACTION(TYPE_DCOMPLEX,dcomplex)		\
		default:						\
		fatal->Report( "bad type in CbindBuiltIn::DoCall" );	\
		}							\
									\
	if ( result_value )						\
		{							\
		int *newshape = (int*) alloc_memory( sizeof(int)*2 );	\
		newshape[ROWS] = rows;					\
		newshape[COLS] = cols;					\
		result_value->AssignAttribute( "shape", 		\
					       new IValue(newshape,2) );	\
		return result_value;					\
		}							\
									\
	return error_ivalue();						\
	}

XBINDBUILTIN(CbindBuiltIn,0,1,1,1,off,off)
XBINDBUILTIN(RbindBuiltIn,1,0,cols,shape[0],offset+shape[0],offset+1)


IValue* IsConstBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();

	if ( len != 1 )
		return (IValue*) Fail( "is_const() takes only one argument" );

	return new IValue( (*args_val)[0]->IsConst() ? glish_true : glish_false );
	}

IValue* IsModifiableBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();

	if ( len != 1 )
		return (IValue*) Fail( "is_const() takes only one argument" );

	const IValue *val = (*args_val)[0];
	return new IValue( ! val->IsConst() && ! val->IsModConst() ? glish_true : glish_false );
	}

IValue* PasteBuiltIn::DoCall( const_args_list* args_val )
	{
	if ( args_val->length() == 0 )
		return (IValue*) Fail( "paste() invoked with no arguments" );

	// First argument gives separator string.
	char* separator = (*args_val)[0]->StringVal();

	charptr* string_vals = 0;

	int len = 1;	// Room for end-of-string.
	int sep_len = strlen( separator );

	int i = 1;
	if ( args_val->length() != 2 || (*args_val)[1]->VecRefDeref()->Type() != TYPE_STRING )
		{
		string_vals = (charptr*) alloc_memory( sizeof(charptr)*args_val->length() );
		for ( ; i < args_val->length(); ++i )
			{
			unsigned int limit = (*args_val)[i]->PrintLimit();
			string_vals[i] = (*args_val)[i]->StringVal( ' ', limit, 1 );
			len += strlen( string_vals[i] ) + sep_len;
			}
		}
	else
		{
		const IValue *val = (IValue*)((*args_val)[1]->Deref());
		if ( val->Type() == TYPE_STRING )
			{
			charptr *strs = val->StringPtr(0);
			int xlen = val->Length();
			string_vals = (charptr*) alloc_memory( sizeof(charptr)*(xlen+1) );
			for ( ; i < xlen+1; ++i )
				{
				string_vals[i] = strdup(strs[i-1]);
				len += strlen( string_vals[i] ) + sep_len;
				}
			}
		else if ( val->Type ( ) == TYPE_SUBVEC_REF )
			{
			VecRef* ref = val->VecRefPtr();
			int xlen = ref->Length();
			string_vals = (charptr*) alloc_memory( sizeof(charptr)*(xlen+1) );
			IValue* theVal = (IValue*) ref->Val();
			charptr *strs = theVal->StringPtr(0);
			int err = 0;
			for ( ; i < xlen+1; ++i )
				{
				int off = ref->TranslateIndex( i-1, &err );
				if ( err ) return (IValue*) Fail("paste");
				string_vals[i] = strdup(strs[off]);
				len += strlen( string_vals[i] ) + sep_len;
				}
			}

			
		}

	char* paste_val = (char*) alloc_memory( sizeof(char)*(len+1) );
	paste_val[0] = '\0';

	for ( int j = 1; j < i; ++j )
		{
		strcat( paste_val, string_vals[j] );

		if ( j < i - 1 )
			strcat( paste_val, separator );

		free_memory( (void*) string_vals[j] );
		}

	free_memory( string_vals );
	free_memory( separator );

	charptr* result = (charptr*) alloc_memory( sizeof(charptr) );
	result[0] = paste_val;

	return new IValue( result, 1 );
	}

IValue* SplitBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();

	if ( len < 1 || len > 2 )
		return (IValue*) Fail( this, " takes 1 or 2 arguments" );

	char* source = (*args_val)[0]->StringVal();

	char* split_chars = " \t\n";
	if ( len == 2 )
		split_chars = (*args_val)[1]->StringVal();

	IValue* result = isplit( source, split_chars );

	free_memory( source );
	if ( len == 2 )
		free_memory( split_chars );

	return result;
	}


IValue* SizeofBuiltIn::DoCall( const_args_list* args_val )
	{
	const Value* val = (*args_val)[0];
	return new IValue( val->Sizeof() );
	}

IValue* AllocInfoBuiltIn::DoCall( const_args_list* )
	{
	struct mallinfo info = mallinfo();
	recordptr rec = create_record_dict();
	rec->Insert(strdup("used"),new IValue(info.uordblks));
	rec->Insert(strdup("unused"),new IValue(info.fordblks));
	return new IValue( rec );
	}

IValue* IsNaNBuiltIn::DoCall( const_args_list* args_val )
	{
	const Value* val = (*args_val)[0];
	glish_type type;

	if ( val && ((type = val->Type()) == TYPE_FLOAT || 
		     type == TYPE_DOUBLE ||
		     type == TYPE_COMPLEX ||
		     type == TYPE_DCOMPLEX ))
		{
		int len = val->Length();
		if ( len > 1 )
			{
			glish_bool *ret = (glish_bool*) alloc_memory( sizeof(glish_bool)*len );
			switch( type )
				{
#define ISNAN_ACTION(tag,type,accessor,extra) 		\
case tag:						\
	{						\
	type *v = val->accessor();			\
	for (int i = 0; i < len; i++)			\
		ret[i] = ( is_a_nan( v[i] extra ) ) ? glish_true : glish_false; \
	}						\
	break;
#define ISNAN_ACTION_CPX_EXTRA  .r ) || ( is_a_nan( v[i].i )

				ISNAN_ACTION(TYPE_FLOAT,float,FloatPtr,)
				ISNAN_ACTION(TYPE_DOUBLE,double,DoublePtr,)
				ISNAN_ACTION(TYPE_COMPLEX,complex,ComplexPtr, ISNAN_ACTION_CPX_EXTRA)
				ISNAN_ACTION(TYPE_DCOMPLEX,dcomplex,DcomplexPtr, ISNAN_ACTION_CPX_EXTRA)
				default: break;
				}

			return new IValue(ret, len);
			}
		else
			{
			if ( type == TYPE_FLOAT || type == TYPE_DOUBLE )
				{
				double nv = val->DoubleVal();
				return new IValue( ( is_a_nan(nv) ) ? 
						   glish_true : glish_false );
				}
			else
				{
				dcomplex nv = val->DcomplexVal();
				return new IValue( ( is_a_nan(nv.r) || is_a_nan(nv.i) ) ?
						   glish_true : glish_false );
				}
			}
		}

	return new IValue( glish_false );
	}

IValue* ReadValueBuiltIn::DoCall( const_args_list* args_val )
	{
	static sos_fd_source FD;
	static sos_in sos( &FD );

	char* filename = (*args_val)[0]->StringVal();
	IValue* result;
	int fd = open(filename,O_RDONLY,0644);

	FD.setFd( fd );

	if ( fd < 0 )
		result = (IValue*) Fail( "could not read value from \"", filename, "\"" );
	else
		result = (IValue*) read_value( sos );

	close(fd);
	free_memory( filename );

	return result;
	}


IValue* WriteValueBuiltIn::DoCall( const_args_list* args_val )
	{
	static sos_fd_sink FD;
	static sos_out sos( &FD );

	if ( (*args_val)[1]->Type() != TYPE_STRING )
		return (IValue*) Fail( "bad type for filename (argument 2), string expected." );

	char* filename = (*args_val)[1]->StringVal();
	const IValue* v = (*args_val)[0];
	IValue *result = 0;

	int fd = open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);

	FD.setFd( fd );

	if ( fd < 0 )
		result = (IValue*) Fail( "could not write value to \"", filename, "\"" );
	else
		write_value( sos, v );

	close(fd);
	free_memory( filename );

	return result ? result : new IValue( glish_true );
	}

IValue* WheneverStmtsBuiltIn::DoCall( const_args_list* args_val )
	{
	Agent* agent = (*args_val)[0]->AgentVal();

	if ( ! agent )
		return (IValue*) Fail("no agent for ", this);
	else
		return agent->AssociatedStatements();
	}

IValue* WheneverActiveBuiltIn::DoCall( const_args_list* args_val )
	{
	const IValue* v = (*args_val)[0];

	if ( ! v->IsNumeric() )
		return (IValue*) Fail("non-numeric argument, ", v );

	int  index = v->IntVal();
	Stmt *s = sequencer->LookupStmt( index );

	if ( ! s )
		return (IValue*) Fail(index, "does not designate a valid \"whenever\" statement" );

	return new IValue ( s->GetActivity() ? glish_true : glish_false );
	}


IValue* ActiveAgentsBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	IValue* r = create_irecord();

	loop_over_list( (*agents), i )
		{
		IValue* a = (*agents)[i]->AgentRecord();
		if ( a )
			{
			IValue* a_ref = new IValue( a, VAL_REF );
			r->SetField( r->NewFieldName(), a_ref );
			Unref( a_ref );
			}
		}

	return r;
	}

IValue* TimeBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	IValue *ret = new IValue((double)0.0);
	(ret->DoublePtr())[0] = get_current_time();
	return ret;
	}


IValue* CreateAgentBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	Agent* user_agent = new UserAgent( sequencer );
	return user_agent->AgentRecord();
	}


#ifdef GLISHTK
IValue* CreateGraphicBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();

	const IValue* arg = (*args_val)[0];

	if ( len < 1 )
		return (IValue*) Fail( this, " requires at least one argument");

	if ( arg->Type() != TYPE_STRING )
		return (IValue*) Fail( this, " requires a string as the first argument");

	const char *type = arg->StringPtr(0)[0];
	IValue *agent = 0;

	if ( type[0] == 'f' && ! strcmp( type, "frame" ) )
		agent = TkFrame::Create( sequencer, args_val );
	else if ( type[0] == 'b' && ! strcmp( type, "button" ) )
		agent = TkButton::Create( sequencer, args_val );
	else if ( type[0] == 's' && ! strcmp( type, "scrollbar" ) )
		agent = TkScrollbar::Create( sequencer, args_val );
	else if ( type[0] == 's' && ! strcmp( type, "scale" ) )
		agent = TkScale::Create( sequencer, args_val );
	else if ( type[0] == 't' && ! strcmp( type, "text" ) )
		agent = TkText::Create( sequencer, args_val );
	else if ( type[0] == 'l' && ! strcmp( type, "label" ) )
		agent = TkLabel::Create( sequencer, args_val );
	else if ( type[0] == 'e' && ! strcmp( type, "entry" ) )
		agent = TkEntry::Create( sequencer, args_val );
	else if ( type[0] == 'm' && ! strcmp( type, "message" ) )
		agent = TkMessage::Create( sequencer, args_val );
	else if ( type[0] == 'l' && ! strcmp( type, "listbox" ) )
		agent = TkListbox::Create( sequencer, args_val );
	else if ( type[0] == 'c' && ! strcmp( type, "canvas" ) )
		agent = TkCanvas::Create( sequencer, args_val );
#ifdef TKPGPLOT
	else if ( type[0] == 'p' && ! strcmp( type, "pgplot" ) )
		agent = TkPgplot::Create( sequencer, args_val );
#else
	else if ( type[0] == 'p' && ! strcmp( type, "pgplot" ) )
		return (IValue*) Fail("This Glish was not configured for PGPLOT");
#endif

	return agent ? agent : error_ivalue();
	}

IValue* TkBuiltIns::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();

	if ( args_val->length() != 1 )
		return (IValue*) Fail( this, " requires one argument");

	const IValue* arg = (*args_val)[0];
	if ( arg->Type() != TYPE_INT )
		return (IValue*) Fail( this, " requires one integer argument");

	IValue *ret = 0;
	switch( arg->IntVal() )
		{
		case 1:
			ret = new IValue( TkHaveGui() ? glish_true : glish_false );
			break;
		case 2:
			TkAgent::HoldEvents();
			break;
		case 3:
			TkAgent::ReleaseEvents();
			break;
		default:
			ret = new IValue( glish_false );
		}

	return ret ? ret : new IValue( glish_true );
	}
#else
IValue* CreateGraphicBuiltIn::DoCall( const_args_list* )
	{ return (IValue*) Fail("This Glish was not configured for graphic clients"); }
IValue* TkBuiltIns::DoCall( const_args_list* )
	{ return new IValue( glish_false ); }
#endif

IValue* SymbolNamesBuiltIn::DoCall( const_args_list *args_val )
	{
	int len = args_val->length();
	Scope *scope = sequencer->GetScope( );
	if ( ! scope || ! scope->Length() )
		return error_ivalue();

	if ( len > 1 )
		return (IValue*) Fail( this, " takes 0 or 1 argument" );

	const IValue *func_val = len > 0 ? (*args_val)[0] : 0 ;
	funcptr func = 0;

	if ( func_val )
 		if ( func_val->Type() != TYPE_FUNC )
			return (IValue*) Fail( this, " only takes a function as an argument");
		else
			func = func_val->FuncVal();

	int cnt = 0;
	charptr *name_ary = (charptr*) alloc_memory( sizeof(charptr)*scope->Length() );
	IterCookie *c = scope->InitForIteration();
	const Expr *member;
	const char *key;
	while ( (member = scope->NextEntry( key, c )) )
		{
		if ( key && key[0] == '*' && key[strlen(key)-1] == '*' )
			continue;

		if ( member && ((VarExpr*)member)->Access() == USE_ACCESS )
			{
			int flag = 0;
			if ( func )
				{
				parameter_list p;
				Parameter arg( VAL_CONST, (Expr*) member ); Ref( (Expr*) member );
				p.append( &arg );
				IValue *r = func->Call( &p, EVAL_COPY );
				if ( r && r->IsNumeric() )
					flag = r->IntVal();
				Unref( r );
				}
			if ( ! func || flag )
				{
				name_ary[cnt] = (char*) alloc_memory( sizeof(char)*
								      (strlen(key)+1) );
				strcpy((char*) name_ary[cnt++], key);
				}
			}
		}

	return new IValue( (charptr*) name_ary, cnt );
	}

IValue* SymbolValueBuiltIn::DoCall( const_args_list *args_val )
	{
	const IValue *str = (*args_val)[0];

	if ( ! str || str->Type() != TYPE_STRING )
		return (IValue*) Fail( this, " takes 1 string argument" );

	IValue *ret = 0;
	charptr *strs = str->StringPtr(0);

	if ( str->Length() > 1 )
		{
		recordptr rptr = create_record_dict();
		for ( int i = 0; i < str->Length(); i++ )
			{
			Expr *exp = sequencer->LookupID( strdup(strs[i]), GLOBAL_SCOPE, 0, 0);
			if ( exp && ((VarExpr*)exp)->Access() == USE_ACCESS )
				{
				IValue *val = exp->CopyEval();
				if ( val )
					rptr->Insert( strdup(strs[i]), val );
				}
			}
		ret = new IValue( rptr );
		}
	else
		{
		Expr *exp = sequencer->LookupID( strdup(strs[0]), GLOBAL_SCOPE, 0, 0);
		if ( exp && ((VarExpr*)exp)->Access() == USE_ACCESS )
			ret = exp->CopyEval();
		}

	return ret ? ret : empty_ivalue();
	}

IValue* SymbolSetBuiltIn::DoCall( const_args_list *args_val )
	{
	int len = args_val->length();

	if ( len < 1 || len > 2 )
		return (IValue*) Fail( this, " takes either 1 record argument or a string and a value" );

	const IValue *arg1 = (*args_val)[0];
	const IValue *arg2 = len > 1 ? (*args_val)[1] : 0;

	if ( ! arg2 )
		{
		if ( arg1->Type() != TYPE_RECORD )
			return (IValue*) Fail( "wrong type for argument 1, record expected" );

		recordptr rptr = arg1->RecordPtr(0);
		IterCookie *c = rptr->InitForIteration();
		IValue *member;
		const char *key;
		while ( (member = (IValue*)(rptr->NextEntry( key, c ))) )
			{
			Expr *id = sequencer->LookupID( strdup(key), GLOBAL_SCOPE, 1, 0 );
			id->Assign( copy_value(member) );
			}
		}
	else
		{
		if ( arg1->Type() != TYPE_STRING )
			return (IValue*) Fail( "wrong type for argument 1, string expected" );

		charptr *strs = arg1->StringPtr(0);
		Expr *id = sequencer->LookupID( strdup(strs[0]), GLOBAL_SCOPE, 1, 0 );
		id->Assign( copy_value( arg2 ) );
		}

	return new IValue( glish_true );
	}

IValue* SymbolDeleteBuiltIn::DoCall( const_args_list *args_val )
	{
	const IValue *str = (*args_val)[0];

	if ( ! str || str->Type() != TYPE_STRING )
		return (IValue*) Fail( this, " takes 1 string argument" );

	charptr *strs = str->StringPtr(0);

	for ( int i = 0; i < str->Length(); i++ )
		sequencer->DeleteVal( strs[i] );

	return new IValue( glish_true );
	}

IValue* MissingBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	Frame* cur = sequencer->CurrentFrame();
	if ( ! cur )
		return empty_ivalue();

	return copy_value( cur->Missing() );
	}

#ifdef GGC
IValue* GarbageBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	sequencer->CollectGarbage( );
	return new IValue( glish_true );
	}
#endif

IValue* CurrentWheneverBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	Notification* n = sequencer->LastNotification();

	if ( ! n )
		return (IValue*) Fail( "no active whenever, in call to", this );

	return new IValue( n->notifiee->stmt()->Index() );
	}

IValue* EvalBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();
	IValue *result = 0;
	if ( len )
		{
		char **lines = (char**) alloc_memory( sizeof(char*)*(len+1) );

		loop_over_list( *args_val, i )
			lines[i] = (*args_val)[i]->StringVal();
		
		lines[len] = 0;

		result = sequencer->Eval( (const char **) lines );

		for ( int j = 0; j < len; j++ )
			free_memory( lines[j] );

		free_memory( lines );
		}

	return result ? result : empty_ivalue();
	}

IValue* LastWheneverExecutedBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	Stmt* s = sequencer->LastWheneverExecuted();

	if ( ! s )
		return (IValue*) Fail( "no whenever's executed, in call to", this );

	return new IValue( s->Index() );
	}


#define DEFINE_AS_XXX_BUILT_IN(name,type,tag,stringcvt,coercer,text,zero,ENTER,EXIT) \
IValue* name( const IValue* arg )					\
	{								\
	int len = arg->Length();					\
									\
	if ( arg->Type() == TYPE_STRING )				\
		{							\
		const charptr* strings = arg->StringPtr(0);		\
		type* result = (type*) alloc_memory( sizeof(type)*len );\
									\
		for ( int i = 0; i < len; ++i )				\
			result[i] = stringcvt( strings[i] );		\
									\
		return new IValue( result, len );			\
		}							\
									\
	if ( ! arg->IsNumeric() )					\
		return (IValue*) Fail( "non-numeric argument to ", text );\
									\
	if ( arg->Type() == tag )					\
		return copy_value( arg );				\
									\
	int is_copy;							\
	ENTER								\
	type* result = arg->coercer( is_copy, len );			\
	EXIT								\
									\
	IValue* ret = new IValue( result, len );			\
	ret->CopyAttributes( arg );					\
	return ret;							\
	}

inline glish_bool string_to_bool( const char* string )
	{ return *string ? glish_true : glish_false; }

DEFINE_AS_XXX_BUILT_IN(as_boolean_built_in, glish_bool, TYPE_BOOL,
	string_to_bool, CoerceToBoolArray, "as_boolean", glish_false,,)

DEFINE_AS_XXX_BUILT_IN(as_short_built_in, short, TYPE_SHORT, atoi,
	CoerceToShortArray, "as_short", 0,glish_fpe_enter();,glish_fpe_exit();)

DEFINE_AS_XXX_BUILT_IN(as_integer_built_in, int, TYPE_INT, atoi,
	CoerceToIntArray, "as_integer", 0,glish_fpe_enter();,glish_fpe_exit();)

DEFINE_AS_XXX_BUILT_IN(as_float_built_in, float, TYPE_FLOAT, atof,
	CoerceToFloatArray, "as_float", 0.0,,)

DEFINE_AS_XXX_BUILT_IN(as_double_built_in, double, TYPE_DOUBLE, atof,
	CoerceToDoubleArray, "as_double", 0.0,,)

DEFINE_AS_XXX_BUILT_IN(as_complex_built_in, complex, TYPE_COMPLEX, atocpx,
	CoerceToComplexArray, "as_complex", complex(0.0, 0.0),,)

DEFINE_AS_XXX_BUILT_IN(as_dcomplex_built_in, dcomplex, TYPE_DCOMPLEX, atodcpx,
	CoerceToDcomplexArray, "as_dcomplex", dcomplex(0.0, 0.0),,)

IValue* as_byte_built_in( const IValue* arg )
	{
	if ( arg->Type() == TYPE_STRING )
		{
		char* arg_str = arg->StringVal();
		int len = strlen( arg_str );
		byte* result = (byte*) alloc_memory( sizeof(byte)*len );

		for ( int i = 0; i < len; ++i )
			result[i] = byte(arg_str[i]);

		free_memory( arg_str );

		return new IValue( result, len );
		}

	int len = arg->Length();
	if ( ! arg->IsNumeric() )
		return (IValue*) Fail( "non-numeric argument to ", "byte" );

	if ( arg->Type() == TYPE_BYTE )
		return copy_value( arg );

	int is_copy;
	glish_fpe_enter();
	byte* result = arg->CoerceToByteArray( is_copy, len );
	glish_fpe_exit();

	return new IValue( result, len );
	}


IValue* as_string_built_in( const IValue* arg )
	{
	if ( arg->Type() == TYPE_STRING )
		return copy_value( arg );

	//
	// At some point, this should properly segment the record.
	//
	if ( arg->Type() == TYPE_RECORD )
		{
		char **ptr = (char**) alloc_memory( sizeof(char*) );
		ptr[0] = arg->StringVal();
		return new IValue( (charptr*) ptr, 1 );
		}

	//
	// Perhaps this too should be segmented.
	//
	if ( arg->Type() == TYPE_FUNC )
		{
		static SOStream sout;
		sout.reset();
		arg->Describe( sout );
		return new IValue( (const char *) sout.str() );
		}

	int len = arg->Length();

	if ( arg->Type() == TYPE_BYTE )
		{
		byte* vals = arg->BytePtr(0);
		char* s = (char*) alloc_memory( sizeof(char)*(len+1) );

		int i = 0;
		for ( ; i < len; ++i )
			s[i] = char(vals[i]);

		s[i] = '\0';

		IValue* result = new IValue( s );
		free_memory( s );

		return result;
		}

	if ( arg->Type() == TYPE_REGEX )
		{
		regexptr *regs = arg->RegexPtr(0);
		char** s = (char**) alloc_memory( sizeof(char*)*len );

		int i = 0;
		for ( ; i < len; ++i )
			s[i] = regs[i]->Description( );;

		IValue* result = new IValue( (charptr*) s, len );
		return result;
		}

	if ( ! arg->IsNumeric() && arg->Type() != TYPE_REGEX )
		return (IValue*) Fail( "non-numeric argument to as_string()" );

	charptr* result = (charptr*) alloc_memory( sizeof(charptr)*len );
	int i;
	char buf[256];

#define COMMA_SEPARATED_SERIES(x,y) x,y
#define COERCE_XXX_TO_STRING_FP_FORMAT(DFLT)			\
	const char *fmt = print_decimal_prec( arg->AttributePtr(), DFLT);
#define COERCE_XXX_TO_STRING_CPX_FORMAT(DFLT)			\
	char fmt_plus[40];					\
	char fmt_minus[40];					\
	const char *fmt = print_decimal_prec( arg->AttributePtr(), DFLT); \
	sprintf(fmt_plus,"%s+%si",fmt,fmt);			\
	sprintf(fmt_minus,"%s%si",fmt,fmt);
#define COERCE_XXX_TO_STRING_SUBVECREF_XLATE			\
	int err;						\
	int index = ref->TranslateIndex( i, &err );		\
	if ( err )						\
		{						\
		free_memory( result );				\
		return (IValue*) Fail( "invalid sub-vector" );	\
		}
#define COERCE_XXX_TO_STRING(tag,type,accessor,format,INDX,rest,XLATE,FORMAT)	\
	case tag:							\
		{							\
		type* vals = arg->accessor(0);				\
		FORMAT							\
		for ( i = 0; i < len; ++i )				\
			{						\
			XLATE						\
			sprintf( buf, format, vals[INDX] rest );	\
			result[i] = strdup( buf );			\
			}						\
		}							\
		break;

#define AS_STRING_ACTION(INDEX,XLATE)						\
	COERCE_XXX_TO_STRING(TYPE_SHORT,short,ShortPtr,"%d",INDEX,,XLATE,)	\
	COERCE_XXX_TO_STRING(TYPE_INT,int,IntPtr,"%d",INDEX,,XLATE,)		\
	COERCE_XXX_TO_STRING(TYPE_FLOAT,float,FloatPtr,fmt,INDEX,,XLATE,	\
			COERCE_XXX_TO_STRING_FP_FORMAT("%.6g"))			\
	COERCE_XXX_TO_STRING(TYPE_DOUBLE,double,DoublePtr,fmt,INDEX,,XLATE,	\
			COERCE_XXX_TO_STRING_FP_FORMAT("%.12g"))		\
	COERCE_XXX_TO_STRING(TYPE_COMPLEX,complex,ComplexPtr,		  	\
		(vals[i].i>=0.0?fmt_plus:fmt_minus),INDEX,			\
		COMMA_SEPARATED_SERIES(.r,vals[i].i),XLATE,			\
			COERCE_XXX_TO_STRING_CPX_FORMAT("%.6g"))		\
	COERCE_XXX_TO_STRING(TYPE_DCOMPLEX,dcomplex,DcomplexPtr,		\
		(vals[i].i>=0.0?fmt_plus:fmt_minus),INDEX,			\
		COMMA_SEPARATED_SERIES(.r,vals[i].i),XLATE,			\
			COERCE_XXX_TO_STRING_CPX_FORMAT("%.12g"))

	switch ( arg->Type() )
		{
		case TYPE_BOOL:
			{
			glish_bool* vals = arg->BoolPtr(0);
			for ( i = 0; i < len; ++i )
				result[i] = strdup( vals[i] ? "T" : "F" );
			}
			break;


		AS_STRING_ACTION(i,)

		case TYPE_SUBVEC_REF:
			{
			VecRef* ref = arg->VecRefPtr();
			switch ( ref->Type() )
				{
				case TYPE_BOOL:
					{
					glish_bool* vals = arg->BoolPtr(0);
					for ( i = 0; i < len; ++i )
						{
						COERCE_XXX_TO_STRING_SUBVECREF_XLATE
						result[i] = strdup( vals[index] ? "T" : "F" );
						}
					}
					break;


				AS_STRING_ACTION(index,COERCE_XXX_TO_STRING_SUBVECREF_XLATE)

				default:
					fatal->Report( "bad type tag in as_string()" );
				}
			}
			break;

		default:
			fatal->Report( "bad type tag in as_string()" );
		}

	return new IValue( result, len );
	}


IValue* type_name_built_in( const IValue* arg )
	{
	glish_type t = arg->Type();

	if ( arg->IsRef() )
		{
		IValue* deref_val = type_name_built_in( (const IValue*)(arg->RefPtr()) );
		char* deref_name = deref_val->StringVal();

		char buf[512];

		sprintf( buf, "%s %s", t == TYPE_REF ? "ref" : "const",
			deref_name );

		free_memory( deref_name );
		Unref( deref_val );

		return new IValue( buf );
		}

	if ( arg->IsVecRef() )
		t = arg->VecRefDeref()->Type();

	return new IValue( type_names[t] );
	}

IValue* is_fail_built_in( const IValue* arg )
	{
	return new IValue( arg->Type() == TYPE_FAIL ? glish_true : glish_false );
	}

IValue* length_built_in( const IValue* arg )
	{
	return new IValue( int( arg->Length() ) );
	}

IValue* field_names_built_in( const IValue* arg )
	{
	if ( arg->Type() != TYPE_RECORD )
		return (IValue*) Fail( "argument to field_names is not a record" );

	recordptr record_dict = arg->RecordPtr(0);
	int len = record_dict->Length();

	charptr* names = (charptr*) alloc_memory( sizeof(charptr)*len );
	const char* key;

	int i = 0;
	for ( ; i < len; ++i )
		{
		IValue* nth_val = (IValue*)record_dict->NthEntry( i, key );
		if ( ! nth_val )
			fatal->Report(
				"bad record in field_names_built_in" );
		names[i] = strdup( key );
		}

	return new IValue( names, i );
	}


char* paste( parameter_list* args )
	{
	PasteBuiltIn paste;

	// Create another parameter list with the separator at the
	// beginning.
	parameter_list args2;
	IValue sep( " " );

	ConstExpr sep_expr( &sep ); Ref(&sep);
	Parameter sep_parm( VAL_CONST, &sep_expr ); Ref(&sep_expr);
	args2.append( &sep_parm );

	loop_over_list( *args, i )
		args2.append( (*args)[i] );

	IValue* args_value = paste.Call( &args2, EVAL_COPY );

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
	IValue sep( " " );
	args2.append( &sep );

	loop_over_list( *args, i )
		args2.append( (*args)[i] );

	IValue* args_value = paste.DoCall( &args2 );
	char* result = args_value->StringVal();
	Unref( args_value );

	return result;
	}


static void add_one_arg_built_in( Sequencer* s, value_func_1_value_arg func,
					const char* name, int do_deref = 1,
					int handle_fail = 0 )
	{
	BuiltIn* b = new OneValueArgBuiltIn( func, name );
	b->SetDeref( do_deref );
	b->SetFailHandling( handle_fail );
	s->AddBuiltIn( b );
	}

//
//### Dummy complex functions
//	These should be supplied later, probably from the `fn' library
//	on `netlib'.
//
dcomplex asin( const dcomplex )
	{
	error->Report( "Sorry, complex arcsine not yet implemented" );
	return dcomplex( 0, 0 );
	}
dcomplex acos( const dcomplex )
	{
	error->Report( "Sorry, complex arccosine not yet implemented" );
	return dcomplex( 0, 0 );
	}
dcomplex atan( const dcomplex )
	{
	error->Report( "Sorry, complex arctangent not yet implemented" );
	return dcomplex( 0, 0 );
	}

void create_built_ins( Sequencer* s, const char *program_name )
	{
	add_one_arg_built_in( s, as_boolean_built_in, "as_boolean" );
	add_one_arg_built_in( s, as_byte_built_in, "as_byte" );
	add_one_arg_built_in( s, as_short_built_in, "as_short" );
	add_one_arg_built_in( s, as_integer_built_in, "as_integer" );
	add_one_arg_built_in( s, as_float_built_in, "as_float" );
	add_one_arg_built_in( s, as_double_built_in, "as_double" );
	add_one_arg_built_in( s, as_complex_built_in, "as_complex" );
	add_one_arg_built_in( s, as_dcomplex_built_in, "as_dcomplex" );
	add_one_arg_built_in( s, as_string_built_in, "as_string" );

	add_one_arg_built_in( s, type_name_built_in, "type_name", 0, 1 );
	add_one_arg_built_in( s, is_fail_built_in, "is_fail", 0, 1 );
	add_one_arg_built_in( s, field_names_built_in, "field_names" );

	s->AddBuiltIn( new NumericVectorBuiltIn( sqrt, sqrt, "sqrt" ) );
	s->AddBuiltIn( new NumericVectorBuiltIn( exp, exp, "exp" ) );
	s->AddBuiltIn( new NumericVectorBuiltIn( log, log, "ln" ) );
	s->AddBuiltIn( new NumericVectorBuiltIn( log10, log10, "log" ) );
	s->AddBuiltIn( new NumericVectorBuiltIn( sin, sin, "sin" ) );
	s->AddBuiltIn( new NumericVectorBuiltIn( cos, cos, "cos" ) );
	s->AddBuiltIn( new NumericVectorBuiltIn( tan, tan, "tan" ) );
	s->AddBuiltIn( new NumericVectorBuiltIn( asin, asin, "asin" ) );
	s->AddBuiltIn( new NumericVectorBuiltIn( acos, acos, "acos" ) );
	s->AddBuiltIn( new NumericVectorBuiltIn( atan, atan, "atan" ) );
	s->AddBuiltIn( new NumericVectorBuiltIn( floor, floor, "floor" ) );
	s->AddBuiltIn( new NumericVectorBuiltIn( ceil, ceil, "ceiling" ) );

	s->AddBuiltIn( new RealBuiltIn );
	s->AddBuiltIn( new ImagBuiltIn );
	s->AddBuiltIn( new ComplexBuiltIn );

	s->AddBuiltIn( new StrlenBuiltIn );

	s->AddBuiltIn( new SumBuiltIn );
	s->AddBuiltIn( new ProdBuiltIn );
	s->AddBuiltIn( new LengthBuiltIn );
	s->AddBuiltIn( new RangeBuiltIn );
	s->AddBuiltIn( new SeqBuiltIn );
	s->AddBuiltIn( new RepBuiltIn );
	s->AddBuiltIn( new NumArgsBuiltIn );
	s->AddBuiltIn( new NthArgBuiltIn );
	s->AddBuiltIn( new RandomBuiltIn );
	s->AddBuiltIn( new CbindBuiltIn );
	s->AddBuiltIn( new RbindBuiltIn );
	s->AddBuiltIn( new IsConstBuiltIn );
	s->AddBuiltIn( new IsModifiableBuiltIn );
	s->AddBuiltIn( new MissingBuiltIn( s ) );

#ifdef GGC
	s->AddBuiltIn( new GarbageBuiltIn( s ) );
#endif

	s->AddBuiltIn( new PasteBuiltIn );
	s->AddBuiltIn( new SplitBuiltIn );
	s->AddBuiltIn( new SizeofBuiltIn );
	s->AddBuiltIn( new AllocInfoBuiltIn );

	s->AddBuiltIn( new IsNaNBuiltIn );

	s->AddBuiltIn( new ReadValueBuiltIn );
	s->AddBuiltIn( new WriteValueBuiltIn );

	s->AddBuiltIn( new WheneverStmtsBuiltIn );

	s->AddBuiltIn( new ActiveAgentsBuiltIn );
	s->AddBuiltIn( new TimeBuiltIn );

	s->AddBuiltIn( new CreateAgentBuiltIn( s ) );
	s->AddBuiltIn( new CreateTaskBuiltIn( s ) );

	s->AddBuiltIn( new CreateGraphicBuiltIn( s ) );
	s->AddBuiltIn( new TkBuiltIns );

	s->AddBuiltIn( new SymbolNamesBuiltIn( s ) );
	s->AddBuiltIn( new SymbolValueBuiltIn( s ) );
	s->AddBuiltIn( new SymbolSetBuiltIn( s ) );
	s->AddBuiltIn( new SymbolDeleteBuiltIn( s ) );

	s->AddBuiltIn( new WheneverActiveBuiltIn( s ) );
	s->AddBuiltIn( new LastWheneverExecutedBuiltIn( s ) );
	s->AddBuiltIn( new CurrentWheneverBuiltIn( s ) );
	s->AddBuiltIn( new EvalBuiltIn( s ) );

	// for libnpd
	init_log( program_name );
	}
