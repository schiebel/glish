// $Id$
// Copyright (c) 1998 Associated Universities Inc.
#ifndef file_h
#define file_h

#include "Glish/Object.h"
#include "stdio.h"

class File : public GlishObject {
    public:
	enum Type { IN, OUT, PIN, POUT, PBOTH, ERR };
	File( char *str_ );
	~File( );
	char *read( );
	void write( charptr buf );
	void close( Type t=PBOTH );
    private:
	char *clean_string( );
	Type type;
	char *str;
	FILE *in;
	FILE *out;
};

#endif
