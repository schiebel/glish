/* $Header$ */

%token TOK_ACTIVATE TOK_ATTR TOK_AWAIT TOK_BREAK TOK_CONST TOK_CONSTANT
%token TOK_DO TOK_ELLIPSIS TOK_ELSE TOK_EXCEPT TOK_EXIT TOK_FOR
%token TOK_FUNCTION TOK_ID TOK_IF TOK_IN TOK_LAST_EVENT TOK_LINK
%token TOK_LOCAL TOK_GLOBAL TOK_WIDER TOK_LOOP TOK_ONLY TOK_PRINT TOK_FAIL
%token TOK_REF TOK_REQUEST TOK_RETURN TOK_SEND TOK_SUBSEQUENCE TOK_TO
%token TOK_UNLINK TOK_VAL TOK_WHENEVER TOK_WHILE TOK_INCLUDE
%token NULL_TOK

%left ','
%right TOK_ASSIGN
%left TOK_OR_OR
%left TOK_AND_AND
%left '|'
%left '&'
%nonassoc TOK_LT TOK_GT TOK_LE TOK_GE TOK_EQ TOK_NE
%left '+' '-'
%left '*' '/' '%'
%right '^'
%nonassoc ':'
%right '!'
%left '.' '[' ']' '(' ')' TOK_ARROW TOK_ATTR TOK_REQUEST

%type <ival> TOK_ACTIVATE TOK_ASSIGN
%type <id> TOK_ID opt_id
%type <event_type> TOK_LAST_EVENT
%type <expr> TOK_CONSTANT expression var function formal_param_default
%type <expr> scoped_expr opt_scoped_expr scoped_lhs_var
%type <expr> function_head block_head subscript
%type <exprlist> subscript_list
%type <event> event
%type <stmt> statement_list statement func_body block
%type <stmt> local_list local_item global_list global_item wider_list wider_item
%type <stmt> whenever_head
%type <ev_list> event_list
%type <param_list> formal_param_list formal_params
%type <param_list> actual_param_list actual_params
%type <param_list> opt_actual_param_list opt_actual_params
%type <param_list> array_record_ctor_list array_record_params
%type <param> formal_param actual_param opt_actual_param
%type <param> array_record_param
%type <val_type> value_type formal_param_class


%{
#if defined(_AIX) && defined(YYBISON)
/* Must occur before the first line of C++ code - barf! */
/* Actually only required when using bison. */
#pragma alloca
#endif

#include <string.h>
#include <stdlib.h>
#include <setjmp.h>

#include "BinOpExpr.h"
#include "Event.h"
#include "Stmt.h"
#include "Func.h"
#include "Reporter.h"
#include "Sequencer.h"
#include "Task.h"
#include "input.h"
%}

%union	{
	char* id;
	last_event_type event_type;
	Expr* expr;
	expr_list* exprlist;
	EventDesignator* event;
	Stmt* stmt;
	event_list* ev_list;
	PDict(Expr)* record;
	parameter_list* param_list;
	Parameter* param;
	value_type val_type;
	int ival;
	}

%{
extern "C" {
	int yyparse();
	void yyerror( char msg[] );
}

#if ! defined(PURE_PARSER)
extern int yylex();
#else
extern int yylex( YYSTYPE * );
#endif

/* Used for recovery after a ^C */
extern jmp_buf glish_top_level;
extern int glish_jmpbuf_set;

Sequencer* current_sequencer = 0;
int in_func_decl = 0;
int current_whenever_index = -1;
static IValue *parse_error = 0;

/* Communication of status between glish_parser() and yyparse() */
static int status;
static Stmt *cur_stmt = null_stmt;

extern void putback_token( int );
Expr* compound_assignment( Expr* lhs, int tok_type, Expr* rhs );
%}

%%


glish:
		statement
			{
        		static int *lookahead = & ( yyclearin );
			static int empty = *lookahead;

			cur_stmt = $1;

			if ( *lookahead != empty )
				putback_token( *lookahead );

			YYACCEPT;
			}

	|	error
			{
			status = 1;
			if ( interactive )
				{
				// Try to throw away the rest of the line.
				statement_can_end = 1;
				first_line = 1;
				}
			YYACCEPT;
			}
	|
			{
			YYABORT;
			}

	;


statement_list:
		statement_list statement
			{ $$ = merge_stmts( $1, $2 ); }
	|
			{ $$ = null_stmt; }
	;


