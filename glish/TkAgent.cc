// $Header$
#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "TkAgent.h"

#ifdef GLISHTK
#include <string.h>
#include <stdlib.h>
#include "Rivet/rivet.h"
#include "Reporter.h"
#include "IValue.h"
#include "Expr.h"

#include <iostream.h>

Rivetobj TkAgent::root = 0;
unsigned int TkFrame::tl_cnt = 0;

static IValue *ScrollToValue( Scrollbar_notify_data *data )
	{
	recordptr rec = create_record_dict();

	rec->Insert( strdup("vertical"), new IValue( data->scrollbar_is_vertical ) );
	rec->Insert( strdup("op"), new IValue( data->scroll_op ) );
	rec->Insert( strdup("newpos"), new IValue( data->newpos ) );

	return new IValue( rec );
	}

static Scrollbar_notify_data *ValueToScroll( const IValue *data )
	{
	if ( data->Type() != TYPE_RECORD )
		return 0;

	IValue *vertical;
	IValue *op;
	IValue *newpos;
	if ( ! (vertical = (IValue*) (data->HasRecordElement( "vertical" )) ) ||
	     ! (op = (IValue*) (data->HasRecordElement( "op" ) )) ||
	     ! (newpos = (IValue*) (data->HasRecordElement( "newpos" ) )) )
		return 0;

	if ( vertical->Type() != TYPE_INT ||
	     ! vertical->Length() ||
	     op->Type() != TYPE_INT ||
	     ! op->Length() ||
	     newpos->Type() != TYPE_DOUBLE ||
	     ! newpos->Length() )
		return 0;

	Scrollbar_notify_data *ret = new Scrollbar_notify_data;

	ret->scrollbar_is_vertical = vertical->IntVal();
	ret->scroll_op = op->IntVal();
	ret->newpos = newpos->DoubleVal();

	return ret;
	}

IValue *glishtk_splitnl( char *str )
	{
	char *prev = str;
	int nls = 0;

	if ( ! str || ! str[0] )
		return new IValue( "" );

	while ( *str )
		if ( *str++ == '\n' ) nls++;

	if ( ! nls )
		return new IValue( prev );

	char **ary = new char*[nls+1];

	for ( nls = 0, str = prev; *str; str++ )
		if ( *str == '\n' )
			{
			if ( prev != str )
				{
				*str = (char) 0;
				ary[nls++] = strdup( prev );
				}
			prev = str+1;
			}

	if ( prev != str )
		ary[nls++] = strdup( prev );

	return new IValue( (const char **) ary, nls );
	}

IValue *glishtk_splitsp_int( char *sel )
	{
	char *start = sel;
	char *end;
	int cnt = 0;
	static int len = 2;
	static int *ary = new int[len];

	while ( *start && (end = strchr(start,' ')) )
		{
		*end = (char) 0;
#define EXPAND_ACTION					\
		if ( cnt >= len )			\
			{				\
			len *= 2;			\
			ary = (int *) realloc(ary, len * sizeof(int));\
			}
		EXPAND_ACTION
		ary[cnt++] = atoi(start);
		*end++ = ' ';
		start = end;
		}

	if ( *start )
		{
		EXPAND_ACTION
		ary[cnt++] = atoi(start);
		}

	return new IValue( ary, cnt, COPY_ARRAY );
	}

char **glishtk_splitsp_str( char *sel, int &cnt )
	{
	char *start = sel;
	char *end;
	cnt = 0;
	static int len = 2;
	static char **ary = new char*[len];

	while ( *start && (end = strchr(start,' ')) )
		{
		*end = (char) 0;
#define EXPAND_ACTION					\
		if ( cnt >= len )			\
			{				\
			len *= 2;			\
			ary = (char **) realloc(ary, len * sizeof(char*) );\
			}
		EXPAND_ACTION
		ary[cnt++] = strdup(start);
		*end++ = ' ';
		start = end;
		}

	if ( *start )
		{
		EXPAND_ACTION
		ary[cnt++] = strdup(start);
		}

	return ary;
	}

inline void glishtk_pack( Rivetobj root, int argc, char **argv)
	{
	rivet_func(root,(int (*)())Tk_PackCmd,argc,argv);
	}


static TkAgent *InvalidArg( int num )
	{
	error->Report("invalid type for argument ", num);
	return 0;
	}

static TkAgent *InvalidNumberOfArgs( int num )
	{
	error->Report("invalid number of arguments, ", num, " expected");
	return 0;
	}

#define SETVAL(var,condition)						\
	const IValue *var      = (*args_val)[c++];			\
	if ( ! ( condition) )						\
		return InvalidArg(--c);
