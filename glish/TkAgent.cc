// $Header$
#include "Glish/glish.h"
RCSID("@(#) $Id$")
#ifdef GLISHTK
#include "TkAgent.h"
#include "TkCanvas.h"

#if defined(TKPGPLOT)
#include "TkPGPLOT.h"
#endif

#include <X11/Xlib.h>
#include <string.h>
#include <stdlib.h>
#include "Rivet/rivet.h"
#include "Reporter.h"
#include "IValue.h"
#include "Sequencer.h"
#include "Expr.h"

#include <iostream.h>

Rivetobj TkAgent::root = 0;
unsigned long TkFrame::top_created = 0;
unsigned long TkFrame::tl_count = 0;
unsigned long TkFrame::frame_count = 0;
unsigned long TkFrame::grab = 0;
PQueue(glishtk_event) *TkAgent::tk_queue = 0;
int TkAgent::hold_events = 0;
int TkAgent::initial_hold = 0;

extern IValue *glishtk_valcast( char * );

class glishtk_event {
    public:
	glishtk_event(Sequencer *s_, TkAgent *a_, const char *n_, IValue *v_,
			  int complain_if_no_interest_=0, NotifyTrigger *t_=0 ) :
			s(s_), agent(a_), nme(n_ ? strdup(n_) : strdup(" ")),
			val(v_), t(t_), complain(complain_if_no_interest_)
			{ Ref(agent); Ref(val); }
	void Post();
	~glishtk_event();
    protected:
	Sequencer *s;
	TkAgent *agent;
	char *nme;
	IValue *val;
	NotifyTrigger *t;
	int complain;
};

void glishtk_event::Post()
	{
	glish_event_posted( s->NewEvent( agent, nme, val, complain, t ) );
	}

glishtk_event::~glishtk_event()
	{
	Unref(val);
	Unref(agent);
	delete nme;
	}

class ScrollbarTrigger : public NotifyTrigger {
    public:
	void NotifyDone( );
	ScrollbarTrigger( TkScrollbar *s );
	~ScrollbarTrigger();
    protected:
	TkScrollbar *sb;
};

void ScrollbarTrigger::NotifyDone( )
	{
	Tk_TimerToken t = rivet_last_timer();
	if ( t && rivet_timer_expired( t ) )
		{
		rivet_cancel_repeat();
		Rivetobj self = sb->Self();
		char buf[256];

		char *ret = rivet_va_cmd(self, "cget", "-repeatinterval", 0);
		int cur = atoi(ret);
		sprintf(buf, "%d", cur + (int)(cur / 0.5));
		rivet_set( self, "-repeatinterval", buf );

		ret = rivet_va_cmd(self, "cget", "-repeatdelay", 0);
		cur = atoi(ret);
		sprintf(buf, "%d", cur + (int)(cur / 0.5));
		rivet_set( self, "-repeatdelay", buf );

		rivet_set_scroll_timer(10);
		}
	}

ScrollbarTrigger::ScrollbarTrigger(  TkScrollbar *s ) : sb(s) { }

ScrollbarTrigger::~ScrollbarTrigger( ) { }


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

IValue *glishtk_str( char *str )
	{
	return new IValue( str );
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
#define EXPAND_ACTION_A					\
		if ( cnt >= len )			\
			{				\
			len *= 2;			\
			ary = (int *) realloc(ary, len * sizeof(int));\
			}
		EXPAND_ACTION_A
		ary[cnt++] = atoi(start);
		*end++ = ' ';
		start = end;
		}

	if ( *start )
		{
		EXPAND_ACTION_A
		ary[cnt++] = atoi(start);
		}

	return new IValue( ary, cnt, COPY_ARRAY );
	}

char **glishtk_splitsp_str_( char *sel, int &cnt )
	{
	char *start = sel;
	char *end;
	cnt = 0;
	static int len = 2;
	static char **ary = new char*[len];

	while ( *start && (end = strchr(start,' ')) )
		{
		*end = (char) 0;
#define EXPAND_ACTION_B					\
		if ( cnt >= len )			\
			{				\
			len *= 2;			\
			ary = (char **) realloc(ary, len * sizeof(char*) );\
			}
		EXPAND_ACTION_B
		ary[cnt++] = strdup(start);
		*end++ = ' ';
		start = end;
		}

	if ( *start )
		{
		EXPAND_ACTION_B
		ary[cnt++] = strdup(start);
		}

	return ary;
	}

IValue *glishtk_splitsp_str( char *s )
	{
	int len=0;
	char **str = glishtk_splitsp_str_(s, len);
	return new IValue( (charptr*) str, len, COPY_ARRAY );
	}