scoped_expr:
		expression
			{ $$ = $$->BuildFrameInfo( SCOPE_UNKNOWN ); }
	;

opt_scoped_expr:	scoped_expr
	|
			{ $$ = 0; }
	;


scoped_lhs_var:
		var
			{ $$ = $$->BuildFrameInfo( SCOPE_LHS ); }
	;

statement:
		block

	|	TOK_LOCAL local_list ';'
			{ $$ = $2; }

	|	TOK_GLOBAL global_list ';'
			{ $$ = $2; }

	|	TOK_WIDER wider_list ';'
			{ $$ = $2; }

	|	whenever_head TOK_DO statement
			{
			((WheneverStmt*) $1)->SetStmt($3); 
			$$ = $1;
			}

	|	TOK_LINK event_list TOK_TO event_list ';'
			{ $$ = new LinkStmt( $2, $4, current_sequencer ); }

	|	TOK_UNLINK event_list TOK_TO event_list ';'
			{ $$ = new UnLinkStmt( $2, $4, current_sequencer ); }

	|	TOK_AWAIT event_list ';'
			{
			$$ = new AwaitStmt( $2, 0, 0, current_sequencer );
			}
	|	TOK_AWAIT TOK_ONLY event_list ';'
			{
			$$ = new AwaitStmt( $3, 1, 0, current_sequencer );
			}
	|	TOK_AWAIT TOK_ONLY event_list TOK_EXCEPT event_list ';'
			{
			$$ = new AwaitStmt( $3, 1, $5, current_sequencer );
			}

	|	TOK_ACTIVATE ';'
			{ $$ = new ActivateStmt( $1, 0, current_sequencer ); }

	|	TOK_ACTIVATE scoped_expr ';'
			{ $$ = new ActivateStmt( $1, $2, current_sequencer ); }

	|	TOK_SEND event '(' actual_param_list ')' ';'
			{
			$$ = new ExprStmt( new SendEventExpr( $2, $4, 0 ) );
			}

	|	event '(' actual_param_list ')' ';'
			{
			$$ = new ExprStmt( new SendEventExpr( $1, $3, 0 ) );
			}

	|	TOK_IF '(' scoped_expr ')' cont statement
			{ $$ = new IfStmt( $3, $6, 0 ); }
	|	TOK_IF '(' scoped_expr ')' cont statement TOK_ELSE statement
			{ $$ = new IfStmt( $3, $6, $8 ); }

	|	TOK_FOR '(' scoped_lhs_var TOK_IN scoped_expr ')' cont statement
			{
			$$ = new ForStmt( $3, $5, $8 );
			}

	|	TOK_WHILE '(' scoped_expr ')' cont statement
			{ $$ = new WhileStmt( $3, $6 ); }

	|	TOK_LOOP ';'	/* "next" statement; "loop" is historical */
			{ $$ = new LoopStmt; }

	|	TOK_BREAK ';'
			{ $$ = new BreakStmt; }

	|	TOK_RETURN ';'
			{ $$ = new ReturnStmt( 0 ); }

	|	TOK_RETURN scoped_expr ';'
			{ $$ = new ReturnStmt( $2 ); }

	|	TOK_EXIT ';'
			{ $$ = new ExitStmt( 0, current_sequencer ); }

	|	TOK_EXIT scoped_expr ';'
			{ $$ = new ExitStmt( $2, current_sequencer ); }

	|	TOK_PRINT actual_param_list ';'
			{ $$ = new PrintStmt( $2 ); }

	|	TOK_FAIL opt_scoped_expr ';'
			{ $$ = new FailStmt( $2 ); }

	|	scoped_expr ';'
			{ $$ = new ExprStmt( $1 ); }

	|	';'
			{ $$ = null_stmt; }
	;

whenever_head: TOK_WHENEVER event_list
			{ $$ = new WheneverStmt( $2, current_sequencer ); }

