// $Id$
// Copyright (c) 1997,1998 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include "Regex.h"
#include "IValue.h"
#include "Glish/Stream.h"
#include "Glish/Reporter.h"

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
		err_ = strdup("bad regular expression");
		return;
		}

	pcnt = rcnt = 0;
	char digit[24];

	char *dptr = digit;
	char *last = subst;
	char *ptr = subst;

	if ( err_ ) free_memory( err_ );
	err_ = 0;
	while (  *ptr && !err_ )
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
					err_ = strdup( regx_buffer );
					}
				else
					{
					refs[rcnt] = (unsigned short) newdigit;
					if ( refs[rcnt] == 0 || refs[rcnt] > reg->nparens )
						{
						sprintf( regx_buffer, "paren reference out of range: %u", refs[rcnt] );
						err_ = strdup( regx_buffer );
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

	if ( ! err_ && last < ptr )
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
	if ( err_ ) return 0;

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
	if ( err_ ) free_memory( err_ );
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
				subst( subst_ ), reg(0), match(match_), match_end(0),
				error_string(0), divider(divider_), flags(flags_), match_count(0),
				desc(0)
	{
	pm.op_pmflags = FOLD(flags) ? PMf_FOLD : 0;
	if ( match ) compile( );
	}

Regex::Regex( const Regex &o ) : subst( o.subst ), reg(0), match( o.match ? strdup(o.match) : 0 ),
				match_end(0), error_string(0), divider( o.divider ), flags( o.flags ),
				match_count(0), desc(0)
	{
	pm.op_pmflags = FOLD(flags) ? PMf_FOLD : 0;
	if ( match ) compile( );
	}

Regex::Regex( const Regex *o )
	{
	desc = 0;

	if ( o )
		{
		subst.setStr( o->subst.str() );
		reg = 0;
		match = o->match ? strdup(o->match) : 0;
		match_end = 0;
		error_string = 0;
		divider = o-> divider;
		flags = o-> flags;
		match_count = 0;
		}
	else
		{
		subst = match = match_end = 0;
		reg = 0;
		error_string = 0;
		divider = '!';
		flags = 0;
		match_count = 0;
		}

	pm.op_pmflags = FOLD(flags) ? PMf_FOLD : 0;
	if ( match ) compile( );
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

#define EVAL_LOOP( KEY, SUBST_ACTION, COND )				\
	{								\
	/* don't match same null twice */				\
	KEY ( COND regxexec( reg, s, s_end, orig,			\
			     count && reg->endp[0] == reg->startp[0] ? 1 : 0,0,1 ) ) \
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
		XMATCH->update( orig, this );				\
		SUBST_ACTION						\
		s = reg->endp[0];					\
		}							\
	}

#define EVAL_ACTION( STR, SUBST_ACTION )				\
	char *orig = STR;						\
	char *s = orig;							\
	char *s_end = s + strlen(s);					\
									\
	count = 0;							\
									\
	EVAL_LOOP(if, SUBST_ACTION, )					\
									\
	if ( count > 0 && GLOBAL(flags) )				\
		EVAL_LOOP(while, SUBST_ACTION, s < s_end && )		\
									\
	if ( count == 0 )						\
		XMATCH->failed( orig, this );				\
									\
	match_count += count;


int Regex::Eval( char **&root, int &root_len, RegexMatch *XMATCH, int offset, int &len, 
		 IValue **error, int free_it, char **alt_src, int alt_len )
	{

	if ( &len == &glish_dummy_int )
		len = 1;

	if ( ! reg || ! match )
		{
		if ( error ) *error = (IValue*) Fail( "bad regular expression" );
		return -1;
		}

	int swap_io = 0;
	int resized = 0;

	if ( ! root )
		if ( alt_src )
			swap_io = 1;
		else
			{
			if ( error ) *error = (IValue*) Fail( "no source strings" );
			return -1;
			}

	match_count = 0;
	int count = 0;

	if ( subst.str() )
		{
		char **outs = root;
		int outs_len = root_len;
		int outs_off = offset;

		int splits = subst.splitCount();

		if ( swap_io )
			{
			outs = (char**) alloc_memory(sizeof(char*)*len);
			outs_len = len;
			outs_off = 0;
			root = alt_src;
			root_len = alt_len;
			}

		for ( int i=0,mc=0; i < len; ++i,++mc )
			{
			char *free_str = 0;
			subst.splitReset();
			char *dest = regx_buffer;
			EVAL_ACTION( root[ offset + (swap_io ? mc : i) ], SUBST_PLACE_ACTION )

			if ( subst.err() )
				{
				if ( free_str ) free_memory( free_str );
				if ( error ) *error = (IValue*) Fail( subst.err() );
				return -1;
				}

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
					len += count * splits;
					outs_len += count * splits;
					outs = (char**) realloc_memory(outs, sizeof(char*)*(outs_len+1));

					resized = 1;

					if ( outs_off+i+1 < root_len && ! swap_io )
						move_ptrs( &outs[outs_off+i+count*splits+1], &outs[outs_off+i+1], root_len-i );

					if ( free_it && ! swap_io ) free_str = outs[outs_off+i];

					subst.split(&outs[outs_off+i],regx_buffer);
					i += count * splits;

					if (! swap_io ) { root = outs; root_len = outs_len; }
					}
				else
					{
					if ( free_it && ! swap_io ) free_memory(outs[outs_off+i]);
					outs[outs_off+i] = strdup( regx_buffer );
					}
				}
			else if ( swap_io )
				{
				if ( free_it && ! swap_io ) free_str = outs[outs_off+i];
				outs[outs_off+i] = strdup(root[ offset + (swap_io ? mc : i) ]);
				}

			if ( free_str ) free_memory( free_str );
			}

		if ( resized || swap_io ) { root = outs; root_len = outs_len; }

		return match_count;
		}
	else
		{
		if ( swap_io )
			{
			root = alt_src;
			root_len = alt_len;
			}

		for ( int i=0; i < len; ++i )
			{
			EVAL_ACTION( root[offset+i], )
			}

		return match_count;
		}
	}

