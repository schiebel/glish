// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef input_h
#define input_h

#include <stdio.h>

#include "Glish/glish.h"

class Sequencer;
class Str;
extern Sequencer* current_sequencer;

// Whether to use the editline library.
#define USE_EDITLINE 1

extern FILE* yyin;
extern int interactive;
extern int statement_can_end;
extern int first_line;
extern char* yytext;
extern const char* glish_init[];

extern int in_func_decl;

extern void restart_yylex( FILE* input_file );
extern void scan_strings( const char** strings );
extern char *readline_read( const char prompt[], char history='\0' );
extern int interactive_read( FILE* file, const char prompt[], char buf[],
				int max_size );

#endif	/* input_h */