expression:
		'(' expression ')'
			{ $$ = $2; }

	|	expression TOK_ASSIGN expression
			{ $$ = compound_assignment( $1, $2, $3 ); }

	|	expression TOK_OR_OR expression
			{ $$ = new OrExpr( $1, $3 ); }
	|	expression TOK_AND_AND expression
			{ $$ = new AndExpr( $1, $3 ); }

	|	expression '|' expression
			{ $$ = new LogOrExpr( $1, $3 ); }
	|	expression '&' expression
			{ $$ = new LogAndExpr( $1, $3 ); }

	|	expression TOK_LT expression
			{ $$ = new LT_Expr( $1, $3 ); }
	|	expression TOK_GT expression
			{ $$ = new GT_Expr( $1, $3 ); }
	|	expression TOK_LE expression
			{ $$ = new LE_Expr( $1, $3 ); }
	|	expression TOK_GE expression
			{ $$ = new GE_Expr( $1, $3 ); }
	|	expression TOK_EQ expression
			{ $$ = new EQ_Expr( $1, $3 ); }
	|	expression TOK_NE expression
			{ $$ = new NE_Expr( $1, $3 ); }

	|	expression '+' expression
			{ $$ = new AddExpr( $1, $3 ); }
	|	expression '-' expression
			{ $$ = new SubtractExpr( $1, $3 ); }
	|	expression '*' expression
			{ $$ = new MultiplyExpr( $1, $3 ); }
	|	expression '/' expression
			{ $$ = new DivideExpr( $1, $3 ); }
	|	expression '%' expression
			{ $$ = new ModuloExpr( $1, $3 ); }
	|	expression '^' expression
			{ $$ = new PowerExpr( $1, $3 ); }

	|	'-' expression	%prec '!'
			{ $$ = new NegExpr( $2 ); }
	|	'+' expression	%prec '!'
			{ $$ = $2; }
	|	'!' expression
			{ $$ = new NotExpr( $2 ); }

	|	expression '[' subscript_list ']'
			{ $$ = new ArrayRefExpr( $1, $3 ); }

	|	expression '.' TOK_ID
			{ $$ = new RecordRefExpr( $1, $3 ); }

	|	expression TOK_ATTR
			{ $$ = new AttributeRefExpr( $1 ); }

	|	expression TOK_ATTR '[' expression ']'
			{ $$ = new AttributeRefExpr( $1, $4 ); }

	|	expression TOK_ATTR TOK_ID
			{ $$ = new AttributeRefExpr( $1, $3 ); }

	|	'[' '=' ']'
			{ $$ = new ConstructExpr( 0 ); }

	|	'[' array_record_ctor_list ']'
			{ $$ = new ConstructExpr( $2 ); }

	|	expression ':' expression
			{ $$ = new RangeExpr( $1, $3 ); }

	|	expression '(' opt_actual_param_list ')'
			{ $$ = new CallExpr( $1, $3, current_sequencer ); }

	|	value_type expression	%prec '^'
			{ $$ = new RefExpr( $2, $1 ); }

	|	TOK_REQUEST event '(' actual_param_list ')'
			{ $$ = new SendEventExpr( $2, $4, 1 ); }

	|	TOK_LAST_EVENT
			{ $$ = new LastEventExpr( current_sequencer, $1 ); }

	|	TOK_INCLUDE expression	%prec '!'
			{ $$ = new IncludeExpr( $2, current_sequencer ); }

	|	function

	|	var

	|	TOK_CONSTANT
	;


local_list:	local_list ',' local_item
			{ $$ = merge_stmts( $1, $3 ); }
	|	local_item
	;

local_item:	TOK_ID TOK_ASSIGN scoped_expr
			{
			Expr* id = current_sequencer->LookupID( $1, LOCAL_SCOPE, 1, 0, 0 );

			Ref(id);
			$$ = new ExprStmt( compound_assignment( id, $2, $3 ) );

			if ( $2 != 0 )
				warn->Report( "compound assignment in", $$ );
			}
	|	TOK_ID
			{
			(void) current_sequencer->LookupID( $1, LOCAL_SCOPE, 1, 0, 0 );
			$$ = null_stmt;
			}
	;


global_list:	global_list ',' global_item
			{ $$ = merge_stmts( $1, $3 ); }
	|	global_item
	;

global_item:	TOK_ID TOK_ASSIGN scoped_expr
			{
			Expr* id =
				current_sequencer->LookupID( $1, GLOBAL_SCOPE, 1, 0 );

			Ref(id);
			$$ = new ExprStmt( compound_assignment( id, $2, $3 ) );

			if ( $2 != 0 )
				warn->Report( "compound assignment in", $$ );
			}
	|	TOK_ID
			{
			(void) current_sequencer->LookupID( $1, GLOBAL_SCOPE, 1, 0 );
			$$ = null_stmt;
			}
	;


