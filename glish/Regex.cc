// $Id$
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include "Regex.h"
#include "IValue.h"
#include "Glish/Stream.h"
#include "Reporter.h"

static char regx_buffer[2048];
static jmp_buf regx_jmpbuf;

void glish_regx_error_handler( const char *pat, va_list va )
	{
	vsprintf( regx_buffer, pat, va );
	longjmp( regx_jmpbuf, 1 );
	}

void init_regex( ) { regxseterror( glish_regx_error_handler ); }


#define memcpy_killbs( to, from, len ) \
{char last='\0'; int j=0; for (int i=0; i<len; ++i) last=(((to)[j] = (from)[i])=='\\'?(last=='\\'?(++j,'\0'):'\\'):(to)[j++]);len=j;}

#define SIZE_P								\
	{								\
	if ( pcnt >= psze )						\
		{							\
		if (  psze > 0 )					\
			{						\
			psze *= 2;					\
			startp = (char**) realloc_memory( startp, psze * sizeof(char*) ); \
			endp = (char**) realloc_memory( startp, psze * sizeof(char*) ); \
			}						\
		else							\
			{						\
			psze = 5;					\
			startp = (char**) alloc_memory( psze * sizeof(char*) ); \
			endp = (char**) alloc_memory( psze * sizeof(char*) ); \
			}						\
		}							\
	}
#define SIZE_R								\
	{								\
	if ( rcnt >= rsze )						\
		{							\
		if (  rsze > 0 )					\
			{						\
			rsze *= 2;					\
			refs = (unsigned short*) realloc_memory( refs, rsze * sizeof(unsigned short) ); \
			}						\
		else							\
			{						\
			rsze = 5;					\
			refs = (unsigned short*) alloc_memory( rsze * sizeof(unsigned short) ); \
			}						\
		}							\
	}


void regxsubst::compile( regexp *reg_, char *subst_ )
	{
	if ( subst ) free_memory( subst );
	subst = subst_;
	compile( reg_ );
	}

void regxsubst::compile( regexp *reg_ )
	{
	reg = reg_;

	if ( ! reg || ! subst )
		{
		err = strdup("bad regular expression");
		return;
		}

	pcnt = rcnt = bsize = 0;
	char digit[24];

	char *dptr = digit;
	char *last = subst;
	char *ptr = subst;

	if ( err ) free_memory( err );
	err = 0;
	while (  *ptr && !err )
		switch ( *ptr++ )
			{
		    case '$':
			if ( isdigit((unsigned char)(*ptr)) )
				{
				if ( last+1 < ptr )
					{
					SIZE_P
					startp[pcnt] = last;
					endp[pcnt++] = ptr-1;
					SIZE_R
					refs[rcnt++] = 0;
					}

				*dptr++ = *ptr++;
				for ( ; isdigit((unsigned char)(*ptr)); *dptr++=*ptr++ );
				*dptr = '\0';
				SIZE_R
				int newdigit =  atoi(digit);
				if ( newdigit < 0 || newdigit > 65535 )
					{
					sprintf( regx_buffer, "paren reference overflow: %d", newdigit);
					err = strdup( regx_buffer );
					}
				else
					{
					refs[rcnt] = (unsigned short) newdigit;
					if ( refs[rcnt] == 0 || refs[rcnt] > reg->nparens )
						{
						sprintf( regx_buffer, "paren reference out of range: %u", refs[rcnt] );
						err = strdup( regx_buffer );
						}
					else rcnt++;
					}

				dptr = digit;
				last = ptr;
				}
			break;
		    case '\\': ++ptr;
			}

	if ( ! err && last < ptr )
		{
		SIZE_P
		startp[pcnt] = last;
		endp[pcnt++] = ptr;
		SIZE_R
		refs[rcnt++] = 0;
		}
	}