#define SETSTR(var)							\
	SETVAL(var##_v_, var##_v_ ->Type() == TYPE_STRING &&		\
				var##_v_ ->Length() > 0 )		\
	charptr var = ( var##_v_ ->StringPtr() )[0];
#define SETDIM(var)							\
	SETVAL(var##_v_, var##_v_ ->Type() == TYPE_STRING &&		\
				var##_v_ ->Length() > 0   ||		\
				var##_v_ ->IsNumeric() )		\
	char var##_char_[30];						\
	charptr var = 0;						\
	if ( var##_v_ ->Type() == TYPE_STRING )				\
		var = ( var##_v_ ->StringPtr() )[0];			\
	else								\
		{							\
		sprintf(var##_char_,"%dp", var##_v_ ->IntVal());	\
		var = var##_char_;					\
		}
#define SETINT(var)							\
	SETVAL(var##_v_, var##_v_ ->IsNumeric() &&			\
				var##_v_ ->Length() > 0 )		\
	int var = var##_v_ ->IntVal();

#define EXPRVAL(var,EVENT)						\
	Expr *var##_expr_ = (*args)[c++]->Arg();			\
	const IValue *var = var##_expr_ ->ReadOnlyEval();		\
	const IValue *var##_val_ = var;					\
	if ( ! var )							\
		{							\
		error->Report("bad value", EVENT);			\
		var##_expr_ ->ReadOnlyDone(var##_val_);			\
		return 0;						\
		}

#define EXPRSTR(var,EVENT)						\
	Expr *var##_expr_ = (*args)[c++]->Arg();			\
	const IValue *var##_val_ = var##_expr_ ->ReadOnlyEval();		\
	charptr var = 0;						\
	if ( ! var##_val_ || var##_val_ ->Type() != TYPE_STRING ||	\
		var##_val_ ->Length() <= 0 )				\
		{							\
		error->Report("bad value for ", EVENT);			\
		var##_expr_ ->ReadOnlyDone(var##_val_);			\
		return 0;						\
		}							\
	else								\
		var = ( var##_val_ ->StringPtr() )[0];
#define EXPRDIM(var,EVENT)						\
	Expr *var##_expr_ = (*args)[c++]->Arg();			\
	const IValue *var##_val_ = var##_expr_ ->ReadOnlyEval();		\
	charptr var = 0;						\
	char var##_char_[30];						\
	if ( ! var##_val_ || ( var##_val_ ->Type() != TYPE_STRING &&	\
			       ! var##_val_ ->IsNumeric() ) ||		\
		var##_val_ ->Length() <= 0 )				\
		{							\
		error->Report("bad value for ", EVENT);			\
		var##_expr_ ->ReadOnlyDone(var##_val_);			\
		return 0;						\
		}							\
	else								\
		if ( var##_val_ ->Type() == TYPE_STRING	)		\
			var = ( var##_val_ ->StringPtr() )[0];		\
		else							\
			{						\
			sprintf(var##_char_,"%dp", var##_val_->IntVal());\
			var = var##_char_;				\
			}

#define EXPRINT(var,EVENT)                                              \
        Expr *var##_expr_ = (*args)[c++]->Arg();                        \
        const IValue *var##_val_ = var##_expr_ ->ReadOnlyEval();         \
        int var = 0;                                                    \
        if ( ! var##_val_ || ! var##_val_ ->IsNumeric() ||              \
                var##_val_ ->Length() <= 0 )                            \
                {                                                       \
                error->Report("bad value for ", EVENT);                 \
                var##_expr_ ->ReadOnlyDone(var##_val_);                 \
                return 0;                                   		\
                }                                                       \
        else                                                            \
                var = var##_val_ ->IntVal();


#define EXPRINT2(var,EVENT)						\
	Expr *var##_expr_ = (*args)[c++]->Arg();			\
	const IValue *var##_val_ = var##_expr_ ->ReadOnlyEval();		\
        char var##_char_[30];						\
	charptr var = 0;						\
	if ( ! var##_val_ || var##_val_ ->Length() <= 0 )		\
		{							\
		error->Report("bad value for ", EVENT);			\
		var##_expr_ ->ReadOnlyDone(var##_val_);			\
		return 0;						\
		}							\
	if ( var##_val_ -> IsNumeric() )				\
		{							\
		int var##_int_ = var##_val_ ->IntVal();			\
		var = var##_char_;					\
		sprintf(var##_char_,"%d",var##_int_);			\
		}							\
	else if ( var##_val_ -> Type() == TYPE_STRING )			\
		var = ( var##_val_ ->StringPtr() )[0];			\
	else								\
		{							\
		error->Report("bad type for ", EVENT);			\
		var##_expr_ ->ReadOnlyDone(var##_val_);			\
		return 0;						\
		}

#define EXPRDOUBLEPTR(var, NUM, EVENT)					\
	Expr *var##_expr_ = (*args)[c++]->Arg();			\
	const IValue *var##_val_ = var##_expr_ ->ReadOnlyEval();		\
	double *var = 0;						\
	if ( ! var##_val_ || var##_val_ ->Type() != TYPE_DOUBLE ||	\
		var##_val_ ->Length() < NUM )				\
		{							\
		error->Report("bad value for ", EVENT);			\
		var##_expr_ ->ReadOnlyDone(var##_val_);			\
		return 0;						\
		}							\
	else								\
		var = var##_val_ ->DoublePtr();

#define EXPR_DONE(var)							\
	var##_expr_ ->ReadOnlyDone(var##_val_);
#define HASARG( args, cond )						\
	if ( ! (args->length() cond) )					\
		{							\
		error->Report("wrong number of arguments");		\
		return 0;						\
		}

#define DEFINE_DTOR(CLASS)				\
CLASS::~CLASS( )					\
	{						\
	if ( frame )					\
		frame->RemoveElement( this );		\
	UnMap();					\
	}

char *glishtk_nostr(Rivetobj self, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	return rivet_va_cmd( self, cmd, 0 );
	}

char *glishtk_onestr(Rivetobj self, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one string function";
	HASARG( args, > 0 )
	int c = 0;
	EXPRSTR( str, event_name )
	ret = (char*) rivet_set( self, (char*) cmd, (char*) str );
	EXPR_DONE( str )
	return ret;
	}

char *glishtk_onedim(Rivetobj self, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one dim function";
	HASARG( args, > 0 )
	int c = 0;
	EXPRDIM( dim, event_name )
	ret = (char*) rivet_set( self, (char*) cmd, (char*) dim );
	EXPR_DONE( dim )
	return ret;
	}

char *glishtk_oneint(Rivetobj self, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one int function";
	HASARG( args, > 0 )
	int c = 0;
	EXPRINT2( i, event_name )
	ret = (char*) rivet_set( self, (char*) cmd, (char*) i );
	EXPR_DONE( i )
	return ret;
	}

char *glishtk_onebinary(Rivetobj self, const char *cmd, const char *ptrue, const char *pfalse,
				parameter_list *args, int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one binary function";
	HASARG( args, > 0 )
	int c = 0;
	EXPRINT( i, event_name )
	ret = (char*) rivet_set( self, (char*) cmd, (char*)(i ? ptrue : pfalse) );
	EXPR_DONE( i )
	return ret;
	}

char *glishtk_onebool(Rivetobj self, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	return glishtk_onebinary(self, cmd, "true", "false", args, is_request, log);
	}

char *glishtk_oneidx(TkAgent *a, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one index function";

	HASARG( args, > 0 )
	int c = 0;
	EXPRSTR( index, event_name )

	ret = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( index ), 0);

	EXPR_DONE( index )
	return ret;
	}

char *glishtk_oneortwoidx(TkAgent *a, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one-or-two index function";

	HASARG( args, > 0 )
	int c = 0;
	EXPRSTR( start, event_name )
	if ( args->length() > 1 )
		{
		EXPRSTR( end, event_name )
		ret = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( start ),
					     a->IndexCheck( end ), 0);
		EXPR_DONE( end )
		}
	else
		ret = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( start ), 0);
	EXPR_DONE( start )
	return ret;
	}

char *glishtk_strandidx(TkAgent *a, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one-or-two index function";

	HASARG( args, > 0 )
	int c = 0;
	EXPRVAL( val, event_name );
	char *str = val->StringVal( ' ', 0, 1 );
	if ( args->length() > 1 )
		{
		EXPRSTR( where, event_name )
		ret = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( where ), str, 0);
		EXPR_DONE( where )
		}
	else
		ret = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( "end" ), str, 0);
	delete str;
	EXPR_DONE( val )
	return ret;
	}

char *glishtk_strwithidx(TkAgent *a, const char *cmd, const char *param,
				parameter_list *args, int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one-or-two index function";

	HASARG( args, > 0 )
	int c = 0;
	EXPRVAL( val, event_name );
	char *str = val->StringVal( ' ', 0, 1 );
	ret = rivet_va_cmd(a->Self(), cmd, a->IndexCheck(param), str, 0);
	delete str;
	EXPR_DONE( val )
	return ret;
	}

char *glishtk_no2str(TkAgent *a, const char *cmd, const char *param,
				parameter_list *args, int is_request, int log )
	{
	return rivet_va_cmd( a->Self(), cmd, param, 0 );
	}

char *glishtk_listbox_insert_action(TkAgent *a, const char *cmd, IValue *str_v, charptr where="end" )
	{
	int len = str_v->Length();

	if ( ! len ) return 0;

	char **argv = new char*[len+3];
	charptr *strs = str_v->StringPtr();

	argv[0] = 0;
	argv[1] = (char*) cmd;
	argv[2] = (char*) a->IndexCheck( where );
	for ( int c=0; c < len; ++c )
		argv[c+3] = (char *) strs[c];
		
	rivet_cmd(a->Self(), c+3, argv);
	delete argv;
	}

char *glishtk_listbox_insert(TkAgent *a, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *event_name = "listbox insert function";

	HASARG( args, > 0 )
	int c = 0;
	EXPRVAL( val, event_name )
	if ( val->Type() != TYPE_STRING )
		{
		error->Report("invalid argument type");
		EXPR_DONE(val)
		return 0;
		}
	if ( args->length() > 1 )
		{
		EXPRSTR( where, event_name )
		glishtk_listbox_insert_action(a, (char*) cmd, (IValue*) val, where );
		EXPR_DONE( where )
		}
	else
		glishtk_listbox_insert_action(a, (char*) cmd, (IValue*) val );
	EXPR_DONE( val )
	return "";
	}


char *glishtk_listbox_get_int(TkAgent *a, const char *cmd, IValue *val )
	{
	int len = val->Length();

	if ( ! len )
		return 0;

	static int rlen = 200;
	static char *ret = new char[rlen];
	int *index = val->IntPtr();
	char buf[40];
	int cnt=0;

	ret[0] = (char) 0;
	for ( int i=0; i < len; i++ )
		{
		sprintf(buf,"%d",index[i]);
		char *v = rivet_va_cmd(a->Self(), cmd, a->IndexCheck(buf), 0);
		if ( v )
			{
			int vlen = strlen(v);
			while ( cnt+vlen+1 >= rlen )
				{
				rlen *= 2;
				ret = (char *) realloc(ret, rlen);
				}
			if ( cnt )
				ret[cnt++] = '\n';
			memcpy(&ret[cnt],v,vlen);
			cnt += vlen;
			}
		}

	ret[cnt] = '\0';
	return ret;
	}

char *glishtk_listbox_get(TkAgent *a, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "listbox get function";

	HASARG( args, > 0 )
	int c = 0;
	EXPRVAL( val, event_name )
	if ( val->Type() == TYPE_STRING && val->Length() )
		{
		if ( args->length() > 1 )
			{
			EXPRSTR( end, event_name )
			ret = rivet_va_cmd( a->Self(), cmd,
					    a->IndexCheck( val->StringPtr()[0] ),
					    a->IndexCheck( end ), 0 );
			EXPR_DONE( end )
			}
		else
			ret = rivet_va_cmd( a->Self(), cmd,
					    a->IndexCheck( val->StringPtr()[0] ), 0 );
		}

	else if ( val->Type() == TYPE_INT && val->Length() )
			ret = glishtk_listbox_get_int( a, (char*) cmd, (IValue*) val );
	else
		error->Report("invalid argument type");

	EXPR_DONE( val )
	return ret;
	}


char *glishtk_scrolled_update(Rivetobj self, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	static char ret[5];
	char *event_name = "scrolled update function";
	HASARG( args, > 0 )
	int c = 0;
	EXPRVAL( data, event_name )

	Scrollbar_notify_data *data_ = ValueToScroll( data );
	if ( ! data_ )
		return 0;
	rivet_scrollbar_set_client_view( self, data_ );
	delete data_;

	EXPR_DONE( data )
	ret[0] = (char) 0;
	return ret;
	}

char *glishtk_scrollbar_update(Rivetobj self, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	static char ret[5];
	char *event_name = "scrollbar update function";
	HASARG( args, > 0 )
	int c = 0;
	EXPRDOUBLEPTR( firstlast, 2, event_name )
	rivet_scrollbar_set( self, firstlast[0], firstlast[1] );
	EXPR_DONE( firstlast )
	ret[0] = (char) 0;
	return ret;
	}

IValue *TkProc::operator()(Rivetobj s, parameter_list*arg, int x, int y)
		{
		char *val = 0;
		if ( proc )
			val = (*proc)(s,cmdstr,arg,x,y);
		else if ( proc1 )
			val = (*proc1)(s,cmdstr,param,arg,x,y);
		else if ( proc2 )
			val = (*proc2)(s,cmdstr,param,param2,arg,x,y);
		else if ( fproc != 0 && frame != 0 )
			val = (frame->*fproc)( arg, x, y );
		else if ( aproc != 0 && agent != 0 )
			val = (*aproc)(agent, cmdstr, arg, x, y);
		else if ( aproc2 != 0 && agent != 0 )
			val = (*aproc2)(agent, cmdstr, param, arg, x, y);
		else
			return error_ivalue();

		if ( convert && val )
			return (*convert)(val);
		else
			return new IValue( glish_true );
		}

TkAgent::TkAgent( Sequencer *s ) : Agent( s )
	{
	agent_ID = "<graphic>";
	self = 0;
	frame = 0;

	if ( ! root )
		{
		char *argv[3];
		argv[0] = "glish";
		argv[1] = 0;

		root = rivet_init(1, argv);
		}

	procs.Insert("background", new TkProc("-bg", glishtk_onestr));
	procs.Insert("foreground", new TkProc("-fg", glishtk_onestr));
	procs.Insert("relief", new TkProc("-relief", glishtk_onestr));
	procs.Insert("borderwidth", new TkProc("-borderwidth", glishtk_onedim));
	}

IValue *TkAgent::UnrecognizedEvent( )
	{
	error->Report(this," unrecognized event");
	return error_ivalue();
	}

void TkAgent::UnMap()
	{
	if ( self )
		rivet_destroy_window( self );
	frame = 0;
	self = 0;
	}

const char **TkAgent::PackInstruction()
	{
	return 0;
	}

IValue *TkAgent::SendEvent( const char* event_name, parameter_list* args,
				int is_request, int log )
	{
	if ( ! IsValid() )
		{
		error->Report("graphic is defunct");
		return error_ivalue();
		}

	TkProc *proc = procs[event_name];

	if ( proc != 0 )
		return Invoke( proc, args, is_request, log );
	else
		return 0;
	}

IValue *TkAgent::Invoke(TkProc *proc, parameter_list*arg, int x, int y)
	{
	return (*proc)( self, arg, x, y );
	}

charptr TkAgent::IndexCheck( charptr c )
	{
	return c;
	}

TkAgent::~TkAgent( ) { }


TkFrame::TkFrame( Sequencer *s, charptr relief_, charptr side_, charptr borderwidth,
		  charptr padx_, charptr pady_, charptr expand_, charptr background, charptr width,
		  charptr height, charptr title ) : TkAgent( s ), is_tl( 1 ), pseudo( 0 )
	{
	char *argv[13];

	if ( ! root )
		{
		error->Report("Frame creation failed, check DISPLAY environment variable.");
		return;
		}

	if ( tl_cnt++ )
		{
		int c = 2;
		argv[0] = argv[1] = 0;
		argv[c++] = "-borderwidth";
		argv[c++] = "0p";
		argv[c++] = "-width";
		argv[c++] = (char*) width;
		argv[c++] = "-height";
		argv[c++] = (char*) height;
		argv[c++] = "-background";
		argv[c++] = (char*) background;
		pseudo = rivet_create(ToplevelClass, root, c, argv);
		if ( title && title[0] )
			rivet_va_func(pseudo, (int (*)()) Tk_WmCmd, "title",
				      rivet_path(pseudo), title, 0 );
		}
	else
		if ( title && title[0] )
			rivet_va_func(root, (int (*)()) Tk_WmCmd, "title",
				      rivet_path(root), title, 0 );

	agent_ID = "<graphic:frame>";
	side = strdup(side_);
	padx = strdup(padx_);
	pady = strdup(pady_);
	expand = strdup(expand_);

	int c = 2;
	argv[0] = argv[1] = 0;
	argv[c++] = "-relief";
	argv[c++] = (char*) relief_;
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-width";
	argv[c++] = (char*) width;
	argv[c++] = "-height";
	argv[c++] = (char*) height;
	argv[c++] = "-background";
	argv[c++] = (char*) background;

	self = rivet_create(FrameClass, pseudo ? pseudo : root, c, argv);

	if ( ! self )
		fatal->Report("Rivet creation failed in TkFrame::TkFrame");

	AddElement( this );

	if ( frame )
		{
		frame->AddElement( this );
		frame->Pack();
		}
	else
		Pack();

	procs.Insert("padx", new TkProc( this, &TkFrame::SetPadx ));
	procs.Insert("pady", new TkProc( this, &TkFrame::SetPady ));
	procs.Insert("expand", new TkProc( this, &TkFrame::SetExpand ));
	procs.Insert("side", new TkProc( this, &TkFrame::SetSide ));
	}

TkFrame::TkFrame( Sequencer *s, TkFrame *frame_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_, charptr background,
		  charptr width, charptr height ) : TkAgent( s ), is_tl( 0 ), pseudo( 0 )
	{
	frame = frame_;

	char *argv[12];

	if ( ! root )
		{
		error->Report("Frame creation failed, check DISPLAY environment variable.");
		return;
		}

	agent_ID = "<graphic:frame>";
	side = strdup(side_);
	padx = strdup(padx_);
	pady = strdup(pady_);
	expand = strdup(expand_);

	int c = 2;
	argv[0] = argv[1] = 0;
	argv[c++] = "-relief";
	argv[c++] = (char*) relief_;
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-width";
	argv[c++] = (char*) width;
	argv[c++] = "-height";
	argv[c++] = (char*) height;
	argv[c++] = "-background";
	argv[c++] = (char*) background;

	self = rivet_create(FrameClass, frame->Self(), c, argv);

	if ( ! self )
		fatal->Report("Rivet creation failed in TkFrame::TkFrame");

	AddElement( this );

	if ( frame )
		{
		frame->AddElement( this );
		frame->Pack();
		}
	else
		Pack();

	procs.Insert("padx", new TkProc( this, &TkFrame::SetPadx ));
	procs.Insert("pady", new TkProc( this, &TkFrame::SetPady ));
	procs.Insert("expand", new TkProc( this, &TkFrame::SetExpand ));
	procs.Insert("side", new TkProc( this, &TkFrame::SetSide ));
	}

void TkFrame::UnMap()
	{

	if ( ! elements.length() )
		return;

	// Remove ourselves from the list
	elements.remove_nth( 0 );

	if ( frame )
		frame->RemoveElement( this );

	while ( elements.length() )
		{
		TkAgent *a = elements.remove_nth( 0 );
		a->UnMap( );
		}

	TkAgent::UnMap();
	}

TkFrame::~TkFrame( )
	{
	if ( frame )
		frame->RemoveElement( this );

	UnMap();

	if ( is_tl )
		--tl_cnt;

	if ( pseudo )
		rivet_destroy_window( pseudo );
	}

char *TkFrame::SetSide( parameter_list *args, int is_request, int log )
	{
	HASARG( args, > 0 )
	int c = 0;
	EXPRSTR( side_, "" )
	if ( side_[0] != side[0] || strcmp(side, side_) )
		{
		delete side;
		side = strdup( side_ );
		Pack();
		}
	EXPR_DONE( side_ )
	return "";
	}

char *TkFrame::SetPadx( parameter_list *args, int is_request, int log )
	{
	HASARG( args, > 0 )
	int c = 0;
	EXPRDIM( padx_, "" )
	if ( padx_[0] != padx[0] || strcmp(padx, padx_) )
		{
		delete padx;
		padx = strdup( padx_ );
		Pack();
		}
	EXPR_DONE( padx_ )
	return "";
	}

char *TkFrame::SetPady( parameter_list *args, int is_request, int log )
	{
	HASARG( args, > 0 )
	int c = 0;
	EXPRDIM( pady_, "" )
	if ( pady_[0] != pady[0] || strcmp(pady, pady_) )
		{
		delete pady;
		pady = strdup( pady_ );
		Pack();
		}
	EXPR_DONE( pady_ )
	return "";
	}

char *TkFrame::SetExpand( parameter_list *args, int is_request, int log )
	{
	HASARG( args, > 0 )
	int c = 0;
	EXPRDIM( expand_, "" )
	if ( expand_[0] != expand[0] || strcmp(expand, expand_) )
		{
		delete expand;
		expand = strdup( expand_ );
		Pack();
		}
	EXPR_DONE( expand_ )
	return "";
	}

void TkFrame::PackSpecial( TkAgent *agent )
	{
	const char **instr = agent->PackInstruction();

	int cnt = 0;
	while ( instr[cnt] ) cnt++;

	char **argv = new char*[cnt+8];

	int i = 1;
	argv[0] = 0;
	argv[i++] = rivet_path( agent->Self() );
	argv[i++] = "-side";
	argv[i++] = side;
	argv[i++] = "-padx";
	argv[i++] = padx;
	argv[i++] = "-pady";
	argv[i++] = pady;

	cnt=0;
	while ( instr[cnt] )
		argv[i++] = (char*) instr[cnt++];

	glishtk_pack(root,i,argv);
	delete argv;
	}

void TkFrame::Pack( )
	{
	if ( elements.length() )
		{
		char **argv = new char*[elements.length()+7];

		int c = 1;
		argv[0] = 0;
		loop_over_list( elements, i )
			if ( elements[i]->PackInstruction() )
				PackSpecial( elements[i] );
			else
				argv[c++] = rivet_path(elements[i]->Self() );

		argv[c++] = "-side";
		argv[c++] = side;
		argv[c++] = "-padx";
		argv[c++] = padx;
		argv[c++] = "-pady";
		argv[c++] = pady;

		glishtk_pack(root,c,argv);

		if ( frame )
			frame->Pack();

		delete argv;
		}
	}

void TkFrame::RemoveElement( TkAgent *obj )
	{
	if ( elements.is_member(obj) )
		elements.remove(obj);
	}

TkAgent *TkFrame::Create( Sequencer *s, const_args_list *args_val )
	{
	TkFrame *ret;

	if ( args_val->length() != 12 )
		return InvalidNumberOfArgs(12);

	int c = 1;
	SETVAL( parent, parent->Type() == TYPE_BOOL || parent->IsAgentRecord() )
	SETSTR( relief )
	SETSTR( side )
	SETDIM( borderwidth )
	SETDIM( padx )
	SETDIM( pady )
	SETSTR( expand )
	SETSTR( background )
	SETDIM( width )
	SETDIM( height )
	SETSTR( title )

	if ( parent->Type() == TYPE_BOOL )
		ret =  new TkFrame( s, relief, side, borderwidth, padx, pady, expand,
				    background, width, height, title );
	else
		ret =  new TkFrame( s, (TkFrame*)parent->AgentVal(), relief,
				    side, borderwidth, padx, pady, expand, background,
				    width, height );

	return ret;
	}

const char **TkFrame::PackInstruction()
	{
	static char *ret[5];
	int c = 0;

	if ( strcmp(expand,"none") )
		{
		ret[c++] = "-fill";
		ret[c++] = expand;
		ret[c++] = "-expand";
		ret[c++] = "true";
		ret[c++] = 0;
		return ret;
		}

	return 0;
	}




DEFINE_DTOR(TkButton)

int buttoncb(Rivetobj button, XEvent *unused1, ClientData assoc, ClientData unused2)
	{
	((TkButton*)assoc)->ButtonPressed();
	return TCL_OK;
	}

TkButton::TkButton( Sequencer *s, TkFrame *frame_, charptr label, charptr padx, charptr pady,
		  int width, int height, charptr justify, charptr font, charptr relief,
		  charptr borderwidth, charptr foreground, charptr background, int disabled )
			: TkAgent( s )
	{
	frame = frame_;
	char *argv[28];

	char width_[30];
	char height_[30];

	sprintf(width_,"%d", width);
	sprintf(height_,"%d", height);

	int c = 2;
	argv[0] = argv[1] = 0;
	argv[c++] = "-padx";
	argv[c++] = (char*) padx;
	argv[c++] = "-pady";
	argv[c++] = (char*) pady;
	argv[c++] = "-width";
	argv[c++] = width_;
	argv[c++] = "-height";
	argv[c++] = height_;
	argv[c++] = "-justify";
	argv[c++] = (char*) justify;
	argv[c++] = "-text";
	argv[c++] = (char*) label;
	if ( font[0] )
		{
		argv[c++] = "-font";
		argv[c++] = (char*) font;
		}
	argv[c++] = "-relief";
	argv[c++] = (char*) relief;
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-fg";
	argv[c++] = (char*) foreground;
	argv[c++] = "-bg";
	argv[c++] = (char*) background;
	argv[c++] = "-state";
	argv[c++] = disabled ? "disabled" : "normal";
	argv[c++] = "-command";
	argv[c++] = rivet_new_callback( (int (*)()) buttoncb, (ClientData) this, 0);

	self = rivet_create(ButtonClass, frame->Self(), c, argv);
	agent_ID = "<graphic:button>";

	if ( ! self )
		fatal->Report("Rivet creation failed in TkButton::TkButton");

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("text", new TkProc("-text", glishtk_onestr));
	procs.Insert("font", new TkProc("-font", glishtk_onestr));
	procs.Insert("justify", new TkProc("-justify", glishtk_onestr));
	procs.Insert("height", new TkProc("-height", glishtk_onedim));
	procs.Insert("width", new TkProc("-width", glishtk_onedim));
	procs.Insert("padx", new TkProc("-padx", glishtk_onedim));
	procs.Insert("pady", new TkProc("-pady", glishtk_onedim));
	procs.Insert("disabled", new TkProc("-state", "disabled", "normal", glishtk_onebinary));
	}

void TkButton::ButtonPressed( )
	{
	CreateEvent( "press", new IValue( glish_true ) );
	}

TkAgent *TkButton::Create( Sequencer *s, const_args_list *args_val )
	{
	TkButton *ret;

	if ( args_val->length() != 14 )
		return InvalidNumberOfArgs(14);

	int c = 1;
	SETVAL( parent, parent->IsAgentRecord() )
	SETSTR( label )
	SETDIM( padx )
	SETDIM( pady )
	SETINT( width )
	SETINT( height )
	SETSTR( justify )
	SETSTR( font )
	SETSTR( relief )
	SETDIM( borderwidth )
	SETSTR( foreground )
	SETSTR( background )
	SETINT( disabled )

	ret =  new TkButton( s, (TkFrame*)parent->AgentVal(), label, padx, pady, width, height, justify, font, relief, borderwidth, foreground, background, disabled );

	return ret;
	}      


DEFINE_DTOR(TkScale)

int scalecb(Rivetobj scale, XEvent *unused1, ClientData assoc, ClientData calldata)
	{
	((TkScale*)assoc)->ValueSet( *((double*) calldata ) );
	return TCL_OK;
	}

TkScale::TkScale ( Sequencer *s, TkFrame *frame_, int from, int to, charptr len,
		   charptr text, charptr orient, charptr relief, charptr borderwidth,
		   charptr foreground, charptr background )
			: TkAgent( s )
	{
	frame = frame_;
	char *argv[22];
	char from_[40];
	char to_[40];

	sprintf(from_,"%d",from);
	sprintf(to_,"%d",to);

	int c = 2;
	argv[0] = argv[1] = 0;
	argv[c++] = "-from";
	argv[c++] = from_;
	argv[c++] = "-to";
	argv[c++] = to_;
	argv[c++] = "-length";
	argv[c++] = (char*) len;
	argv[c++] = "-orient";
	argv[c++] = (char*) orient;
	argv[c++] = "-label";
	argv[c++] = (char*) text;
	argv[c++] = "-relief";
	argv[c++] = (char*) relief;
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-fg";
	argv[c++] = (char*) foreground;
	argv[c++] = "-bg";
	argv[c++] = (char*) background;
	argv[c++] = "-command";
	argv[c++] = rivet_new_callback((int (*)()) scalecb, (ClientData) this, 0);

	self = rivet_create(ScaleClass, frame->Self(), c, argv);
	agent_ID = "<graphic:scale>";

	if ( ! self )
		fatal->Report("Rivet creation failed in TkScale::TkScale");

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("length", new TkProc("-length", glishtk_onedim));
	procs.Insert("orient", new TkProc("-orient", glishtk_onestr));
	procs.Insert("text", new TkProc("-label", glishtk_onestr));
	procs.Insert("end", new TkProc("-to", glishtk_oneint));
	procs.Insert("start", new TkProc("-from", glishtk_oneint));
	}

void TkScale::ValueSet( double d )
	{
	CreateEvent( "value", new IValue( d ) );
	}

TkAgent *TkScale::Create( Sequencer *s, const_args_list *args_val )
	{
	TkScale *ret;

	if ( args_val->length() != 11 )
		return InvalidNumberOfArgs(11);

	int c = 1;
	SETVAL( parent, parent->IsAgentRecord() )
	SETINT( start )
	SETINT( end )
	SETDIM( len )
	SETSTR( text )
	SETSTR( orient )
	SETSTR( relief )
	SETDIM( borderwidth )
	SETSTR( foreground )
	SETSTR( background )

	ret = new TkScale( s, (TkFrame*)parent->AgentVal(), start, end, len, text, orient, relief, borderwidth, foreground, background );

	return ret;
	}      


DEFINE_DTOR(TkText)

int text_yscrollcb(Rivetobj button, XEvent *unused1, ClientData assoc, ClientData calldata)
	{
	double *firstlast = (double*)calldata;
	((TkText*)assoc)->yScrolled( firstlast );
	return TCL_OK;
	}

int text_xscrollcb(Rivetobj button, XEvent *unused1, ClientData assoc, ClientData calldata)
	{
	double *firstlast = (double*)calldata;
	((TkText*)assoc)->xScrolled( firstlast );
	return TCL_OK;
	}

TkText::TkText( Sequencer *s, TkFrame *frame_, int width, int height, charptr wrap,
		charptr font, int disabled, charptr text, charptr relief,
		charptr borderwidth, charptr foreground, charptr background,
		charptr fill_ )
			: TkAgent( s ), fill(0)
	{
	frame = frame_;
	char *argv[24];

	self = rivet_create(TextClass, frame->Self(), 0, 0);

	agent_ID = "<graphic:text>";
	if ( ! self )
		fatal->Report("Rivet creation failed in TkText::TkText");

	char width_[30];
	char height_[30];
	sprintf(width_,"%d",width);
	sprintf(height_,"%d",height);

	int c = 2;
	argv[0] = 0; argv[1] = "config";
	argv[c++] = "-width";
	argv[c++] = width_;
	argv[c++] = "-height";
	argv[c++] = height_;
	argv[c++] = "-wrap";
	argv[c++] = (char*) wrap;
	argv[c++] = "-relief";
	argv[c++] = (char*) relief;
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-fg";
	argv[c++] = (char*) foreground;
	argv[c++] = "-bg";
	argv[c++] = (char*) background;
	argv[c++] = "-state";
	argv[c++] = disabled ? "disabled" : "normal";
	if ( font[0] )
		{
		argv[c++] = "-font";
		argv[c++] = (char*) font;
		}
	argv[c++] = "-yscrollcommand";
	argv[c++] = rivet_new_callback((int (*)()) text_yscrollcb, (ClientData) this, 0);
	argv[c++] = "-xscrollcommand";
	argv[c++] = rivet_new_callback((int (*)()) text_xscrollcb, (ClientData) this, 0);

	rivet_cmd(self, c, argv);

	if ( text[0] )
		rivet_va_cmd( self, "insert", "end", text, 0 );

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("view", new TkProc("", glishtk_scrolled_update));
	procs.Insert("width", new TkProc("-width", glishtk_onedim));
	procs.Insert("height", new TkProc("-height", glishtk_onedim));
	procs.Insert("wrap", new TkProc("-wrap", glishtk_onestr));
	procs.Insert("font", new TkProc("-font", glishtk_onestr));
	procs.Insert("disabled", new TkProc("-state", "disabled", "normal", glishtk_onebinary));

	procs.Insert("see", new TkProc(this, "see", glishtk_oneidx));
	procs.Insert("delete", new TkProc(this, "delete", glishtk_oneortwoidx));
	procs.Insert("get", new TkProc(this, "get", glishtk_oneortwoidx, glishtk_splitnl));
	procs.Insert("insert", new TkProc(this, "insert", glishtk_strandidx));
	procs.Insert("append", new TkProc(this, "insert", "end", glishtk_strwithidx));
	procs.Insert("prepend", new TkProc(this, "insert", "start", glishtk_strwithidx));
	}

TkAgent *TkText::Create( Sequencer *s, const_args_list *args_val )
	{
	TkText *ret;

	if ( args_val->length() != 13 )
		return InvalidNumberOfArgs(13);

	int c = 1;
	SETVAL( parent, parent->IsAgentRecord() )
	SETINT( width )
	SETINT( height )
	SETSTR( wrap )
	SETSTR( font )
	SETINT( disabled )
	SETVAL( text, 1 )
	SETSTR( relief )
	SETDIM( borderwidth )
	SETSTR( foreground )
	SETSTR( background )
	SETSTR( fill )

	char *text_str = text->StringVal( ' ', 0, 1 );
	ret =  new TkText( s, (TkFrame*)parent->AgentVal(), width, height, wrap, font, disabled, text_str, relief, borderwidth, foreground, background, fill );
	delete text_str;

	return ret;
	}      

void TkText::yScrolled( const double *d )
	{
	CreateEvent( "yscroll", new IValue( (double*) d, 2, COPY_ARRAY ) );
	}

void TkText::xScrolled( const double *d )
	{
	CreateEvent( "xscroll", new IValue( (double*) d, 2, COPY_ARRAY ) );
	}

charptr TkText::IndexCheck( charptr s )
	{
	if ( s && s[0] == 's' && ! strcmp(s,"start") )
		return "1.0";
	return s;
	}

const char **TkText::PackInstruction()
	{
	static char *ret[5];
	int c = 0;
	if ( fill )
		{
		ret[c++] = "-fill";
		ret[c++] = fill;
		ret[c++] = "-expand";
		ret[c++] = "true";
		ret[c++] = 0;
		return ret;
		}
	else
		return 0;
	}


DEFINE_DTOR(TkScrollbar)

int scrollbarcb(Rivetobj button, XEvent *unused1, ClientData assoc, ClientData calldata)
	{
	Scrollbar_notify_data *data = (Scrollbar_notify_data*) calldata;
	((TkScrollbar*)assoc)->Scrolled( ScrollToValue( data ) );
	return TCL_OK;
	}

TkScrollbar::TkScrollbar( Sequencer *s, TkFrame *frame_, charptr orient,
			  charptr foreground, charptr background )
				: TkAgent( s )
	{
	frame = frame_;
	char *argv[10];

	int c = 2;
	argv[0] = argv[1] = 0;
	argv[c++] = "-orient";
	argv[c++] = (char*) orient;
	argv[c++] = "-command";
	argv[c++] = rivet_new_callback((int (*)()) scrollbarcb, (ClientData) this, 0);

	self = rivet_create(ScrollbarClass, frame->Self(), c, argv);

	agent_ID = "<graphic:scrollbar>";

	if ( ! self )
		fatal->Report("Rivet creation failed in TkScrollbar::TkScrollbar");

	// Setting foreground and background colors at creation
	// time kills the goose
	rivet_set( self, "-bg", (char*) background );
	rivet_set( self, "-fg", (char*) foreground );

	frame->AddElement( this );
	frame->Pack();

	rivet_scrollbar_set( self, 0.0, 1.0 );

	procs.Insert("view", new TkProc("", glishtk_scrollbar_update));
	procs.Insert("orient", new TkProc("-orient", glishtk_onestr));
	}

const char **TkScrollbar::PackInstruction()
	{
	static char *ret[3];
	ret[0] = "-fill";
	ret[2] = 0;
	char *orient = rivet_va_cmd(self, "cget", "-orient", 0);
	if ( orient[0] == 'v' && ! strcmp(orient,"vertical") )
		ret[1] = "y";
	else
		ret[1] = "x";
	return ret;
	}

TkAgent *TkScrollbar::Create( Sequencer *s, const_args_list *args_val )
	{
	TkScrollbar *ret;

	if ( args_val->length() != 5 )
		return InvalidNumberOfArgs(5);

	int c = 1;
	SETVAL( parent, parent->IsAgentRecord() )
	SETSTR( orient )
	SETSTR( foreground )
	SETSTR( background )

	ret = new TkScrollbar( s, (TkFrame*)parent->AgentVal(), orient, foreground, background );

	return ret;
	}      

void TkScrollbar::Scrolled( IValue *data )
	{
	CreateEvent( "scroll", data );
	}


DEFINE_DTOR(TkLabel)

TkLabel::TkLabel( Sequencer *s, TkFrame *frame_, charptr text, charptr justify,
		  charptr padx, charptr pady, int width_, charptr font, charptr relief,
		  charptr borderwidth, charptr foreground, charptr background )
			: TkAgent( s )
	{
	frame = frame_;
	char *argv[22];
	char width[30];

	sprintf(width,"%d",width_);

	int c = 2;
	argv[0] = argv[1] = 0;
	argv[c++] = "-text";
	argv[c++] = (char*) text;
	argv[c++] = "-justify";
	argv[c++] = (char*) justify;
	argv[c++] = "-padx";
	argv[c++] = (char*) padx;
	argv[c++] = "-pady";
	argv[c++] = (char*) pady;
	if ( font[0] )
		{
		argv[c++] = "-font";
		argv[c++] = (char*) font;
		}
	argv[c++] = "-relief";
	argv[c++] = (char*) relief;
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-fg";
	argv[c++] = (char*) foreground;
	argv[c++] = "-bg";
	argv[c++] = (char*) background;
	argv[c++] = "-width";
	argv[c++] = width;

	self = rivet_create(LabelClass, frame->Self(), c, argv);
	agent_ID = "<graphic:label>";

	if ( ! self )
		fatal->Report("Rivet creation failed in TkLabel::TkLabel");

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("font", new TkProc("-font", glishtk_onestr));
	procs.Insert("height", new TkProc("-height", glishtk_oneint));
	procs.Insert("width", new TkProc("-width", glishtk_oneint));
	procs.Insert("text", new TkProc("-text", glishtk_onestr));
	procs.Insert("justify", new TkProc("-justify", glishtk_onestr));
	procs.Insert("padx", new TkProc("-padx", glishtk_onedim));
	procs.Insert("pady", new TkProc("-pady", glishtk_onedim));
	}

TkAgent *TkLabel::Create( Sequencer *s, const_args_list *args_val )
	{
	TkLabel *ret;

	if ( args_val->length() != 12 )
		return InvalidNumberOfArgs(12);

	int c = 1;
	SETVAL( parent, parent->IsAgentRecord() )
	SETSTR( text )
	SETSTR( justify )
	SETDIM( padx )
	SETDIM( pady )
	SETSTR( font )
	SETINT( width )
	SETSTR( relief )
	SETDIM( borderwidth )
	SETSTR( foreground )
	SETSTR( background )

	ret =  new TkLabel( s, (TkFrame*)parent->AgentVal(), text, justify, padx, pady, width,
			    font, relief, borderwidth, foreground, background );

	return ret;
	}      


DEFINE_DTOR(TkEntry)

int entry_returncb(Rivetobj entry, XEvent *unused1, ClientData assoc, ClientData unused2)
	{
	((TkEntry*)assoc)->ReturnHit();
	return TCL_OK;
	}

int entry_xscrollcb(Rivetobj entry, XEvent *unused1, ClientData assoc, ClientData calldata)
	{
	double *firstlast = (double*)calldata;
	((TkEntry*)assoc)->xScrolled( firstlast );
	return TCL_OK;
	}

TkEntry::TkEntry( Sequencer *s, TkFrame *frame_, int width,
		 charptr justify, charptr font, charptr relief, 
		 charptr borderwidth, charptr foreground, charptr background,
		 int disabled, int show, int exportselection )
			: TkAgent( s )
	{
	frame = frame_;
	char *argv[24];
	char width_[30];

	sprintf(width_,"%d",width);

	int c = 2;
	argv[0] = argv[1] = 0;
	argv[c++] = "-width";
	argv[c++] = width_;
	argv[c++] = "-justify";
	argv[c++] = (char*) justify;
	if ( font[0] )
		{
		argv[c++] = "-font";
		argv[c++] = (char*) font;
		}
	argv[c++] = "-relief";
	argv[c++] = (char*) relief;
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-fg";
	argv[c++] = (char*) foreground;
	argv[c++] = "-bg";
	argv[c++] = (char*) background;
	argv[c++] = "-state";
	argv[c++] = disabled ? "disabled" : "normal";
	if ( ! show )
		{
		argv[c++] = "-show";
		argv[c++] = show ? "true" : "false";
		}
	argv[c++] = "-exportselection";
	argv[c++] = exportselection ? "true" : "false";
	argv[c++] = "-xscrollcommand";
	argv[c++] = rivet_new_callback((int (*)()) entry_xscrollcb, (ClientData) this, 0);

	self = rivet_create(EntryClass, frame->Self(), c, argv);
	agent_ID = "<graphic:entry>";

	if ( ! self )
		fatal->Report("Rivet creation failed in TkEntry::TkEntry");

	if ( rivet_create_binding(self, 0, "<Return>", (int (*)()) entry_returncb, (ClientData) this, 1, 0) == TCL_ERROR )
		fatal->Report("Rivet binding creation failed in TkEntry::TkEntry");

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("view", new TkProc("", glishtk_scrolled_update));
	procs.Insert("justify", new TkProc("-justify", glishtk_onestr));
	procs.Insert("font", new TkProc("-font", glishtk_onestr));
	procs.Insert("width", new TkProc("-width", glishtk_oneint));
	procs.Insert("disabled", new TkProc("-state", "disabled", "normal", glishtk_onebinary));
	procs.Insert("exportselection", new TkProc("-exportselection", glishtk_onebool));
	procs.Insert("show", new TkProc("-show", glishtk_onebool));
	procs.Insert("get", new TkProc("get", glishtk_nostr, glishtk_splitnl));
	procs.Insert("insert", new TkProc(this, "insert", glishtk_strandidx));
	procs.Insert("delete", new TkProc(this, "delete", glishtk_oneortwoidx));
	}

void TkEntry::ReturnHit( )
	{
	IValue *ret = new IValue( rivet_va_cmd( self, "get", 0 ) );
	CreateEvent( "return", ret );
	}

void TkEntry::xScrolled( const double *d )
	{
	CreateEvent( "xscroll", new IValue( (double*) d, 2, COPY_ARRAY ) );
	}

TkAgent *TkEntry::Create( Sequencer *s, const_args_list *args_val )
	{
	TkEntry *ret;

	if ( args_val->length() != 12 )
		return InvalidNumberOfArgs(12);

	int c = 1;
	SETVAL( parent, parent->IsAgentRecord() )
	SETINT( width )
	SETSTR( justify )
	SETSTR( font )
	SETSTR( relief )
	SETDIM( borderwidth )
	SETSTR( foreground )
	SETSTR( background )
	SETINT( disabled )
	SETINT( show )
	SETINT( exp )

	ret =  new TkEntry( s, (TkFrame*)parent->AgentVal(), width, justify,
			    font, relief, borderwidth, foreground, background,
			    disabled, show, exp );

	return ret;
	}      

charptr TkEntry::IndexCheck( charptr s )
	{
	if ( s && s[0] == 's' && ! strcmp(s,"start") )
		return "0";
	return s;
	}


DEFINE_DTOR(TkMessage)

TkMessage::TkMessage( Sequencer *s, TkFrame *frame_, charptr text, charptr width, charptr justify,
		      charptr font, charptr padx, charptr pady, charptr relief, charptr borderwidth,
		      charptr foreground, charptr background )
			: TkAgent( s )
	{
	frame = frame_;
	char *argv[22];

	int c = 2;
	argv[0] = argv[1] = 0;
	argv[c++] = "-text";
	argv[c++] = (char*) text;
	argv[c++] = "-justify";
	argv[c++] = (char*) justify;
	argv[c++] = "-width";
	argv[c++] = (char*) width;
	if ( font[0] )
		{
		argv[c++] = "-font";
		argv[c++] = (char*) font;
		}
	argv[c++] = "-padx";
	argv[c++] = (char*) padx;
	argv[c++] = "-pady";
	argv[c++] = (char*) pady;
	argv[c++] = "-relief";
	argv[c++] = (char*) relief;
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-fg";
	argv[c++] = (char*) foreground;
	argv[c++] = "-bg";
	argv[c++] = (char*) background;

	self = rivet_create(MessageClass, frame->Self(), c, argv);
	agent_ID = "<graphic:message>";

	if ( ! self )
		fatal->Report("Rivet creation failed in TkMessage::TkMessage");

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("text", new TkProc("-text", glishtk_onestr));
	procs.Insert("width", new TkProc("-width", glishtk_onedim));
	procs.Insert("justify", new TkProc("-justify", glishtk_onestr));
	procs.Insert("font", new TkProc("-font", glishtk_onestr));
	procs.Insert("padx", new TkProc("-padx", glishtk_onedim));
	procs.Insert("pady", new TkProc("-pady", glishtk_onedim));
	}

TkAgent *TkMessage::Create( Sequencer *s, const_args_list *args_val )
	{
	TkMessage *ret;

	if ( args_val->length() != 12 )
		return InvalidNumberOfArgs(12);

	int c = 1;
	SETVAL( parent, parent->IsAgentRecord() )
	SETSTR( text )
	SETDIM( width )
	SETSTR( justify )
	SETSTR( font )
	SETDIM( padx )
	SETDIM( pady )
	SETSTR( relief )
	SETDIM( borderwidth )
	SETSTR( foreground )
	SETSTR( background )

	ret =  new TkMessage( s, (TkFrame*)parent->AgentVal(), text, width, justify, font, padx, pady, relief, borderwidth, foreground, background );

	return ret;
	}      

DEFINE_DTOR(TkListbox)

int listbox_yscrollcb(Rivetobj button, XEvent *unused1, ClientData assoc, ClientData calldata)
	{
	double *firstlast = (double*)calldata;
	((TkListbox*)assoc)->yScrolled( firstlast );
	return TCL_OK;
	}

int listbox_xscrollcb(Rivetobj button, XEvent *unused1, ClientData assoc, ClientData calldata)
	{
	double *firstlast = (double*)calldata;
	((TkListbox*)assoc)->xScrolled( firstlast );
	return TCL_OK;
	}

int listbox_button1cb(Rivetobj entry, XEvent *unused1, ClientData assoc, ClientData unused2)
	{
	((TkListbox*)assoc)->elementSelected();
	return TCL_OK;
	}

TkListbox::TkListbox( Sequencer *s, TkFrame *frame_, int width, int height, charptr mode,
		      charptr font, charptr relief, charptr borderwidth,
		      charptr foreground, charptr background, int exportselection )
			: TkAgent( s )
	{
	frame = frame_;
	char *argv[24];
	char width_[40];
	char height_[40];

	sprintf(width_,"%d",width);
	sprintf(height_,"%d",height);

	int c = 2;
	argv[0] = argv[1] = 0;
	argv[c++] = "-width";
	argv[c++] = width_;
	argv[c++] = "-height";
	argv[c++] = height_;
	argv[c++] = "-selectmode";
	argv[c++] = (char*) mode;
	if ( font[0] )
		{
		argv[c++] = "-font";
		argv[c++] = (char*) font;
		}
	argv[c++] = "-relief";
	argv[c++] = (char*) relief;
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-fg";
	argv[c++] = (char*) foreground;
	argv[c++] = "-bg";
	argv[c++] = (char*) background;
	argv[c++] = "-exportselection";
	argv[c++] = exportselection ? "true" : "false";
	argv[c++] = "-yscrollcommand";
	argv[c++] = rivet_new_callback((int (*)()) listbox_yscrollcb, (ClientData) this, 0);
	argv[c++] = "-xscrollcommand";
	argv[c++] = rivet_new_callback((int (*)()) listbox_xscrollcb, (ClientData) this, 0);

	self = rivet_create(ListboxClass, frame->Self(), c, argv);
	agent_ID = "<graphic:listbox>";

	if ( ! self )
		fatal->Report("Rivet creation failed in TkListbox::TkListbox");

	if ( rivet_create_binding(self, 0, "<ButtonRelease-1>", (int (*)()) listbox_button1cb, (ClientData) this, 1, 0) == TCL_ERROR )
		fatal->Report("Rivet binding creation failed in TkListbox::TkListbox");


	frame->AddElement( this );
	frame->Pack();

	procs.Insert("view", new TkProc("", glishtk_scrolled_update));
	procs.Insert("mode", new TkProc("-selectmode", glishtk_onestr));
	procs.Insert("font", new TkProc("-font", glishtk_onestr));
	procs.Insert("height", new TkProc("-height", glishtk_oneint));
	procs.Insert("width", new TkProc("-width", glishtk_oneint));
	procs.Insert("exportselection", new TkProc("-exportselection", glishtk_onebool));
	procs.Insert("see", new TkProc(this, "see", glishtk_oneidx));
	procs.Insert("clear", new TkProc(this, "select", "clear", glishtk_no2str));
	procs.Insert("selection", new TkProc("curselection", glishtk_nostr, glishtk_splitsp_int));
	procs.Insert("delete", new TkProc(this, "delete", glishtk_oneortwoidx));
	procs.Insert("insert", new TkProc(this, "insert", glishtk_listbox_insert));
	procs.Insert("get", new TkProc(this, "get", glishtk_listbox_get, glishtk_splitnl));
	}

TkAgent *TkListbox::Create( Sequencer *s, const_args_list *args_val )
	{
	TkListbox *ret;

	if ( args_val->length() != 11 )
		return InvalidNumberOfArgs(11);

	int c = 1;
	SETVAL( parent, parent->IsAgentRecord() )
	SETINT( width )
	SETINT( height )
	SETSTR( mode )
	SETSTR( font )
	SETSTR( relief )
	SETDIM( borderwidth )
	SETSTR( foreground )
	SETSTR( background )
	SETINT( exp )

	ret =  new TkListbox( s, (TkFrame*)parent->AgentVal(), width, height, mode, font, relief, borderwidth, foreground, background, exp );

	return ret;
	}      

charptr TkListbox::IndexCheck( charptr s )
	{
	if ( s && s[0] == 's' && ! strcmp(s,"start") )
		return "0";
	return s;
	}

void TkListbox::yScrolled( const double *d )
	{
	CreateEvent( "yscroll", new IValue( (double*) d, 2, COPY_ARRAY ) );
	}

void TkListbox::xScrolled( const double *d )
	{
	CreateEvent( "xscroll", new IValue( (double*) d, 2, COPY_ARRAY ) );
	}

void TkListbox::elementSelected(  )
	{
	CreateEvent( "select", glishtk_splitsp_int(rivet_va_cmd( self, "curselection", 0 )) );
	}

#endif