wider_list:	wider_list ',' wider_item
			{ $$ = merge_stmts( $1, $3 ); }
	|	wider_item
	;

wider_item:	TOK_ID TOK_ASSIGN scoped_expr
			{
			Expr* id =
				current_sequencer->LookupID( strdup($1), ANY_SCOPE, 0, 0 );

			if ( ! id )
				{
				warn->Report( "\"",$1,"\" ", "not found in 'wider', non-global scope; defaults to 'local'");
				id = current_sequencer->InstallID( $1, LOCAL_SCOPE, 0 );
				}
			else
				delete $1;

			Ref(id);
			$$ = new ExprStmt( compound_assignment( id, $2, $3 ) );

			if ( $2 != 0 )
				warn->Report( "compound assignment in", $$ );
			}
	|	TOK_ID
			{
			Expr* id =
				current_sequencer->LookupID( strdup($1), ANY_SCOPE, 0, 0 );

			if ( ! id )
				{
				warn->Report( "\"",$1,"\" ", "not found in 'wider', non-global scope; defaults to 'local'");
				id = current_sequencer->InstallID( $1, LOCAL_SCOPE, 0 );
				}
			else
				delete $1;

			$$ = null_stmt;
			}
	;


block:		block_head statement_list '}'
			{ 
			int frame_size = current_sequencer->PopScope();

			$$ = new StmtBlock( frame_size, $2, current_sequencer );
			}

block_head:	'{'
			{
			current_sequencer->PushScope();
			$$ = 0;
			}
	;

function:	function_head opt_id '(' formal_param_list ')' cont func_body
		no_cont
			{
			int frame_size = current_sequencer->PopScope();
			IValue *err = 0;

			UserFunc* ufunc = new UserFunc( $4, $7, frame_size,
							current_sequencer, $1, err );
			if ( ! err )
				{
				$$ = new FuncExpr( ufunc );

				if ( $2 )
					{
					if ( current_sequencer->GetScopeType() == GLOBAL_SCOPE )
						{ 
						// Create global reference to function.
						Expr* func =
							current_sequencer->LookupID(
							$2, LOCAL_SCOPE );

						IValue* ref = new IValue( ufunc );
						func->Assign( ref );

						/* keep ufunc from being deleted with $$ */
						Ref(ufunc);
						}
					else
						{
						Expr *lhs = 
							current_sequencer->LookupID( $2, LOCAL_SCOPE);
						Ref(lhs);
						$$ = compound_assignment( lhs, 0, $$ );
						}
					}

				in_func_decl = 0;
				}
			else
				{
				$$ = new ValExpr( err );
				Ref(err);
				Unref( ufunc );
				in_func_decl = 0;
				}
			}
	;

function_head:	TOK_FUNCTION
			{
			in_func_decl = 1;
			current_sequencer->PushScope( FUNC_SCOPE );
			$$ = 0;
			}

	|	TOK_SUBSEQUENCE
			{
			in_func_decl = 1;
			current_sequencer->PushScope( FUNC_SCOPE );
			$$ = current_sequencer->InstallID( strdup( "self" ),
								LOCAL_SCOPE );
			Ref($$);
			}
	;

formal_param_list:
		formal_params
	|
			{ $$ = new parameter_list; }
	;

formal_params:	formal_params ',' formal_param
			{ $1->append( $3 ); }

	|	formal_param
			{
			$$ = new parameter_list;
			$$->append( $1 );
			}
	;

formal_param:	formal_param_class TOK_ID formal_param_default
			{
			Expr* param =
				current_sequencer->InstallID( $2, LOCAL_SCOPE );
			Ref(param);
			$$ = new FormalParameter( (const char *) $2, $1, param, 0, $3 );
			}

	|	TOK_ELLIPSIS formal_param_default
			{
			Expr* ellipsis = current_sequencer->LookupID(strdup("..."), LOCAL_SCOPE, 1, 0 );

			Ref(ellipsis);
			$$ = new FormalParameter( VAL_CONST, ellipsis, 1, $2 );
			}
	;

formal_param_class:
		value_type
	|
			{ $$ = VAL_VAL; }
	;

formal_param_default:
		'=' scoped_expr
			{ $$ = $2; }
	|
			{ $$ = 0; }
	;

actual_param_list:
		actual_params
	|
			{ $$ = new parameter_list; }
	;

