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
	regxsubst() : subst(0), reg(0),	startp(0), endp(0),
			pcnt(0), psze(0), refs(0), rcnt(0),
			rsze(0), splits(0), scnt(0), ssze(0),
			err_(0), split_count(0) { }
	regxsubst( char *s ) : subst(s), reg(0), startp(0), endp(0),
			pcnt(0), psze(0), refs(0), rcnt(0),
			rsze(0), splits(0), scnt(0), ssze(0),
			err_(0), split_count(0) { }
	regxsubst( const regxsubst &o ) : subst( o.subst ? strdup(o.subst) : 0 ),
			reg(0), startp(0), endp(0), pcnt(0), psze(0),
			refs(0), rcnt(0), rsze(0), splits(0), scnt(0),
			ssze(0), err_(0), split_count(0) { } 
	~regxsubst();
	void compile( regexp *reg_ );
	void compile( regexp *reg_, char *subst_ );
	char *apply( char *dest );
	void split( char **dest, const char *src );
	const char *err( ) const { return err_; }
	const char *str( ) const { return subst; };
	void setStr( char *s );
	void setStr( const char *s ) { setStr( (char*)( s ? strdup(s) : 0 ) ); }

	int splitCount( ) const { return split_count; }
	void splitReset( ) { scnt = 0; }

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
	char **splits;
	int scnt;
	int ssze;
	char *err_;
	int split_count;
};

class Regex : public GlishObject {
     public:

	static unsigned int GLOBAL( unsigned int mask=~((unsigned int) 0) ) { return mask & 1<<0; }
	static unsigned int FOLD( unsigned int mask=~((unsigned int) 0) ) { return mask & 1<<1; }

	enum regex_type { MATCH, SUBST };

	regex_type Type() const { return subst.str() ? SUBST : MATCH; }

	Regex( char *match_, char divider_ = '!', unsigned int flags_ = 0, char *subst_ = 0 );
	Regex( const Regex &oth );
	Regex( const Regex *oth );

	//
	// latter two parameters are ONLY used when doing a substitution, i.e. for
	// matches the number of matches is always returned.
	//
	IValue *Eval( char **&strs, int &len, int in_place = 0, int free_it = 0,
		      int return_matches = 1, int can_resize = 0, char **alt_src = 0 );

	//
	// Lower level routines to allow a series of REs to be applied to
	// each string. The latter allocates memory which must be freed.
	//
	int Eval( char *&string, int in_place = 0 );

	//
	// returns non-null string if an error occurred
	// while creating the regular expression
	//
	const char *Error( ) const { return error_string ? error_string : subst.err(); }

	void Describe( OStream& s ) const;

	// returns an allocated string
	const char *Description( ) const;

	IValue *GetMatch( );

	//
	// how many entries is the string going to be split into
	// with each match?
	//
	int Splits() const { return subst.str() ? subst.splitCount() : 0; }

	int matchCount() const { return match_count; }

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
	unsigned int flags;
	int match_count;

	char *desc;
};

struct match_node;
glish_declare(PList,match_node);
typedef PList(match_node) match_node_list;

class MatchMgr {
    public:
	MatchMgr( ) { }
	~MatchMgr( );
	void clear( );
	void add( Regex * );
	void update( Regex * );
	IValue *get( );
    private:
	match_node_list list;
};

extern void copy_regexs( void *to_, void *from_, unsigned int len );

#endif