char *regxsubst::apply( const char *str )
	{
	if ( err ) return 0;

	char *out = regx_buffer;
	int off = 0;

	if ( reg->startp[0] != str )
		{
		int len = reg->startp[0] - str;
		memcpy( out, str, len );
		out += len;
		}

	for ( int x = 0; x < rcnt; ++x )
		{
		if ( refs[x] )
			{
			int len = reg->endp[refs[x]] - reg->startp[refs[x]];
			memcpy_killbs( out, reg->startp[refs[x]], len );
			out += len;
			}
		else
			{
			int len = endp[off] - startp[off];
			memcpy_killbs( out, startp[off], len );
			out += len;
			++off;
			}
		}

	const char *strend = str + strlen(str);
	if ( reg->endp[0] !=  strend )
		{
		int len = strend - reg->endp[0];
		memcpy( out, reg->endp[0], len );
		out += len;
		}

	*out = '\0';
	return strdup(regx_buffer);
	}

void regxsubst::setStr( char *s )
	{
	if ( subst ) free_memory(subst);
	subst = s;
	}

regxsubst::~regxsubst( )
	{
	if ( startp ) free_memory( startp );
	if ( endp ) free_memory( endp );
	if ( refs ) free_memory( refs );
	if ( err ) free_memory( err );
	if ( subst ) free_memory(subst);
	}


void Regex::compile( )
	{
	if ( ! match ) return;

	if ( setjmp(regx_jmpbuf) == 0 )
		{
		match_end = match + strlen(match);
		reg = regxcomp( match, match_end, &pm );
		if ( subst.str() ) subst.compile( reg );
		}
	else
		{
		reg = 0;	// probably need to free it
		error_string = strdup( regx_buffer );
		}
	}

Regex::Regex( char *match_, char divider_, unsigned int flags_, char *subst_ ) :
				subst( subst_ ), reg(0), match(match_),
				match_end(0) ,match_val(0), match_res(0), match_len(0), alloc_len(0),
				error_string(0), divider(divider_), flags(flags_)
	{
	pm.op_pmflags = FOLD(flags) ? PMf_FOLD : 0;
	if ( match ) compile( );
	}

Regex::Regex( const Regex &o ) : subst( o.subst ), reg(0), match( o.match ? strdup(o.match) : 0 ),
				match_end(0), match_val(0), match_res(0), match_len(0), alloc_len(0),
				error_string(0), divider( o.divider ), flags( o.flags )
	{
	pm.op_pmflags = FOLD(flags) ? PMf_FOLD : 0;
	if ( match ) compile( );
	}