actual_params:	actual_params ',' actual_param
			{ $1->append( $3 ); }

	|	actual_param
			{
			$$ = new parameter_list;
			$$->append( $1 );
			}
	;

actual_param:	scoped_expr
			{ $$ = new ActualParameter( VAL_VAL, $1 ); }

	|	TOK_ID '=' scoped_expr
			{
			Ref( $3 );
			$$ = new ActualParameter( $1, VAL_VAL, $3, 0, $3 );
			}

	|	TOK_ELLIPSIS
			{
			Expr* ellipsis =
				current_sequencer->LookupID(
					strdup( "..." ), LOCAL_SCOPE, 0 );

			if ( ! ellipsis )
				{
				error->Report( "\"...\" not available" ); 
				$$ = new ActualParameter( VAL_VAL,
					new ConstExpr( error_ivalue() ) );
				}

			else
				{
				Ref(ellipsis);
				$$ = new ActualParameter( VAL_VAL, ellipsis, 1 );
				}
			}
	;

array_record_ctor_list:
		array_record_params
	|
			{ $$ = new parameter_list; }
	;

array_record_params:	array_record_params ',' array_record_param
			{ $1->append( $3 ); }

	|	array_record_param
			{
			$$ = new parameter_list;
			$$->append( $1 );
			}
	;

array_record_param:	scoped_expr
			{ $$ = new ActualParameter( VAL_VAL, $1 ); }

	|	TOK_CONST TOK_ID '=' scoped_expr
			{
			Ref( $4 );
			$$ = new ActualParameter( $2, VAL_CONST, $4, 0, $4 );
			}

	|	TOK_ID '=' scoped_expr
			{
			Ref( $3 );
			$$ = new ActualParameter( $1, VAL_VAL, $3, 0, $3 );
			}

	|	TOK_ELLIPSIS
			{
			Expr* ellipsis =
				current_sequencer->LookupID(
					strdup( "..." ), LOCAL_SCOPE, 0 );

			if ( ! ellipsis )
				{
				error->Report( "\"...\" not available" ); 
				$$ = new ActualParameter( VAL_VAL,
					new ConstExpr( error_ivalue() ) );
				}

			else
				{
				Ref(ellipsis);
				$$ = new ActualParameter( VAL_VAL, ellipsis, 1 );
				}
			}
	;

opt_actual_param_list:
		opt_actual_params
	|
			{ $$ = new parameter_list; }
	;

opt_actual_params:	opt_actual_params ',' opt_actual_param
			{ $1->append( $3 ); }
	|	opt_actual_params ','
			{
			$1->append( new ActualParameter() );
			}

	|	','
			{ // Something like "foo(,)" - two missing parameters.
			$$ = new parameter_list;
			$$->append( new ActualParameter() );
			$$->append( new ActualParameter() );
       			}

	|	',' opt_actual_param
			{
			$$ = new parameter_list;
			$$->append( new ActualParameter() );
			$$->append( $2 );
       			}

	|	opt_actual_param
			{
			$$ = new parameter_list;
			$$->append( $1 );
       			}
	;

opt_actual_param:	scoped_expr
			{ $$ = new ActualParameter( VAL_VAL, $1 ); }

	|	TOK_ID '=' scoped_expr
			{
			Ref( $3 );
			$$ = new ActualParameter( $1, VAL_VAL, $3, 0, $3 );
			}

	|	TOK_ELLIPSIS
			{
			Expr* ellipsis =
				current_sequencer->LookupID(
					strdup( "..." ), LOCAL_SCOPE, 0 );

			if ( ! ellipsis )
				{
				error->Report( "\"...\" not available" ); 
				$$ = new ActualParameter( VAL_VAL,
					new ConstExpr( error_ivalue() ) );
				}

			else
				{
				Ref(ellipsis);
				$$ = new ActualParameter( VAL_VAL, ellipsis, 1 );
				}
			}
	;


subscript_list: subscript_list ',' subscript
			{ $1->append( $3 ); }
	|	subscript_list ','
			{ $1->append( 0 ); }
	|	','
			{
			$$ = new expr_list;
			$$->append( 0 );
			$$->append( 0 );
			}
	|	',' subscript
			{
			$$ = new expr_list;
			$$->append( 0 );
			$$->append( $2 );
			}
	|	subscript
			{
			$$ = new expr_list;
			$$->append( $1 );
			}
	;


