// $Header$

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"

#include <string.h>
#include <stream.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

// For MAXINT, MAXFLOAT, HUGE.
#include <values.h>

#include "Sds/sdsgen.h"
#include "Npd/npd.h"
#include "glish_event.h"
#include "BuiltIn.h"
#include "Reporter.h"
#include "Task.h"
#include "Sequencer.h"
#include "Frame.h"
#ifdef GLISHTK
#include "TkAgent.h"
#include "TkCanvas.h"
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
			{
			error->Report( this, " takes", num_args, " argument",
					num_args == 1 ? ";" : "s;",
					num_args_present, " given" );

			return error_ivalue();
			}
		}

	loop_over_list( *args, j )
		{
		Parameter* arg = (*args)[j];
		if ( ! arg->Arg() )
			{
			error->Report( "missing parameter invalid for", this );
			return error_ivalue();
			}
		if ( arg->Name() )
			{
			error->Report( this,
					" does not have a parameter named \"",
					arg->Name(), "\"" );
			return error_ivalue();
			}
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
				if ( do_deref )
					arg_val = (const IValue*) (arg_val->Deref());

				args_vals->append( arg_val );
				}
			}

		else
			{
			arg_val = arg->Arg()->ReadOnlyEval();
			if ( do_deref )
				arg_val = (const IValue*) (arg_val->Deref());

			args_vals->append( arg_val );
			}
		}

	IValue* result;

	if ( do_call )
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
			result = DoCall( args_vals );
		}
	else
		result = error_ivalue();

	loop_over_list( *args, k )
		{
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

void BuiltIn::DescribeSelf( ostream& s ) const
	{
	s << description << "()";
	}

int BuiltIn::AllNumeric( const_args_list* args_vals, glish_type& max_type,
	int strings_okay )
	{
	max_type = TYPE_STRING;

	loop_over_list( *args_vals, i )
		{
		const IValue* arg = (*args_vals)[i];

		if ( arg->IsNumeric() )
			{
			max_type = max_numeric_type( max_type, arg->Type() );
			continue;
			}

		if ( strings_okay && arg->Type() == TYPE_STRING )
			continue;

		error->Report( "argument #", i + 1, "to", this,
			"is not numeric", strings_okay ? " or a string" : "" );
		return 0;
		}

	return 1;
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
		{
		error->Report( this, " requires a numeric argument" );
		return error_ivalue();
		}

	int len = arg->Length();
	glish_type type = arg->Type();

#define NUMERIC_BUILTIN_ACTION(type,accessor,fn)	\
	{						\
	int is_copy;					\
	type* args_vec = arg->accessor( is_copy, len );	\
	type* stor = new type[len];			\
							\
	for ( int i = 0; i < len; ++i )			\
		stor[i] = (*fn)( args_vec[i] );		\
							\
	if ( is_copy )					\
		delete args_vec;			\
							\
	result = new IValue( stor, len );		\
	result->CopyAttributes( arg );			\
	}

	if ( type == TYPE_COMPLEX || type == TYPE_DCOMPLEX )
		NUMERIC_BUILTIN_ACTION(dcomplex,CoerceToDcomplexArray,cfunc)
	else
		NUMERIC_BUILTIN_ACTION(double,CoerceToDoubleArray,func)

	return result;
	}

IValue* RealBuiltIn::DoCall( const_args_list* args_val )
	{
	const IValue* v = (*args_val)[0];

	if ( ! v->IsNumeric() )
		{
		error->Report( this, " requires a numeric argument" );
		return error_ivalue();
		}

	IValue* result;

#define RE_IM_BUILTIN_ACTION(tag,type,subtype,accessor,OFFSET,elem,XLATE)\
	case tag:						\
		{						\
		int is_copy;					\
		int len = v->Length();				\
		subtype* stor = new subtype[len];		\
		type* from = v->accessor( is_copy, len );	\
		for ( int i = 0; i < len; i++ )			\
			{					\
			XLATE					\
			stor[i] = from[OFFSET] elem;		\
			}					\
		if ( is_copy )					\
			delete from;				\
		result = new IValue( stor, len );		\
		result->CopyAttributes( v );			\
		}						\
		break;

	switch ( v->Type() )
		{
RE_IM_BUILTIN_ACTION(TYPE_COMPLEX,complex,float,CoerceToComplexArray,i,.r,)
RE_IM_BUILTIN_ACTION(TYPE_DCOMPLEX,dcomplex,double,CoerceToDcomplexArray,i,.r,)

		case TYPE_SUBVEC_REF:
			{
			VecRef* ref = v->VecRefPtr();
			IValue* theVal = (IValue*) ref->Val();
			int theLen = theVal->Length();

			switch ( theVal->Type() )
				{

#define RE_IM_BUILTIN_ACTION_XLATE				\
	int err;						\
	int off = ref->TranslateIndex( i, &err );		\
	if ( err )						\
		{						\
		delete stor;					\
		if ( is_copy )					\
			delete from;				\
		error->Report("invalid index (=",i+1,"), sub-vector reference may be bad");\
		return error_ivalue();				\
		}

RE_IM_BUILTIN_ACTION(TYPE_COMPLEX,complex,float,CoerceToComplexArray,i,.r,RE_IM_BUILTIN_ACTION_XLATE)
RE_IM_BUILTIN_ACTION(TYPE_DCOMPLEX,dcomplex,double,CoerceToDcomplexArray,i,.r,RE_IM_BUILTIN_ACTION_XLATE)

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
		{
		error->Report( this, " requires a numeric argument" );
		return error_ivalue();
		}

	IValue* result;

	switch ( v->Type() )
		{
RE_IM_BUILTIN_ACTION(TYPE_COMPLEX,complex,float,CoerceToComplexArray,i,.i,)
RE_IM_BUILTIN_ACTION(TYPE_DCOMPLEX,dcomplex,double,CoerceToDcomplexArray,i,.i,)

		case TYPE_SUBVEC_REF:
			{
			VecRef* ref = v->VecRefPtr();
			IValue* theVal = (IValue*) ref->Val();
			int theLen = theVal->Length();

			switch ( theVal->Type() )
				{

RE_IM_BUILTIN_ACTION(TYPE_COMPLEX,complex,float,CoerceToComplexArray,i,.i,RE_IM_BUILTIN_ACTION_XLATE)
RE_IM_BUILTIN_ACTION(TYPE_DCOMPLEX,dcomplex,double,CoerceToDcomplexArray,i,.i,RE_IM_BUILTIN_ACTION_XLATE)

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

IValue* ComplexBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();
	IValue* result;

	if ( len < 1 || len > 2 )
		{
		error->Report( this, " takes 1 or 2 arguments" );
		return error_ivalue();
		}

	if ( len == 2 )
		{
		const IValue* rv = (*args_val)[0];
		const IValue* iv = (*args_val)[1];

		if ( ! rv->IsNumeric() || ! iv->IsNumeric() )
			{
			error->Report( this,
				" requires one or two numeric arguments" );
			return error_ivalue();
			}

		int rlen = rv->Length();
		int ilen = iv->Length();

		int rscalar = rlen == 1;
		int iscalar = ilen == 1;

		if ( rlen != ilen && ! rscalar && ! iscalar )
			{
			error->Report(
				"different-length operands in expression (",
					rlen, " vs. ", ilen, "):\n\t",
					this );
			return error_ivalue();
			}

		glish_type maxt = max_numeric_type( rv->Type(), iv->Type() );

#define COMPLEXBUILTIN_TWOPARM_ACTION(tag,type,rettype,accessor,coerce)	\
	case tag:							\
		{							\
		int r_is_copy;						\
		int i_is_copy;						\
		int maxlen = rlen > ilen ? rlen : ilen;			\
		rettype* r = rv->accessor( r_is_copy, rlen );		\
		rettype* i = iv->accessor( i_is_copy, ilen );		\
		type* stor = new type[maxlen];				\
		for ( int cnt = 0; cnt < maxlen; ++cnt )		\
			{						\
			stor[cnt].r = coerce( r[rscalar ? 0 : cnt] );	\
			stor[cnt].i = coerce( i[iscalar ? 0 : cnt] );	\
			}						\
		if ( r_is_copy )					\
			delete r;					\
		if ( i_is_copy )					\
			delete i;					\
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
			{
			error->Report( this,
				" requires one or two numeric arguments" );
			return error_ivalue();
			}

#define COMPLEXBUILTIN_ONEPARM_ACTION(tag,type,rettype,accessor,coerce)	\
	case tag:						\
		{						\
		int is_copy;					\
		int vlen = v->Length();				\
		rettype* vp = v->accessor( is_copy, vlen );	\
		type* stor = new type[vlen];			\
		for ( int cnt = 0; cnt < vlen; ++cnt )		\
			{					\
			stor[cnt].r = coerce( vp[cnt] );	\
			stor[cnt].i = 0;			\
			}					\
		if ( is_copy )					\
			delete vp;				\
		result = new IValue( stor, vlen );			\
		}						\
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

	if ( ! AllNumeric( args_val, max_type ) )
		return error_ivalue();

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
			delete val_array;			\
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

	if ( ! AllNumeric( args_val, max_type ) )
		return error_ivalue();

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
				delete val_array;			\
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
			error->Report( "bad type in ProdBuiltIn::DoCall()" );
			return 0;
		}

	return result;
	}

IValue* LengthBuiltIn::DoCall( const_args_list* args_val )
	{
	int num = args_val->length();

	if ( num > 1 )
		{
		int* len = new int[args_val->length()];
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

	if ( ! AllNumeric( args_val, max_type ) )
		return error_ivalue();

#define RANGEBUILTIN_ACTION(tag,type,accessor,max)			\
	case tag:							\
		{							\
		type min_val = (type) max;				\
		type max_val = (type) -max;				\
									\
		loop_over_list( *args_val, i )				\
			{						\
			const IValue* val = (*args_val)[i];		\
			int len = val->Length();			\
			int is_copy;					\
									\
			type* val_array = val->accessor( is_copy, len );\
									\
			for ( int j = 0; j < len; ++j )			\
				{					\
				if ( val_array[j] < min_val )		\
					min_val = val_array[j];		\
									\
				if ( val_array[j] > max_val )		\
					max_val = val_array[j];		\
				}					\
									\
			if ( is_copy )					\
				delete val_array;			\
			}						\
		type* range = new type[2];				\
		range[0] = min_val;					\
		range[1] = max_val;					\
									\
		result = new IValue( range, 2 );				\
		}							\
			break;

	switch ( max_type )
		{
RANGEBUILTIN_ACTION(TYPE_DCOMPLEX,dcomplex,CoerceToDcomplexArray,HUGE)
RANGEBUILTIN_ACTION(TYPE_COMPLEX,complex,CoerceToComplexArray,MAXFLOAT)
RANGEBUILTIN_ACTION(TYPE_DOUBLE,double,CoerceToDoubleArray,HUGE)
RANGEBUILTIN_ACTION(TYPE_FLOAT,float,CoerceToFloatArray,MAXFLOAT)
		case TYPE_BOOL:
		case TYPE_BYTE:
		case TYPE_SHORT:
RANGEBUILTIN_ACTION(TYPE_INT,int,CoerceToIntArray,MAXINT)
		default:
			result = error_ivalue();
		}

	return result;
	}

IValue* SeqBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();

	if ( len == 0 || len > 3 )
		{
		error->Report( this, " takes from one to three arguments" );
		return error_ivalue();
		}

	double starting_point = 1.0;
	double stopping_point;
	double stride = 1.0;

	const IValue* arg;

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
		return error_ivalue();
		}

	if ( (starting_point < stopping_point && stride < 0) ||
	     (starting_point > stopping_point && stride > 0) )
		{
		error->Report( "in call to ", this,
				", stride has incorrect sign" );
		return error_ivalue();
		}

	double range = stopping_point - starting_point;
	int num_vals = int( range / stride ) + 1;

	if ( num_vals > 1e6 )
		{
		error->Report( "ridiculously large sequence in call to ",
				this );
		return error_ivalue();
		}

	double* result = new double[num_vals];

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
		{
		error->Report( "non-numeric parameters invalid for", this );
		return error_ivalue();
		}

	if ( times->Length() != 1 && times->Length() != element->Length() )
		{
		error->Report( this,
				": parameter vectors have unequal lengths" );
		return error_ivalue();
		}

	int times_is_copy;
	int times_len = times->Length();
	int* times_vec = times->CoerceToIntArray( times_is_copy, times_len );

	for ( int x = 0; x < times_len; ++x )
		if ( times_vec[x] < 0 )
			{
			error->Report( "invalid replication parameter, 2nd (",
					times_vec[x], "), in ", this );
			if ( times_is_copy )
				delete times_vec;
			return error_ivalue();
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
			case tag:						\
				{						\
				type* vec = new type[veclen];			\
				type* elm = element->accessor(0);		\
				for ( i=0; i < times_len; ++i )			\
					for ( int j=0; j < times_vec[i]; ++j )	\
					  vec[off++] = copy_func( elm[i] );	\
				ret = new IValue( vec, veclen );		\
				}						\
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

			default:
				error->Report(
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
				case tag:				\
					{				\
					type val = element->accessor();	\
					type *vec = new type[len];	\
					for (int i = 0; i < len; i++)	\
						vec[i] = copy_func(val);\
					ret = new IValue( vec, len );	\
					CLEANUP_VAL			\
					}				\
					break;

			REPBUILTIN_ACTION_B(TYPE_BOOL,glish_bool,BoolVal,,)
			REPBUILTIN_ACTION_B(TYPE_BYTE,byte,ByteVal,,)
			REPBUILTIN_ACTION_B(TYPE_SHORT,short,ShortVal,,)
			REPBUILTIN_ACTION_B(TYPE_INT,int,IntVal,,)
			REPBUILTIN_ACTION_B(TYPE_FLOAT,float,FloatVal,,)
			REPBUILTIN_ACTION_B(TYPE_DOUBLE,double,DoubleVal,,)
			REPBUILTIN_ACTION_B(TYPE_COMPLEX,complex,ComplexVal,,)
			REPBUILTIN_ACTION_B(TYPE_DCOMPLEX,dcomplex,DcomplexVal,,)
			REPBUILTIN_ACTION_B(TYPE_STRING,charptr,StringVal,strdup,delete (char *)val;)

				default:
					error->Report(
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
			case tag:					\
				{					\
				type* val = element->accessor(0);	\
				type* vec = new type[veclen];		\
				for ( int j = 0; j < repl; ++j )	\
					for ( int i = 0; i < e_len; ++i )\
						vec[off++] =  copy_func(val[i]);\
				ret = new IValue( vec, veclen );		\
				}					\
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

				default:
					error->Report(
					"bad type in RepBuiltIn::DoCall()" );
				}
			}
		}

	if ( times_is_copy )
		delete times_vec;

	return ret ? ret : error_ivalue();
	}

IValue* NumArgsBuiltIn::DoCall( const_args_list* args_val )
	{
	return new IValue( args_val->length() );
	}

IValue* NthArgBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();

	if ( len <= 0 )
		{
		error->Report( "first argument missing in call to", this );
		return error_ivalue();
		}

	int n = (*args_val)[0]->IntVal();

	if ( n < 0 || n >= len )
		{
		error->Report( "first argument (=", n, ") to", this,
				" out of range: ", len - 1,
				"additional arguments supplied" );
		return error_ivalue();
		}

	return copy_value( (*args_val)[n] );
	}

IValue* RandomBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();
	const IValue *val = 0;
	int arg1 = 0;
	int arg2 = 0;

	if ( len > 2 )
		{
		error->Report( this, " takes from zero to two arguments" );
		return error_ivalue();
		}

	if ( len >= 1 )
		{
		val = (*args_val)[0];

		if ( ! val->IsNumeric() )
			{
			error->Report( "non-numeric parameter invalid for",
						this );
			return error_ivalue();
			}

		arg1 = val->IntVal();
		}

	if ( len == 2 ) 
		{
		val = (*args_val)[1];

		if ( ! val->IsNumeric() )
			{
			error->Report( "non-numeric parameter invalid for",
						this );
			return error_ivalue();
			}

		arg2 = val->IntVal();

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
			int *ival = new int[arg1];
			for ( int i = arg1 - 1; i >= 0; i-- )
				ival[i] = (int) random_long();

			ret = new IValue( ival, arg1 );
			}
		}
	else
		{
		int diff = arg2 - arg1;
		if ( diff <= 0 )
			{
			error->Report( "invalid range for",
						this );
			return error_ivalue();
			}
		ret =  new IValue( (int)((unsigned long)random_long() % 
						(diff+1)) + arg1 );
		}

	return ret;
	}

#define XBIND_MIXTYPE_ERROR						\
	{								\
	error->Report( "both numeric and non-numeric arguments" );	\
	return error_ivalue();						\
	}

#define XBIND_CLEANUP							\
	if ( shape_is_copy )						\
		delete( shape );

#define XBIND_ALLOC_PTR(tag, type)					\
	case tag:							\
		result = new type[cols*rows];				\
		break;

#define XBIND_PLACE_ACTION(tag,type,array,to,from,access)		\
	case tag:							\
		((type*)result)[to]  = (type)( array[from] access );	\
		break;

#define XBIND_PLACE_ELEMENT(array,to,from,access)			\
	switch ( result_type )						\
		{							\
	XBIND_PLACE_ACTION(TYPE_BOOL,glish_bool,array,to,from,access)	\
	XBIND_PLACE_ACTION(TYPE_INT,int,array,to,from,access)		\
	XBIND_PLACE_ACTION(TYPE_BYTE,byte,array,to,from,access)		\
	XBIND_PLACE_ACTION(TYPE_SHORT,short,array,to,from,access)	\
	XBIND_PLACE_ACTION(TYPE_FLOAT,float,array,to,from,access)	\
	XBIND_PLACE_ACTION(TYPE_DOUBLE,double,array,to,from,access)	\
	XBIND_PLACE_ACTION(TYPE_COMPLEX,complex,array,to,from,)		\
	XBIND_PLACE_ACTION(TYPE_DCOMPLEX,dcomplex,array,to,from,)	\
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
		{							\
		error->Report( "invalid sub-vector" );			\
		return error_ivalue();					\
		}

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
		{							\
		error->Report(this, " takes at least two arguments");	\
		return error_ivalue();					\
		}							\
									\
	loop_over_list( *args_vals, i )					\
		{							\
		const IValue *arg = (*args_vals)[i];			\
		int arg_len = arg->Length();				\
		const attributeptr attr = arg->AttributePtr();		\
		const IValue *attr_val;					\
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
			{						\
			error->Report("invalid type (argument ",i+1,")");\
			return error_ivalue();				\
			}						\
									\
		if (  attr && (shape_v = (const IValue*)((*attr)["shape"])) &&	\
		      shape_v != false_value && shape_v->IsNumeric() &&	\
		      (shape_len = shape_v->Length()) > 1 )		\
			{						\
			if ( shape_len > 2 )				\
				{					\
				error->Report( "argument (",i+1,	\
				  ") with dimensionality greater than 2" );\
				return error_ivalue();			\
				}					\
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
					error->Report( 			\
					"mismatch in number of rows" );	\
					XBIND_CLEANUP			\
					return error_ivalue();		\
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
				{					\
				error->Report( 				\
					"mismatch in number of rows" );	\
				return error_ivalue();			\
				}					\
			}						\
		}							\
									\
	if ( rows < 0 )							\
		rows = minrows;						\
									\
	void *result;							\
	IValue *result_value = 0;					\
	if ( result_type == TYPE_STRING )				\
		{							\
		error->Report("sorry not implemented for strings yet");	\
		return error_ivalue();					\
		}							\
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
		int arg_len = arg->Length();				\
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
		charptr* string_ptr;					\
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
		int *newshape = new int[2];				\
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

IValue* PasteBuiltIn::DoCall( const_args_list* args_val )
	{
	if ( args_val->length() == 0 )
		{
		error->Report( "paste() invoked with no arguments" );
		return error_ivalue();
		}

	// First argument gives separator string.
	char* separator = (*args_val)[0]->StringVal();

	charptr* string_vals = new charptr[args_val->length()];

	int len = 1;	// Room for end-of-string.
	int sep_len = strlen( separator );

	for ( int i = 1; i < args_val->length(); ++i )
		{
		unsigned int limit = (*args_val)[i]->PrintLimit();
		string_vals[i] = (*args_val)[i]->StringVal( ' ', limit, 1 );
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

	return new IValue( result, 1 );
	}

IValue* SplitBuiltIn::DoCall( const_args_list* args_val )
	{
	int len = args_val->length();

	if ( len < 1 || len > 2 )
		{
		error->Report( this, " takes 1 or 2 arguments" );
		return error_ivalue();
		}

	char* source = (*args_val)[0]->StringVal();

	char* split_chars = " \t\n";
	if ( len == 2 )
		split_chars = (*args_val)[1]->StringVal();

	IValue* result = isplit( source, split_chars );

	delete source;
	if ( len == 2 )
		delete split_chars;

	return result;
	}


IValue* ReadValueBuiltIn::DoCall( const_args_list* args_val )
	{
	char* file_name = (*args_val)[0]->StringVal();

	int sds = (int) sds_access( file_name, SDS_FILE, SDS_READ );

	IValue* result;

	if ( sds < 0 )
		{
		error->Report( "could not read value from \"", file_name,
				"\"" );
		result = error_ivalue();
		}

	else
		result = read_ivalue_from_SDS( sds );

	delete file_name;

	return result;
	}


IValue* WriteValueBuiltIn::DoCall( const_args_list* args_val )
	{
	char* file_name = (*args_val)[1]->StringVal();
	const IValue* v = (*args_val)[0];

	int result = 1;

	if ( v->Type() == TYPE_OPAQUE )
		{
		int sds = v->SDS_IndexVal();

		if ( sds_ass( sds, file_name, SDS_FILE ) != sds )
			{
			error->Report( "could not save opaque value to \"",
					file_name, "\"" );
			result = 0;
			}
		}

	else
		{
		int sds = (int) sds_new( (char*) "" );

		if ( sds < 0 )
			{
			error->Report( "problem saving value to \"", file_name,
					"\", SDS error code = ", sds );
			result = 0;
			}

		else
			{
			del_list d;

			(*args_val)[0]->AddToSds( sds, &d );

			if ( sds_ass( sds, file_name, SDS_FILE ) != sds )
				{
				error->Report( "could not save value to \"",
						file_name, "\"" );
				result = 0;
				}

			sds_destroy( sds );

			delete_list( &d );
			}
		}

	delete file_name;

	return new IValue( result );
	}


IValue* WheneverStmtsBuiltIn::DoCall( const_args_list* args_val )
	{
	Agent* agent = (*args_val)[0]->AgentVal();

	if ( ! agent )
		return error_ivalue();

	else
		return agent->AssociatedStatements();
	}


IValue* ActiveAgentsBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	IValue* r = create_irecord();

	loop_over_list( agents, i )
		{
		IValue* a = agents[i]->AgentRecord();
		r->SetField( r->NewFieldName(), new IValue( a, VAL_REF ) );
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


IValue* CreateGraphicBuiltIn::DoCall( const_args_list* args_val )
	{
#ifdef GLISHTK
	int len = args_val->length();

	const IValue* arg = (*args_val)[0];

	if ( len < 1 )
		{
		error->Report( this, " requires at least one argument");
		return error_ivalue();
		}

	if ( arg->Type() != TYPE_STRING )
		{
		error->Report( this, " requires a string as the first argument");
		return error_ivalue();
		}

	const char *type = arg->StringPtr(0)[0];
	TkAgent *agent = 0;

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

	return agent ? agent->AgentRecord() : error_ivalue();
#else
	error->Report("This Glish was not configured for graphic clients");
	return error_ivalue();
#endif
	}


IValue* SymbolNamesBuiltIn::DoCall( const_args_list *args_val )
	{
	int len = args_val->length();
	Scope *scope = sequencer->GetScope( );
	if ( ! scope || ! scope->Length() )
		return error_ivalue();

	if ( len > 1 )
		{
		error->Report( this, " takes 0 or 1 argument" );
		return error_ivalue();
		}

	const IValue *func_val = len > 0 ? (*args_val)[0] : 0 ;
	funcptr func = 0;

	if ( func_val )
 		if ( func_val->Type() != TYPE_FUNC )
			{
			error->Report( this, " only takes a function as an argument");
			return error_ivalue();
			}
		else
			func = func_val->FuncVal();

	int cnt = 0;
	charptr *name_ary = new charptr[ scope->Length() ];
	IterCookie *c = scope->InitForIteration();
	const Expr *member;
	const char *key;
	while ( (member = scope->NextEntry( key, c )) )
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
			name_ary[cnt] = new char[strlen( key )+1];
			strcpy((char*) name_ary[cnt++], key);
			}
		}

	return new IValue( (charptr*) name_ary, cnt );
	}

IValue* SymbolValueBuiltIn::DoCall( const_args_list *args_val )
	{
	int len = args_val->length();
	const IValue *str = (*args_val)[0];

	if ( ! str || str->Type() != TYPE_STRING )
		{
		error->Report( this, " takes 1 string argument" );
		return error_ivalue();
		}

	charptr *strs = str->StringPtr(0);
	recordptr rptr = create_record_dict();
	for ( int i = 0; i < str->Length(); i++ )
		{
		Expr *exp = sequencer->LookupID( strdup(strs[i]), GLOBAL_SCOPE, 0, 0);
		if ( exp )
			{
			IValue *val = exp->CopyEval();
			if ( val )
				rptr->Insert( strdup(strs[i]), val );
			}
		}

	return new IValue( rptr );
	}

IValue* SymbolSetBuiltIn::DoCall( const_args_list *args_val )
	{
	int len = args_val->length();

	if ( len < 1 || len > 2 )
		{
		error->Report( this, " takes either 1 record argument or a string and a value" );
		return error_ivalue();
		}

	const IValue *arg1 = (*args_val)[0];
	const IValue *arg2 = len > 1 ? (*args_val)[1] : 0;

	if ( ! arg2 )
		{
		if ( arg1->Type() != TYPE_RECORD )
			{
			error->Report( "wrong type for argument 1, record expected" );
			return error_ivalue();
			}

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
			{
			error->Report( "wrong type for argument 1, string expected" );
			return error_ivalue();
			}

		charptr *strs = arg1->StringPtr(0);
		Expr *id = sequencer->LookupID( strdup(strs[0]), GLOBAL_SCOPE, 1, 0 );
		id->Assign( copy_value( arg2 ) );
		}

	return new IValue( glish_true );
	}

IValue* MissingBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	Frame* cur = sequencer->CurrentFrame();
	if ( ! cur )
		return empty_ivalue();

	return copy_value( cur->Missing() );
	}

IValue* CurrentWheneverBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	Notification* n = sequencer->LastNotification();

	if ( ! n )
		{
		error->Report( "no active whenever, in call to", this );
		return new IValue( 0 );
		}

	return new IValue( n->notifiee->stmt->Index() );
	}

IValue* LastWheneverExecutedBuiltIn::DoCall( const_args_list* /* args_val */ )
	{
	Stmt* s = sequencer->LastWheneverExecuted();

	if ( ! s )
		{
		error->Report( "no whenever's executed, in call to", this );
		return new IValue( 0 );
		}

	return new IValue( s->Index() );
	}


#define DEFINE_AS_XXX_BUILT_IN(name,type,tag,stringcvt,coercer,text,zero) \
IValue* name( const IValue* arg )					\
	{								\
	int len = arg->Length();					\
									\
	if ( arg->Type() == TYPE_STRING )				\
		{							\
		const charptr* strings = arg->StringPtr(0);		\
		type* result = new type[len];				\
									\
		for ( int i = 0; i < len; ++i )				\
			result[i] = stringcvt( strings[i] );		\
									\
		return new IValue( result, len );			\
		}							\
									\
	if ( ! arg->IsNumeric() )					\
		{							\
		error->Report( "non-numeric argument to ", text );	\
		return new IValue( type(zero) );			\
		}							\
									\
	if ( arg->Type() == tag )					\
		return copy_value( arg );				\
									\
	int is_copy;							\
	type* result = arg->coercer( is_copy, len );			\
									\
	IValue* ret = new IValue( result, len );			\
	ret->CopyAttributes( arg );					\
	return ret;							\
	}

glish_bool string_to_bool( const char* string )
	{
	int successful;
	double d = text_to_double( string, successful );
	if ( successful )
		return glish_bool( int( d ) );
	else
		return glish_false;
	}

DEFINE_AS_XXX_BUILT_IN(as_boolean_built_in, glish_bool, TYPE_BOOL,
	string_to_bool, CoerceToBoolArray, "as_boolean", glish_false)

DEFINE_AS_XXX_BUILT_IN(as_short_built_in, short, TYPE_SHORT, atoi,
	CoerceToShortArray, "as_short", 0)

DEFINE_AS_XXX_BUILT_IN(as_integer_built_in, int, TYPE_INT, atoi,
	CoerceToIntArray, "as_integer", 0)

DEFINE_AS_XXX_BUILT_IN(as_float_built_in, float, TYPE_FLOAT, atof,
	CoerceToFloatArray, "as_float", 0.0)

DEFINE_AS_XXX_BUILT_IN(as_double_built_in, double, TYPE_DOUBLE, atof,
	CoerceToDoubleArray, "as_double", 0.0)

DEFINE_AS_XXX_BUILT_IN(as_complex_built_in, complex, TYPE_COMPLEX, atocpx,
	CoerceToComplexArray, "as_complex", complex(0.0, 0.0))

DEFINE_AS_XXX_BUILT_IN(as_dcomplex_built_in, dcomplex, TYPE_DCOMPLEX, atodcpx,
	CoerceToDcomplexArray, "as_dcomplex", dcomplex(0.0, 0.0))

IValue* as_byte_built_in( const IValue* arg )
	{
	if ( arg->Type() == TYPE_STRING )
		{
		char* arg_str = arg->StringVal();
		int len = strlen( arg_str );
		byte* result = new byte[len];

		for ( int i = 0; i < len; ++i )
			result[i] = byte(arg_str[i]);

		delete arg_str;

		return new IValue( result, len );
		}

	int len = arg->Length();
	if ( ! arg->IsNumeric() )
		{
		error->Report( "non-numeric argument to ", "byte" );
		return new IValue( byte(0) );
		}

	if ( arg->Type() == TYPE_BYTE )
		return copy_value( arg );

	int is_copy;
	byte* result = arg->CoerceToByteArray( is_copy, len );

	return new IValue( result, len );
	}


IValue* as_string_built_in( const IValue* arg )
	{
	if ( arg->Type() == TYPE_STRING )
		return copy_value( arg );

	if ( ! arg->IsNumeric() )
		{
		error->Report( "non-numeric argument to as_string()" );
		return new IValue( "" );
		}

	int len = arg->Length();

	if ( arg->Type() == TYPE_BYTE )
		{
		byte* vals = arg->BytePtr(0);
		char* s = new char[len+1];

		for ( int i = 0; i < len; ++i )
			s[i] = char(vals[i]);

		s[i] = '\0';

		IValue* result = new IValue( s );
		delete s;

		return result;
		}

	charptr* result = new charptr[len];
	int i;
	char buf[256];

#define COMMA_SEPARATED_SERIES(x,y) x,y
#define COERCE_XXX_TO_STRING_FP_FORMAT(DFLT)			\
	const char *fmt = print_decimal_prec( arg->AttributePtr(), DFLT);
#define COERCE_XXX_TO_STRING_CPX_FORMAT(DFLT)			\
	char fmt_plus[40];					\
	char fmt_minus[40];					\
	const char *fmt = print_decimal_prec( arg->AttributePtr(), DFLT); \
	strcpy(fmt_plus,fmt);					\
	strcat(fmt_plus,"+");					\
	strcat(fmt_plus,fmt);					\
	strcpy(fmt_minus,fmt);					\
	strcat(fmt_minus,fmt);
#define COERCE_XXX_TO_STRING_SUBVECREF_XLATE			\
	int err;						\
	int index = ref->TranslateIndex( i, &err );		\
	if ( err )						\
		{						\
		error->Report( "invalid sub-vector" );		\
		delete result;					\
		return error_ivalue();			\
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

		delete deref_name;
		Unref( deref_val );

		return new IValue( buf );
		}

	if ( arg->IsVecRef() )
		t = arg->VecRefDeref()->Type();

	return new IValue( type_names[t] );
	}

IValue* length_built_in( const IValue* arg )
	{
	return new IValue( int( arg->Length() ) );
	}

IValue* field_names_built_in( const IValue* arg )
	{
	if ( arg->Type() != TYPE_RECORD )
		{
		error->Report( "argument to field_names is not a record" );
		return error_ivalue();
		}

	recordptr record_dict = arg->RecordPtr(0);
	IterCookie* c = record_dict->InitForIteration();
	int len = record_dict->Length();

	charptr* names = new charptr[len];
	const char* key;

	for ( int i = 0; i < len; ++i )
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
					const char* name, int do_deref = 1 )
	{
	BuiltIn* b = new OneValueArgBuiltIn( func, name );
	b->SetDeref( do_deref );
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

	add_one_arg_built_in( s, type_name_built_in, "type_name", 0 );
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
	s->AddBuiltIn( new MissingBuiltIn( s ) );

	s->AddBuiltIn( new PasteBuiltIn );
	s->AddBuiltIn( new SplitBuiltIn );

	s->AddBuiltIn( new ReadValueBuiltIn );
	s->AddBuiltIn( new WriteValueBuiltIn );

	s->AddBuiltIn( new WheneverStmtsBuiltIn );

	s->AddBuiltIn( new ActiveAgentsBuiltIn );
	s->AddBuiltIn( new TimeBuiltIn );

	s->AddBuiltIn( new CreateAgentBuiltIn( s ) );
	s->AddBuiltIn( new CreateGraphicBuiltIn( s ) );
	s->AddBuiltIn( new CreateTaskBuiltIn( s ) );

	s->AddBuiltIn( new SymbolNamesBuiltIn( s ) );
	s->AddBuiltIn( new SymbolValueBuiltIn( s ) );
	s->AddBuiltIn( new SymbolSetBuiltIn( s ) );

	s->AddBuiltIn( new LastWheneverExecutedBuiltIn( s ) );
	s->AddBuiltIn( new CurrentWheneverBuiltIn( s ) );

	sds_init();

#ifdef AUTHENTICATE
	init_log( program_name );
#endif
	}