inline void glishtk_pack( Rivetobj root, int argc, char **argv)
	{
// 	for (int i=0; i < argc; i++)
// 		cout << (argv[i] ? argv[i] : "pack") << " ";
// 	cout << endl;
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
	charptr var = ( var##_v_ ->StringPtr(0) )[0];
#define SETDIM(var)							\
	SETVAL(var##_v_, var##_v_ ->Type() == TYPE_STRING &&		\
				var##_v_ ->Length() > 0   ||		\
				var##_v_ ->IsNumeric() )		\
	char var##_char_[30];						\
	charptr var = 0;						\
	if ( var##_v_ ->Type() == TYPE_STRING )				\
		var = ( var##_v_ ->StringPtr(0) )[0];			\
	else								\
		{							\
		sprintf(var##_char_,"%d", var##_v_ ->IntVal());	\
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

#define EXPRSTRVAL(var,EVENT)						\
	Expr *var##_expr_ = (*args)[c++]->Arg();			\
	const IValue *var = var##_expr_ ->ReadOnlyEval();		\
	const IValue *var##_val_ = var;					\
	if ( ! var || var ->Type() != TYPE_STRING ||			\
		var->Length() <= 0 )					\
		{							\
		error->Report("bad value for ", EVENT);			\
		var##_expr_ ->ReadOnlyDone(var);			\
		return 0;						\
		}
#define EXPRSTR(var,EVENT)						\
	charptr var = 0;						\
	EXPRSTRVAL(var##_val_, EVENT)					\
	Expr *var##_expr_ = var##_val__expr_;				\
	var = ( var##_val_ ->StringPtr(0) )[0];
#define EXPRDIM(var,EVENT)						\
	Expr *var##_expr_ = (*args)[c++]->Arg();			\
	const IValue *var##_val_ = var##_expr_ ->ReadOnlyEval();	\
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
			var = ( var##_val_ ->StringPtr(0) )[0];		\
		else							\
			{						\
			sprintf(var##_char_,"%d", var##_val_->IntVal());\
			var = var##_char_;				\
			}

#define EXPRINT(var,EVENT)                                              \
        Expr *var##_expr_ = (*args)[c++]->Arg();                        \
        const IValue *var##_val_ = var##_expr_ ->ReadOnlyEval();        \
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
	const IValue *var##_val_ = var##_expr_ ->ReadOnlyEval();	\
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
		var = ( var##_val_ ->StringPtr(0) )[0];			\
	else								\
		{							\
		error->Report("bad type for ", EVENT);			\
		var##_expr_ ->ReadOnlyDone(var##_val_);			\
		return 0;						\
		}

#define EXPRDOUBLEPTR(var, NUM, EVENT)					\
	Expr *var##_expr_ = (*args)[c++]->Arg();			\
	const IValue *var##_val_ = var##_expr_ ->ReadOnlyEval();	\
	double *var = 0;						\
	if ( ! var##_val_ || var##_val_ ->Type() != TYPE_DOUBLE ||	\
		var##_val_ ->Length() < NUM )				\
		{							\
		error->Report("bad value for ", EVENT);			\
		var##_expr_ ->ReadOnlyDone(var##_val_);			\
		return 0;						\
		}							\
	else								\
		var = var##_val_ ->DoublePtr(0);

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
		{					\
		frame->RemoveElement( this );		\
		frame->Pack();				\
		}					\
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

char *glishtk_oneintlist(Rivetobj self, const char *cmd, int howmany, parameter_list *args,
				int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one int list function";
	HASARG( args, >= howmany )
	static int len = 4;
	static char *buf = new char[len*128];
	static char elem[128];

	if ( ! howmany )
		howmany = args->length();

	while ( howmany > len )
		{
		len *= 2;
		buf = (char *) realloc(buf, len * sizeof(char) * 128);
		}

	int c = 0;
	for ( int x=0; x < howmany; x++ )
		{
		EXPRINT( v, event_name )
		sprintf(elem,"%d ",v);
		strcat(buf,elem);
		EXPR_DONE( v )
		}

	ret = (char*) rivet_set( self, (char*) cmd, buf );
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

struct strary_ret {
  char **ary;
  int len;
};

IValue *glishtk_strary_to_value( char *s )
	{
	strary_ret *r = (strary_ret*) s;
	IValue *ret = new IValue((charptr*) r->ary,r->len);
	delete r;
	return ret;
	}
char *glishtk_oneortwoidx_strary(TkAgent *a, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *event_name = "one-or-two index function";

	HASARG( args, > 0 )
	int c = 0;

	strary_ret *ret = 0;
	if ( args->length() >= 2 )
		{
		ret = new strary_ret;
		ret->ary = new char*[(int)(args->length()/2)];
		ret->len = 0;
		for ( int i=0; i+1 < args->length(); i+=2 )
			{
			EXPRSTR(one, event_name)
			EXPRSTR(two, event_name)
			char *s = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( one ), a->IndexCheck( two ), 0);
			if ( s ) ret->ary[ret->len++] = strdup(s);
			EXPR_DONE(one)
			EXPR_DONE(two)
			}
		}
	else
		{
		EXPRVAL(start,event_name)
	        if ( ! start || start->Type() != TYPE_STRING ||
		     start->Length() <= 0 )
			{
			error->Report("bad value for ", event_name);
			EXPR_DONE(start)
			return 0;
			}

		if ( start->Length() > 1 )
			{
			ret = new strary_ret;
			ret->len = 0;
			ret->ary = new char*[(int)(start->Length() / 2)];
			charptr *idx = start->StringPtr(0);
			for ( int i = 0; i+1 < start->Length(); i+=2 )
				{
				char *s = rivet_va_cmd(a->Self(), cmd,
						       a->IndexCheck( idx[i] ),
						       a->IndexCheck( idx[i+1] ),0);

				if ( s ) ret->ary[ret->len++] = strdup(s);
				}
			}
		else
			{
			char *s = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( (start->StringPtr(0))[0] ), 0);
			if ( s )
				{
				ret = new strary_ret;
			        ret->len = 1;
				ret->ary = new char*[1];
				ret->ary[0] = strdup(s);
				}
			}
		}
			
	return (char*) ret;
	}

char *glishtk_oneortwoidx2str(TkAgent *a, const char *cmd, const char *param,
			      parameter_list *args, int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one-or-two index function";

	HASARG( args, > 0 )
	int c = 0;
	EXPRSTR( start, event_name )
	if ( args->length() > 1 )
		{
		EXPRSTR( end, event_name )
		ret = rivet_va_cmd(a->Self(), cmd, param, a->IndexCheck( start ),
					     a->IndexCheck( end ), 0);
		EXPR_DONE( end )
		}
	else
		ret = rivet_va_cmd(a->Self(), cmd, param, a->IndexCheck( start ), 0);
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

char *glishtk_text_append(TkAgent *a, const char *cmd, const char *param,
				parameter_list *args, int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "text append function";

	HASARG( args, > 0 )
	int c = 0;
	EXPRVAL( val, event_name );
	char *str = val->StringVal( ' ', 0, 1 );
	ret = rivet_va_cmd(a->Self(), cmd, a->IndexCheck(param), str, 0);
	rivet_va_cmd(a->Self(), "see", a->IndexCheck("end"), 0);
	delete str;
	EXPR_DONE( val )
	return ret;
	}

char *glishtk_text_tagfunc(Rivetobj self, const char *cmd, const char *param,
			   parameter_list *args, int is_request, int log )
	{
	char *event_name = "tag function";
		
	if ( args-> length() < 2 )
		{
		error->Report("wrong number of arguments");
		return 0;
		}
	int c = 0;
	EXPRSTR(tag, event_name)
	int argc = 0;
	char *argv[8];
	argv[argc++] = 0;
	argv[argc++] = (char*) cmd;
	argv[argc++] = (char*) param;
	argv[argc++] = (char*) tag;
	if ( args->length() >= 3 )
		for ( int i=c; i+1 < args->length(); i+=2 )
			{
			EXPRSTR(one, event_name)
			argv[argc] = (char*)one;
			EXPRSTR(two, event_name)
			argv[argc+1] = (char*)two;
			rivet_cmd(self,argc+2,argv);
			EXPR_DONE(one)
			EXPR_DONE(two)
			}
	else
		{
		EXPRSTRVAL(str_v, event_name)
		charptr *s = str_v->StringPtr(0);

		if ( str_v->Length() == 1 )
			{
			argv[argc] = (char*)s[0];
			rivet_cmd(self,argc+1,argv);
			}
		else
			for ( int i=0; i+1 < str_v->Length(); i+=2 )
				{
				argv[argc] = (char*)s[i];
				argv[argc+1] = (char*)s[i+1];
				rivet_cmd(self,argc+2,argv);
				}

		EXPR_DONE(str_v)
		}

	EXPR_DONE(tag)
	return 0;
	}

char *glishtk_text_configfunc(Rivetobj self, const char *cmd, const char *param,
			      parameter_list *args, int is_request, int log )
	{
	char *event_name = "tag function";
	if ( args->length() < 2 )
		{
		error->Report("wrong number of arguments");
		return 0;
		}
	int c = 0;
	char buf[512];
	int argc = 0;
	char *argv[8];
	argv[argc++] = 0;
	argv[argc++] = (char*) cmd;
	argv[argc++] = (char*) param;
	EXPRSTR(tag, event_name)
	argv[argc++] = (char*) tag;
	for ( int i=c; i < args->length(); i++ )
		{
		if ( (*args)[i]->Name() )
			{
			int doit = 1;
			sprintf(buf,"-%s",(*args)[i]->Name());
			argv[argc] = buf;
			c = i;
			EXPRVAL(val, event_name)
			if ( val->Type() == TYPE_STRING )
				argv[argc+1] = (char*)((val->StringPtr(0))[0]);
			else if ( val->Type() == TYPE_BOOL )
				argv[argc+1] = val->BoolVal() ? "true" : "false";
			else
				doit = 0;
			
			if ( doit ) rivet_cmd(self,argc+2,argv);
			EXPR_DONE(val)
			}
		}
	EXPR_DONE(tag)
	return 0;
	}

char *glishtk_text_rangesfunc(Rivetobj self, const char *cmd, const char *param,
			      parameter_list *args, int is_request, int log )
	{
	char *event_name = "tag function";
	if ( args->length() < 1 )
		{
		error->Report("wrong number of arguments");
		return 0;
		}
	int c = 0;
	int argc = 0;
	char *argv[8];
	argv[argc++] = 0;
	argv[argc++] = (char*) cmd;
	argv[argc++] = (char*) param;
	EXPRSTR(tag, event_name)
	argv[argc++] = (char*) tag;
	rivet_cmd(self,argc,argv);
	EXPR_DONE(tag)
	return 	self->interp->result;
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
	charptr *strs = str_v->StringPtr(0);

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
	int *index = val->IntPtr(0);
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
					    a->IndexCheck( val->StringPtr(0)[0] ),
					    a->IndexCheck( end ), 0 );
			EXPR_DONE( end )
			}
		else
			ret = rivet_va_cmd( a->Self(), cmd,
					    a->IndexCheck( val->StringPtr(0)[0] ), 0 );
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

char *glishtk_button_state(TkAgent *a, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "button state function";

	int c = 0;
	if ( args->length() >= 1 )
		{
		EXPRINT( i, event_name )
		((TkButton*)a)->State( i ? 1 : 0 );
		EXPR_DONE( i )
		}

	ret = ((TkButton*)a)->State( ) ? "T" : "F";
	return ret;
	}

extern "C" int rivet_menuentry_set(Rivetobj,char*,char*,char*);

char *glishtk_menu_onestr(TkAgent *a, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char index[100];
	TkButton *Self = (TkButton*)a;
	TkButton *Parent = Self->Parent();
	char *ret = 0;
	char *event_name = "one string menu function";
	HASARG( args, > 0 )
	int c = 0;
	EXPRSTR( str, event_name )
	sprintf(index,"%d",Parent->Index(Self) - 1);
	ret = (char*) rivet_va_cmd( Parent->Menu(), "entryconfigure", index, (char*) cmd, (char*) str, 0 );
	EXPR_DONE( str )
	return ret;
	}

char *glishtk_menu_onebinary(TkAgent *a, const char *cmd, const char *ptrue, const char *pfalse,
				parameter_list *args, int is_request, int log )
	{
	char index[100];
	TkButton *Self = (TkButton*)a;
	TkButton *Parent = Self->Parent();
	char *ret = 0;
	char *event_name = "one binary menu function";
	HASARG( args, > 0 )
	int c = 0;
	EXPRINT( i, event_name )
	sprintf(index,"%d",Parent->Index(Self));
	ret = (char*) rivet_va_cmd( Parent->Menu(), "entryconfigure", index, (char*) cmd, (char*)(i ? ptrue : pfalse), 0 );
	EXPR_DONE( i )
	return ret;
	}

IValue *glishtk_strtobool( char *str )
	{
	if ( str && (*str == 'T' || *str == '1') )
		return new IValue( glish_true );
	else
		return new IValue( glish_false );
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
	else if ( fproc && frame )
		val = (frame->*fproc)( arg, x, y );
	else if ( aproc && agent )
		val = (*aproc)(agent, cmdstr, arg, x, y);
	else if ( aproc2 && agent )
		val = (*aproc2)(agent, cmdstr, param, arg, x, y);
	else if ( aproc3 && agent )
		val = (*aproc3)(agent, cmdstr, param, param2, arg, x, y);
	else if ( iproc )
		val = (*iproc)(s, cmdstr, i, arg, x, y);
	else if ( iproc1 )
		val = (*iproc1)(s, cmdstr, param, i, arg, x, y);
#if defined(TKPGPLOT)
	else if ( pgproc && pgplot )
		val = (pgplot->*pgproc)( arg, x, y );
#endif
	else
		return error_ivalue();

	Sequencer::HoldQueue();
	while ( Tk_DoOneEvent( TK_ALL_EVENTS & ~TK_FILE_EVENTS & ~TK_TIMER_EVENTS | TK_DONT_WAIT ) ) ;
	Sequencer::ReleaseQueue();

	if ( convert && val )
		return (*convert)(val);
	else
		return new IValue( glish_true );
	}

void TkAgent::PostTkEvent( const char *s, IValue *v, int complain_if_no_interest,
			   NotifyTrigger *t )
	{
	if ( hold_events )
		tk_queue->EnQueue( new glishtk_event( sequencer, this, s, v, complain_if_no_interest, t ) );
	else
		glish_event_posted( sequencer->NewEvent( this, s, v, complain_if_no_interest, t ) );
	}

void TkAgent::ReleaseEvents()
	{
	glishtk_event* e;

	if ( initial_hold )
		{
		hold_events -= initial_hold - 1;
		if ( hold_events <= 0 )
			hold_events = 1;
		initial_hold = 0;
		}

	if ( hold_events && ! --hold_events )
		while ( (e = tk_queue->DeQueue()) )
			{
			e->Post();
			delete e;
			}
	}


TkAgent::TkAgent( Sequencer *s ) : Agent( s )
	{
	agent_ID = "<graphic>";

	if ( tk_queue == 0 )
		tk_queue = new PQueue(glishtk_event)();

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
	return (IValue*) Fail(this," unrecognized event");
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
		return (IValue*) Fail("graphic is defunct");

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

int TkAgent::CanExpand() const
	{
	return 0;
	}

TkAgent::~TkAgent( )
	{
	IterCookie* c = procs.InitForIteration();

	TkProc* member;
	const char* key;
	while ( (member = procs.NextEntry( key, c )) )
		delete member;
	}

int glishtk_delframe_cb(Rivetobj frame, XEvent *unused1, ClientData assoc, ClientData unused2)
	{
	((TkFrame*)assoc)->KillFrame();
	return TCL_OK;
	}

TkFrame::TkFrame( Sequencer *s, charptr relief_, charptr side_, charptr borderwidth,
		  charptr padx_, charptr pady_, charptr expand_, charptr background, charptr width,
		  charptr height, charptr cursor, charptr title ) : TkAgent( s ), is_tl( 1 ),
		  pseudo( 0 ), tag(0), radio_id(0), canvas(0), side(0), padx(0), pady(0), expand(0)

	{
	char *argv[15];

	agent_ID = "<graphic:frame>";

	HoldEvents();
	initial_hold += 1;

	if ( ! root )
		fatal->Report("Frame creation failed, check DISPLAY environment variable.");

	id = ++frame_count;
	tl_count++;

	if ( top_created )
		{
		int c = 2;
		argv[0] = argv[1] = 0;
		argv[c++] = "-borderwidth";
		argv[c++] = "0";
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
		{
		top_created = 1;
		if ( title && title[0] )
			rivet_va_func(root, (int (*)()) Tk_WmCmd, "title",
				      rivet_path(root), title, 0 );
		}

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
	if ( cursor && *cursor )
		{
		argv[c++] = "-cursor";
		argv[c++] = (char*) cursor;
		}

	self = rivet_create(FrameClass, pseudo ? pseudo : root, c, argv);

	if ( ! self )
		fatal->Report("Rivet creation failed in TkFrame::TkFrame");

	rivet_va_func(self, (int(*)()) Tk_WmCmd, "protocol", rivet_path((pseudo ? pseudo : root)), "WM_DELETE_WINDOW",
		      rivet_new_callback((int (*)()) glishtk_delframe_cb,(ClientData) this, 0), 0);

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
	procs.Insert("grab", new TkProc( this, &TkFrame::GrabCB ));
	procs.Insert("fonts", new TkProc( this, &TkFrame::FontsCB, glishtk_valcast ));
	procs.Insert("release", new TkProc( this, &TkFrame::ReleaseCB ));
	procs.Insert("cursor", new TkProc("-cursor", glishtk_onestr));
	}

TkFrame::TkFrame( Sequencer *s, TkFrame *frame_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_, charptr background,
		  charptr width, charptr height, charptr cursor ) : TkAgent( s ), is_tl( 0 ), pseudo( 0 ),
		  tag(0), radio_id(0), canvas(0), side(0), padx(0), pady(0), expand(0)
	{
	char *argv[14];
	frame = frame_;

	agent_ID = "<graphic:frame>";

	id = ++frame_count;
	if ( ! root )
		fatal->Report("Frame creation failed, check DISPLAY environment variable.");

	if ( ! frame || ! frame->Self() ) return;

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
	if ( cursor && *cursor )
		{
		argv[c++] = "-cursor";
		argv[c++] = (char*) cursor;
		}

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
	procs.Insert("grab", new TkProc( this, &TkFrame::GrabCB ));
	procs.Insert("fonts", new TkProc( this, &TkFrame::FontsCB, glishtk_valcast ));
	procs.Insert("release", new TkProc( this, &TkFrame::ReleaseCB ));
	procs.Insert("cursor", new TkProc("-cursor", glishtk_onestr));
	}

TkFrame::TkFrame( Sequencer *s, TkCanvas *canvas_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_, charptr background,
		  charptr width, charptr height, const char *tag_ ) : TkAgent( s ), is_tl( 0 ),
		  pseudo( 0 ), radio_id(0), side(0), padx(0), pady(0), expand(0)
	{
	char *argv[12];
	frame = 0;
	canvas = canvas_;
	tag = strdup(tag_);

	agent_ID = "<graphic:frame>";

	id = ++frame_count;
	if ( ! root )
		fatal->Report("Frame creation failed, check DISPLAY environment variable.");

	if ( ! canvas || ! canvas->Self() ) return;

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

	self = rivet_create(FrameClass, canvas->Self(), c, argv);

	if ( ! self )
		fatal->Report("Rivet creation failed in TkFrame::TkFrame");

//	AddElement( this );

	if ( frame )
		{
		frame->AddElement( this );
		frame->Pack();
		}
	else
		Pack();

	procs.Insert("padx", new TkProc( this, &TkFrame::SetPadx ));
	procs.Insert("pady", new TkProc( this, &TkFrame::SetPady ));
	procs.Insert("tag", new TkProc( this, &TkFrame::GetTag, glishtk_str ));
	procs.Insert("side", new TkProc( this, &TkFrame::SetSide ));
	procs.Insert("grab", new TkProc( this, &TkFrame::GrabCB ));
	procs.Insert("fonts", new TkProc( this, &TkFrame::FontsCB, glishtk_valcast ));
	procs.Insert("release", new TkProc( this, &TkFrame::ReleaseCB ));
	procs.Insert("cursor", new TkProc("-cursor", glishtk_onestr));
	}

void TkFrame::UnMap()
	{
	if ( grab && grab == id )
		Release();

	if ( canvas )
		canvas->Remove( this );
	else
		// Remove ourselves from the list
		// -- not done with canvas
		elements.remove_nth( 0 );

	if ( frame )
		frame->RemoveElement( this );

	while ( elements.length() )
		{
		TkAgent *a = elements.remove_nth( 0 );
		a->UnMap( );
		}

	int unmap_root = self && ! pseudo && ! frame && ! canvas;
	TkAgent::UnMap();

	if ( pseudo )
		{
		rivet_destroy_window( pseudo );
		pseudo = 0;
		}

	if ( unmap_root )
		rivet_unmap_window( root );
	}

TkFrame::~TkFrame( )
	{
	if ( frame )
		frame->RemoveElement( this );

	UnMap();

	if ( is_tl )
		--tl_count;

	if ( pseudo )
		{
		rivet_destroy_window( pseudo );
		pseudo = 0;
		}

	if ( tag )
		delete tag;

	if ( side ) delete side;
	if ( padx ) delete padx;
	if ( pady ) delete pady;
	if ( expand ) delete expand;

	if ( ! tl_count )
		// Empty queue
		while( Tk_DoOneEvent( TK_X_EVENTS | TK_IDLE_EVENTS | TK_DONT_WAIT ) != 0 );
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

char *TkFrame::GetTag( parameter_list *args, int is_request, int log )
	{
	return tag;
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

char *TkFrame::Grab( int global_scope )
	{
	if ( grab ) return 0;

	if ( global_scope )
		rivet_va_func(self, (int(*)()) Tk_GrabCmd, "set", "-global", rivet_path(self), 0);
	else
		rivet_va_func(self, (int(*)()) Tk_GrabCmd, "set", rivet_path(self), 0);

	grab = id;
	return "";
	}

char *TkFrame::GrabCB( parameter_list *args, int, int )
	{
	if ( grab ) return 0;

	int global_scope = 0;
	if ( args->length() > 0 )
		{
		int c = 0;
		EXPRDIM( str, "" )

		if ( ! strcmp(str,"global") )
			global_scope = 1;

		EXPR_DONE( str )
		}

	return Grab( global_scope );
	}

char *TkFrame::FontsCB( parameter_list *args, int, int )
	{
	char *wild = "-*-*-*-*-*-*-*-*-*-*-*-*-*-*";
	char **fonts = 0;
	int len = 0;
	int c = 0;
	if ( args->length() == 1 )
		{
		EXPRVAL( val, "TkFrame::FontsCB" )
		if ( val->Type() == TYPE_STRING && val->Length() > 0 )
			fonts = XListFonts(self->display, val->StringPtr(0)[0], 32768, &len);
		else if ( val->Type() == TYPE_INT && val->Length() > 0 )
			fonts = XListFonts(self->display, wild, val->IntVal(), &len);
		EXPR_DONE( val )
		}
	else if ( args->length() >= 2 )
		{
		EXPRSTR( str, "TkFrame::FontsCB" )
		EXPRINT( l, "TkFrame::FontsCB" )
		fonts = XListFonts(self->display, str, l, &len);
		EXPR_DONE( str )
		EXPR_DONE( l )
		}
	else
		fonts = XListFonts(self->display, wild, 32768, &len);

	IValue *ret = fonts ? new IValue( (charptr*) fonts, len, COPY_ARRAY ) : false_ivalue();
	XFreeFontNames(fonts);
	return (char*) ret;
	}

char *TkFrame::Release( )
	{
	if ( ! grab || grab != id ) return 0;

	rivet_va_func(self, (int(*)()) Tk_GrabCmd, "release", rivet_path(self), 0);

	grab = 0;
	return "";
	}

char *TkFrame::ReleaseCB( parameter_list *, int, int )
	{
	return Release( );
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

int TkFrame::ExpandNum(const TkAgent *except, unsigned int grtOReqt) const
	{
	int cnt = 0;
	loop_over_list( elements, i )
		{
		if ( (! except || elements[i] != except) &&
		     elements[i] != this && elements[i]->CanExpand() )
			cnt++;
		if ( grtOReqt && cnt >= grtOReqt )
			break;
		}
	return cnt;
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

		if ( c > 1 )
			{
			argv[c++] = "-side";
			argv[c++] = side;
			argv[c++] = "-padx";
			argv[c++] = padx;
			argv[c++] = "-pady";
			argv[c++] = pady;

			glishtk_pack(root,c,argv);
			}

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

void TkFrame::KillFrame( )
	{
	PostTkEvent( "killed", new IValue( glish_true ) );
	UnMap();
	}

TkAgent *TkFrame::Create( Sequencer *s, const_args_list *args_val )
	{
	TkFrame *ret = 0;

	if ( args_val->length() != 13 )
		return InvalidNumberOfArgs(13);

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
	SETSTR( cursor )
	SETSTR( title )

	if ( parent->Type() == TYPE_BOOL )
		ret =  new TkFrame( s, relief, side, borderwidth, padx, pady, expand,
				    background, width, height, cursor, title );
	else
		{
		Agent *agent = parent->AgentVal();
		if ( ! strcmp("<graphic:frame>", agent->AgentID()) )
			ret =  new TkFrame( s, (TkFrame*)agent, relief,
					    side, borderwidth, padx, pady, expand, background,
					    width, height, cursor );
		}

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

		if ( ! canvas && (! frame || ! strcmp(expand,"both") ||
			! strcmp(expand,"x") && (! strcmp(frame->side,"left") || 
						 ! strcmp(frame->side,"right"))  ||
			! strcmp(expand,"y") && (! strcmp(frame->side,"top") || 
						 ! strcmp(frame->side,"bottom"))) )
			{
			ret[c++] = "-expand";
			ret[c++] = "true";
			}
		else
			{
			ret[c++] = "-expand";
			ret[c++] = "false";
			}
		ret[c++] = 0;
		return ret;
		}

	return 0;
	}

int TkFrame::CanExpand() const
	{
	return ! canvas && (! frame || ! strcmp(expand,"both") ||
		! strcmp(expand,"x") && (! strcmp(frame->side,"left") || 
					 !strcmp(frame->side,"right")) ||
		! strcmp(expand,"y") && (! strcmp(frame->side,"top") || 
					 ! strcmp(frame->side,"bottom")) );
	}

void TkButton::UnMap()
	{
	if ( self && frame )
		{
		if ( type == MENU )
			while ( entry_list.length() )
				{
				TkAgent *a = entry_list.remove_nth( 0 );
				a->UnMap( );
				}

		rivet_destroy_window( self );
		}
	else if ( menu )
		{
		char index[100];
		sprintf(index,"%d",menu->Index(this));
		rivet_va_cmd( menu->Menu(), "delete", index, 0 );
		menu->Remove(this);
		}

	menu = 0;
	frame = 0;
	self = 0;
	}

TkButton::~TkButton( )
	{
	if ( frame )
		{
		frame->RemoveElement( this );
		frame->Pack();
		}

	if ( value ) Unref(value);

	UnMap();
	}

unsigned long TkButton::button_count = 0;
static unsigned char dont_invoke_button = 0;

int buttoncb(Rivetobj button, XEvent *unused1, ClientData assoc, ClientData unused2)
	{
	((TkButton*)assoc)->ButtonPressed();
	return TCL_OK;
	}

int tk_button_menupost(Rivetobj menu, XEvent *unused1, ClientData ignored, ClientData unused2)
	{
	return TCL_OK;
	}

int TkButton::Index( TkButton *item ) const
	{
	for ( int i=0; i < entry_list.length(); i++ )
		if ( item == entry_list[i] )
			return i+1;

	return 0;
	}

TkButton::TkButton( Sequencer *s, TkFrame *frame_, charptr label, charptr type_,
		    charptr padx, charptr pady, int width, int height, charptr justify,
		    charptr font, charptr relief, charptr borderwidth, charptr foreground,
		    charptr background, int disabled, const IValue *val, charptr fill_ )
			: value(0), TkAgent( s ), state(0), next_menu_entry(0), menu(0), menu_base(0), fill(0)
	{
	type = PLAIN;
	frame = frame_;
	char *argv[32];

	agent_ID = "<graphic:button>";

	id = ++button_count;

	if ( ! frame || ! frame->Self() ) return;

	char width_[30];
	char height_[30];
	char var_name[256];
	char val_name[256];

	sprintf(width_,"%d", width);
	sprintf(height_,"%d", height);

	if ( ! strcmp(type_, "radio") ) type = RADIO;
	else if ( ! strcmp(type_, "check") ) type = CHECK;
	else if ( ! strcmp(type_, "menu") ) type = MENU;

	int c = 2;
	argv[0] = argv[1] = 0;

	if ( type == RADIO )
		{
		sprintf(var_name,"%s%x",type_,frame->Id());
		argv[c++] = "-variable";
		argv[c++] = var_name;
		sprintf(val_name,"BVaLuE%x",Id());
		argv[c++] = "-value";
		argv[c++] = val_name;
		}

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
	if ( type != MENU )
		{
		argv[c++] = "-command";
		argv[c++] = rivet_new_callback( (int (*)()) buttoncb, (ClientData) this, 0);
		}

	switch ( type )
		{
		case RADIO:
			self = rivet_create(RadioButtonClass, frame->Self(), c, argv);
			break;
		case CHECK:
			self = rivet_create(CheckButtonClass, frame->Self(), c, argv);
			break;
		case MENU:
			self = rivet_create(MenubuttonClass, frame->Self(), c, argv);
			rivet_set(self, "-postcommand", rivet_new_callback( (int (*)()) tk_button_menupost, (ClientData) this, 0));
			argv[0] = argv[1] = 0;
			argv[2] = "-tearoff";
			argv[3] = "0";
			menu_base = rivet_create(MenuClass, self, 4, argv);
			rivet_set(self, "-menu", rivet_path(menu_base));
			break;
		default:
			self = rivet_create(ButtonClass, frame->Self(), c, argv);
			break;
		}

	if ( ! self )
		fatal->Report("Rivet creation failed in TkButton::TkButton");

	value = val ? copy_value( val ) : 0;

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

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
	procs.Insert("state", new TkProc(this, "", glishtk_button_state, glishtk_strtobool));
	}

TkButton::TkButton( Sequencer *s, TkButton *frame_, charptr label, charptr type_,
		    charptr padx, charptr pady, int width, int height, charptr justify,
		    charptr font, charptr relief, charptr borderwidth, charptr foreground,
		    charptr background, int disabled, const IValue *val )
			: value(0), TkAgent( s ), state(0), next_menu_entry(0), menu_base(0)
	{
	type = PLAIN;

	menu = frame_;
	frame = 0;

	agent_ID = "<graphic:button>";

	if ( ! frame_->IsMenu() )
		fatal->Report("internal error with creation of menu entry");

	if ( ! menu || ! menu->Self() ) return;

	char *argv[34];

	id = ++button_count;

	char width_[30];
	char height_[30];
	char var_name[256];
	char val_name[256];

	sprintf(width_,"%d", width);
	sprintf(height_,"%d", height);

	if ( ! strcmp(type_, "radio") ) type = RADIO;
	else if ( ! strcmp(type_, "check") ) type = CHECK;
	else if ( ! strcmp(type_, "menu") ) type = MENU;

	int c = 3;
	argv[0] = argv[2] = 0;
	argv[1] = "add";

	if ( type == RADIO )
		{
		sprintf(var_name,"%s%x",type_,menu->Id());
		argv[c++] = "-variable";
		argv[c++] = var_name;
		sprintf(val_name,"BVaLuE%x",Id());
		argv[c++] = "-value";
		argv[c++] = val_name;
		}

#if 0
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
#endif
	argv[c++] = "-label";
	argv[c++] = (char*) label;

	if ( font[0] )
		{
		argv[c++] = "-font";
		argv[c++] = (char*) font;
		}

#if 0
	argv[c++] = "-relief";
	argv[c++] = (char*) relief;
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-fg";
	argv[c++] = (char*) foreground;
	argv[c++] = "-bg";
	argv[c++] = (char*) background;
#endif
	argv[c++] = "-state";
	argv[c++] = disabled ? "disabled" : "normal";

	argv[c++] = "-command";
	argv[c++] = rivet_new_callback( (int (*)()) buttoncb, (ClientData) this, 0);

	switch ( type )
		{
		case RADIO:
			argv[2] = "radio";
			rivet_cmd(menu->Menu(), c, argv);
			break;
		case CHECK:
			argv[2] = "check";
			rivet_cmd(menu->Menu(), c, argv);
			break;
		default:
			argv[2] = "command";
			rivet_cmd(menu->Menu(), c, argv);
			break;
		}

	self = menu->Menu();

	value = val ? copy_value( val ) : 0;

	menu->Add(this);

	procs.Insert("text", new TkProc(this, "-label", glishtk_menu_onestr));
	procs.Insert("font", new TkProc(this, "-font", glishtk_menu_onestr));
	procs.Insert("background", new TkProc(this, "-background", glishtk_menu_onestr));
	procs.Insert("foreground", new TkProc(this, "-foreground", glishtk_menu_onestr));
	procs.Insert("disabled", new TkProc(this, "-state", "disabled", "normal", glishtk_menu_onebinary));
	procs.Insert("state", new TkProc(this, "", glishtk_button_state, glishtk_strtobool));
	}

void TkButton::ButtonPressed( )
	{
	if ( type == RADIO )
		if ( frame )
			frame->RadioID( Id() );
		else
			menu->RadioID( Id() );
	else if ( type == CHECK )
		state = state ? 0 : 1;

	if ( dont_invoke_button == 0 )
		{
		IValue *v = value ? copy_value(value) : new IValue( glish_true );

		attributeptr attr = v->ModAttributePtr();
		attr->Insert( strdup("state"), type != CHECK || state ? new IValue( glish_true ) :
							    new IValue( glish_false ) ) ;

		PostTkEvent( "press", v );
		}
	else
		dont_invoke_button = 0;
	}

TkAgent *TkButton::Create( Sequencer *s, const_args_list *args_val )
	{
	TkButton *ret;

	if ( args_val->length() != 17 )
		return InvalidNumberOfArgs(17);

	int c = 1;
	SETVAL( parent, parent->IsAgentRecord() )
	SETSTR( label )
	SETSTR( type )
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
	SETVAL( val, 1 )
	SETSTR( fill )

	Agent *agent = parent->AgentVal();

	if ( ! strcmp( agent->AgentID(), "<graphic:button>") &&
	     ((TkButton*)agent)->IsMenu() )
		ret =  new TkButton( s, (TkButton*)agent, label, type, padx, pady, width, height, justify, font, relief, borderwidth, foreground, background, disabled, val );
	else
		ret =  new TkButton( s, (TkFrame*)agent, label, type, padx, pady, width, height, justify, font, relief, borderwidth, foreground, background, disabled, val, fill );

	return ret;
	}

unsigned char TkButton::State() const
	{
	unsigned char ret = 0;
	if ( type == RADIO )
		if ( frame )
			ret = frame->RadioID() == Id();
		else
			ret = menu->RadioID() == Id();
	else if ( type == CHECK )
		ret = state;
	return ret;
	}

void TkButton::State(unsigned char s)
	{
	if ( type == PLAIN || State() && s || ! State() && ! s )
		return;

	if ( type == RADIO && s == 0 )
		{
		char var_name[256];

		if ( frame )
			{
			sprintf(var_name,"radio%x",frame->Id());
			frame->RadioID( 0 );
			}
		else
			{
			sprintf(var_name,"radio%x",menu->Id());
			menu->RadioID( 0 );
			}

		Tcl_SetVar( self->interp, var_name, "", TCL_GLOBAL_ONLY );
		}
	else
		{
		dont_invoke_button = 1;
		if ( frame )
			rivet_va_cmd( self, "invoke", 0 );
		else if ( menu )
			{
			char index[100];
			sprintf(index,"%d",menu->Index(this)-1);
			rivet_va_cmd( menu->Menu(), "invoke", index, 0 );
			}
		}
	}

#define STD_EXPAND_PACKINSTRUCTION(CLASS)		\
const char **CLASS::PackInstruction()			\
	{						\
	static char *ret[5];				\
	int c = 0;					\
	if ( fill )					\
		{					\
		ret[c++] = "-fill";			\
		ret[c++] = fill;			\
		if ( ! strcmp(fill,"both") ||		\
		     ! strcmp(fill, frame->Expand()) ||	\
		     frame->NumChildren() == 1 &&	\
		     ! strcmp(fill,"y") )		\
			{				\
			ret[c++] = "-expand";		\
			ret[c++] = "true";		\
			}				\
		else					\
			{				\
			ret[c++] = "-expand";		\
			ret[c++] = "false";		\
			}				\
		ret[c++] = 0;				\
		return ret;				\
		}					\
	else						\
		return 0;				\
	}

#define STD_EXPAND_CANEXPAND(CLASS)			\
int CLASS::CanExpand() const				\
	{						\
	if ( fill && ( ! strcmp(fill,"both") || ! strcmp(fill, frame->Expand()) || \
		     frame->NumChildren() == 1 && ! strcmp(fill,"y")) ) \
		return 1;				\
	return 0;					\
	}

STD_EXPAND_PACKINSTRUCTION(TkButton)
STD_EXPAND_CANEXPAND(TkButton)

DEFINE_DTOR(TkScale)

int scalecb(Rivetobj scale, XEvent *unused1, ClientData assoc, ClientData calldata)
	{
	((TkScale*)assoc)->ValueSet( *((double*) calldata ) );
	return TCL_OK;
	}

TkScale::TkScale ( Sequencer *s, TkFrame *frame_, int from, int to, charptr len,
		   charptr text, charptr orient, charptr relief, charptr borderwidth,
		   charptr foreground, charptr background, charptr fill_ )
			: TkAgent( s ), fill(0)
	{
	frame = frame_;
	char *argv[22];
	char from_[40];
	char to_[40];

	agent_ID = "<graphic:scale>";

	if ( ! frame || ! frame->Self() ) return;

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

	if ( ! self )
		fatal->Report("Rivet creation failed in TkScale::TkScale");

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);
	else if ( fill_ && ! fill_[0] )
		fill = (orient && ! strcmp(orient,"vertical")) ? strdup("y") :
				(orient && ! strcmp(orient,"horizontal")) ? strdup("x") : 0;

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
	PostTkEvent( "value", new IValue( d ) );
	}

TkAgent *TkScale::Create( Sequencer *s, const_args_list *args_val )
	{
	TkScale *ret;

	if ( args_val->length() != 12 )
		return InvalidNumberOfArgs(12);

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
	SETSTR( fill )

	ret = new TkScale( s, (TkFrame*)parent->AgentVal(), start, end, len, text, orient, relief, borderwidth, foreground, background, fill );

	return ret;
	}      

STD_EXPAND_PACKINSTRUCTION(TkScale)
STD_EXPAND_CANEXPAND(TkScale)

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

	agent_ID = "<graphic:text>";

	if ( ! frame || ! frame->Self() ) return;

	self = rivet_create(TextClass, frame->Self(), 0, 0);

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
	procs.Insert("get", new TkProc(this, "get", glishtk_oneortwoidx_strary, glishtk_strary_to_value));
	procs.Insert("insert", new TkProc(this, "insert", glishtk_strandidx));
	procs.Insert("append", new TkProc(this, "insert", "end", glishtk_text_append));
	procs.Insert("prepend", new TkProc(this, "insert", "start", glishtk_strwithidx));
	procs.Insert("addtag", new TkProc("tag", "add", glishtk_text_tagfunc));
	procs.Insert("deltag", new TkProc("tag", "delete", glishtk_text_rangesfunc));
	procs.Insert("config", new TkProc("tag", "configure", glishtk_text_configfunc));
	procs.Insert("ranges", new TkProc("tag", "ranges", glishtk_text_rangesfunc, glishtk_splitsp_str));
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
	PostTkEvent( "yscroll", new IValue( (double*) d, 2, COPY_ARRAY ) );
	}

void TkText::xScrolled( const double *d )
	{
	PostTkEvent( "xscroll", new IValue( (double*) d, 2, COPY_ARRAY ) );
	}

charptr TkText::IndexCheck( charptr s )
	{
	if ( s && s[0] == 's' && ! strcmp(s,"start") )
		return "1.0";
	return s;
	}

STD_EXPAND_PACKINSTRUCTION(TkText)
STD_EXPAND_CANEXPAND(TkText)

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

	agent_ID = "<graphic:scrollbar>";

	if ( ! frame || ! frame->Self() ) return;

	int c = 2;
	argv[0] = argv[1] = 0;
	argv[c++] = "-orient";
	argv[c++] = (char*) orient;
	argv[c++] = "-command";
	argv[c++] = rivet_new_callback((int (*)()) scrollbarcb, (ClientData) this, 0);

	self = rivet_create(ScrollbarClass, frame->Self(), c, argv);

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
	static char *ret[7];
	ret[0] = "-fill";
	char *orient = rivet_va_cmd(self, "cget", "-orient", 0);
	if ( orient[0] == 'v' && ! strcmp(orient,"vertical") )
		ret[1] = "y";
	else
		ret[1] = "x";
	ret[2] = ret[4] = 0;
	if ( frame->ExpandNum(this,1) == 0 || ! strcmp(frame->Expand(),ret[1]) )
		{
		ret[2] = "-expand";
		ret[3] = "true";
		}
	else
		{
		ret[2] = "-expand";
		ret[3] = "false";
		}

	return ret;
	}

int TkScrollbar::CanExpand() const
	{
	return 1;
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
	PostTkEvent( "scroll", data, 0, new ScrollbarTrigger( this ) );
	}


DEFINE_DTOR(TkLabel)

TkLabel::TkLabel( Sequencer *s, TkFrame *frame_, charptr text, charptr justify,
		  charptr padx, charptr pady, int width_, charptr font, charptr relief,
		  charptr borderwidth, charptr foreground, charptr background, charptr fill_ )
			: TkAgent( s ), fill(0)
	{
	frame = frame_;
	char *argv[22];
	char width[30];

	agent_ID = "<graphic:label>";

	if ( ! frame || ! frame->Self() ) return;

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

	if ( ! self )
		fatal->Report("Rivet creation failed in TkLabel::TkLabel");

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

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

	if ( args_val->length() != 13 )
		return InvalidNumberOfArgs(13);

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
	SETSTR( fill )

	ret =  new TkLabel( s, (TkFrame*)parent->AgentVal(), text, justify, padx, pady, width,
			    font, relief, borderwidth, foreground, background, fill );

	return ret;
	}      

STD_EXPAND_PACKINSTRUCTION(TkLabel)
STD_EXPAND_CANEXPAND(TkLabel)


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
		 int disabled, int show, int exportselection, charptr fill_ )
			: TkAgent( s ), fill(0)
	{
	frame = frame_;
	char *argv[24];
	char width_[30];

	agent_ID = "<graphic:entry>";

	if ( ! frame || ! frame->Self() ) return;

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

	if ( ! self )
		fatal->Report("Rivet creation failed in TkEntry::TkEntry");

	if ( rivet_create_binding(self, 0, "<Return>", (int (*)()) entry_returncb, (ClientData) this, 1, 0) == TCL_ERROR )
		fatal->Report("Rivet binding creation failed in TkEntry::TkEntry");

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

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
	if ( strcmp("disabled", rivet_va_cmd(self, "cget", "-state", 0)) )
		{
		IValue *ret = new IValue( rivet_va_cmd( self, "get", 0 ) );
		PostTkEvent( "return", ret );
		}
	}

void TkEntry::xScrolled( const double *d )
	{
	PostTkEvent( "xscroll", new IValue( (double*) d, 2, COPY_ARRAY ) );
	}

TkAgent *TkEntry::Create( Sequencer *s, const_args_list *args_val )
	{
	TkEntry *ret;

	if ( args_val->length() != 13 )
		return InvalidNumberOfArgs(13);

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
	SETSTR( fill )

	ret =  new TkEntry( s, (TkFrame*)parent->AgentVal(), width, justify,
			    font, relief, borderwidth, foreground, background,
			    disabled, show, exp, fill );

	return ret;
	}      

charptr TkEntry::IndexCheck( charptr s )
	{
	if ( s && s[0] == 's' && ! strcmp(s,"start") )
		return "0";
	return s;
	}


STD_EXPAND_PACKINSTRUCTION(TkEntry)
STD_EXPAND_CANEXPAND(TkEntry)

DEFINE_DTOR(TkMessage)

TkMessage::TkMessage( Sequencer *s, TkFrame *frame_, charptr text, charptr width, charptr justify,
		      charptr font, charptr padx, charptr pady, charptr relief, charptr borderwidth,
		      charptr foreground, charptr background, charptr fill_ )
			: TkAgent( s ), fill(0)
	{
	frame = frame_;
	char *argv[22];

	agent_ID = "<graphic:message>";

	if ( ! frame || ! frame->Self() ) return;

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

	if ( ! self )
		fatal->Report("Rivet creation failed in TkMessage::TkMessage");

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

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

	if ( args_val->length() != 13 )
		return InvalidNumberOfArgs(13);

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
	SETSTR( fill )

	ret =  new TkMessage( s, (TkFrame*)parent->AgentVal(), text, width, justify, font, padx, pady, relief, borderwidth, foreground, background, fill );

	return ret;
	}      

STD_EXPAND_PACKINSTRUCTION(TkMessage)
STD_EXPAND_CANEXPAND(TkMessage)

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
		      charptr foreground, charptr background, int exportselection, charptr fill_ )
			: TkAgent( s ), fill(0)
	{
	frame = frame_;
	char *argv[24];
	char width_[40];
	char height_[40];

	agent_ID = "<graphic:listbox>";

	if ( ! frame || ! frame->Self() ) return;

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

	if ( ! self )
		fatal->Report("Rivet creation failed in TkListbox::TkListbox");

	if ( rivet_create_binding(self, 0, "<ButtonRelease-1>", (int (*)()) listbox_button1cb, (ClientData) this, 1, 0) == TCL_ERROR )
		fatal->Report("Rivet binding creation failed in TkListbox::TkListbox");

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("view", new TkProc("", glishtk_scrolled_update));
	procs.Insert("mode", new TkProc("-selectmode", glishtk_onestr));
	procs.Insert("font", new TkProc("-font", glishtk_onestr));
	procs.Insert("height", new TkProc("-height", glishtk_oneint));
	procs.Insert("width", new TkProc("-width", glishtk_oneint));
	procs.Insert("exportselection", new TkProc("-exportselection", glishtk_onebool));
	procs.Insert("see", new TkProc(this, "see", glishtk_oneidx));
	procs.Insert("selection", new TkProc("curselection", glishtk_nostr, glishtk_splitsp_int));
	procs.Insert("select", new TkProc(this, "select", "set", glishtk_oneortwoidx2str));
	procs.Insert("clear", new TkProc(this, "select", "clear", glishtk_oneortwoidx2str));
	procs.Insert("delete", new TkProc(this, "delete", glishtk_oneortwoidx));
	procs.Insert("insert", new TkProc(this, "insert", glishtk_listbox_insert));
	procs.Insert("get", new TkProc(this, "get", glishtk_listbox_get, glishtk_splitnl));
	}

TkAgent *TkListbox::Create( Sequencer *s, const_args_list *args_val )
	{
	TkListbox *ret;

	if ( args_val->length() != 12 )
		return InvalidNumberOfArgs(12);

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
	SETSTR( fill )

	ret =  new TkListbox( s, (TkFrame*)parent->AgentVal(), width, height, mode, font, relief, borderwidth, foreground, background, exp, fill );

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
	PostTkEvent( "yscroll", new IValue( (double*) d, 2, COPY_ARRAY ) );
	}

void TkListbox::xScrolled( const double *d )
	{
	PostTkEvent( "xscroll", new IValue( (double*) d, 2, COPY_ARRAY ) );
	}

void TkListbox::elementSelected(  )
	{
	PostTkEvent( "select", glishtk_splitsp_int(rivet_va_cmd( self, "curselection", 0 )) );
	}

STD_EXPAND_PACKINSTRUCTION(TkListbox)
STD_EXPAND_CANEXPAND(TkListbox)

int TkHaveGui()
	{
	Display *display;
	int ret = 0;

	if ( (display=XOpenDisplay(NULL)) != NULL )
		{
		ret = 1;
		XCloseDisplay(display);
		}

	return ret;
	}

#endif