subscript:	scoped_expr
	;


func_body:	'{' statement_list '}'
			{ $$ = $2; }

	|	expression	%prec ','
			{ $$ = new ExprStmt( $1->BuildFrameInfo( SCOPE_UNKNOWN ) ); }
	;


var:		TOK_ID
			{
			$$ = current_sequencer->LookupID( strdup($1), LOCAL_SCOPE, 0 );

			if ( ! $$ )
				$$ = CreateVarExpr( $1, current_sequencer );
			else
				delete $1;

			Ref($$);
			}
	;

opt_id:		TOK_ID
	|
			{ $$ = 0; }
	;


event_list:	event_list ',' event
			{ $$->append( $3 ); }
	|	event
			{
			$$ = new event_list;
			$$->append( $1 );
			}
	;

event:		scoped_expr TOK_ARROW '[' scoped_expr ']'
			{ $$ = new EventDesignator( $1, $4 ); }
	|	scoped_expr TOK_ARROW TOK_ID
			{ $$ = new EventDesignator( $1, $3 ); }
	|	scoped_expr TOK_ARROW '*' no_cont
			{ $$ = new EventDesignator( $1, (Expr*) 0 ); }
	;


value_type:	TOK_REF
			{ $$ = VAL_REF; }
	|	TOK_CONST
			{ $$ = VAL_CONST; }
	|	TOK_VAL
			{ $$ = VAL_VAL; }
	;


cont:			{ statement_can_end = 0; }
	;

no_cont:		{ statement_can_end = 1; }
	;

%%


extern "C"
void yyerror( char msg[] )
	{
	if ( in_func_decl )
		{
		in_func_decl = 0;
		current_sequencer->PopScope();
		}

	current_whenever_index = -1;

	if ( ! status )
		{
		parse_error = (IValue*) generate_error( msg, " at or near '", yytext, "'" );
		error->Report( msg, " at or near '", yytext, "'" );
		}
	}


void clear_error()
	{
	status = 0;
	}

IValue *glish_parser( Stmt *&stmt )
	{
	int ret;
	cur_stmt = stmt = null_stmt;

	error->SetCount(0);
	status = 0;
	while ( ! (ret = yyparse()) )
		{
		if ( ! interactive )
			stmt = merge_stmts( stmt, cur_stmt );

		else
			{
			Stmt *loc_stmt = cur_stmt;
			cur_stmt = null_stmt;

			IValue *val = 0;
			if ( setjmp(glish_top_level) == 0 )
				{
				glish_jmpbuf_set = 1;
				stmt_flow_type flow;
				val = loc_stmt->Exec( 1, flow );
				if ( flow != FLOW_NEXT )
					warn->Report("control flow (loop/break/return) ignored" );
				}

			glish_jmpbuf_set = 0;

			if ( current_sequencer->ErrorResult() )
				{
				if ( Sequencer::CurSeq()->System().OLog() )
					Sequencer::CurSeq()->System().DoOLog(current_sequencer->ErrorResult());

				message->Report( current_sequencer->ErrorResult() );
				current_sequencer->ClearErrorResult();
				}
			else if ( val )
				{
				message->Report( val );
				Unref( val );
				}

			NodeUnref(loc_stmt);
			first_line = 1;
			}
		}

	if ( ! status ) return 0;
	IValue *tmp = parse_error;
	parse_error = 0;
	return tmp ? tmp : error_ivalue( "parse error" );
	}

Expr* compound_assignment( Expr* lhs, int tok_type, Expr* rhs )
	{
	switch ( tok_type )
		{
		case 0:	// ordinary :=
			return new AssignExpr( lhs, rhs );

#define CMPD(token, expr)						\
	case token:							\
		Ref(lhs);						\
		return new AssignExpr( lhs, new expr( lhs, rhs ) );

		CMPD('+', AddExpr);
		CMPD('-', SubtractExpr);
		CMPD('*', MultiplyExpr);
		CMPD('/', DivideExpr);
		CMPD('%', ModuloExpr);
		CMPD('^', PowerExpr);

		CMPD('|', LogOrExpr);
		CMPD('&', LogAndExpr);

		CMPD(TOK_OR_OR, OrExpr);
		CMPD(TOK_AND_AND, AndExpr);

		default:
			fatal->Report( "unknown compound assignment type =",
					tok_type );
			return 0;
		}
	}
