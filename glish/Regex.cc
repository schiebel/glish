// $Id$
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "Regex.h"
#include "IValue.h"
#include "Glish/Stream.h"
#include "Reporter.h"

Regex::Regex( char *match_, char *subst_ ) : reg(0), match(match_), match_end(0), subst( subst_ ),
						match_val(0), match_res(0), match_len(0), alloc_len(0)
	{
	if ( match ) match_end = match + strlen(match);
	}

#define INIT_REGX							\
if ( ! reg )								\
	{								\
	if ( match )							\
		{							\
		reg = regxcomp( match, match_end, &pm );		\
		if ( ! reg ) return (IValue*) Fail( "bad regular expression" ); \
		}							\
	else								\
		return (IValue*) Fail( "bad regular expression" );	\
	}

#define ADJUST_MATCH(LENMOD,DUMMY)					\
if ( match_val )							\
	{								\
	Unref( match_val );						\
	match_val = 0;							\
	}								\
									\
if ( match_res )							\
	{								\
	for (int i = 0; i < match_len; i++ )				\
		free_memory( match_res[i] );				\
	if ( reg->nparens LENMOD > alloc_len )				\
		{							\
		alloc_len = reg->nparens LENMOD;			\
		match_res = (char**) realloc_memory( match_res, 	\
				alloc_len * sizeof(char*) );		\
		}							\
	}								\
else if ( reg->nparens > 0 )						\
	{								\
	alloc_len = reg->nparens LENMOD;				\
	match_res = (char **) alloc_memory( alloc_len * sizeof(char*) );\
	}

#define EVAL_ACTION(OP,INDEXINIT,COUNT,COUNTINIT)			\
if ( match_len > 0 )							\
	{								\
	if ( OP == glish_true )						\
		for ( register int cnt=1; cnt <= reg->nparens; cnt++ )	\
			{						\
			register int index = INDEXINIT;			\
			register int slen = reg->endp[cnt]-reg->startp[cnt]; \
			match_res[index] = (char*) alloc_memory( slen+1 ); \
			if ( slen > 0 ) memcpy(match_res[index], reg->startp[cnt], slen); \
			match_res[index][slen] = '\0';			\
			}						\
	else								\
		for ( register int cnt=0; cnt < reg->nparens; cnt++ )	\
			{						\
			COUNTINIT;					\
			match_res[COUNT] = (char*) alloc_memory( 1 );	\
			match_res[COUNT][0] = '\0';			\
			}						\
	}

static char regex_buffer[2048];
IValue *Regex::Eval( char *string )
	{

	INIT_REGX
	ADJUST_MATCH(,)

	match_len = reg->nparens;
	glish_bool ret = regxexec( reg, string, string+strlen(string), string, 1,0,1 ) ? glish_true : glish_false;
	EVAL_ACTION( ret , cnt-1 , cnt , )

	return new IValue(  ret );
	}

IValue *Regex::Eval( char **strs, int len )
	{

	INIT_REGX
	ADJUST_MATCH(*len,)

	IValue *ret = 0;
	glish_bool *r = (glish_bool*) alloc_memory( sizeof(glish_bool) * len );

	match_len = reg->nparens * len;
	for ( int i=0; i < len; ++i )
		{
		r[i] = regxexec( reg, strs[i], strs[i]+strlen(strs[i]), strs[i], 1,0,1 ) ? glish_true : glish_false;
		EVAL_ACTION( r[i] , i+(cnt-1)%reg->nparens*len , index , register int index = i+cnt%reg->nparens*len )
		}

	return new IValue( r, len );
	}

IValue *Regex::GetMatch( )
	{
	if ( match_val ) return match_val;

	if ( match_len > 0 )
		{
		match_val = new IValue( (charptr*) match_res, match_len );
		if ( match_len > reg->nparens )
			{
			int *shape = (int*) alloc_memory( 2 * sizeof(int) );
			shape[0] = (int)(match_len / reg->nparens);
			shape[1] = (int)(reg->nparens);
			match_val->AssignAttribute("shape", new IValue(shape,2));
			}
		alloc_len = match_len = 0;
		match_res = 0;
		}
	else
		match_val = empty_ivalue();

	return match_val;
	}

void Regex::Describe( OStream& s ) const
	{
	if ( subst )
		s << "s!" << match << "!" << subst << "!";
	else
		s << "m!" << match << "!";
	}

Regex::~Regex()
	{
	if ( match ) free_memory( match );
	if ( subst ) free_memory( subst );
	}
