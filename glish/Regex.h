// $Id$
// Copyright (c) 1997 Associated Universities Inc.
#ifndef regex_h_
#define regex_h_

#include "Glish/Object.h"
#include "regx/regexp.h"

class IValue;
class OStream;

class regxsubst {
    public:
	regxsubst() : subst(0), reg(0),
			startp(0), endp(0), pcnt(0), psze(0),
			refs(0), rcnt(0), rsze(0), bsize(0), err(0) { }
	regxsubst( char *s ) : subst(s), reg(0),
			startp(0), endp(0), pcnt(0), psze(0),
			refs(0), rcnt(0), rsze(0), bsize(0), err(0) { }
	regxsubst( const regxsubst &o ) : subst( o.subst ? strdup(o.subst) : 0 ),
			reg(0), startp(0), endp(0), pcnt(0), psze(0),
			refs(0), rcnt(0), rsze(0), bsize(0), err(0) { } 
	~regxsubst();
	void compile( regexp *reg_ );
	void compile( regexp *reg_, char *subst_ );
	char *apply( const char *str );
	const char *error( ) const { return err; }
	const char *str( ) const { return subst; };
	void setStr( char *s );
	void setStr( const char *s ) { setStr( (char*)( s ? strdup(s) : 0 ) ); }
    protected:
	char *subst;
	regexp *reg;
	char **startp;
	char **endp;
	int pcnt;
	int psze;
	unsigned short *refs;
	int rcnt;
	int rsze;
	int bsize;
	char *err;
};

class Regex : public GlishObject {
     public:

	enum regex_type { MATCH, SUBST };

	regex_type Type() const { return subst.str() ? SUBST : MATCH; }

	Regex( char *match_, char divider_ = '!', char *subst_ = 0 );
	Regex( const Regex &oth );
	Regex( const Regex *oth );

	IValue *Eval( char *string );
	IValue *Eval( char **strs, int len );

	//
	// Lower level routines to allow a series of REs to be applied to
	// each string. The latter allocates memory which must be freed.
	//
	glish_bool mEval( char *string );
	char *sEval( char *string );

	//
	// returns non-null string if an error occurred
	// while creating the regular expression
	//
	const char *Error( ) const { return error_string; }

	void Describe( OStream& s ) const;

	// returns an allocated string
	char *Description( char *ret = 0 ) const;

	IValue *GetMatch( );

	~Regex();

     protected:
	void compile( );

	regxsubst subst;
	regexp *reg;
	PMOP   pm;

	char   *match;
	char   *match_end;

	char   *error_string;

	IValue *match_val;
	char   **match_res;
	int    match_len;
	int    alloc_len;

	char divider;
};

#endif