int Regex::Describe( OStream& s, const ioOpt & ) const
	{
	if ( subst.str() )
		s << "s" << divider << match << divider << subst.str() << divider;
	else
		s << "m" << divider << match << divider;
	return 1;
	}


const char *Regex::Description( ) const
	{
	if ( desc ) return desc;

	if ( ! match || ! reg )
		{
		((Regex*)this)->desc = strdup( "bad <regex>" );
		return desc;
		}

	int mlen = match_end - match;
	if ( subst.str() )
		{
		int slen = strlen(subst.str());
		((Regex*)this)->desc = (char*) alloc_memory( mlen + slen + 5 );
		char *ptr = desc;
		*ptr++ = 's'; *ptr++ = divider;
		memcpy( ptr, match, mlen );
		ptr += mlen; *ptr++ = divider;
		memcpy( ptr, subst.str(), slen );
		ptr += slen; *ptr++ = divider; *ptr = '\0';
		}
	else
		{
		((Regex*)this)->desc = (char*) alloc_memory( mlen + 4 );
		char *ptr = desc;
		*ptr++ = 'm'; *ptr++ = divider;
		memcpy( ptr, match, mlen );
		ptr += mlen; *ptr++ = divider; *ptr = '\0';
		}

	return desc;
	}


Regex::~Regex()
	{
	if ( match ) free_memory( match );
	if ( error_string ) free_memory( error_string );
	}

//
// --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- --- ---
//
struct str_node {
	str_node( char *s ) : base(s) { }
	~str_node( );
	void update( Regex *reg );
	IValue *get( Regex *reg );
	char *base;
	name_list strs;
};

str_node::~str_node( )
	{
	for ( int i=strs.length()-1; i >= 0; --i )
		free_memory( strs.remove_nth(i) );
	}

void str_node::update( Regex *reg )
	{
	for ( register int cnt=1; cnt <= reg->R()->nparens; cnt++ )
		{
		if ( reg->R()->endp[cnt] > reg->R()->startp[cnt] )
			{
			register int slen = reg->R()->endp[cnt]-reg->R()->startp[cnt];
			register char *buf = (char*) alloc_memory( slen+1 );
			if ( slen > 0 ) memcpy(buf, reg->R()->startp[cnt], slen);
			buf[slen] = '\0';
			strs.append( buf );
			}
		else
			strs.append( strdup("") );
		}
	}

