// $Id$
// Copyright (c) 1998 Associated Universities Inc.
#ifndef file_h
#define file_h

#include "Glish/Object.h"
#include "stdio.h"

class File : public GlishObject {
    public:
	enum Type { IN, OUT, PIN, POUT, PBOTH, ERR };
	File( const char *str_ );
	~File( );
	char *read( );
	void write( charptr buf );
	void close( Type t=PBOTH );
	void Describe( OStream& s ) const;
	const char *Description( ) const;
	Type type( ) { return type_; }
    private:
	char *clean_string( );
	Type type_;
	char *str;
	char *desc;
	FILE *in;
	FILE *out;
};

#endif