Regex::Regex( const Regex *o )
	{

	if ( o )
		{
		subst.setStr( o->subst.str() );
		reg = 0;
		match = o->match ? strdup(o->match) : 0;
		match_end = 0;
		match_val = 0;
		match_res = 0;
		match_len = 0;
		alloc_len = 0;
		error_string = 0;
		divider = o-> divider;
		flags = o-> flags;
		}
	else
		{
		subst = match = match_end = 0;
		reg = 0;
		match_val = 0;
		match_res = 0;
		match_len = 0;
		alloc_len = 0;
		error_string = 0;
		divider = '!';
		flags = 0;
		}

	pm.op_pmflags = FOLD(flags) ? PMf_FOLD : 0;
	if ( match ) compile( );
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

#define EVAL_ACTION(OP,INDEXINIT,COUNT,COUNTINIT,SUBSTT,SUBSTF)		\
if ( OP == glish_true )							\
	{								\
	if ( match_len > 0 )						\
		{							\
		for ( register int cnt=1; cnt <= reg->nparens; cnt++ )	\
			{						\
			register int index = INDEXINIT;			\
			register int slen = reg->endp[cnt]-reg->startp[cnt]; \
			match_res[index] = (char*) alloc_memory( slen+1 ); \
			if ( slen > 0 ) memcpy(match_res[index], reg->startp[cnt], slen); \
			match_res[index][slen] = '\0';			\
			}						\
		}							\
	SUBSTT								\
	}								\
else									\
	{								\
	if ( match_len > 0 )						\
		{							\
		for ( register int cnt=0; cnt < reg->nparens; cnt++ )	\
			{						\
			COUNTINIT;					\
			match_res[COUNT] = (char*) alloc_memory( 1 );	\
			match_res[COUNT][0] = '\0';			\
			}						\
		}							\
	SUBSTF								\
	}


IValue *Regex::Eval( char **strs, int len, int in_place, int return_matches )
	{

	if ( ! reg || ! match )
		return (IValue*) Fail( "bad regular expression" );

	ADJUST_MATCH(*len,)

	IValue *ret = 0;

	match_len = reg->nparens * len;

	if ( subst.str() )
		{
		char **outs = strs;
		glish_bool *mret = 0;

		if ( ! in_place )
			outs = (char**) alloc_memory(sizeof(char*)*len);
		else if ( return_matches )
			mret = (glish_bool*) alloc_memory(sizeof(glish_bool)*len);

		for ( int i=0; i < len; ++i )
			{
			glish_bool ret = regxexec( reg, strs[i], strs[i]+strlen(strs[i]), strs[i], 1,0,1 ) ? glish_true : glish_false;
			if ( return_matches ) mret[i] = ret;

			EVAL_ACTION( ret , i+(cnt-1)%reg->nparens*len , index ,		\
				     register int index = i+cnt%reg->nparens*len,	\
				     register char *tmp = outs[i]; 			\
				     outs[i] = subst.apply(strs[i]);			\
				     if ( in_place ) free_memory( tmp );,		\
				     if ( ! in_place ) outs[i] = strdup(strs[i]); )
			}

		return in_place ? return_matches ? new IValue( mret, len ) : 0 : new IValue(  (charptr*) outs, len );
		}
	else
		{
		glish_bool *r = (glish_bool*) alloc_memory( sizeof(glish_bool) * len );
		for ( int i=0; i < len; ++i )
			{
			r[i] = regxexec( reg, strs[i], strs[i]+strlen(strs[i]), strs[i], 1,0,1 ) ? glish_true : glish_false;
			EVAL_ACTION( r[i] , i+(cnt-1)%reg->nparens*len , index ,	\
				     register int index = i+cnt%reg->nparens*len,,)
			}

		return new IValue( r, len );
		}
	}


glish_bool Regex::Eval( char *&string, int in_place = 0 )
	{

	ADJUST_MATCH(,)

	match_len = reg->nparens;
	glish_bool ret = regxexec( reg, string, string+strlen(string), string, 1,0,1 ) ? glish_true : glish_false;

	if ( subst.str() && in_place )
		{
		EVAL_ACTION( ret , cnt-1 , cnt , ,				\
			     register char *tmp = string; 			\
			     string = subst.apply(string);			\
			     free_memory( tmp );, )
		}
	else
		{
		EVAL_ACTION( ret , cnt-1 , cnt , , , )
		}

	return ret;
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
	if ( subst.str() )
		s << "s" << divider << match << divider << subst.str() << divider;
	else
		s << "m" << divider << match << divider;
	}


char *Regex::Description( char *ret ) const
	{
	if ( ! match || ! reg ) return strdup( "bad <regex>" );

	int mlen = match_end - match;
	if ( subst.str() )
		{
		int slen = strlen(subst.str());
		if ( ! ret ) ret = (char*) alloc_memory( mlen + slen + 5 );
		char *ptr = ret;
		*ptr++ = 's'; *ptr++ = divider;
		memcpy( ptr, match, mlen );
		ptr += mlen; *ptr++ = divider;
		memcpy( ptr, subst.str(), slen );
		*ptr += slen; *ptr++ = divider; *ptr = '\0';
		}
	else
		{
		if ( ! ret ) ret = (char*) alloc_memory( mlen + 4 );
		char *ptr = ret;
		*ptr++ = 'm'; *ptr++ = divider;
		memcpy( ptr, match, mlen );
		ptr += mlen; *ptr++ = divider; *ptr = '\0';
		}

	return ret;
	}


Regex::~Regex()
	{
	if ( match ) free_memory( match );
	if ( error_string ) free_memory( error_string );
	}