IValue *str_node::get( Regex *reg )
	{
	int len = strs.length();
	int parens = reg->R()->nparens;

	if ( len <= 0 || parens <= 0 ||
	     len % parens != 0 )
		{
		IValue *empty = empty_ivalue();
		empty->Polymorph( TYPE_STRING );
		return  empty;
		}

	char **ary = (char**) alloc_memory( sizeof(char*) * len );
	IValue *ret = 0;
	int rows = len/parens;

	if ( parens > 1 && len > parens )
		{
		int from = len-1;
		for ( int row=rows-1; row >= 0; --row )
			for ( int col=parens-1; col >= 0; --col )
				ary[col*rows+row] = strs.remove_nth(from--);
		ret = new IValue( (charptr*) ary, len );
		int *shape_i = (int*) alloc_memory( sizeof(int) * 2 );
		shape_i[0] = rows;
		shape_i[1] = parens;
		ret->AssignAttribute( "shape", new IValue( shape_i, 2 ) );
		}
	else if ( len > 0 )
		{
		for ( int i=len-1; i >= 0; --i )
			ary[i] = strs.remove_nth(i);
		ret = new IValue( (charptr*) ary, len );
		}

	return ret;
	}

glish_declare(PList,str_node);
typedef PList(str_node) str_node_list;

struct match_node {
	match_node( Regex *r ) : reg(r) { Ref(reg); }
	~match_node( );
	void update( char * );
	void failed( char * );
	IValue *get( );
	Regex *reg;
	str_node_list list;
};

match_node::~match_node( )
	{
	Unref(reg);
	for ( int i=list.length()-1; i >= 0; --i )
		delete list.remove_nth(i);
	}

void match_node::update( char *s )
	{
	for ( int i=0; i < list.length(); ++i )
		if ( list[i]->base == s )
			{
			list[i]->update( reg );
			return;
			}

	str_node *newsn = new str_node(s);
	newsn->update( reg );
	list.append( newsn );
	}

void match_node::failed( char *s )
	{
	for ( int i=0; i < list.length(); ++i )
		if ( list[i]->base == s )
			return;

	list.append( new str_node(s) );
	}

IValue *match_node::get( )
	{
	if ( list.length() > 1 )
		{
		recordptr newr = create_record_dict();
		IValue *ret = new IValue( newr );
		for ( int i=0; i < list.length(); ++i )
			newr->Insert(ret->NewFieldName(1),list[i]->get(reg));
		return ret;
		}

	else if ( list.length() == 1 )
		return list[0]->get(reg);

	else
		{
		IValue *empty = empty_ivalue();
		empty->Polymorph( TYPE_STRING );
		return  empty;
		}
	}

RegexMatch::~RegexMatch( )
	{
	clear( );
	}

void RegexMatch::clear( )
	{
	if ( last ) Unref(last);
	last = 0;

	for ( int i=list.length()-1; i >= 0; --i )
		delete list.remove_nth(i);
	}

void RegexMatch::add( Regex *r )
	{
	for ( int i=0; i < list.length(); ++i )
		if ( list[i]->reg == r ) return;
	list.append( new match_node(r) );
	}

void RegexMatch::update( char *str, Regex *r )
	{
	for ( int i=0; i < list.length(); ++i )
		if ( list[i]->reg == r )
			{
			list[i]->update( str );
			return;
			}

	match_node *newmn = new match_node(r);
	newmn->update( str );
	list.append( newmn );
	}

void RegexMatch::failed( char *str, Regex *r )
	{
	for ( int i=0; i < list.length(); ++i )
		if ( list[i]->reg == r )
			{
			list[i]->failed( str );
			return;
			}

	match_node *newmn = new match_node(r);
	newmn->failed( str );
	list.append( newmn );
	}

IValue *RegexMatch::get( )
	{
	if ( last ) return last;

	if ( list.length() > 1 )
		{
		recordptr newr = create_record_dict();
		last = new IValue( newr );
		for ( int i=0; i < list.length(); ++i )
			newr->Insert(last->NewFieldName(1),list[i]->get());
		}

	else if ( list.length() == 1 )
		last = list[0]->get();

	return last;
	}
