%{
// $Header$

#include <string.h>

#include "Expr.h"
#include "BuiltIn.h"
#include "Reporter.h"
#include "Sequencer.h"
#include "input.h"
#include "y.tab.h"

extern YYSTYPE yylval;

char* input_file_name;

// Whether the last token might be the end of a statement.
bool statement_can_end = false;

bool interactive = false;
bool first_line = true;

// When we convert a newline to a ';' we don't bump the line count just
// yet, as that'll result in the line count being one too high for the
// statement that ends with that ';'.  So instead we set this flag and
// subsequently bump the line count on the next call.
bool bump_line_num = false;

// A class for holding information on an input file: where we are in
// it (i.e., its flex buffer), its name, and the current line number.
// InputFileInfo objects are created without any parameters; they glean
// all the pertinent information from the globals present in input.h.
// To restore the globals to the state they had when the object was
// created, simply delete it.
class InputFileInfo {
public:
	InputFileInfo();
	~InputFileInfo();

protected:
	YY_BUFFER_STATE buf;
	char* filename;
	int line;
	bool save_first_line;
	bool save_bump_line_num;
	bool save_statement_can_end;
	};

// List of input buffers associated with included files.
declare(PList,InputFileInfo);
PList(InputFileInfo) file_list;

static int scanner_read( char buf[], int max_size );
static void new_file();

#undef YY_INPUT
#define YY_INPUT(buf,result,max_size)	\
	result = scanner_read( buf, max_size );

#define RETURN_CONSTANT(value)					\
	{							\
	statement_can_end = true;				\
	yylval.expr = new ConstExpr( new Value( value ) );	\
	return TOK_CONSTANT;					\
	}

#define RETURN_LAST_EVENT(type)					\
	{							\
	statement_can_end = true;				\
	yylval.event_type = type;				\
	return TOK_LAST_EVENT;					\
	}

#define RETURN_COMPOUND(type)					\
	{							\
	yylval.ival = type;					\
	return TOK_ASSIGN;					\
	}

#define RETURN_ACTIVATE(is_activate)				\
	{							\
	statement_can_end = true;				\
	yylval.bval = is_activate;				\
	return TOK_ACTIVATE;					\
	}

#undef YY_BREAK
// The following makes actions by default fall through to the next
// action.  We are careful then that every action ends in a "return"
// or a break.  The reason for bother with this is so that picky
// compilers don't complain about the zillions of actions that
// terminate with a "return" followed by a "break".
#define YY_BREAK
%}

%x quote record_field event_name incl

	static char literal[512];
	static bool is_double_quote;


ID	[A-Za-z_][A-Za-z0-9_]*
WS	[ \t]+
OWS	[ \t]*
D	[0-9]

%%
	// Whether to return a ';' token if a newline is seen.
	bool newline_is_semi = statement_can_end;

	if ( bump_line_num )
		++line_num;

	bump_line_num = false;
	statement_can_end = false;


[!:;,|&({[+*/%^=-]	return yytext[0];

[)\]]		statement_can_end = true; return yytext[0];

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
		break;
		}

"."		BEGIN(record_field); return yytext[0];
"->"		BEGIN(event_name); return TOK_ARROW;

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

activate	RETURN_ACTIVATE(true);
await		return TOK_AWAIT;
break		statement_can_end = true; return TOK_BREAK;
const		return TOK_CONST;
deactivate	RETURN_ACTIVATE(false);
do		return TOK_DO;
else		return TOK_ELSE;
except		return TOK_EXCEPT;
exit		statement_can_end = true; return TOK_EXIT;
for		return TOK_FOR;
func(tion)?	return TOK_FUNCTION;
if		return TOK_IF;
in		return TOK_IN;
link		return TOK_LINK;
local		return TOK_LOCAL;
next|continue	statement_can_end = true; return TOK_LOOP;
only		return TOK_ONLY;
print		return TOK_PRINT;
ref		return TOK_REF;
request		return TOK_REQUEST;
return		statement_can_end = true; return TOK_RETURN;
send		return TOK_SEND;
subseq(uence)?	return TOK_SUBSEQUENCE;
to		return TOK_TO;
unlink		return TOK_UNLINK;
val		return TOK_VAL;
whenever	return TOK_WHENEVER;
while		return TOK_WHILE;

"$agent"	RETURN_LAST_EVENT( EVENT_AGENT );
"$name"		RETURN_LAST_EVENT( EVENT_NAME );
"$value"	RETURN_LAST_EVENT( EVENT_VALUE );

F		RETURN_CONSTANT( false );
T		RETURN_CONSTANT( true );

include		BEGIN(incl); break;

{ID}		{
		statement_can_end = true;
		yylval.id = strdup( yytext );
		return TOK_ID;
		}

