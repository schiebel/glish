// $Id$
// Copyright (c) 1997 Associated Universities Inc.
#ifndef regex_h_
#define regex_h_

#include "Glish/Object.h"
#include "regx/regexp.h"

class IValue;
class OStream;

class Regex : public GlishObject {
     public:

	Regex( char *match_, char *subst_ = 0 );

	IValue *Eval( char *string );
	IValue *Eval( char **strs, int len );

	void Describe( OStream& s ) const;

	IValue *GetMatch( );

	~Regex();
     protected:
	regexp *reg;
	PMOP   pm;

	char   *match;
	char   *match_end;
	char   *subst;

	IValue *match_val;
	char   **match_res;
	int    match_len;
	int    alloc_len;
};

#endif
