%{
/* $Id$
** Copyright (c) 1993 The Regents of the University of California.
** Copyright (c) 1997 Associated Universities Inc.
*/

#include "system.h"
#include <string.h>
#include <iostream.h>
#include <ctype.h>
#if HAVE_OSFCN_H
#include <osfcn.h>
#endif

#include "Expr.h"
#include "BuiltIn.h"
#include "Reporter.h"
#include "Sequencer.h"
#include "Regex.h"
#include "input.h"
#include "y.tab.h"
#include "system.h"

#if ! defined(PURE_PARSER)
extern YYSTYPE yylval;
#define YYLVAL(x) yylval.x
#define YY_DECL int yy_flex ( )
#else
#define YYLVAL(x) yylval->x
#define YY_DECL int yy_flex YY_PROTO(( YYSTYPE *yylval ))
#endif

extern Sequencer* current_sequencer;
#ifdef GGC
extern int glish_do_gc_register;
extern void glish_gc_register( IValue * );
#define DO_GC_REGISTER						\
	if ( glish_do_gc_register )				\
		glish_gc_register( v );
#else
#define DO_GC_REGISTER
#endif

// Whether the last token might be the end of a statement.
int statement_can_end = 0;

extern int interactive;
int first_line = 1;

//Transition Variables
static int request_cnt = 0;
static int send_cnt = 0;

// When we convert a newline to a ';' we don't bump the line count just
// yet, as that'll result in the line count being one too high for the
// statement that ends with that ';'.  So instead we set this flag and
// subsequently bump the line count on the next call.
int bump_line_num = 0;

// A class for holding information on an input file: where we are in
// it (i.e., its flex buffer), its name, and the current line number.
// InputFileInfo objects are created without any parameters; they glean
// all the pertinent information from the globals present in input.h.
// To restore the globals to the state they had when the object was
// created, simply delete it.
class InputFileInfo {
public:
	InputFileInfo( FILE*, YY_BUFFER_STATE );
	~InputFileInfo();

protected:
	YY_BUFFER_STATE buf;
	char* filename;
	int line;
	int save_first_line;
	int save_bump_line_num;
	int save_statement_can_end;
	FILE *file;
	YY_BUFFER_STATE newbuf;
	};

// List of input buffers associated with included files.
glish_declare(PList,InputFileInfo);
PList(InputFileInfo) file_list;

static int scanner_read( char buf[], int max_size );
static void new_file();

//
// This handles intermediate storage of literals. Each time
// it is called it returns a pointer to the beginning of the
// literal. 'clear_literal()' resets the buffer.
//
static char *literal( const char *s = 0, int clear = 0 );
#define clear_literal() literal(0,1)

#undef YY_INPUT
#define YY_INPUT(buf,result,max_size)	\
	result = scanner_read( buf, max_size );

#define RETURN_CONSTANT(value)					\
	{							\
	statement_can_end = 1;					\
	IValue *v = new IValue( value );			\
	DO_GC_REGISTER						\
	YYLVAL(expr) = new ConstExpr( v );			\
	return TOK_CONSTANT;					\
	}

#define RETURN_LAST_EVENT(type)					\
	{							\
	statement_can_end = 1;					\
	YYLVAL(event_type) = type;				\
	return TOK_LAST_EVENT;					\
	}

#define RETURN_LAST_REGEX(type)					\
	{							\
	statement_can_end = 1;					\
	YYLVAL(regex_type) = type;				\
	return TOK_LAST_REGEX;					\
	}

#define RETURN_COMPOUND(type)					\
	{							\
	YYLVAL(ival) = type;					\
	return TOK_ASSIGN;					\
	}

#define RETURN_APPLYRX(type)					\
	{							\
	YYLVAL(ival) = type;					\
	return TOK_APPLYRX;					\
	}

#define RETURN_ACTIVATE(is_activate)				\
	{							\
	statement_can_end = 1;					\
	YYLVAL(ival) = is_activate;				\
	return TOK_ACTIVATE;					\
	}

#define PRAGMA_SHARED(type)					\
	{							\
	current_sequencer->MultiClientScript( Client::type );	\
	++line_num;						\
	BEGIN(INITIAL);						\
	break;							\
	}

#define PRAGMA_ONCE						\
	{							\
	current_sequencer->IncludeOnce( );			\
	++line_num;						\
	BEGIN(INITIAL);						\
	break;							\
	}

#undef YY_BREAK
// The following makes actions by default fall through to the next
// action.  We are careful then that every action ends in a "return"
// or a break.  The reason for bother with this is so that picky
// compilers don't complain about the zillions of actions that
// terminate with a "return" followed by a "break".
#define YY_BREAK

#define yywrap() 1
%}