<record_field>{ID}	|
<event_name>{ID}	{
		// We use a separate start condition for .name's so
		// that they can include reserved words.

		BEGIN(INITIAL);
		statement_can_end = true;
		yylval.id = strdup( yytext );
		return TOK_ID;
		}

<event_name>[*\[]	BEGIN(INITIAL); return yytext[0];

{D}+		RETURN_CONSTANT( atoi( yytext ) );

(({D}*"."?{D}+)|({D}+"."?{D}*))(e[-+ ]?{D}+)?	{
		RETURN_CONSTANT( atof( yytext ) );
		}

["']		{
		literal[0] = '\0';
		is_double_quote = bool( yytext[0] == '"' );
		BEGIN(quote);
		break;
		}

<quote>[^'"\n\\]+	strcat( literal, yytext ); break;
<quote>"\\n"	strcat( literal, "\n" ); break;
<quote>"\\t"	strcat( literal, "\t" ); break;
<quote>"\\r"	strcat( literal, "\r" ); break;
<quote>"\\f"	strcat( literal, "\f" ); break;
<quote>"\\"\n{OWS}	++line_num; break;
<quote>"\\".	strcat( literal, &yytext[1] ); break;

<quote>\"	|
<quote>\'	{
		if ( (is_double_quote && yytext[0] == '\'') ||
		     (! is_double_quote && yytext[0] == '"') )
			strcat( literal, &yytext[0] );

		else
			{
			BEGIN(INITIAL);

			if ( is_double_quote )
				{
				statement_can_end = true;
				yylval.expr = new ConstExpr( split( literal ) );
				return TOK_CONSTANT;
				}

			else
				RETURN_CONSTANT( literal );
			}

		break;
		}

<quote>\n	{
		error->Report( "unmatched quote (",
				is_double_quote ? "\"" : "'", ")" );
		++line_num;
		BEGIN(INITIAL);
		RETURN_CONSTANT( false );
		}

<incl>\"[^"]*\"	{
		yytext[yyleng - 1] = '\0';	// nuke trailing quote
		char* filename = &yytext[1];	// skip leading quote
		FILE* file = fopen( filename, "r" );

		if ( ! file )
			error->Report( "can't open include file \"",
					filename, "\"" );

		else
			{
			// Save current file information.
			file_list.append( new InputFileInfo() );
			input_file_name = strdup( filename );
			YY_BUFFER_STATE incl_buf =
				yy_create_buffer( file, YY_BUF_SIZE );
			yy_switch_to_buffer( incl_buf );
			new_file();
			}

		BEGIN(INITIAL);
		break;
		}

<INITIAL,record_field,event_name>#.*	break; // comment
<INITIAL,record_field,event_name,incl>{WS}+	break; // eat whitespace

<INITIAL,record_field,event_name>\\\n	{
		++line_num;
		first_line = false;
		break;
		}

<INITIAL,record_field,event_name,incl>\n	{
		if ( newline_is_semi )
			{
			bump_line_num = true;
			return ';';
			}

		++line_num;
		first_line = false;
		break;
		}

<INITIAL,record_field,event_name,incl>.	{
		error->Report( "unrecognized character '", yytext, "'" );
		statement_can_end = true;
		BEGIN(INITIAL);
		break;
		}

<<EOF>>		{
		delete input_file_name;		// done with file name

		int nesting = file_list.length();
		if ( nesting > 0 )
			{
			// We're done with this include file, delete it.
			yy_delete_buffer( YY_CURRENT_BUFFER );

			// Pop back to previous file.
			delete file_list.remove_nth( nesting - 1 );
			break;
			}

		else
			yyterminate();
		}
%%


InputFileInfo::InputFileInfo()
	{
	buf = YY_CURRENT_BUFFER;
	filename = input_file_name;
	line = line_num;
	save_first_line = first_line;
	save_bump_line_num = bump_line_num;
	save_statement_can_end = statement_can_end;
	}

InputFileInfo::~InputFileInfo()
	{
	yy_switch_to_buffer( buf );
	input_file_name = filename;
	line_num = line;
	first_line = save_first_line;
	bump_line_num = save_bump_line_num;
	statement_can_end = save_statement_can_end;
	}


int scanner_read( char buf[], int max_size )
	{
	if ( interactive )
		{
		printf( "%s ", first_line ? "-" : "+" );
		fflush( stdout );

		current_sequencer->EventLoop();
		}

	return read( fileno( yyin ), buf, max_size );
	}

void restart_yylex( FILE* input_file )
	{
	static bool first_call = true;

	if ( yyin != stdin )
		fclose( yyin );

	yyin = input_file;

	new_file();

	if ( first_call )
		first_call = false;
	else
		yyrestart( yyin );
	}

void new_file()
	{
	line_num = 1;
	first_line = true;
	bump_line_num = false;
	statement_can_end = false;
	}
