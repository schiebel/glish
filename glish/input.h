// $Header$

#ifndef input_h
#define input_h

#include <stdio.h>

#include "Glish/glish.h"

class Sequencer;
extern Sequencer* current_sequencer;

// Line number to associate with the current expression/statement.
extern int line_num;

extern char* input_file_name;
extern FILE* yyin;
extern bool interactive;
extern bool statement_can_end;
extern bool first_line;

#ifdef masscomp
extern unsigned char* yytext;
#else
extern char* yytext;
#endif

extern bool in_func_decl;

extern int yyparse();
extern int yylex();
extern void restart_yylex( FILE* input_file );

#endif	/* input_h */