%x QUOTE RECORD_FIELD SCAN_EVENT_NAME INCL UNKNOWN_PRAGMA ATTR ATTR_QUOTE REGEX NO_REGEX END_REGEX

	static int is_double_quote = 0;
	int glish_regex_matched = 0;
	static char *match_regex_portion = 0;
	static char regex_delimit = '\0';
	static char regex_start = '\0';
	static unsigned int regex_flags = 0;


ID	[A-Za-z_][A-Za-z0-9_]*
WS	[ \t]+
OWS	[ \t]*
D	[0-9]
RXSTART	[!/#@%^&|~`;:]
RXCONT	[^\n\\!/#@%^&|~`;:]

FLOAT	(({D}*"."?{D}+)|({D}+"."?{D}*))([eE][-+]?{D}+)?

%%
	// Whether to return a ';' token if a newline is seen.
	int newline_is_semi = statement_can_end;

	if ( bump_line_num )
		++line_num;

	bump_line_num = 0;
	statement_can_end = 0;


[!:;,|&({[+*/%^=-]	return yytext[0];

[)\]]		statement_can_end = 1; return yytext[0];

"}"		{
		if ( newline_is_semi )
			{
			unput( '}' );
			return ';';
			}
		else
			return '}';
		}

"++"|"--"	{
		error->Report( yytext, " is not a valid Glish operator" );
		return TOK_FLEX_ERROR;
		}

"."		BEGIN(RECORD_FIELD); return yytext[0];
"->"		BEGIN(SCAN_EVENT_NAME); return TOK_ARROW;
"::"		BEGIN(ATTR); return TOK_ATTR;

"..."		return TOK_ELLIPSIS;

"=="		return TOK_EQ;
"!="		return TOK_NE;
"<="		return TOK_LE;
">="		return TOK_GE;
"<"		return TOK_LT;
">"		return TOK_GT;

"&&"		return TOK_AND_AND;
"||"		return TOK_OR_OR;

":="		RETURN_COMPOUND(0);

"+:="		RETURN_COMPOUND(yytext[0]);
"-:="		RETURN_COMPOUND(yytext[0]);
"*:="		RETURN_COMPOUND(yytext[0]);
"/:="		RETURN_COMPOUND(yytext[0]);
"%:="		RETURN_COMPOUND(yytext[0]);
"^:="		RETURN_COMPOUND(yytext[0]);

"&:="		RETURN_COMPOUND(yytext[0]);
"|:="		RETURN_COMPOUND(yytext[0]);

"&&:="		RETURN_COMPOUND(TOK_AND_AND);
"||:="		RETURN_COMPOUND(TOK_OR_OR);

"~:="		RETURN_COMPOUND(yytext[0]);
"=~"		RETURN_APPLYRX(yytext[0]);
"!~"		RETURN_APPLYRX(yytext[0]);
"~"		RETURN_APPLYRX(0);

activate	RETURN_ACTIVATE(1);
await		return TOK_AWAIT;
break		statement_can_end = 1; return TOK_BREAK;
const		return TOK_CONST;
deactivate	RETURN_ACTIVATE(0);
do		return TOK_DO;
else		return TOK_ELSE;
except		return TOK_EXCEPT;
exit		statement_can_end = 1; return TOK_EXIT;
for		return TOK_FOR;
func(tion)?	return TOK_FUNCTION;
if		return TOK_IF;
in		return TOK_IN;
link		return TOK_LINK;
local		return TOK_LOCAL;
global		return TOK_GLOBAL;
wider		return TOK_WIDER;
next|continue	statement_can_end = 1; return TOK_NEXT;
only		return TOK_ONLY;
print		return TOK_PRINT;
fail		statement_can_end = 1; return TOK_FAIL;
ref		return TOK_REF;
return		statement_can_end = 1; return TOK_RETURN;
request		{
		if ( interactive && yyin == stdin )
			{
			warn->Report("'request' is deprecated, leave out this keyword");
			request_cnt = 1;
			}
		else if ( ! request_cnt )
			{
			warn->Report("'request' is deprecated, leave out this keyword");
			request_cnt = 1;
			}
		break;
		}
send		{
		if ( interactive && yyin == stdin )
			{
			warn->Report("'send' is deprecated, leave out this keyword");
			send_cnt = 1;
			}
		else if ( ! send_cnt )
			{
			warn->Report("'send' is deprecated, leave out this keyword");
			send_cnt = 1;
			}
		break;
		}
subseq(uence)?	return TOK_SUBSEQUENCE;
to		return TOK_TO;
unlink		return TOK_UNLINK;
val		return TOK_VAL;
whenever	return TOK_WHENEVER;
while		return TOK_WHILE;

"$agent"	RETURN_LAST_EVENT( EVENT_AGENT );
"$name"		RETURN_LAST_EVENT( EVENT_NAME );
"$value"	RETURN_LAST_EVENT( EVENT_VALUE );
"$m"		RETURN_LAST_REGEX( REGEX_MATCH );

F		RETURN_CONSTANT( glish_false );
T		RETURN_CONSTANT( glish_true );

InClUdE		BEGIN(INCL); break;
include		return TOK_INCLUDE;

pragma{WS}shared{WS}user{OWS};?{OWS}\n		PRAGMA_SHARED(USER)
pragma{WS}shared{WS}group{OWS};?{OWS}\n		PRAGMA_SHARED(GROUP)
pragma{WS}shared{WS}world{OWS};?{OWS}\n		PRAGMA_SHARED(WORLD)
pragma{WS}include{WS}once{OWS};?{OWS}\n		PRAGMA_ONCE
pragma		BEGIN(UNKNOWN_PRAGMA); break;

<INITIAL,NO_REGEX>{ID} {
		statement_can_end = 1;
		YYLVAL(id) = strdup( yytext );
		BEGIN(INITIAL);
		return TOK_ID;
		}

<RECORD_FIELD,ATTR,SCAN_EVENT_NAME>{ID}	{
		// We use a separate start condition for these names so
		// that they can include reserved words.

		BEGIN(INITIAL);
		statement_can_end = 1;
		YYLVAL(id) = strdup( yytext );
		return TOK_ID;
		}

<SCAN_EVENT_NAME>[*\[]	BEGIN(INITIAL); return yytext[0];

{D}+		RETURN_CONSTANT( atoi( yytext ) );

{FLOAT}		{
		RETURN_CONSTANT( atof( yytext ) );
		}

{FLOAT}i	{
		RETURN_CONSTANT( atodcpx( yytext ) );
		}

["']		{
		clear_literal();
		is_double_quote = yytext[0] == '"';
		BEGIN(QUOTE);
		break;
		}

[ms]{RXSTART}	{
		clear_literal();
		regex_start = yytext[0];
		regex_delimit = yytext[1];
		match_regex_portion = 0;
		BEGIN(REGEX);
		break;
		}
		
<REGEX>{RXCONT}+			literal( yytext ); break;
<QUOTE,ATTR_QUOTE>[^'"\n\\]+		literal( yytext ); break;
<QUOTE,REGEX,ATTR_QUOTE>"\\n"		literal( "\n" ); break;
<QUOTE,REGEX,ATTR_QUOTE>"\\t"		literal( "\t" ); break;
<QUOTE,REGEX,ATTR_QUOTE>"\\r"		literal( "\r" ); break;
<QUOTE,REGEX,ATTR_QUOTE>"\\f"		literal( "\f" ); break;
<QUOTE,REGEX,ATTR_QUOTE>"\\v"		literal( "\v" ); break;
<QUOTE,REGEX,ATTR_QUOTE>"\\e"		{ char escape[2] = { '\x01b', '\0' };
					  literal( escape ); break; }
<QUOTE,REGEX,ATTR_QUOTE>"\\a"		literal( "\a" ); break;
<QUOTE,REGEX,ATTR_QUOTE>"\\"\n{OWS}	++line_num; break;
<QUOTE,ATTR_QUOTE>"\\".			literal( &yytext[1] ); break;
<REGEX>"\\".				literal( yytext ); break;


<ATTR>"["	{
		BEGIN(INITIAL);
		return yytext[0];
		}

<ATTR>["']	{
		clear_literal();
		is_double_quote = yytext[0] == '"';
		BEGIN(ATTR_QUOTE);
		break;
		}

<QUOTE>\"	|
<QUOTE>\'	{
		if ( (is_double_quote && yytext[0] == '\'') ||
		     (! is_double_quote && yytext[0] == '"') )
			literal( &yytext[0] );

		else
			{
			BEGIN(INITIAL);

			if ( is_double_quote )
				{
				statement_can_end = 1;
				IValue *v = isplit( literal());
#ifdef GGC
				if ( glish_do_gc_register )
					glish_gc_register( v );
#endif
				YYLVAL(expr) = new ConstExpr( v );
				return TOK_CONSTANT;
				}

			else
				RETURN_CONSTANT( literal() );
			}

		break;
		}

<ATTR_QUOTE>\"	|
<ATTR_QUOTE>\'	{
		if ( (is_double_quote && yytext[0] == '\'') ||
		     (! is_double_quote && yytext[0] == '"') )
			literal( &yytext[0] );

		else
			{
			BEGIN(INITIAL);
			statement_can_end = 1;
			YYLVAL(id) = strdup( literal() );
			return TOK_ID;
			}

		break;
		}

<REGEX>{RXSTART} {
		if ( yytext[0] != regex_delimit )
			{
			literal(yytext);
			break;
			}
		else if ( regex_start == 'm' || match_regex_portion )
			{
			regex_flags = 0;
			BEGIN(END_REGEX);
			break;
			}
		else
			{
			match_regex_portion = strdup(literal());
			clear_literal();
			break;
			}
		}

<END_REGEX>[gi] {
		regex_flags |= yytext[0] == 'g' ?  Regex::GLOBAL() : yytext[0] == 'i' ? Regex::FOLD() : 0;
		break;
		}

<END_REGEX>\n	|
<END_REGEX>. {
		statement_can_end = 1;
		unput(yytext[0]);
		IValue *v = 0;
		Regex *r = regex_start == 'm' ? new Regex( strdup(literal()), regex_delimit, regex_flags ) :
				new Regex(match_regex_portion, regex_delimit, regex_flags, strdup(literal()));
		if ( ! r->Error() )
			v = new IValue( r );
		else
			{
			v = (IValue*) generate_error(r->Error());
			delete r;
			}
		DO_GC_REGISTER
		YYLVAL(expr) = new ConstExpr( v );
		glish_regex_matched = 1;
		BEGIN(INITIAL);
		return TOK_REGEX;
		}

<REGEX>\n	{
		unput('\n');
		char *bad = literal();
		int len = strlen(bad)-1;
		for ( int i = len; i >= 0; --i )
			{
			unput(bad[i]);
			}
		unput( regex_delimit );
		if ( match_regex_portion )
			{
			len = strlen(match_regex_portion)-1;
			for ( int i = len; i >= 0; --i )
				unput(match_regex_portion[i]);
			free_memory(match_regex_portion);
			match_regex_portion = 0;
			unput( regex_delimit );
			}
		unput( regex_start );
		BEGIN(NO_REGEX);
		break;
		}


<QUOTE,ATTR_QUOTE>\n	{
		error->Report( "unmatched quote (",
				is_double_quote ? "\"" : "'", ")" );
		++line_num;
		BEGIN(INITIAL);
		return TOK_FLEX_ERROR;
		}


<INCL>\"[^"]*\"	{
		yytext[yyleng - 1] = '\0';	// nuke trailing quote
		char* filename = which_include( &yytext[1] );	// skip leading quote

		if ( ! filename )
			error->Report( "can't open include file \"",
					&yytext[1], "\"" );
		else
			{
			FILE* file = fopen( filename, "r" );

			if ( ! file )
				{
				error->Report( "can't open include file \"",
						filename, "\"" );
				delete filename;
				}
			else
				{
				// Save current file information.
				YY_BUFFER_STATE incl_buf =
					yy_create_buffer( file, YY_BUF_SIZE );
				file_list.append( new InputFileInfo( file, incl_buf ) );
				yy_switch_to_buffer( incl_buf );
				new_file();
				}

			}

		BEGIN(INITIAL);
		break;
		}

<INITIAL,RECORD_FIELD,SCAN_EVENT_NAME,ATTR>#.*			break; // comment
<INITIAL,RECORD_FIELD,SCAN_EVENT_NAME,INCL,UNKNOWN_PRAGMA,ATTR>{WS}	break; // eat whitespace

<UNKNOWN_PRAGMA>.*	{
		static Dict(int) unknown_pragma;
		if ( ! unknown_pragma.Lookup(yytext) )
			{
			unknown_pragma.Insert(strdup(yytext),1);
			warn->Report( "unknown pragma found:", yytext );
			}
		BEGIN(INITIAL);
		break;
		}

<INITIAL,RECORD_FIELD,SCAN_EVENT_NAME,ATTR>\\\n	{
		++line_num;
		first_line = 0;
		break;
		}

<ATTR>\n	{
		// Treat this case as ending the newline, so that
		// "foo := bar::" works as expected.
		BEGIN(INITIAL);
		bump_line_num = 1;
		return ';';
		}

<ATTR>.		{ // Allow "bar:::=" to work.
		unput( yytext[0] );
		statement_can_end = 1;
		BEGIN(INITIAL);
		break;
		}

<*>\n		{
		if ( newline_is_semi )
			{
			bump_line_num = 1;
			return ';';
			}

		++line_num;
		first_line = 0;
		break;
		}

<*>.		{
		error->Report( "unrecognized character '", yytext, "'" );
		statement_can_end = 1;
		BEGIN(INITIAL);
		return TOK_FLEX_ERROR;
		}

<<EOF>>		{
		if ( newline_is_semi )
			return ';';

		int nesting = file_list.length();
		if ( nesting > 0 )
			{
			// Pop back to previous file.
			delete file_list.remove_nth( nesting - 1 );
			break;
			}

		else
			yyterminate();
		}
%%


InputFileInfo::InputFileInfo( FILE *f, YY_BUFFER_STATE s )
	{
	buf = YY_CURRENT_BUFFER;
	line = line_num;
	save_first_line = first_line;
	save_bump_line_num = bump_line_num;
	save_statement_can_end = statement_can_end;
	file = f;
	newbuf = s;
	}

InputFileInfo::~InputFileInfo()
	{
	yy_switch_to_buffer( buf );
	line_num = line;
	first_line = save_first_line;
	bump_line_num = save_bump_line_num;
	statement_can_end = save_statement_can_end;
	if ( newbuf ) yy_delete_buffer( newbuf );
	if ( file ) fclose( file );
	}


// If non-null, we're scanning an array of strings, with our present
// position given by input_offset.
static const char** input_strings;
static int input_offset;

void scan_strings( const char** strings )
	{
	input_strings = strings;
	input_offset = 0;
	}

int scanner_read( char buf[], int max_size )
	{
	if ( input_strings )
		{
		const char* s = input_strings[input_offset];
		if ( ! s )
			{ // All done.
			input_strings = 0;
			return 0;
			}

		char* bufptr = buf;
		while ( max_size > 0 && s )
			{
			int len = strlen( s );
			if ( len >= max_size )
				// Okay, we've read enough.
				return bufptr - buf;

			strcpy( bufptr, s );
			bufptr[len] = '\n';
			++len;	// add in the newline
			bufptr += len;
			max_size -= len;

			// Move on to the next string.
			s = input_strings[++input_offset];
			}

		return bufptr - buf;
		}

	if ( interactive && yyin == stdin )
		{
		const char* prompt = first_line ? "- " : "+ ";
		if ( Sequencer::CurSeq()->System().ILog() )
			{
			int len = interactive_read( yyin, prompt, buf, max_size );
			Sequencer::CurSeq()->System().DoILog(buf,len);
			return len;
			}
		else
			return interactive_read( yyin, prompt, buf, max_size );
		}
	else
		return read( fileno( yyin ), buf, max_size );
	}

void restart_yylex( FILE* input_file )
	{
	static int first_call = 1;

	if ( yyin && yyin != stdin )
		fclose( yyin );

	yyin = input_file;

	new_file();

	if ( first_call )
		first_call = 0;
	else
		yyrestart( yyin );
	}

static int put_back_token = NULL_TOK;
int
#if ! defined(PURE_PARSER)
yylex ( )
#else
yylex ( YYSTYPE *yylval )
#endif
	{
	if ( put_back_token == NULL_TOK )
		return 
#if ! defined(PURE_PARSER)
			yy_flex ( );
#else
			yy_flex ( yylval );
#endif
	else
		{
		int cur_token = put_back_token;
		put_back_token = NULL_TOK;
		return cur_token;
		}
	}

void putback_token( int tok )
	{
	put_back_token = tok;
	}

void new_file()
	{
	line_num = 1;
	first_line = 1;
	bump_line_num = 0;
	statement_can_end = 0;
	}

char *literal( const char *new_str, int clear )
	{
	static unsigned int len = 512;
	static unsigned int curlen = 0;
	static char *string = (char*) alloc_memory( sizeof(char)*len );

	if ( clear )
		curlen = 0;

	if ( ! new_str )
		{
		string[curlen] = '\0';
		return string;
		}

	int newlen = strlen(new_str);

	if ( curlen + newlen + 1 > len )
		{
		while ( curlen + newlen + 1 > len )
			len *= 2;
		string = (char*) realloc_memory( (void*) string, len );
		}

	memcpy( &string[curlen], new_str, newlen + 1 );
	curlen += newlen;
	return string;
	}

struct flex_buf {
	flex_buf( YY_BUFFER_STATE buf_, int putback_ ) : buf(buf_), putback(putback_) { }
	YY_BUFFER_STATE buf;
	int putback;
};

void *current_flex_buffer()
	{
	return new flex_buf(YY_CURRENT_BUFFER, put_back_token);
	}

void *new_flex_buffer( FILE *fptr )
	{
	return new flex_buf(yy_create_buffer(fptr, YY_BUF_SIZE), NULL_TOK);
	}

void set_flex_buffer(void *new_buf)
	{
	flex_buf *buf = (flex_buf*) new_buf;
	yy_switch_to_buffer(buf->buf);
	put_back_token = buf->putback;
	}

void delete_flex_buffer(void *old_buf)
	{
	flex_buf *buf = (flex_buf*) old_buf;
	yy_delete_buffer(buf->buf);
	delete buf;
	}
