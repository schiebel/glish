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

#define move_ptrs(to,from,count)					\
{for( int XIX=count-1; XIX>=0; --XIX ) (to)[XIX]=(from)[XIX];}

#define SPLIT_SIG 65535

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

#define SIZE_S								\
	{								\
	if ( scnt >= ssze )						\
		{							\
		if (  ssze > 0 )					\
			{						\
			ssze *= 2;					\
			splits = (char**) realloc_memory( splits, ssze * sizeof(char*) ); \
			}						\
		else							\
			{						\
			ssze = 5;					\
			splits = (char**) alloc_memory( ssze * sizeof(char*) ); \
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

	split_count = 0;

	if ( ! reg || ! subst )
		{
		err = strdup("bad regular expression");
		return;
		}

	pcnt = rcnt = 0;
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
				if ( newdigit < 0 || newdigit > 65534 )
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
			else if ( *ptr == '$' )
				{
				if ( last+1 < ptr )
					{
					SIZE_P
					startp[pcnt] = last;
					endp[pcnt++] = ptr-1;
					SIZE_R
					refs[rcnt++] = 0;
					}

				SIZE_R
				refs[rcnt++] = SPLIT_SIG;
				++split_count;
				++ptr;

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

char *regxsubst::apply( char *dest )
	{
	if ( err ) return 0;

	int off = 0;

	for ( int x = 0; x < rcnt; ++x )
		{
		if ( refs[x] == SPLIT_SIG )
			{
			SIZE_S
			splits[scnt++] = dest;
			}
		else if ( refs[x] )
			{
			int len = reg->endp[refs[x]] - reg->startp[refs[x]];
			memcpy_killbs( dest, reg->startp[refs[x]], len );
			dest += len;
			}
		else
			{
			int len = endp[off] - startp[off];
			memcpy_killbs( dest, startp[off], len );
			dest += len;
			++off;
			}
		}

	return dest;
	}

void regxsubst::split( char **dest, const char *src )
	{
	if ( scnt <= 0 ) return;

	if ( splits[0] < src ) fatal->Report( "initial split is before source in regxsubst::split( )" );

	for ( int i=0; i < scnt; ++i, ++dest )
		{
		int len = splits[i] - src;
		if ( len > 0 )
			{
			*dest = (char*) alloc_memory(len+1);
			memcpy( *dest, src, len );
			(*dest)[len] = '\0';
			}
		else
			*dest = strdup("");

		src = splits[i];
		}

	*dest = strdup( src );
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
				error_string(0), divider(divider_), flags(flags_), match_count(0)
	{
	pm.op_pmflags = FOLD(flags) ? PMf_FOLD : 0;
	if ( match ) compile( );
	}

Regex::Regex( const Regex &o ) : subst( o.subst ), reg(0), match( o.match ? strdup(o.match) : 0 ),
				match_end(0), match_val(0), match_res(0), match_len(0), alloc_len(0),
				error_string(0), divider( o.divider ), flags( o.flags ), match_count(0)
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
		match_count = 0;
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
		match_count = 0;
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

#define MATCH_ACTION(OP,INDEXINIT,COUNT,COUNTINIT)			\
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
	}

#define SUBST_PLACE_ACTION						\
	if ( reg->startp[0] && reg->endp[0] )				\
		{							\
		if ( reg->startp[0] != s )				\
			{						\
			int len = reg->startp[0] - s;			\
			memcpy( dest, s, len );				\
			dest += len;					\
			}						\
		dest = subst.apply( dest );				\
		}

#define EVAL_LOOP( KEY, SUBST_ACTION )					\
	KEY ( s < s_end && regxexec( reg, s, s_end, orig, 1,0,1 ) )	\
		{							\
		++count;						\
									\
		if ( reg->subbase && reg->subbase != orig )		\
			{						\
			char *m = s;					\
			s = orig;					\
			orig = reg->subbase;				\
			s = orig + (m - s);				\
			s_end = s + (s_end - m);			\
			}						\
									\
		SUBST_ACTION						\
		s = reg->endp[0];					\
		}

#define EVAL_ACTION( STR, SUBST_ACTION )				\
	char *dest = regx_buffer;					\
	char *orig = STR;						\
	char *s = orig;							\
	char *s_end = s + strlen(s);					\
									\
	count = 0;							\
	if ( GLOBAL(flags) )						\
		EVAL_LOOP(while, SUBST_ACTION)				\
	else								\
		EVAL_LOOP(if, SUBST_ACTION)				\
	match_count += count;


IValue *Regex::Eval( char **&strs, int &len, int in_place, int free_it,
		     int return_matches, int can_resize, char **alt_src )
	{

	if ( ! reg || ! match )
		return (IValue*) Fail( "bad regular expression" );

	int swap_io = 0;
	int resized = 0;

	if ( ! strs )
		if ( alt_src )
			swap_io = 1;
		else
			return 0;

	int inlen = len;
	ADJUST_MATCH(*len,)

	IValue *ret = 0;

	match_count = 0;
	match_len = reg->nparens * len;
	int count = 0;

	if ( subst.str() )
		{
		char **outs = strs;
		int *mret = 0;

		int splits = subst.splitCount();

		if ( ! in_place || swap_io )
			{
			outs = (char**) alloc_memory(sizeof(char*)*len);
			if ( swap_io ) strs = alt_src;
			}
		else 
			{
			if ( return_matches )
				mret = (int*) alloc_memory(sizeof(int)*len);
			if ( splits && ! can_resize )
				{
				splits = 0;
				warn->Report( "line splitting requires resizing, can't do it" );
				}
			}

		for ( int i=0,mc=0; i < len; ++i,++mc )
			{
			char *free_str = 0;
			subst.splitReset();
			EVAL_ACTION( strs[ in_place && ! swap_io ? i : mc ], SUBST_PLACE_ACTION )

			if ( return_matches ) mret[mc] = count;

			if ( count )
				{
				if ( s < s_end )
					{
					memcpy( dest, s, s_end - s );
					dest += s_end - s;
					}
				*dest = '\0';

				if ( splits )
					{
					int xlenx = len;
					len += count * splits;
					outs = (char**) realloc_memory(outs, sizeof(char*)*(len+1));
					if ( in_place && ! swap_io ) strs = outs;
					resized = 1;

					if ( i+1 < xlenx && in_place && ! swap_io )
						move_ptrs( &outs[i+count*splits+1], &outs[i+1], xlenx-i );

					if ( in_place && free_it && ! swap_io ) free_str = outs[i];

					subst.split(&outs[i],regx_buffer);
					i += count * splits;
					}
				else
					{
					if ( in_place && free_it && ! swap_io ) free_memory(outs[i]);
					outs[i] = strdup( regx_buffer );
					}
				}
			else if ( ! in_place || swap_io )
				{
				if ( free_it && ! swap_io ) free_str = outs[i];
				outs[i] = strdup(strs[ in_place && ! swap_io ? i : mc ]);
				}

			MATCH_ACTION( count, mc+(cnt-1)%reg->nparens*len , index ,	\
				     register int index = mc+cnt%reg->nparens*len )

			if ( free_str ) free_memory( free_str );

			}

		if ( resized || swap_io ) strs = outs;

		return in_place ? return_matches ? new IValue( mret, len ) : 0 : new IValue(  (charptr*) outs, len );
		}
	else
		{
		int *r = (int*) alloc_memory( sizeof(int) * len );

		for ( int i=0; i < len; ++i )
			{
			EVAL_ACTION( strs[i], )

			r[i] = count;
			MATCH_ACTION( count, i+(cnt-1)%reg->nparens*len , index ,	\
				     register int index = i+cnt%reg->nparens*len )
			}

		return new IValue( r, len );
		}
	}


int Regex::Eval( char *&string, int in_place )
	{

	ADJUST_MATCH(,)

	match_len = reg->nparens;

	int count = 0;
	if ( subst.str() )
		{
		if ( in_place )
			{
			EVAL_ACTION( string, SUBST_PLACE_ACTION )

			if ( count )
				{
				if ( s < s_end )
					{
					memcpy( dest, s, s_end - s );
					dest += s_end - s;
					}
				*dest = '\0';

				register char *tmp = string;
				string = strdup( regx_buffer );
				free_memory( tmp );
				}

			MATCH_ACTION( count , cnt-1 , cnt , )
			}
		}
	else
		{
		EVAL_ACTION( string, )

		MATCH_ACTION( count , cnt-1 , cnt , )
		}

	return count;
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
		ptr += slen; *ptr++ = divider; *ptr = '\0';
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
