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

Regex::Regex( char *match_, char *subst_ ) : subst( subst_ ), reg(0), match(match_), match_end(0),
						match_val(0), match_res(0), match_len(0), alloc_len(0),
						error_string(0)
	{
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

IValue *Regex::Eval( char *string )
	{

	if ( ! reg || ! match )
		return (IValue*) Fail( "bad regular expression" );

	ADJUST_MATCH(,)

	match_len = reg->nparens;
	glish_bool ret = regxexec( reg, string, string+strlen(string), string, 1,0,1 ) ? glish_true : glish_false;

	if ( subst.str() )
		{
		char **outs = (char**) alloc_memory(sizeof(char*));
		EVAL_ACTION( ret , cnt-1 , cnt , , outs[0] = subst.apply( string );, outs[0] = strdup("");)
		return new IValue( (charptr*) outs, 1 );
		}
	else
		{
		EVAL_ACTION( ret , cnt-1 , cnt , , , )
		return new IValue(  ret );
		}
	}

IValue *Regex::Eval( char **strs, int len )
	{

	if ( ! reg || ! match )
		return (IValue*) Fail( "bad regular expression" );

	ADJUST_MATCH(*len,)

	IValue *ret = 0;

	match_len = reg->nparens * len;

	if ( subst.str() )
		{
		char **outs = (char**) alloc_memory(sizeof(char*)*len);
		for ( int i=0; i < len; ++i )
			{
			glish_bool ret = regxexec( reg, strs[i], strs[i]+strlen(strs[i]), strs[i], 1,0,1 ) ? glish_true : glish_false;
			EVAL_ACTION( ret , i+(cnt-1)%reg->nparens*len , index ,		\
				     register int index = i+cnt%reg->nparens*len,	\
				     outs[i] = subst.apply(strs[i]);, outs[i] = strdup(""); )
			}
		return new IValue(  (charptr*) outs, len );
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
		s << "s!" << match << "!" << subst.str() << "!";
	else
		s << "m!" << match << "!";
	}


char *Regex::Description( ) const
	{
	if ( ! match || ! reg ) return strdup( "bad <regex>" );

	char *ret = 0;

	int mlen = match_end - match;
	if ( subst.str() )
		{
		int slen = strlen(subst.str());
		ret = (char*) alloc_memory( mlen + slen + 5 );
		char *ptr = ret;
		*ptr++ = 's'; *ptr++ = '!';
		memcpy( ptr, match, mlen );
		ptr += mlen; *ptr++ = '!';
		memcpy( ptr, subst.str(), slen );
		*ptr += slen; *ptr++ = '!'; *ptr = '\0';
		}
	else
		{
		ret = (char*) alloc_memory( mlen + 4 );
		char *ptr = ret;
		*ptr++ = 'm'; *ptr++ = '!';
		memcpy( ptr, match, mlen );
		ptr += mlen; *ptr++ = '!'; *ptr = '\0';
		}

	return ret;
	}


Regex::~Regex()
	{
	if ( match ) free_memory( match );
	if ( error_string ) free_memory( error_string );
	}
