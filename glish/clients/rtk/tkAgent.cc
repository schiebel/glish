// $Id$
// Copyright (c) 1997,1998 Associated Universities Inc.
//

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#ifdef GLISHTK
#include "tkAgent.h"
#include "tkCanvas.h"

#if defined(XTKPGPLOT)
#include "TkPgplot.h"
#endif

#include <X11/Xlib.h>
#include <string.h>
#include <stdlib.h>
#include "Rivet/rivet.h"
#include "Reporter.h"
#include "Glish/Value.h"
#include "system.h"

unsigned long TkRadioContainer::count = 0;

extern ProxyStore *global_store;

Rivetobj TkAgent::root = 0;
unsigned long TkFrame::top_created = 0;
unsigned long TkFrame::tl_count = 0;
unsigned long TkFrame::grab = 0;
PQueue(glishtk_event) *TkAgent::tk_queue = 0;
int TkAgent::hold_tk_events = 0;
int TkAgent::hold_glish_events = 0;
Value *TkAgent::last_error = 0;
Value *TkAgent::bitmap_path = 0;

extern Value *glishtk_valcast( char * );

class glishtk_event {
    public:
	glishtk_event( TkAgent *a_, const char *n_, Value *v_ ) :
			agent(a_), nme(n_ ? strdup(n_) : strdup(" ")), val(v_)
			{ Ref(agent); Ref(val); }
	void Post();
	~glishtk_event();
	Value *value() { return val; }
    protected:
	TkAgent *agent;
	char *nme;
	Value *val;
};

void glishtk_event::Post()
	{
	agent->PostEvent( nme, val );
	}

glishtk_event::~glishtk_event()
	{
	Unref(val);
	Unref(agent);
	free_memory( nme );
	}

static Value *ScrollToValue( Scrollbar_notify_data *data )
	{
	recordptr rec = create_record_dict();

	rec->Insert( strdup("vertical"), new Value( data->scrollbar_is_vertical ) );
	rec->Insert( strdup("op"), new Value( data->scroll_op ) );
	rec->Insert( strdup("newpos"), new Value( data->newpos ) );

	return new Value( rec );
	}

static Scrollbar_notify_data *ValueToScroll( const Value *data )
	{
	if ( data->Type() != TYPE_RECORD )
		return 0;

	Value *vertical;
	Value *op;
	Value *newpos;
	if ( ! (vertical = (Value*) (data->HasRecordElement( "vertical" )) ) ||
	     ! (op = (Value*) (data->HasRecordElement( "op" ) )) ||
	     ! (newpos = (Value*) (data->HasRecordElement( "newpos" ) )) )
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

Value *glishtk_splitnl( char *str )
	{
	char *prev = str;
	int nls = 0;

	if ( ! str || ! str[0] )
		return new Value( "" );

	while ( *str )
		if ( *str++ == '\n' ) nls++;

	if ( ! nls )
		return new Value( prev );

	char **ary = (char**) alloc_memory( sizeof(char*)*(nls+1) );

	for ( nls = 0, str = prev; *str; str++ )
		if ( *str == '\n' )
			{
			*str = (char) 0;
			ary[nls++] = strdup( prev );
			prev = str+1;
			}

	if ( prev != str )
		ary[nls++] = strdup( prev );

	return new Value( (const char **) ary, nls );
	}

Value *glishtk_str( char *str )
	{
	return new Value( str );
	}

Value *glishtk_splitsp_int( char *sel )
	{
	char *start = sel;
	char *end;
	int cnt = 0;
	static int len = 2;
	static int *ary = (int*) alloc_memory( sizeof(int)*len );

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

	return new Value( ary, cnt, COPY_ARRAY );
	}

char **glishtk_splitsp_str_( char *sel, int &cnt )
	{
	char *start = sel;
	char *end;
	cnt = 0;
	static int len = 2;
	static char **ary = (char**) alloc_memory( sizeof(char*)*len );

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

Value *glishtk_splitsp_str( char *s )
	{
	int len=0;
	char **str = glishtk_splitsp_str_(s, len);
	return new Value( (charptr*) str, len, COPY_ARRAY );
	}

inline void glishtk_pack( Rivetobj root, int argc, char **argv)
	{
// 	for (int i=0; i < argc; i++)
// 		cout << (argv[i] ? argv[i] : "pack") << " ";
// 	cout << endl;
	rivet_func(root,(int (*)())Tk_PackCmd,argc,argv);
	}


#define InvalidArg( num )						\
	{								\
	global_store->Error( "invalid type for argument " ## #num );	\
	return;								\
	}

#define InvalidNumberOfArgs( num )					\
	{								\
	global_store->Error( "invalid number of arguments, " ## #num ## " expected" );\
	return;								\
	}

#define SETINIT								\
	if ( args->Type() != TYPE_RECORD )				\
		{							\
		global_store->Error("bad value");			\
		return;							\
		}							\
									\
	Ref( args );							\
	recordptr rptr = args->RecordPtr(0);				\
	int c = 0;							\
	const char *key;

#define SETDONE Unref(args);

#define SETVAL(var,condition)						\
	const Value *var      = rptr->NthEntry( c++, key );		\
	if ( ! ( condition) )						\
		InvalidArg(c-1);

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

#define SETDOUBLE(var)							\
	SETVAL(var##_v_, var##_v_ ->IsNumeric() &&			\
				var##_v_ ->Length() > 0 )		\
	double var = var##_v_ ->DoubleVal();

#define EXPRINIT(EVENT)							\
	if ( args->Type() != TYPE_RECORD )				\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}							\
									\
	/*Ref(args);*/							\
	recordptr rptr = args->RecordPtr(0);				\
	int c = 0;							\
	const char *key;

#define EXPRVAL(var,EVENT)						\
	const Value *var = rptr->NthEntry( c++, key );			\
	const Value *var##_val_ = var;					\
	if ( ! var )							\
		{							\
		global_store->Error("bad value: %", EVENT);		\
		return 0;						\
		}

#define EXPRSTRVALXX(var,EVENT,LINE)					\
	const Value *var = rptr->NthEntry( c++, key );			\
	LINE								\
	if ( ! var || var ->Type() != TYPE_STRING ||			\
		var->Length() <= 0 )					\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}

#define EXPRSTRVAL(var,EVENT) EXPRSTRVALXX(var,EVENT,const Value *var##_val_ = var;)

#define EXPRSTR(var,EVENT)						\
	charptr var = 0;						\
	EXPRSTRVALXX(var##_val_, EVENT,)				\
	var = ( var##_val_ ->StringPtr(0) )[0];

#define EXPRDIM(var,EVENT)						\
	const Value *var##_val_ = rptr->NthEntry( c++, key );		\
	charptr var = 0;						\
	char var##_char_[30];						\
	if ( ! var##_val_ || ( var##_val_ ->Type() != TYPE_STRING &&	\
			       ! var##_val_ ->IsNumeric() ) ||		\
		var##_val_ ->Length() <= 0 )				\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
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

#define EXPRINTVALXX(var,EVENT,LINE)                                    \
        const Value *var = rptr->NthEntry( c++, key );			\
        LINE                                                            \
        if ( ! var || ! var ->IsNumeric() || var ->Length() <= 0 )      \
                {                                                       \
                global_store->Error("bad value: %s", EVENT);            \
                return 0;                                   		\
                }

#define EXPRINTVAL(var,EVENT)  EXPRINTVALXX(var,EVENT, const Value *var##_val_ = var;)

#define EXPRINT(var,EVENT)                                              \
        int var = 0;                                                    \
	EXPRINTVALXX(var##_val_,EVENT,)                                 \
        var = var##_val_ ->IntVal();

#define EXPRINT2(var,EVENT)						\
	const Value *var##_val_ = rptr->NthEntry( c++, key );		\
        char var##_char_[30];						\
	charptr var = 0;						\
	if ( ! var##_val_ || var##_val_ ->Length() <= 0 )		\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
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
		global_store->Error("bad type: %s", EVENT);		\
		return 0;						\
		}

#define EXPRDOUBLE(var,EVENT)                                           \
        const Value *var##_val_ = rptr->NthEntry( c++, key );           \
        double var = 0;                                                 \
        if ( ! var##_val_ || ! var##_val_ ->IsNumeric() ||              \
                var##_val_ ->Length() <= 0 )                            \
                {                                                       \
                global_store->Error("bad value: %s", EVENT);            \
                return 0;                                   		\
                }                                                       \
        else                                                            \
                var = var##_val_ ->DoubleVal();

#define EXPRDOUBLE2(var,EVENT)						\
	const Value *var##_val_ = rptr->NthEntry( c++, key );		\
        char var##_char_[30];						\
	charptr var = 0;						\
	if ( ! var##_val_ || var##_val_ ->Length() <= 0 )		\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}							\
	if ( var##_val_ -> IsNumeric() )				\
		{							\
		double var##_double_ = var##_val_ ->DoubleVal();	\
		var = var##_char_;					\
		sprintf(var##_char_,"%f",var##_double_);		\
		}							\
	else if ( var##_val_ -> Type() == TYPE_STRING )			\
		var = ( var##_val_ ->StringPtr(0) )[0];			\
	else								\
		{							\
		global_store->Error("bad type: %s", EVENT);		\
		return 0;						\
		}

#define EXPRDOUBLEPTR(var, NUM, EVENT)					\
	const Value *var##_val_ = rptr->NthEntry( c++, key );		\
	double *var = 0;						\
	if ( ! var##_val_ || var##_val_ ->Type() != TYPE_DOUBLE ||	\
		var##_val_ ->Length() < NUM )				\
		{							\
		global_store->Error("bad value: %s", EVENT);		\
		return 0;						\
		}							\
	else								\
		var = var##_val_ ->DoublePtr(0);

#define EXPR_DONE(var)

#define HASARG( args, cond )						\
	if ( ! (args->Length() cond) )					\
		{							\
		global_store->Error("wrong number of arguments");	\
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

#define HANDLE_CTOR_ERROR(STR)					\
		{						\
		frame = 0;					\
		SetError( (Value*) generate_error( STR ) );	\
		return;						\
		}

#define CREATE_RETURN						\
	if ( ! ret || ! ret->IsValid() )			\
		{						\
		Value *err = ret->GetError();			\
		if ( err )					\
			{					\
			global_store->Error( err );		\
			Unref( err );				\
			}					\
		else						\
			global_store->Error( "tk widget creation failed" ); \
		}						\
	else							\
		ret->SendCtor("newtk");				\
								\
	SETDONE


#define GEOM_GET(WHAT)									\
	char *c_##WHAT = (char*) rivet_va_func( tlead, (int (*)()) Tk_WinfoCmd, #WHAT,	\
						rivet_path(tlead), 0 );			\
	int WHAT = c_##WHAT ? atoi(c_##WHAT) : 0;


//                  <-------X/WIDTH-------->
//                                                      A  ==  'nw'
//               ^  A           2          C            2  ==  'n'
//               |   +--------------------+             C  ==  'ne'
//               |   |                    |             1  ==  'w'
//  Y/HEIGHT     |  1|          X         |3            X  ==  'c'
//               |   |                    |             3  ==  'e'
//               |   +--------------------+             B  ==  'sw'
//               v  B           4          D            4  ==  's'
//                                                      D  ==  'se'
//
const char *glishtk_popup_geometry( Rivetobj tlead, charptr pos )
	{
	static char geometry[80];

	GEOM_GET(rootx)
	GEOM_GET(rooty)

	if ( ! pos || ! *pos ) return "+0+0";

	if ( pos[0] == 'n' )
		if ( pos[1] == 'w' )
			sprintf(geometry,"+%d+%d",rootx,rooty);					// ==> A
		else
			{
			GEOM_GET(width)
			sprintf(geometry,"+%d+%d",rootx+(pos[1]?width:width/2), rooty);		// ==> 2, C
			}
	else
		{
		GEOM_GET(height);
		if ( pos[0] == 'w' || pos[1] == 'w' )
			sprintf(geometry, "+%d+%d", rootx, rooty+(pos[1]?height:height/2));	// ==> 1, B
		else
			{
			GEOM_GET(width)
			switch( pos[0] )
				{
			    case 'c':
				sprintf(geometry, "+%d+%d", rootx+(width/2), rooty+(height/2));	// ==> X
				break;
			    case 's':
				sprintf(geometry, "+%d+%d", rootx+(pos[1]?width:width/2), 	// ==> 4, D
					rooty+height);
				break;
			    case 'e':
				sprintf(geometry, "+%d+%d", rootx+width, rooty+height/2);	// ==> 3
				break;
			    default:
				strcpy( geometry, "+0+0");
				}
			}
		}

	return geometry;
	}

char *glishtk_nostr(Rivetobj self, const char *cmd, Value * )
	{
	return rivet_va_cmd( self, cmd, 0 );
	}

char *glishtk_onestr(Rivetobj self, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Type() == TYPE_STRING )
		{
		const char *str = args->StringPtr(0)[0];
		ret = (char*) rivet_set( self, (char*) cmd, (char*) str );
		}
	else
		ret = rivet_va_cmd(self, "cget", cmd, 0);

	return ret;
	}

char *glishtk_bitmap(Rivetobj self, const char *cmd, Value *args )
	{
	char *ret = 0;
	char *event_name = "one string function";

	if ( args->Type() == TYPE_STRING )
		{
		const char *str = args->StringPtr(0)[0];
		char *bitmap = (char*) alloc_memory(strlen(str)+2);
		sprintf(bitmap,"@%s",str);
		ret = (char*) rivet_set( self, (char*) cmd, bitmap );
		free_memory( bitmap );
		}
	else
		{
		ret = rivet_va_cmd(self, "cget", cmd, 0);
		if ( *ret == '@' ) ++ret;
		}

	return ret;
	}

char *glishtk_onedim(Rivetobj self, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_STRING )
		ret = (char*) rivet_set( self, (char*) cmd, (char*) args->StringPtr(0)[0] );
	else if ( args->Type() != TYPE_BOOL && args->IsNumeric() )
		{
		char buf[30];
		sprintf(buf,"%d",args->IntVal());
		ret = (char*) rivet_set( self, (char*) cmd, buf );
		}
	else
		ret = rivet_va_cmd(self, "cget", cmd, 0);

	return ret;
	}

char *glishtk_winfo(Rivetobj self, const char *cmd, Value *args )
	{
	return (char*) rivet_va_func( self, (int (*)()) Tk_WinfoCmd, (char*) cmd, rivet_path(self), 0 );
	}

char *glishtk_oneint(Rivetobj self, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() != TYPE_BOOL )
		{
		if ( args->IsNumeric() )
			{
			char buf[30];
			sprintf(buf,"%d",args->IntVal());
			ret = (char*) rivet_set( self, (char*) cmd, buf );
			}
		else if ( args->Type() == TYPE_STRING )
			ret = (char*) rivet_set( self, (char*) cmd, (char*) args->StringPtr(0)[0] );
		}
	else
		ret = rivet_va_cmd(self, "cget", cmd, 0);

	return ret;
	}

char *glishtk_width(Rivetobj self, const char *cmd, Value *args )
	{
	return (char*) new Value( Tk_Width(self->tkwin) );
	}

char *glishtk_height(Rivetobj self, const char *cmd, Value *args )
	{
	return (char*) new Value( Tk_Height(self->tkwin) );
	}

char *glishtk_onedouble(Rivetobj self, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() != TYPE_BOOL )
		{
		if ( args->IsNumeric() )
			{
			char buf[30];
			sprintf(buf,"%f",args->DoubleVal());
			ret = (char*) rivet_set( self, (char*) cmd, buf );
			}
		else if ( args->Type() == TYPE_STRING )
			ret = (char*) rivet_set( self, (char*) cmd, (char*) args->StringPtr(0)[0] );
		}
	else
		ret = rivet_va_cmd(self, "cget", cmd, 0);

	return ret;
	}

char *glishtk_onebinary(Rivetobj self, const char *cmd, const char *ptrue, const char *pfalse,
				Value *args )
	{
	char *ret = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");

	else if ( args->IsNumeric() )
		ret = (char*) rivet_set( self, (char*) cmd, (char*)(args->IntVal() ? ptrue : pfalse) );
	else
		global_store->Error("wrong type, numeric expected");
	
	return ret;
	}

char *glishtk_onebool(Rivetobj self, const char *cmd, Value *args )
	{
	return glishtk_onebinary(self, cmd, "true", "false", args);
	}

char *glishtk_oneidx(TkAgent *a, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_STRING )
		ret = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( args->StringPtr(0)[0] ), 0);
	else
		global_store->Error("wrong type, string expected");

	return ret;
	}

char *glishtk_disable_cb(TkAgent *a, const char *cmd, Value *args )
	{
	if ( ! *cmd )
		{
		if ( args->Length() <= 0 )
			global_store->Error("zero length value");
		else if ( args->IsNumeric() )
			{
			if ( args->IntVal() )
				a->Disable( );
			else
				a->Enable( );
			}
		else
			global_store->Error("wrong type, numeric expected");
		}
	else
		if ( *cmd == '1' )
			a->Disable( );
		else
			a->Enable( 0 );

	return 0;
	}

char *glishtk_oneortwoidx(TkAgent *a, const char *cmd, Value *args )
	{
	char *ret = 0;
	char *event_name = "one-or-two index function";

	if ( args->Type() == TYPE_RECORD )
		{
		HASARG( args, > 1 )
		EXPRINIT( event_name )
		EXPRSTR( start, event_name )
		EXPRSTR( end, event_name )
		a->EnterEnable();
		ret = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( start ),
					     a->IndexCheck( end ), 0);
		a->ExitEnable();
		EXPR_DONE( end )
		EXPR_DONE( start )
		}
	else if ( args->Type() == TYPE_STRING )
		{
		a->EnterEnable();
		ret = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( args->StringPtr(0)[0] ), 0);
		a->ExitEnable();
		}
	return ret;
	}

struct strary_ret {
  char **ary;
  int len;
};

Value *glishtk_strary_to_value( char *s )
	{
	strary_ret *r = (strary_ret*) s;
	Value *ret = new Value((charptr*) r->ary,r->len);
	delete r;
	return ret;
	}
char *glishtk_oneortwoidx_strary(TkAgent *a, const char *cmd, Value *args )
	{
	char *event_name = "one-or-two index function";

	strary_ret *ret = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_RECORD )
		{
		HASARG( args, >= 2 )
		EXPRINIT( event_name )

		strary_ret *ret = 0;

		ret = new strary_ret;
		ret->ary = (char**) alloc_memory( sizeof(char*)*((int)(args->Length()/2)) );
		ret->len = 0;
		for ( int i=0; i+1 < args->Length(); i+=2 )
			{
			EXPRSTR(one, event_name)
			EXPRSTR(two, event_name)
			char *s = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( one ), a->IndexCheck( two ), 0);
			if ( s ) ret->ary[ret->len++] = strdup(s);
			EXPR_DONE(one)
			EXPR_DONE(two)
			}
		}
	else if ( args->Type() == TYPE_STRING )
		{
		if ( args->Length() > 1 )
			{
			ret = new strary_ret;
			ret->len = 0;
			ret->ary = (char**) alloc_memory( sizeof(char*)*((int)(args->Length() / 2)) );
			charptr *idx = args->StringPtr(0);
			for ( int i = 0; i+1 < args->Length(); i+=2 )
				{
				char *s = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( idx[i] ),
						       a->IndexCheck( idx[i+1] ),0);

				if ( s ) ret->ary[ret->len++] = strdup(s);
				}
			}
		else
			{
			char *s = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( (args->StringPtr(0))[0] ), 0);
			if ( s )
				{
				ret = new strary_ret;
			        ret->len = 1;
				ret->ary = (char**) alloc_memory( sizeof(char*) );
				ret->ary[0] = strdup(s);
				}
			}
		}
	else
		global_store->Error("wrong type");
			
	return (char*) ret;
	}

char *glishtk_listbox_select(TkAgent *a, const char *cmd, const char *param,
			      Value *args )
	{
	char *ret = 0;
	char *event_name = "one-or-two index function";

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_RECORD )
		{
		HASARG( args, > 1 )
		EXPRINIT( event_name)
		EXPRSTR( start, event_name )
		EXPRSTR( end, event_name )
		ret = rivet_va_cmd(a->Self(), cmd, param, a->IndexCheck( start ),
					     a->IndexCheck( end ), 0);
		rivet_va_cmd(a->Self(), "activate", a->IndexCheck( end ), 0 );
		EXPR_DONE( end )
		EXPR_DONE( start )
		}
	else if ( args->Type() == TYPE_STRING )
		{
		const char *start = args->StringPtr(0)[0];
		ret = rivet_va_cmd(a->Self(), cmd, param, a->IndexCheck( start ), 0);
		rivet_va_cmd(a->Self(), "activate", a->IndexCheck( start ), 0 );
		}

	return ret;
	}

char *glishtk_strandidx(TkAgent *a, const char *cmd, Value *args )
	{
	char *ret = 0;
	char *event_name = "one-or-two index function";

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_RECORD )
		{
		HASARG( args, > 1 )
		EXPRINIT( event_name)
		EXPRSTR( str, event_name );
		EXPRSTR( where, event_name )
		a->EnterEnable();
		ret = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( where ), str, 0);
		a->ExitEnable();
		EXPR_DONE( where )
		EXPR_DONE( str )
		}
	else if ( args->Type() == TYPE_STRING )
		{
		a->EnterEnable();
		ret = rivet_va_cmd(a->Self(), cmd, a->IndexCheck( "end" ), args->StringPtr(0)[0], 0);
		a->ExitEnable();
		}
	else
		global_store->Error("wrong type, string expected");

	return ret;
	}

char *glishtk_text_append(TkAgent *a, const char *cmd, const char *param,
				Value *args )
	{
	char *event_name = "text append function";

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_RECORD )
		{
		HASARG( args, > 1 )
		EXPRINIT( event_name)
		char **argv = (char**) alloc_memory(sizeof(char*) * (args->Length()+3));
		int argc = 0;
		argv[argc++] = 0;
		argv[argc++] = (char*) cmd;
		if ( param ) argv[argc++] = (char*) a->IndexCheck(param);
		int start = argc;
		for ( int i=0; i < args->Length(); ++i )
			{
			EXPRVAL( val, event_name )
			char *s = val->StringVal( ' ', 0, 1 );
			argv[argc++] = i != 1 || param ? val->StringVal( ' ', 0, 1 ) :
					strdup(a->IndexCheck(s));
			free_memory(s);
			EXPR_DONE( val )
			}
		a->EnterEnable();
		if ( ! param && argc > 3 )
			{
			char *tmp = argv[3];
			argv[3] = argv[2];
			argv[2] = tmp;
			}
		rivet_cmd(a->Self(), argc, argv);
		if ( param ) rivet_va_cmd(a->Self(), "see", a->IndexCheck(param), 0);
		a->ExitEnable();
		for ( LOOPDECL i = start; i < argc; ++i )
			free_memory(argv[i]);
		free_memory( argv );
		}
	else if ( args->Type() == TYPE_STRING && param )
		{
		char *s = args->StringVal( ' ', 0, 1 );
		rivet_va_cmd(a->Self(), cmd, param, s, 0);
		free_memory(s);
		}
	else
		global_store->Error("wrong arguments");

	return 0;
	}

char *glishtk_text_tagfunc(Rivetobj self, const char *cmd, const char *param,
			   Value *args )
	{
	char *event_name = "tag function";
		
	if ( args->Length() < 2 )
		{
		global_store->Error("wrong number of arguments");
		return 0;
		}

	EXPRINIT( event_name)
	EXPRSTR(tag, event_name)
	int argc = 0;
	char *argv[8];
	argv[argc++] = 0;
	argv[argc++] = (char*) cmd;
	argv[argc++] = (char*) param;
	argv[argc++] = (char*) tag;
	if ( args->Length() >= 3 )
		for ( int i=0; i+1 < args->Length(); i+=2 )
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
			      Value *args )
	{
	char *event_name = "tag function";
	if ( args->Length() < 2 )
		{
		global_store->Error("wrong number of arguments");
		return 0;
		}
	EXPRINIT( event_name)
	char buf[512];
	int argc = 0;
	char *argv[8];
	argv[argc++] = 0;
	argv[argc++] = (char*) cmd;
	argv[argc++] = (char*) param;
	EXPRSTR(tag, event_name)
	argv[argc++] = (char*) tag;
	const Value *val;
	for ( int i=c; i < args->Length(); i++ )
		{
		const Value *val = rptr->NthEntry( i, key );
		if ( strncmp( key, "arg", 3 ) )
			{
			int doit = 1;
			sprintf(buf,"-%s",key);
			argv[argc] = buf;
			if ( val->Type() == TYPE_STRING )
				argv[argc+1] = (char*)((val->StringPtr(0))[0]);
			else if ( val->Type() == TYPE_BOOL )
				argv[argc+1] = val->BoolVal() ? "true" : "false";
			else
				doit = 0;
			
			if ( doit ) rivet_cmd(self,argc+2,argv);
			}
		}
	EXPR_DONE(tag)
	return 0;
	}

char *glishtk_text_rangesfunc(Rivetobj self, const char *cmd, const char *param,
			      Value *args )
	{
	char *ret = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_STRING )
		{
		rivet_va_cmd( self, cmd, param, args->StringPtr(0)[0], 0 );
		ret = self->interp->result;
		}
	else
		global_store->Error("wrong type, string expected");

	return ret;
	}

char *glishtk_no2str(TkAgent *a, const char *cmd, const char *param, Value * )
	{
	return rivet_va_cmd( a->Self(), cmd, param, 0 );
	}

char *glishtk_listbox_insert_action(TkAgent *a, const char *cmd, Value *str_v, charptr where="end" )
	{
	int len = str_v->Length();

	if ( ! len ) return 0;

	char **argv = (char**) alloc_memory( sizeof(char*)*(len+3) );
	charptr *strs = str_v->StringPtr(0);

	argv[0] = 0;
	argv[1] = (char*) cmd;
	argv[2] = (char*) a->IndexCheck( where );
	int c=0;
	for ( ; c < len; ++c )
		argv[c+3] = (char *) strs[c];
		
	rivet_cmd(a->Self(), c+3, argv);
	free_memory( argv );
	return "";
	}

char *glishtk_listbox_insert(TkAgent *a, const char *cmd, Value *args )
	{
	char *event_name = "listbox insert function";

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_RECORD )
		{
		HASARG( args, > 1 )
		EXPRINIT( event_name)
		EXPRSTRVAL( val, event_name )
		EXPRSTR( where, event_name )
		glishtk_listbox_insert_action(a, (char*) cmd, (Value*) val, where );
		EXPR_DONE( where )
		EXPR_DONE( val )
		}
	else if ( args->Type() == TYPE_STRING )
		glishtk_listbox_insert_action(a, (char*) cmd, args );
	else
		global_store->Error("wrong type, string expected");

	return "";
	}

char *glishtk_listbox_get_int(TkAgent *a, const char *cmd, Value *val )
	{
	int len = val->Length();

	if ( ! len )
		return 0;

	static int rlen = 200;
	static char *ret = (char*) alloc_memory( sizeof(char)*rlen );
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

char *glishtk_listbox_get(TkAgent *a, const char *cmd, Value *args )
	{
	char *ret = 0;
	char *event_name = "listbox get function";

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_RECORD )
		{
		HASARG( args, > 1 )
		EXPRINIT( event_name)
		EXPRSTR( start, event_name )
		EXPRSTR( end, event_name )
		ret = rivet_va_cmd( a->Self(), cmd, a->IndexCheck( start ), a->IndexCheck( end ), 0 );
		EXPR_DONE( end )
		EXPR_DONE( val )
		}
	else if ( args->Type() == TYPE_STRING )
		ret = rivet_va_cmd( a->Self(), cmd, a->IndexCheck( args->StringPtr(0)[0] ), 0 );
	else if ( args->Type() == TYPE_INT )
		ret = glishtk_listbox_get_int( a, (char*) cmd, args );
	else
		global_store->Error("invalid argument type");

	return ret;
	}

char *glishtk_listbox_nearest(TkAgent *a, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->IsNumeric() )
		{
		char ycoord[30];
		sprintf(ycoord,"%d", args->IntVal());
		ret = rivet_va_cmd( a->Self(), "nearest", ycoord, 0 );
		}
	else
		global_store->Error("wrong type, numeric expected");

	return ret;
	}


char *glishtk_scrolled_update(Rivetobj self, const char *, Value *data )
	{
	static char ret[5];
	Scrollbar_notify_data *data_ = ValueToScroll( data );
	if ( ! data_ ) return 0;
	rivet_scrollbar_set_client_view( self, data_ );
	delete data_;
	ret[0] = (char) 0;
	return ret;
	}

char *glishtk_scrollbar_update(Rivetobj self, const char *, Value *val )
	{
	static char ret[5];

	if ( val->Type() != TYPE_DOUBLE || val->Length() < 2 )
		{
		global_store->Error("scrollbar update function");
		return 0;
		}

	double *firstlast = val->DoublePtr(0);
	rivet_scrollbar_set( self, firstlast[0], firstlast[1] );
	ret[0] = (char) 0;
	return ret;
	}

char *glishtk_button_state(TkAgent *a, const char *, Value *args )
	{
	char *ret = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() != TYPE_BOOL && args->IsNumeric() )
		((TkButton*)a)->State( args->IntVal() ? 1 : 0 );

	ret = ((TkButton*)a)->State( ) ? "T" : "F";
	return ret;
	}

extern "C" int rivet_menuentry_set(Rivetobj,char*,char*,char*);

char *glishtk_menu_onestr(TkAgent *a, const char *cmd, Value *args )
	{
	TkButton *Self = (TkButton*)a;
	TkButton *Parent = Self->Parent();
	char *ret = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_STRING )
		ret = (char*) rivet_va_cmd( Parent->Menu(), "entryconfigure", Self->Index(),
					    (char*) cmd, (char*) args->StringPtr(0)[0], 0 );
	else
		global_store->Error("wrong type, string expected");

	return ret;
	}

char *glishtk_menu_onebinary(TkAgent *a, const char *cmd, const char *ptrue, const char *pfalse,
				Value *args )
	{
	TkButton *Self = (TkButton*)a;
	TkButton *Parent = Self->Parent();
	char *ret = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	if ( args->IsNumeric() )
		ret = (char*) rivet_va_cmd( Parent->Menu(), "entryconfigure", Self->Index(),
					    (char*) cmd, (char*)(args->IntVal() ? ptrue : pfalse), 0 );
	else
		global_store->Error("wrong type, numeric expected");

	return ret;
	}

Value *glishtk_strtobool( char *str )
	{
	if ( str && (*str == 'T' || *str == '1') )
		return new Value( glish_true );
	else
		return new Value( glish_false );
	}

Value *glishtk_strtoint( char *str )
	{
	return new Value( atoi(str) );
	}

Value *glishtk_strtofloat( char *str )
	{
	return new Value( atof(str) );
	}

Value *TkProc::operator()(Rivetobj s, Value *arg)
	{
	char *val = 0;

//** 	ProxyStore::HoldQueue();
	while ( TkAgent::DoOneTkEvent( TK_X_EVENTS | TK_IDLE_EVENTS | TK_DONT_WAIT ) ) ;
//** 	ProxyStore::ReleaseQueue();

	if ( proc )
		val = (*proc)(s,cmdstr,arg);
	else if ( proc1 )
		val = (*proc1)(s,cmdstr,param,arg);
	else if ( proc2 )
		val = (*proc2)(s,cmdstr,param,param2,arg);
	else if ( fproc != 0 && frame != 0 )
		val = (frame->*fproc)( arg );
	else if ( aproc != 0 && agent != 0 )
		val = (*aproc)(agent, cmdstr, arg );
	else if ( aproc2 != 0 && agent != 0 )
		val = (*aproc2)(agent, cmdstr, param, arg);
	else if ( aproc3 != 0 && agent != 0 )
		val = (*aproc3)(agent, cmdstr, param, param2, arg);
	else if ( iproc )
		val = (*iproc)(s, cmdstr, i, arg);
	else if ( iproc1 )
		val = (*iproc1)(s, cmdstr, param, i, arg);
#if defined(XTKPGPLOT)
	else if ( pgproc && pgplot )
		val = (pgplot->*pgproc)( arg);
#endif
	else
		return error_value();


//**	ProxyStore::HoldQueue();
	while ( TkAgent::DoOneTkEvent( TK_X_EVENTS | TK_IDLE_EVENTS | TK_DONT_WAIT ) ) ;
//**	ProxyStore::ReleaseQueue();

	if ( val != (void*) TCL_ERROR )
		{
		if ( convert && val )
			return (*convert)(val);
		else
			return new Value( glish_true );
		}
	else
		return new Value( glish_false );
	}

void TkAgent::ProcessEvent( const char *name, Value *val )
	{
	if ( ! IsValid() )
		Error("graphic is defunct");

	TkProc *proc = procs[name];

	if ( proc != 0 )
		{
		Value *v = (*proc)( self, val );
		if ( v && ReplyPending() ) Reply( v );
		}
	else
		Error("unknown event");
	}

void TkAgent::EnterEnable() { }
void TkAgent::ExitEnable() { }

void TkAgent::SetError( Value *v )
	{
	if ( last_error ) Unref(last_error);
	last_error = v;
	}

void TkAgent::PostTkEvent( const char *s, Value *v )
	{
	Ref(this);
	if ( hold_glish_events )
		tk_queue->EnQueue( new glishtk_event( this, s, v ) );
	else
		PostEvent( s, v );
	Unref(this);
	}

void TkAgent::FlushGlishEvents()
	{
	if ( hold_glish_events )
		{
		glishtk_event* e = 0;

		hold_glish_events = 0;
		while ( (e = tk_queue->DeQueue()) )
			{
#ifdef GGC
			ProxyStore::CurSeq()->UnregisterValue(e->value());
#endif
			e->Post();
			delete e;
			}
		}
	}

int TkAgent::DoOneTkEvent( int flags, int hold_wait )
	{
//** 	if ( shutting_glish_down ) return 0;

	int ret = 0;
	if ( hold_tk_events )
		{
		if ( flags & TK_FILE_EVENTS )
			ret = Tk_DoOneEvent( TK_FILE_EVENTS | (hold_wait ? 0 : TK_DONT_WAIT) );
		}
	else
		ret = Tk_DoOneEvent( flags );

	return ret;
	}

int TkAgent::DoOneTkEvent( )
	{
	int ret = 0;

	if ( hold_tk_events )
		ret = Tk_DoOneEvent( TK_FILE_EVENTS | TK_TIMER_EVENTS );
	else
		ret = Tk_DoOneEvent( 0 );

	return ret;
	}

void TkAgent::SetBitmapPath( const Value *p )
	{
	if ( p->Type() == TYPE_STRING )
		{
		if ( bitmap_path ) Unref( bitmap_path );
		bitmap_path = new Value( *p );
		}
	}

char *TkAgent::which_bitmap( const char* filename )
	{
	charptr *paths = bitmap_path ? bitmap_path->StringPtr() : 0;
	int len = bitmap_path ? bitmap_path->Length() : 0;

	if ( ! paths || filename[0] == '/' || filename[0] == '.' )
		{
		if ( access( filename, R_OK ) == 0 )
			return strdup( filename );
		else
			return 0;
		}

	char directory[1024];

	for ( int i = 0; i < len; i++ )
		if ( paths[i] && strlen(paths[i]) )
			{
			sprintf( directory, "%s/%s", paths[i], filename );

			if ( access( directory, R_OK ) == 0 )
				return strdup( directory );
			}

	return 0;
	}


static int (*glishtk_dflt_xioerror_handler)(Display *) = 0;
int glishtk_xioerror_handler(Display *d)
	{
	glish_cleanup();
	if ( glishtk_dflt_xioerror_handler )
		(*glishtk_dflt_xioerror_handler)(d);
	exit(1);
	return 1;
	}

char *glishtk_agent_map(TkAgent *a, const char *cmd, Value *)
	{
	a->SetMap( cmd[0] == 'M' ? 1 : 0, cmd[1] == 'T' ? 1 : 0 );
	return 0;
	}

void TkAgent::SetMap( int do_map, int toplevel )
	{
	int dont_map_ = do_map ? 0 : 1;
	if ( dont_map != dont_map_ )
		{
		dont_map = dont_map_;
		if ( ! toplevel )
			{
			if ( dont_map )
				rivet_va_func(self, (int (*)())Tk_PackCmd, "forget", rivet_path(self), 0);
			if ( frame ) frame->Pack();
			}
		else
			{
			Rivetobj win =  TopLevel();
			if ( win )
				{
				if ( dont_map )
					rivet_va_func(win, (int(*)()) Tk_WmCmd, "withdraw", rivet_path(win), 0);
				else
					rivet_va_func(win, (int(*)()) Tk_WmCmd, "deiconify", rivet_path(win), 0);
				}
			}
		}
	}

extern "C" void rivet_focus_follows_mouse(Rivetobj ref);
TkAgent::TkAgent( ProxyStore *s ) : Proxy( s ), dont_map( 0 ), disable_count(0)
	{
	agent_ID = "<graphic>";
	enable_state = 0;

	if ( tk_queue == 0 )
		tk_queue = new PQueue(glishtk_event)();

	self = 0;
	frame = 0;

	if ( ! root )
		{
		char *argv[3];
		argv[0] = "glish";
		argv[1] = 0;

		if ( (root = rivet_init(1, argv)) )
			{
			glishtk_dflt_xioerror_handler = 
				XSetIOErrorHandler(glishtk_xioerror_handler);
		
			rivet_focus_follows_mouse(root);
			}
		}

	procs.Insert("background", new TkProc("-bg", glishtk_onestr, glishtk_str));
	procs.Insert("foreground", new TkProc("-fg", glishtk_onestr, glishtk_str));
	procs.Insert("relief", new TkProc("-relief", glishtk_onestr, glishtk_str));
	procs.Insert("borderwidth", new TkProc("-borderwidth", glishtk_onedim, glishtk_strtoint));
	procs.Insert("pixelwidth", new TkProc("width",glishtk_winfo, glishtk_strtoint));
	procs.Insert("pixelheight", new TkProc("height",glishtk_winfo, glishtk_strtoint));
	}


void TkAgent::Disable( )
	{
	disable_count++;
	}

void TkAgent::Enable( int force )
	{
	if ( force ) disable_count = 0;
	else disable_count--;
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

void TkAgent::BindEvent(const char *event, Value *rec)
	{
	PostTkEvent( event, rec );
	}

Rivetobj TkAgent::TopLevel()
	{
	return frame ? frame->TopLevel() : 0;
	}

int TkAgent::IsPseudo( )
	{
	return 1;
	}

struct glishtk_bindinfo
	{
	TkAgent *agent;
	char *event_name;
	char *tk_event_name;
	glishtk_bindinfo( TkAgent *c, const char *event, const char *tk_event ) :
			agent(c), event_name(strdup(event)),
			tk_event_name(strdup(tk_event)) { }
	~glishtk_bindinfo()
		{
		free_memory( tk_event_name );
		free_memory( event_name );
		}
	};

int glishtk_bindcb(Rivetobj agent, XEvent *xevent, ClientData assoc, int keysym, int)
	{
	glishtk_bindinfo *info = (glishtk_bindinfo*) assoc;
	int dummy;
	recordptr rec = create_record_dict();

	int *wpt = (int*) alloc_memory( sizeof(int)*2 );
	wpt[0] = xevent->xkey.x;
	wpt[1] = xevent->xkey.y;
	rec->Insert( strdup("wpoint"), new Value( wpt, 2 ) );
	if ( xevent->type == KeyPress )
		rec->Insert( strdup("key"), new Value( rivet_expand_event(agent, "A", xevent, keysym, &dummy) ) );
	info->agent->BindEvent(info->event_name, new Value( rec ) );
	return TCL_OK;
	}

char *glishtk_bind(TkAgent *agent, const char *, Value *args )
	{
	char *event_name = "agent bind function";
	EXPRINIT( event_name)
	if ( args->Length() >= 2 )
		{
		EXPRSTR( button, event_name )
		EXPRSTR( event, event_name )
		glishtk_bindinfo *binfo = 
			new glishtk_bindinfo(agent, event, button);

		if ( rivet_create_binding(agent->Self(), 0, (char*)button, (int (*)()) glishtk_bindcb,
					  (ClientData) binfo, 1, 0) == TCL_ERROR )
			{
			global_store->Error("Error, binding not created.");
			delete binfo;
			}

		EXPR_DONE( event )
		EXPR_DONE( button )
		}

	return 0;
	}

int glishtk_delframe_cb(Rivetobj, XEvent *, ClientData assoc, ClientData)
	{
	((TkFrame*)assoc)->KillFrame();
	return TCL_OK;
	}

void glishtk_resizeframe_cb( ClientData clientData, XEvent *eventPtr)
	{
	if ( eventPtr->xany.type == ConfigureNotify )
		{
		TkFrame *f = (TkFrame*) clientData;
		f->ResizeEvent();
		}
	}

void glishtk_moveframe_cb( ClientData clientData, XEvent *eventPtr)
	{
	if ( eventPtr->xany.type == ConfigureNotify &&
	     eventPtr->xany.send_event )
		{
		TkFrame *f = (TkFrame*) clientData;
		f->LeaderMoved();
		}
	}

void TkFrame::Disable( )
	{
	loop_over_list( elements, i )
		if ( elements[i] != this )
			elements[i]->Disable( );
	}

void TkFrame::Enable( int force )
	{
	loop_over_list( elements, i )
		if ( elements[i] != this )
			elements[i]->Enable( force );
	}

TkFrame::TkFrame( ProxyStore *s, charptr relief_, charptr side_, charptr borderwidth, charptr padx_,
		  charptr pady_, charptr expand_, charptr background, charptr width, charptr height,
		  charptr cursor, charptr title, charptr icon, int new_cmap, TkAgent *tlead_, charptr tpos_ ) :
		  TkRadioContainer( s ), side(0), padx(0), pady(0), expand(0), tag(0), canvas(0),
		  is_tl( 1 ), pseudo( 0 ), reject_first_resize(1), tlead(tlead_), tpos(0), unmapped(0)

	{
	char *argv[17];
	char geometry[40];

	agent_ID = "<graphic:frame>";

	if ( ! root )
		HANDLE_CTOR_ERROR("Frame creation failed, check DISPLAY environment variable.")

	tl_count++;

	if ( tlead )
		{
		Ref( tlead );
		tpos = strdup(tpos_);
		while ( TkAgent::DoOneTkEvent( TK_X_EVENTS | TK_IDLE_EVENTS | TK_DONT_WAIT ) ) ;
		}

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
		if ( tlead )
			{
			rivet_va_func(pseudo, (int (*)()) Tk_WmCmd, "transient",
				      rivet_path(pseudo), rivet_path(tlead->Self()), 0 );
			rivet_va_func(pseudo, (int (*)()) Tk_WmCmd, "overrideredirect",
				      rivet_path(pseudo), "true", 0 );
			const char *geometry = glishtk_popup_geometry( tlead->Self(), tpos );
			rivet_va_func(pseudo, (int (*)()) Tk_WmCmd, "geometry",
				      rivet_path(pseudo), geometry, 0 );

			Rivetobj top = tlead->TopLevel();
			Tk_CreateEventHandler((Tk_Window)top->tkwin, StructureNotifyMask,
					      glishtk_moveframe_cb, this );
			}
		}
	else
		{
		top_created = 1;
		if ( title && title[0] )
			rivet_va_func(root, (int (*)()) Tk_WmCmd, "title",
				      rivet_path(root), title, 0 );
		if ( tlead )
			{
			rivet_va_func(root, (int (*)()) Tk_WmCmd, "transient",
				      rivet_path(root), rivet_path(tlead->Self()), 0 );
			rivet_va_func(root, (int (*)()) Tk_WmCmd, "overrideredirect",
				      rivet_path(root), "true", 0 );
			const char *geometry = glishtk_popup_geometry( tlead->Self(), tpos );
			rivet_va_func(root, (int (*)()) Tk_WmCmd, "geometry",
				      rivet_path(root), geometry, 0 );

			Rivetobj top = tlead->TopLevel();
			Tk_CreateEventHandler((Tk_Window)top->tkwin, StructureNotifyMask,
					      glishtk_moveframe_cb, this );
			}
		}

	side = strdup(side_);
	padx = strdup(padx_);
	pady = strdup(pady_);
	expand = strdup(expand_);

	int c = 2;
	argv[0] = argv[1] = 0;

	if ( new_cmap )
		{
		argv[c++] = "-colormap";
		argv[c++] = "new";
		}

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
		HANDLE_CTOR_ERROR("Rivet creation failed in TkFrame::TkFrame")

	rivet_va_func(self, (int(*)()) Tk_WmCmd, "protocol", rivet_path((pseudo ? pseudo : root)),
		      "WM_DELETE_WINDOW",rivet_new_callback( (int (*)()) glishtk_delframe_cb,
							     (ClientData) this, 0), 0 );

	if ( icon && strlen( icon ) )
		{
		char *expanded = which_bitmap(icon);
		if ( expanded )
			{
			char *icon_ = (char*) alloc_memory(strlen(expanded)+2);
			sprintf(icon_,"@%s",expanded);
			rivet_va_func(self, (int(*)()) Tk_WmCmd, "iconbitmap",
				      rivet_path((pseudo ? pseudo : root)),icon_, 0);
			free_memory( expanded );
			free_memory( icon_ );
			}
		}

	//
	// Clearing the height/width of toplevel frames fixes problems
	// with configuring the widget. When setting the cursor, for
	// example, the frame & children go crazy resizing themselves.
	//
	rivet_clear_frame_dims( self );
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
	procs.Insert("cursor", new TkProc("-cursor", glishtk_onestr, glishtk_str));
	procs.Insert("icon", new TkProc( this, &TkFrame::SetIcon ));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));

	procs.Insert("width", new TkProc("", glishtk_width, glishtk_valcast));
	procs.Insert("height", new TkProc("", glishtk_height, glishtk_valcast));

	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));

	procs.Insert("map", new TkProc(this, "MT", glishtk_agent_map));
	procs.Insert("unmap", new TkProc(this, "UT", glishtk_agent_map));

	procs.Insert("raise", new TkProc( this, &TkFrame::Raise ));
	procs.Insert("title", new TkProc( this, &TkFrame::Title ));

	Tk_CreateEventHandler((Tk_Window)self->tkwin, StructureNotifyMask, glishtk_resizeframe_cb, this );

	size[0] = self->tkwin->reqWidth;
	size[1] = self->tkwin->reqHeight;
	}

TkFrame::TkFrame( ProxyStore *s, TkFrame *frame_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_, charptr background,
		  charptr width, charptr height, charptr cursor, int new_cmap ) : TkRadioContainer( s ),
		  side(0), padx(0), pady(0), expand(0), tag(0), canvas(0), is_tl( 0 ),
		  pseudo( 0 ), reject_first_resize(0), tlead(0), tpos(0), unmapped(0)

	{
	char *argv[16];
	frame = frame_;

	agent_ID = "<graphic:frame>";

	if ( ! root )
		HANDLE_CTOR_ERROR("Frame creation failed, check DISPLAY environment variable.")

	if ( ! frame || ! frame->Self() ) return;

	side = strdup(side_);
	padx = strdup(padx_);
	pady = strdup(pady_);
	expand = strdup(expand_);

	int c = 2;
	argv[0] = argv[1] = 0;

	if ( new_cmap )
		{
		argv[c++] = "-colormap";
		argv[c++] = "new";
		}

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
		HANDLE_CTOR_ERROR("Rivet creation failed in TkFrame::TkFrame")

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
	procs.Insert("cursor", new TkProc("-cursor", glishtk_onestr, glishtk_str));
	procs.Insert("map", new TkProc(this, "MC", glishtk_agent_map));
	procs.Insert("unmap", new TkProc(this, "UC", glishtk_agent_map));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));

	procs.Insert("width", new TkProc("", glishtk_width, glishtk_valcast));
	procs.Insert("height", new TkProc("", glishtk_height, glishtk_valcast));

	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));
	}

TkFrame::TkFrame( ProxyStore *s, TkCanvas *canvas_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_, charptr background,
		  charptr width, charptr height, const char *tag_ ) : TkRadioContainer( s ), side(0),
		  padx(0), pady(0), expand(0), is_tl( 0 ), pseudo( 0 ), reject_first_resize(0),
		  tlead(0), tpos(0), unmapped(0)

	{
	char *argv[12];
	frame = 0;
	canvas = canvas_;
	tag = strdup(tag_);

	agent_ID = "<graphic:frame>";

	if ( ! root )
		HANDLE_CTOR_ERROR("Frame creation failed, check DISPLAY environment variable.")

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
		HANDLE_CTOR_ERROR("Rivet creation failed in TkFrame::TkFrame")

	//
	// Clearing the height/width of toplevel frames fixes problems
	// with configuring the widget. When setting the cursor, for
	// example, the frame & children go crazy resizing themselves.
	//
	rivet_clear_frame_dims( self );
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
	procs.Insert("cursor", new TkProc("-cursor", glishtk_onestr, glishtk_str));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));

	procs.Insert("width", new TkProc("", glishtk_width, glishtk_valcast));
	procs.Insert("height", new TkProc("", glishtk_height, glishtk_valcast));

	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));
	}

void TkFrame::UnMap()
	{
	if ( unmapped ) return;
	unmapped = 1;

	if ( RefCount() > 0 ) Ref(this);

	Tk_DeleteEventHandler((Tk_Window)self->tkwin, StructureNotifyMask, glishtk_resizeframe_cb, this );

	if ( grab && grab == Id() )
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

	if ( canvas )
		rivet_va_cmd( canvas->Self(), "delete", tag, 0 );
	else if ( self )
		rivet_destroy_window( self );

	canvas = 0;
	frame = 0;
	self = 0;

	if ( pseudo )
		{
		rivet_destroy_window( pseudo );
		pseudo = 0;
		}

	if ( unmap_root )
		rivet_unmap_window( root );

	if ( tlead )
		{
		Rivetobj top = tlead->TopLevel();
		if ( top )
			Tk_DeleteEventHandler((Tk_Window)top->tkwin, StructureNotifyMask,
					      glishtk_moveframe_cb, this );
		Unref( tlead );
		tlead = 0;
		}

	if ( ! tl_count )
		// Empty queue
		while( DoOneTkEvent( TK_X_EVENTS | TK_DONT_WAIT ) != 0 );

	if ( RefCount() > 0 ) Unref(this);
	}

TkFrame::~TkFrame( )
	{
	if ( frame )
		frame->RemoveElement( this );
	if ( canvas )
		canvas->Remove( this );

	if ( is_tl )
		--tl_count;

	free_memory( side );
	free_memory( padx );
	free_memory( pady );
	free_memory( expand );
	if ( tpos ) free_memory( tpos );

	UnMap();

	if ( tag )
		free_memory( tag );
	}

char *TkFrame::SetIcon( Value *args )
	{
	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_STRING )
		{
		const char *icon = args->StringPtr(0)[0];
		if ( icon && strlen(icon) )
			{
			char *icon_ = (char*) alloc_memory(strlen(icon)+2);
			sprintf(icon_,"@%s",icon);
			rivet_va_func(self, (int(*)()) Tk_WmCmd, "iconbitmap",
				      rivet_path((pseudo ? pseudo : root)),icon_, 0);
			free_memory( icon_ );
			}
		}
	else
		global_store->Error("wrong type, string expected");

	return "";
	}

char *TkFrame::SetSide( Value *args )
	{
	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_STRING )
		{
		const char *side_ = args->StringPtr(0)[0];
		if ( side_[0] != side[0] || strcmp(side, side_) )
			{
			free_memory( side );
			side = strdup( side_ );
			Pack();
			}
		}
	else
		global_store->Error("wrong type, string expected");

	return "";
	}

char *TkFrame::SetPadx( Value *args )
	{
	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_STRING )
		{
		const char *padx_ = args->StringPtr(0)[0];
		if ( padx_[0] != padx[0] || strcmp(padx, padx_) )
			{
			free_memory( padx );
			padx = strdup( padx_ );
			Pack();
			}
		}
	else if ( args->Type() != TYPE_BOOL && args->IsNumeric() )
		{
		char padx_[30];
		sprintf(padx_, "%d", args->IntVal());
		if ( padx_[0] != padx[0] || strcmp(padx, padx_) )
			{
			free_memory( padx );
			padx = strdup( padx_ );
			Pack();
			}
		}
	else
		global_store->Error("wrong type, string expected");

	return "";
	}

char *TkFrame::SetPady( Value *args )
	{
	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_STRING )
		{
		const char *pady_ = args->StringPtr(0)[0];
		if ( pady_[0] != pady[0] || strcmp(pady, pady_) )
			{
			free_memory( pady );
			pady = strdup( pady_ );
			Pack();
			}
		}
	else if ( args->Type() != TYPE_BOOL && args->IsNumeric() )
		{
		char pady_[30];
		sprintf(pady_, "%d", args->IntVal());
		if ( pady_[0] != pady[0] || strcmp(pady, pady_) )
			{
			free_memory( pady );
			pady = strdup( pady_ );
			Pack();
			}
		}
	else
		global_store->Error("wrong type, string expected");

	return "";
	}

char *TkFrame::GetTag( Value * )
	{
	return tag;
	}

char *TkFrame::SetExpand( Value *args )
	{
	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_STRING )
		{
		const char *expand_ = args->StringPtr(0)[0];
		if ( expand_[0] != expand[0] || strcmp(expand, expand_) )
			{
			free_memory( expand );
			expand = strdup( expand_ );
			Pack();
			}
		}
	else
		global_store->Error("wrong type, string expected");

	return "";
	}

char *TkFrame::Grab( int global_scope )
	{
	if ( grab ) return 0;

	if ( global_scope )
		rivet_va_func(self, (int(*)()) Tk_GrabCmd, "set", "-global", rivet_path(self), 0);
	else
		rivet_va_func(self, (int(*)()) Tk_GrabCmd, "set", rivet_path(self), 0);

	grab = Id();
	return "";
	}

char *TkFrame::GrabCB( Value *args )
	{
	if ( grab ) return 0;

	int global_scope = 0;

	if ( args->Type() == TYPE_STRING && args->Length() > 0 && ! strcmp(args->StringPtr(0)[0],"global") )
		global_scope = 1;

	return Grab( global_scope );
	}

char *TkFrame::Raise( Value *args )
	{
	TkAgent *agent = 0;
	if ( args->IsAgentRecord( ) && (agent = (TkAgent*) store->GetProxy(args)) )
		rivet_va_func( TopLevel(), (int (*)()) Tk_RaiseCmd,
			       rivet_path(TopLevel()), rivet_path(agent->TopLevel()), 0 );
	else
		rivet_va_func( TopLevel(), (int (*)()) Tk_RaiseCmd, rivet_path(TopLevel()), 0 );

	return "";
	}

char *TkFrame::Title( Value *args )
	{
	if ( args->Type() == TYPE_STRING )
		{
		Rivetobj top = TopLevel( );
		rivet_va_func( top, (int (*)()) Tk_WmCmd, "title",
			       rivet_path(top), args->StringPtr(0)[0], 0 );
		}
	else
		global_store->Error("wrong type, string expected");

	return "";
	}

char *TkFrame::FontsCB( Value *args )
	{
	char *wild = "-*-*-*-*-*-*-*-*-*-*-*-*-*-*";
	char **fonts = 0;
	int len = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_STRING )
		fonts = XListFonts(self->display, args->StringPtr(0)[0], 32768, &len);
	else if ( args->Type() != TYPE_BOOL && args->IsNumeric() )
		fonts = XListFonts(self->display, wild, args->IntVal(), &len);
	else if ( args->Type() == TYPE_RECORD )
		{
		EXPRINIT("TkFrame::FontsCB")
		EXPRSTR( str, "TkFrame::FontsCB" )
		EXPRINT( l, "TkFrame::FontsCB" )
		fonts = XListFonts(self->display, str, l, &len);
		EXPR_DONE( str )
		EXPR_DONE( l )
		}
	else
		fonts = XListFonts(self->display, wild, 32768, &len);

	Value *ret = fonts ? new Value( (charptr*) fonts, len, COPY_ARRAY ) : new Value( glish_false );
	XFreeFontNames(fonts);
	return (char*) ret;
	}

char *TkFrame::Release( )
	{
	if ( ! grab || grab != Id() ) return 0;

	rivet_va_func(self, (int(*)()) Tk_GrabCmd, "release", rivet_path(self), 0);

	grab = 0;
	return "";
	}

char *TkFrame::ReleaseCB( Value * )
	{
	return Release( );
	}

void TkFrame::PackSpecial( TkAgent *agent )
	{
	const char **instr = agent->PackInstruction();

	int cnt = 0;
	while ( instr[cnt] ) cnt++;

	char **argv = (char**) alloc_memory( sizeof(char*)*(cnt+8) );

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
	free_memory( argv );
	}

int TkFrame::ExpandNum(const TkAgent *except, unsigned int grtOReqt) const
	{
	unsigned int cnt = 0;
	loop_over_list( elements, i )
		{
		if ( (! except || elements[i] != except) &&
		     elements[i] != (TkAgent*) this && elements[i]->CanExpand() )
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
		char **argv = (char**) alloc_memory( sizeof(char*)*(elements.length()+7) );

		int c = 1;
		argv[0] = 0;
		loop_over_list( elements, i )
			{
			if ( elements[i]->DontMap() ) continue;
			if ( elements[i]->PackInstruction() )
				PackSpecial( elements[i] );
			else
				argv[c++] = rivet_path(elements[i]->Self() );
			}

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

		free_memory( argv );
		}
	}

void TkFrame::RemoveElement( TkAgent *obj )
	{
	if ( elements.is_member(obj) )
		elements.remove(obj);
	}

void TkFrame::KillFrame( )
	{
	PostTkEvent( "killed", new Value( glish_true ) );
	UnMap();
	}

void TkFrame::ResizeEvent( )
	{
	if ( reject_first_resize )
		reject_first_resize = 0;
	else
		{
		recordptr rec = create_record_dict();

		rec->Insert( strdup("old"), new Value( size, 2, COPY_ARRAY ) );
		size[0] = self->tkwin->changes.width;
		size[1] = self->tkwin->changes.height;
		rec->Insert( strdup("new"), new Value( size, 2, COPY_ARRAY ) );

		//
		//  This is needed to let the lower widgets repack, and
		//  resize; otherwise, this resize event isn't too useful
		//  because all information from the lower widgets is
		//  "one off".
		//
//** 		ProxyStore::HoldQueue();
		while ( TkAgent::DoOneTkEvent( TK_IDLE_EVENTS | TK_DONT_WAIT ) ) ;
//** 		ProxyStore::ReleaseQueue();

		PostTkEvent( "resize", new Value( rec ) );
		}
	}

void TkFrame::LeaderMoved( )
	{
	if ( ! tlead ) return;

	const char *geometry = glishtk_popup_geometry( tlead->Self(), tpos );
	rivet_va_func(pseudo ? pseudo : root, (int (*)()) Tk_WmCmd, "geometry",
		      rivet_path(pseudo ? pseudo : root), geometry, 0 );
	while ( TkAgent::DoOneTkEvent( TK_X_EVENTS | TK_IDLE_EVENTS | TK_DONT_WAIT ) ) ;
	rivet_va_func( pseudo ? pseudo : root, (int (*)()) Tk_RaiseCmd,
		       rivet_path(pseudo ? pseudo : root)/*, rivet_path(tlead->TopLevel())*/, 0 );
	}

void TkFrame::Create( ProxyStore *s, Value *args, void * )
	{
	TkFrame *ret = 0;

	if ( args->Length() != 16 )
		InvalidNumberOfArgs(16);

	SETINIT
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
	SETSTR( icon )
	SETINT( new_cmap )
	SETVAL( tlead, tlead->Type() == TYPE_BOOL || tlead->IsAgentRecord() )
	SETSTR( tpos )

	if ( parent->Type() == TYPE_BOOL )
		{
		TkAgent *tl = (TkAgent*)(tlead->IsAgentRecord() ? global_store->GetProxy(tlead) : 0);

		if ( tl && strncmp( tl->AgentID(), "<graphic:", 9 ) )
			{
			SETDONE
			global_store->Error("bad transient leader");
			return;
			}

		ret =  new TkFrame( s, relief, side, borderwidth, padx, pady, expand, background,
				    width, height, cursor, title, icon, new_cmap, (TkAgent*) tl, tpos );
		}
	else
		{
		TkAgent *agent = (TkAgent*)global_store->GetProxy(parent);
		if ( agent && ! strcmp("<graphic:frame>", agent->AgentID()) )
			ret =  new TkFrame( s, (TkFrame*)agent, relief,
					    side, borderwidth, padx, pady, expand, background,
					    width, height, cursor, new_cmap );
		else
			{
			SETDONE
			global_store->Error("bad parent type");
			return;
			}
		}

	CREATE_RETURN
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
		return (const char**) ret;
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

Rivetobj TkFrame::TopLevel( )
	{
	return frame ? frame->TopLevel() : canvas ? canvas->TopLevel() :
		pseudo ? pseudo : root;
	}

void TkButton::UnMap()
	{
	if ( unmapped ) return;
	unmapped = 1;

	if ( frame ) frame->RemoveElement( this );

	if ( RefCount() > 0 ) Ref(this);

	if ( type == RADIO && radio && frame != radio && menu != radio )
		Unref(radio);

	radio = 0;

	if ( type == MENU )
		{
		while ( entry_list.length() )
			{
			TkAgent *a = entry_list.remove_nth( 0 );
			a->UnMap( );
			}

		if ( menu )
			{
			menu->Remove(this);
			rivet_va_cmd( Menu(), "delete", Index(), 0 );
			}
		else
			{
			if ( self ) rivet_set( self, "-postcommand", "" );
			rivet_destroy_window( self );
			}
		}

	else if ( menu )
		{
		menu->Remove(this);
		rivet_va_cmd( Menu(), "delete", Index(), 0 );
		}

	else if ( self )
		{
		rivet_set( self, "-command", "" );
		rivet_destroy_window( self );
		}

	menu = 0;
	frame = 0;
	self = 0;
	menu_base = 0;

	if ( RefCount() > 0 ) Unref(this);
	}

TkButton::~TkButton( )
	{
	if ( frame )
		{
		frame->RemoveElement( this );
		frame->Pack();
		}

	if ( value )
		{
#ifdef GGC
		sequencer->UnregisterValue( value );
#endif
		Unref(value);
		}

	UnMap();
	}

static unsigned char dont_invoke_button = 0;

int buttoncb(Rivetobj, XEvent *, ClientData assoc, ClientData)
	{
	((TkButton*)assoc)->ButtonPressed();
	return TCL_OK;
	}

int tk_button_menupost(Rivetobj, XEvent *, ClientData, ClientData)
	{
	return TCL_OK;
	}


void TkButton::EnterEnable()
	{
	if ( ! enable_state && disable_count )
		{
		enable_state++;
		if ( frame )
			rivet_set( self, "-state", "normal" );
		else
			rivet_va_cmd( Parent()->Menu(), "entryconfigure", Index(), "-state", "normal", 0 );
		}
	}

void TkButton::ExitEnable()
	{
	if ( enable_state && --enable_state == 0 )
		if ( frame )
			rivet_set( self, "-state", "disabled" );
		else
			rivet_va_cmd( Parent()->Menu(), "entryconfigure", Index(), "-state", "disabled", 0 );
	}

void TkButton::Disable( )
	{
	disable_count++;
	if ( frame )
		rivet_set( self, "-state", "disabled" );
	else
		rivet_va_cmd( Parent()->Menu(), "entryconfigure", Index(), "-state", "disabled", 0 );
	}

void TkButton::Enable( int force )
	{
	if ( disable_count <= 0 ) return;

	if ( force )
		disable_count = 0;
	else
		disable_count--;

	if ( disable_count ) return;

	if ( frame )
		rivet_set( self, "-state", "normal" );
	else
		rivet_va_cmd( Parent()->Menu(), "entryconfigure", Index(), "-state", "normal", 0 );
		
	}

TkButton::TkButton( ProxyStore *s, TkFrame *frame_, charptr label, charptr type_,
		    charptr padx, charptr pady, int width, int height, charptr justify,
		    charptr font, charptr relief, charptr borderwidth, charptr foreground,
		    charptr background, int disabled, const Value *val, charptr anchor,
		    charptr fill_, charptr bitmap_, TkRadioContainer *group )
			: TkRadioContainer( s ), value(0), state(0), menu(0), radio(group),
			  menu_base(0),  next_menu_entry(0), menu_index(0), fill(0), unmapped(0)
	{
	type = PLAIN;
	frame = frame_;
	char *argv[34];

	agent_ID = "<graphic:button>";

	if ( ! frame || ! frame->Self() ) return;

	char width_[30];
	char height_[30];
	char var_name[256];
	char val_name[256];
	char *bitmap = 0;

	sprintf(width_,"%d", width);
	sprintf(height_,"%d", height);

	if ( ! strcmp(type_, "radio") ) type = RADIO;
	else if ( ! strcmp(type_, "check") ) type = CHECK;
	else if ( ! strcmp(type_, "menu") ) type = MENU;

	if ( type == RADIO && radio && frame != radio )
		Ref( radio );
	else if ( type != RADIO )
		radio = 0;

	int c = 2;
	argv[0] = argv[1] = 0;

	if ( type == RADIO )
		{
		sprintf(var_name,"%s%lx",type_,radio->Id());
		argv[c++] = "-variable";
		argv[c++] = var_name;
		sprintf(val_name,"BVaLuE%lx",Id());
		argv[c++] = "-value";
		argv[c++] = val_name;
		}

	argv[c++] = "-padx";
	argv[c++] = (char*) padx;
	argv[c++] = "-pady";
	argv[c++] = (char*) pady;
	argv[c++] = "-justify";
	argv[c++] = (char*) justify;

	if ( bitmap_ && strlen( bitmap_ ) )
		{
		char *expanded = which_bitmap(bitmap_);
		if ( expanded )
			{
			bitmap = (char*) alloc_memory(strlen(expanded)+2);
			sprintf(bitmap,"@%s",expanded);
			argv[c++] = "-bitmap";
			argv[c++] = bitmap;
			free_memory( expanded );
			}
		}

	if ( ! bitmap )
		{
		argv[c++] = "-width";
		argv[c++] = width_;
		argv[c++] = "-height";
		argv[c++] = height_;
		argv[c++] = "-text";
		argv[c++] = (char*) label;
		}

	argv[c++] = "-anchor";
	argv[c++] = (char*) anchor;

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
	if ( disabled ) disable_count++;

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
			if ( ! self )
				HANDLE_CTOR_ERROR("Rivet creation failed in TkButton::TkButton")
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

	if ( bitmap ) free_memory(bitmap);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkButton::TkButton")

	value = val ? copy_value( val ) : 0;
#ifdef GGC
	if ( value ) sequencer->RegisterValue( value );
#endif

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("text", new TkProc("-text", glishtk_onestr, glishtk_str));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("bitmap", new TkProc("-bitmap", glishtk_bitmap, glishtk_str));
	procs.Insert("justify", new TkProc("-justify", glishtk_onestr, glishtk_str));
	procs.Insert("height", new TkProc("-height", glishtk_onedim, glishtk_strtoint));
	procs.Insert("width", new TkProc("-width", glishtk_onedim, glishtk_strtoint));
	procs.Insert("padx", new TkProc("-padx", glishtk_onedim, glishtk_strtoint));
	procs.Insert("pady", new TkProc("-pady", glishtk_onedim, glishtk_strtoint));
	procs.Insert("state", new TkProc(this, "", glishtk_button_state, glishtk_strtobool));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	procs.Insert("anchor", new TkProc("-anchor", glishtk_onestr, glishtk_str));

	procs.Insert("disabled", new TkProc(this, "", glishtk_disable_cb));
	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));
	}

TkButton::TkButton( ProxyStore *s, TkButton *frame_, charptr label, charptr type_,
		    charptr /*padx*/, charptr /*pady*/, int width, int height, charptr /*justify*/,
		    charptr font, charptr /*relief*/, charptr /*borderwidth*/, charptr foreground,
		    charptr background, int disabled, const Value *val, charptr bitmap_,
		    TkRadioContainer *group )
			: TkRadioContainer( s ), value(0), state(0), radio(group),
			  menu_base(0), next_menu_entry(0), menu_index(0), unmapped(0)
	{
	type = PLAIN;

	menu = frame_;
	frame = 0;

	agent_ID = "<graphic:button>";

	if ( ! frame_->IsMenu() )
		HANDLE_CTOR_ERROR("internal error with creation of menu entry")

	if ( ! menu || ! menu->Self() ) return;

	char *argv[38];

	char width_[30];
	char height_[30];
	char var_name[256];
	char val_name[256];
	char *bitmap = 0;

	sprintf(width_,"%d", width);
	sprintf(height_,"%d", height);

	if ( ! strcmp(type_, "radio") ) type = RADIO;
	else if ( ! strcmp(type_, "check") ) type = CHECK;
	else if ( ! strcmp(type_, "menu") ) type = MENU;

	if ( type == RADIO && radio && menu != radio )
		Ref( radio );
	else if ( type != RADIO )
		radio = 0;

	int c = 3;
	argv[0] = argv[2] = 0;
	argv[1] = "add";

	if ( type == RADIO )
		{
		sprintf(var_name,"%s%lx",type_,radio->Id());
		argv[c++] = "-variable";
		argv[c++] = var_name;
		sprintf(val_name,"BVaLuE%lx",Id());
		argv[c++] = "-value";
		argv[c++] = val_name;
		}


#if 0
	argv[c++] = "-padx";
	argv[c++] = (char*) padx;
	argv[c++] = "-pady";
	argv[c++] = (char*) pady;
	argv[c++] = "-justify";
	argv[c++] = (char*) justify;
#endif

	if ( bitmap_ && strlen( bitmap_ ) )
		{
		char *expanded = which_bitmap(bitmap_);
		if ( expanded )
			{
			bitmap = (char*) alloc_memory(strlen(expanded)+2);
			sprintf(bitmap,"@%s",expanded);
			argv[c++] = "-bitmap";
			argv[c++] = bitmap;
			free_memory( expanded );
			}
		}

	if ( ! bitmap )
		{
#if 0
		argv[c++] = "-width";
		argv[c++] = width_;
		argv[c++] = "-height";
		argv[c++] = height_;
#endif
		argv[c++] = "-label";
		argv[c++] = (char*) label;
		}

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
#endif
	argv[c++] = "-foreground";
	argv[c++] = (char*) foreground;
	argv[c++] = "-background";
	argv[c++] = (char*) background;
	argv[c++] = "-state";
	argv[c++] = disabled ? "disabled" : "normal";
	if ( disabled ) disable_count++;

	argv[c++] = "-command";
	argv[c++] = rivet_new_callback( (int (*)()) buttoncb, (ClientData) this, 0);

	switch ( type )
		{
		case RADIO:
			argv[2] = "radio";
			rivet_cmd(Menu(), c, argv);
			self = Menu();
			break;
		case CHECK:
			argv[2] = "check";
			rivet_cmd(Menu(), c, argv);
			self = Menu();
			break;
		case MENU:
			{
			argv[2] = "cascade";
			char *av[10];
			av[0] = av[1] = 0;
			av[2] = "-tearoff";
			av[3] = "0";
			self = menu_base = rivet_create(MenuClass, menu->Menu(), 4, av);
			argv[c++] = "-menu";
			argv[c++] = rivet_path(self);
			rivet_cmd(menu->Menu(), c, argv);
			}
			break;
		default:
			argv[2] = "command";
			rivet_cmd(Menu(), c, argv);
			self = Menu();
			break;
		}

	if ( bitmap ) free_memory(bitmap);

	value = val ? copy_value( val ) : 0;
#ifdef GGC
	if ( value ) sequencer->RegisterValue( value );
#endif

	menu_index = strdup( rivet_va_cmd(menu->Menu(), "index", "last", 0) );

        menu->Add(this);

	procs.Insert("text", new TkProc(this, "-label", glishtk_menu_onestr));
	procs.Insert("font", new TkProc(this, "-font", glishtk_menu_onestr));
	procs.Insert("bitmap", new TkProc("-bitmap", glishtk_bitmap, glishtk_str));
	procs.Insert("background", new TkProc(this, "-background", glishtk_menu_onestr));
	procs.Insert("foreground", new TkProc(this, "-foreground", glishtk_menu_onestr));
	procs.Insert("state", new TkProc(this, "", glishtk_button_state, glishtk_strtobool));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));

	procs.Insert("disabled", new TkProc(this, "", glishtk_disable_cb));
	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));
	}

void TkButton::ButtonPressed( )
	{
	if ( type == RADIO )
		radio->RadioID( Id() );
	else if ( type == CHECK )
		state = state ? 0 : 1;

	if ( dont_invoke_button == 0 )
		{
		Value *v = value ? copy_value(value) : new Value( glish_true );

		attributeptr attr = v->ModAttributePtr();
		attr->Insert( strdup("state"), type != CHECK || state ? new Value( glish_true ) :
							    new Value( glish_false ) ) ;

		PostTkEvent( "press", v );
		}
	else
		dont_invoke_button = 0;
	}

void TkButton::Create( ProxyStore *s, Value *args, void * )
	{
	TkButton *ret;

	if ( args->Length() != 19 )
		InvalidNumberOfArgs(19);

	SETINIT
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
	SETSTR( anchor )
	SETSTR( fill )
	SETSTR( bitmap )
	SETVAL( group, group->IsAgentRecord() )


	TkAgent *agent = (TkAgent*) (global_store->GetProxy(parent));
	TkAgent *grp = (TkAgent*) (global_store->GetProxy(group));
	if ( agent && grp && 
	     ( ! strcmp( grp->AgentID(),"<graphic:button>" ) && ((TkButton*)grp)->IsMenu() ||
	       ! strcmp( grp->AgentID(), "<graphic:frame>") ) )
		{
		if ( ! strcmp( agent->AgentID(), "<graphic:button>") &&
		     ((TkButton*)agent)->IsMenu() )
				ret =  new TkButton( s, (TkButton*)agent, label, type, padx, pady,
						     width, height, justify, font, relief, borderwidth,
						     foreground, background, disabled, val, bitmap,
						     (TkRadioContainer*) grp );
		else if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
			ret =  new TkButton( s, (TkFrame*)agent, label, type, padx, pady, width, height,
					     justify, font, relief, borderwidth, foreground, background,
					     disabled, val, anchor, fill, bitmap, (TkRadioContainer*) grp );
		}
	else
		{
		SETDONE
		global_store->Error("bad parent (or group) type");
		return;
		}

	CREATE_RETURN
	}

unsigned char TkButton::State() const
	{
	unsigned char ret = 0;
	if ( type == RADIO )
		ret = radio->RadioID() == Id();
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
		sprintf(var_name,"radio%lx",radio->Id());
		radio->RadioID( 0 );
		Tcl_SetVar( self->interp, var_name, "", TCL_GLOBAL_ONLY );
		}
	else
		{
		EnterEnable();
		dont_invoke_button = 1;
		if ( frame )
			rivet_va_cmd( self, "invoke", 0 );
		else if ( menu )
			rivet_va_cmd( Menu(), "invoke", Index(), 0 );
		ExitEnable();
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
		return (const char **) ret;		\
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

Rivetobj TkButton::TopLevel( )
	{
	return frame ? frame->TopLevel() : menu ? menu->TopLevel() : 0;
	}

DEFINE_DTOR(TkScale)

void TkScale::UnMap()
	{
	if ( self ) rivet_set( self, "-command", "" );
	TkAgent::UnMap();
	}

unsigned int TkScale::scale_count = 0;
int scalecb(Rivetobj, XEvent *, ClientData assoc, ClientData calldata)
	{
	((TkScale*)assoc)->ValueSet( *((double*) calldata ) );
	return TCL_OK;
	}

char *glishtk_scale_value(TkAgent *a, const char *, Value *args )
	{

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() != TYPE_BOOL && args->IsNumeric() )
		((TkScale*)a)->SetValue( args->DoubleVal() );

	return 0;
	}

TkScale::TkScale ( ProxyStore *s, TkFrame *frame_, double from, double to, double value, charptr len,
		   charptr text, double resolution, charptr orient, int width, charptr font,
		   charptr relief, charptr borderwidth, charptr foreground, charptr background,
		   charptr fill_ )
			: TkAgent( s ), fill(0), from_(from), to_(to), discard_event(1)
	{
	char var_name[256];
	frame = frame_;
	char *argv[30];
	char from_c[40];
	char to_c[40];
	char resolution_[40];
	char width_[30];
	id = ++scale_count;

	agent_ID = "<graphic:scale>";

	if ( ! frame || ! frame->Self() ) return;

	sprintf(var_name,"ScAlE%d\n",id);

	sprintf(from_c,"%f",from);
	sprintf(to_c,"%f",to);

	sprintf(resolution_,"%f",resolution);
	sprintf(width_,"%d", width);

	int c = 2;
	argv[0] = argv[1] = 0;
	argv[c++] = "-from";
	argv[c++] = from_c;
	argv[c++] = "-to";
	argv[c++] = to_c;
	argv[c++] = "-length";
	argv[c++] = (char*) len;
	argv[c++] = "-resolution";
	argv[c++] = (char*) resolution_;
	argv[c++] = "-orient";
	argv[c++] = (char*) orient;
	argv[c++] = "-label";
	argv[c++] = (char*) text;
	argv[c++] = "-width";
	argv[c++] = (char*) width_;
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
	argv[c++] = "-variable";
	argv[c++] = var_name;
	argv[c++] = "-command";
	argv[c++] = rivet_new_callback((int (*)()) scalecb, (ClientData) this, 0);

	self = rivet_create(ScaleClass, frame->Self(), c, argv);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkScale::TkScale")

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);
	else if ( fill_ && ! fill_[0] )
		fill = (orient && ! strcmp(orient,"vertical")) ? strdup("y") :
				(orient && ! strcmp(orient,"horizontal")) ? strdup("x") : 0;

	SetValue(from_);
	frame->AddElement( this );
	frame->Pack();

	if ( value > from && value <= to )
		{
		char val[256];
		sprintf(val,"%g",value);
		Tcl_SetVar( self->interp, var_name, val, TCL_GLOBAL_ONLY );
		}

	procs.Insert("length", new TkProc("-length", glishtk_onedim, glishtk_strtoint));
	procs.Insert("orient", new TkProc("-orient", glishtk_onestr, glishtk_str));
	procs.Insert("text", new TkProc("-label", glishtk_onestr, glishtk_str));
	procs.Insert("end", new TkProc("-to", glishtk_onedouble, glishtk_strtofloat));
	procs.Insert("start", new TkProc("-from", glishtk_onedouble, glishtk_strtofloat));
	procs.Insert("value", new TkProc(this, "", glishtk_scale_value));
	procs.Insert("resolution", new TkProc("-resolution", glishtk_onedouble));
	procs.Insert("width", new TkProc("-width", glishtk_onedim, glishtk_strtoint));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	}

void TkScale::ValueSet( double d )
	{
	if ( ! discard_event )
		PostTkEvent( "value", new Value( d ) );
	discard_event = 0;
	}

void TkScale::SetValue( double d )
	{
	char var_name[256];
	char val[256];
	if ( d >= from_ && d <= to_ )
		{
		sprintf(var_name,"ScAlE%d\n",id);
		sprintf(val,"%g",d);
		Tcl_SetVar( self->interp, var_name, val, TCL_GLOBAL_ONLY );
		}
	}

void TkScale::Create( ProxyStore *s, Value *args, void * )
	{
	TkScale *ret;

	if ( args->Length() != 15 )
		InvalidNumberOfArgs(15);

	SETINIT
	SETVAL( parent, parent->IsAgentRecord() )
	SETDOUBLE( start )
	SETDOUBLE( end )
	SETDOUBLE( value )
	SETDIM( len )
	SETSTR( text )
	SETDOUBLE( resolution )
	SETSTR( orient )
	SETINT( width )
	SETSTR( font )
	SETSTR( relief )
	SETDIM( borderwidth )
	SETSTR( foreground )
	SETSTR( background )
	SETSTR( fill )

	TkAgent *agent = (TkAgent*) (global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret = new TkScale( s, (TkFrame*)agent, start, end, value, len, text, resolution, orient, width, font, relief, borderwidth, foreground, background, fill );
	else
		{
		SETDONE
		global_store->Error("bad parent type");
		return;
		}

	CREATE_RETURN
	}      

STD_EXPAND_PACKINSTRUCTION(TkScale)
STD_EXPAND_CANEXPAND(TkScale)

DEFINE_DTOR(TkText)

void TkText::UnMap()
	{
	if ( self )
		{
		rivet_set( self, "-xscrollcommand", "" );
		rivet_set( self, "-yscrollcommand", "" );
		}

	TkAgent::UnMap();
	}

int text_yscrollcb(Rivetobj, XEvent *, ClientData assoc, ClientData calldata)
	{
	double *firstlast = (double*)calldata;
	((TkText*)assoc)->yScrolled( firstlast );
	return TCL_OK;
	}

int text_xscrollcb(Rivetobj, XEvent *, ClientData assoc, ClientData calldata)
	{
	double *firstlast = (double*)calldata;
	((TkText*)assoc)->xScrolled( firstlast );
	return TCL_OK;
	}

#define DEFINE_ENABLE_FUNCS(CLASS)				\
void CLASS::EnterEnable()					\
	{							\
	if ( ! enable_state && ! strcmp("disabled", rivet_va_cmd(self, "cget", "-state", 0)) ) \
		{						\
		enable_state++;					\
		rivet_set( self, "-state", "normal" );		\
		}						\
	}							\
								\
void CLASS::ExitEnable()					\
	{							\
	if ( enable_state && --enable_state == 0 )		\
		rivet_set( self, "-state", "disabled" );	\
	}							\
								\
void CLASS::Disable( )						\
	{							\
	disable_count++;					\
	rivet_set( self, "-state", "disabled" );		\
	}							\
								\
void CLASS::Enable( int force )					\
	{							\
	if ( disable_count <= 0 ) return;			\
								\
	if ( force )						\
		disable_count = 0;				\
	else							\
		disable_count--;				\
								\
	if ( disable_count ) return;				\
								\
	rivet_set( self, "-state", "normal" );			\
	}

DEFINE_ENABLE_FUNCS(TkText)

TkText::TkText( ProxyStore *s, TkFrame *frame_, int width, int height, charptr wrap,
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
		HANDLE_CTOR_ERROR("Rivet creation failed in TkText::TkText")

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
	if ( disabled ) disable_count++;

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
	procs.Insert("width", new TkProc("-width", glishtk_onedim, glishtk_strtoint));
	procs.Insert("height", new TkProc("-height", glishtk_onedim, glishtk_strtoint));
	procs.Insert("wrap", new TkProc("-wrap", glishtk_onestr, glishtk_str));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));

	procs.Insert("see", new TkProc(this, "see", glishtk_oneidx));
	procs.Insert("delete", new TkProc(this, "delete", glishtk_oneortwoidx));
	procs.Insert("get", new TkProc(this, "get", glishtk_oneortwoidx_strary, glishtk_strary_to_value));
	procs.Insert("insert", new TkProc(this, "insert", 0, glishtk_text_append));
	procs.Insert("append", new TkProc(this, "insert", "end", glishtk_text_append));
	procs.Insert("prepend", new TkProc(this, "insert", "start", glishtk_text_append));
	procs.Insert("addtag", new TkProc("tag", "add", glishtk_text_tagfunc));
	procs.Insert("deltag", new TkProc("tag", "delete", glishtk_text_rangesfunc));
	procs.Insert("config", new TkProc("tag", "configure", glishtk_text_configfunc));
	procs.Insert("ranges", new TkProc("tag", "ranges", glishtk_text_rangesfunc, glishtk_splitsp_str));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));

	procs.Insert("disabled", new TkProc(this, "", glishtk_disable_cb));
	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));
	}

void TkText::Create( ProxyStore *s, Value *args, void * )
	{
	TkText *ret;

	if ( args->Length() != 12 )
		InvalidNumberOfArgs(12);

	SETINIT
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

	TkAgent *agent = (TkAgent*)(global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		{
		char *text_str = text->StringVal( ' ', 0, 1 );
		ret =  new TkText( s, (TkFrame*)agent, width, height, wrap, font, disabled, text_str, relief, borderwidth, foreground, background, fill );
		free_memory( text_str );
		}
	else
		{
		SETDONE
		global_store->Error("bad parent type");
		return;
		}

	CREATE_RETURN
	}      

void TkText::yScrolled( const double *d )
	{
	PostTkEvent( "yscroll", new Value( (double*) d, 2, COPY_ARRAY ) );
	}

void TkText::xScrolled( const double *d )
	{
	PostTkEvent( "xscroll", new Value( (double*) d, 2, COPY_ARRAY ) );
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

void TkScrollbar::UnMap()
	{
	if ( self ) rivet_set( self, "-command", "" );
	TkAgent::UnMap();
	}

int scrollbarcb(Rivetobj, XEvent *, ClientData assoc, ClientData calldata)
	{
	Scrollbar_notify_data *data = (Scrollbar_notify_data*) calldata;
	((TkScrollbar*)assoc)->Scrolled( ScrollToValue( data ) );
	return TCL_OK;
	}

TkScrollbar::TkScrollbar( ProxyStore *s, TkFrame *frame_, charptr orient,
			  int width, charptr foreground, charptr background )
				: TkAgent( s )
	{
	frame = frame_;
	char *argv[10];

	agent_ID = "<graphic:scrollbar>";

	if ( ! frame || ! frame->Self() ) return;

	char width_[30];
	sprintf(width_,"%d", width);

	int c = 2;
	argv[0] = argv[1] = 0;
	argv[c++] = "-orient";
	argv[c++] = (char*) orient;
	argv[c++] = "-width";
	argv[c++] = width_;
	argv[c++] = "-command";
	argv[c++] = rivet_new_callback((int (*)()) scrollbarcb, (ClientData) this, 0);

	self = rivet_create(ScrollbarClass, frame->Self(), c, argv);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkScrollbar::TkScrollbar")

	// Setting foreground and background colors at creation
	// time kills the goose
	rivet_set( self, "-bg", (char*) background );
	rivet_set( self, "-fg", (char*) foreground );

	frame->AddElement( this );
	frame->Pack();

	rivet_scrollbar_set( self, 0.0, 1.0 );

	procs.Insert("view", new TkProc("", glishtk_scrollbar_update));
	procs.Insert("orient", new TkProc("-orient", glishtk_onestr, glishtk_str));
	procs.Insert("width", new TkProc("-width", glishtk_onedim, glishtk_strtoint));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
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

	return (const char **) ret;
	}

int TkScrollbar::CanExpand() const
	{
	return 1;
	}

void TkScrollbar::Create( ProxyStore *s, Value *args, void * )
	{
	TkScrollbar *ret;

	if ( args->Length() != 5 )
		InvalidNumberOfArgs(5);

	SETINIT
	SETVAL( parent, parent->IsAgentRecord() )
	SETSTR( orient )
	SETINT( width )
	SETSTR( foreground )
	SETSTR( background )

	TkAgent *agent = (TkAgent*)(global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret = new TkScrollbar( s, (TkFrame*)agent, orient, width, foreground, background );
	else
		{
		SETDONE
		global_store->Error("bad parent type");
		return;
		}

	CREATE_RETURN
	}      

void TkScrollbar::Scrolled( Value *data )
	{
	PostTkEvent( "scroll", data );
	}


DEFINE_DTOR(TkLabel)

TkLabel::TkLabel( ProxyStore *s, TkFrame *frame_, charptr text, charptr justify,
		  charptr padx, charptr pady, int width_, charptr font, charptr relief,
		  charptr borderwidth, charptr foreground, charptr background,
		  charptr anchor, charptr fill_ )
			: TkAgent( s ), fill(0)
	{
	frame = frame_;
	char *argv[24];
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
	argv[c++] = "-anchor";
	argv[c++] = (char*) anchor;

	self = rivet_create(LabelClass, frame->Self(), c, argv);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkLabel::TkLabel")

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("height", new TkProc("-height", glishtk_oneint, glishtk_strtoint));
	procs.Insert("width", new TkProc("-width", glishtk_oneint, glishtk_strtoint));
	procs.Insert("text", new TkProc("-text", glishtk_onestr, glishtk_str));
	procs.Insert("anchor", new TkProc("-anchor", glishtk_onestr, glishtk_str));
	procs.Insert("justify", new TkProc("-justify", glishtk_onestr, glishtk_str));
	procs.Insert("padx", new TkProc("-padx", glishtk_onedim, glishtk_strtoint));
	procs.Insert("pady", new TkProc("-pady", glishtk_onedim, glishtk_strtoint));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	}

void TkLabel::Create( ProxyStore *s, Value *args, void * )
	{
	TkLabel *ret;

	if ( args->Length() != 13 )
		InvalidNumberOfArgs(13);

	SETINIT
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
	SETSTR( anchor )
	SETSTR( fill )

	TkAgent *agent = (TkAgent*)(global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret =  new TkLabel( s, (TkFrame*)agent, text, justify, padx, pady, width,
				    font, relief, borderwidth, foreground, background, anchor, fill );
	else
		{
		SETDONE
		global_store->Error("bad parent type");
		return;
		}

	CREATE_RETURN
	}      

STD_EXPAND_PACKINSTRUCTION(TkLabel)
STD_EXPAND_CANEXPAND(TkLabel)


DEFINE_DTOR(TkEntry)

void TkEntry::UnMap()
	{
	if ( self ) rivet_set( self, "-xscrollcommand", "" );
	TkAgent::UnMap();
	}

int entry_returncb(Rivetobj, XEvent *, ClientData assoc, ClientData)
	{
	((TkEntry*)assoc)->ReturnHit();
	return TCL_OK;
	}

int entry_xscrollcb(Rivetobj, XEvent *, ClientData assoc, ClientData calldata)
	{
	double *firstlast = (double*)calldata;
	((TkEntry*)assoc)->xScrolled( firstlast );
	return TCL_OK;
	}

DEFINE_ENABLE_FUNCS(TkEntry)

TkEntry::TkEntry( ProxyStore *s, TkFrame *frame_, int width,
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
	if ( disabled ) disable_count++;

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
		HANDLE_CTOR_ERROR("Rivet creation failed in TkEntry::TkEntry")

	if ( rivet_create_binding(self, 0, "<Return>", (int (*)()) entry_returncb, (ClientData) this, 1, 0) == TCL_ERROR )
		HANDLE_CTOR_ERROR("Rivet binding creation failed in TkEntry::TkEntry")

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("view", new TkProc("", glishtk_scrolled_update));
	procs.Insert("justify", new TkProc("-justify", glishtk_onestr, glishtk_str));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("width", new TkProc("-width", glishtk_oneint, glishtk_strtoint));
	procs.Insert("exportselection", new TkProc("-exportselection", glishtk_onebool));
	procs.Insert("show", new TkProc("-show", glishtk_onebool));
	procs.Insert("get", new TkProc("get", glishtk_nostr, glishtk_splitnl));
	procs.Insert("insert", new TkProc(this, "insert", glishtk_strandidx));
	procs.Insert("delete", new TkProc(this, "delete", glishtk_oneortwoidx));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));

	procs.Insert("disabled", new TkProc(this, "", glishtk_disable_cb));
	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));
	}

void TkEntry::ReturnHit( )
	{
	if ( strcmp("disabled", rivet_va_cmd(self, "cget", "-state", 0)) )
		{
		Value *ret = new Value( rivet_va_cmd( self, "get", 0 ) );
		PostTkEvent( "return", ret );
		}
	}

void TkEntry::xScrolled( const double *d )
	{
	PostTkEvent( "xscroll", new Value( (double*) d, 2, COPY_ARRAY ) );
	}

void TkEntry::Create( ProxyStore *s, Value *args, void * )
	{
	TkEntry *ret;

	if ( args->Length() != 12 )
		InvalidNumberOfArgs(12);

	SETINIT
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

	TkAgent *agent = (TkAgent*)(global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret =  new TkEntry( s, (TkFrame*)agent, width, justify,
				    font, relief, borderwidth, foreground, background,
				    disabled, show, exp, fill );
	else
		{
		SETDONE
		global_store->Error("bad parent type");
		return;
		}

	CREATE_RETURN
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

TkMessage::TkMessage( ProxyStore *s, TkFrame *frame_, charptr text, charptr width, charptr justify,
		      charptr font, charptr padx, charptr pady, charptr relief, charptr borderwidth,
		      charptr foreground, charptr background, charptr anchor, charptr fill_ )
			: TkAgent( s ), fill(0)
	{
	frame = frame_;
	char *argv[24];

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
	argv[c++] = "-anchor";
	argv[c++] = (char*) anchor;

	self = rivet_create(MessageClass, frame->Self(), c, argv);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkMessage::TkMessage")

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("text", new TkProc("-text", glishtk_onestr, glishtk_str));
	procs.Insert("width", new TkProc("-width", glishtk_onedim, glishtk_strtoint));
	procs.Insert("justify", new TkProc("-justify", glishtk_onestr, glishtk_str));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("padx", new TkProc("-padx", glishtk_onedim, glishtk_strtoint));
	procs.Insert("pady", new TkProc("-pady", glishtk_onedim, glishtk_strtoint));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	procs.Insert("anchor", new TkProc("-anchor", glishtk_onestr, glishtk_str));
	}

void TkMessage::Create( ProxyStore *s, Value *args, void * )
	{
	TkMessage *ret;

	if ( args->Length() != 13 )
		InvalidNumberOfArgs(13);

	SETINIT
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
	SETSTR( anchor )
	SETSTR( fill )

	TkAgent *agent = (TkAgent*)(global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret =  new TkMessage( s, (TkFrame*)agent, text, width, justify, font, padx, pady, relief, borderwidth, foreground, background, anchor, fill );
	else
		{
		SETDONE
		global_store->Error("bad parent type");
		return;
		}

	CREATE_RETURN
	}      

STD_EXPAND_PACKINSTRUCTION(TkMessage)
STD_EXPAND_CANEXPAND(TkMessage)

DEFINE_DTOR(TkListbox)

void TkListbox::UnMap()
	{
	if ( self )
		{
		rivet_set( self, "-xscrollcommand", "" );
		rivet_set( self, "-yscrollcommand", "" );
		}

	TkAgent::UnMap();
	}

int listbox_yscrollcb(Rivetobj, XEvent *, ClientData assoc, ClientData calldata)
	{
	double *firstlast = (double*)calldata;
	((TkListbox*)assoc)->yScrolled( firstlast );
	return TCL_OK;
	}

int listbox_xscrollcb(Rivetobj, XEvent *, ClientData assoc, ClientData calldata)
	{
	double *firstlast = (double*)calldata;
	((TkListbox*)assoc)->xScrolled( firstlast );
	return TCL_OK;
	}

int listbox_button1cb(Rivetobj, XEvent *, ClientData assoc, ClientData)
	{
	((TkListbox*)assoc)->elementSelected();
	return TCL_OK;
	}

TkListbox::TkListbox( ProxyStore *s, TkFrame *frame_, int width, int height, charptr mode,
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
		HANDLE_CTOR_ERROR("Rivet creation failed in TkListbox::TkListbox")

	if ( rivet_create_binding(self, 0, "<ButtonRelease-1>", (int (*)()) listbox_button1cb, (ClientData) this, 1, 0) == TCL_ERROR )
		HANDLE_CTOR_ERROR("Rivet binding creation failed in TkListbox::TkListbox")

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("view", new TkProc("", glishtk_scrolled_update));
	procs.Insert("mode", new TkProc("-selectmode", glishtk_onestr, glishtk_str));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("height", new TkProc("-height", glishtk_oneint, glishtk_strtoint));
	procs.Insert("width", new TkProc("-width", glishtk_oneint, glishtk_strtoint));
	procs.Insert("exportselection", new TkProc("-exportselection", glishtk_onebool));
	procs.Insert("see", new TkProc(this, "see", glishtk_oneidx));
	procs.Insert("selection", new TkProc("curselection", glishtk_nostr, glishtk_splitsp_int));
	procs.Insert("select", new TkProc(this, "select", "set", glishtk_listbox_select));
	procs.Insert("clear", new TkProc(this, "select", "clear", glishtk_listbox_select));
	procs.Insert("delete", new TkProc(this, "delete", glishtk_oneortwoidx));
	procs.Insert("insert", new TkProc(this, "insert", glishtk_listbox_insert));
	procs.Insert("get", new TkProc(this, "get", glishtk_listbox_get, glishtk_splitnl));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	procs.Insert("nearest", new TkProc(this, "", glishtk_listbox_nearest, glishtk_strtoint));
	}

void TkListbox::Create( ProxyStore *s, Value *args, void * )
	{
	TkListbox *ret;

	if ( args->Length() != 11 )
		InvalidNumberOfArgs(11);

	SETINIT
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

	TkAgent *agent = (TkAgent*)(global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret =  new TkListbox( s, (TkFrame*)agent, width, height, mode, font, relief, borderwidth, foreground, background, exp, fill );
	else
		{
		SETDONE
		global_store->Error("bad parent type");
		return;
		}

	CREATE_RETURN
	}      

charptr TkListbox::IndexCheck( charptr s )
	{
	if ( s && s[0] == 's' && ! strcmp(s,"start") )
		return "0";
	return s;
	}

void TkListbox::yScrolled( const double *d )
	{
	PostTkEvent( "yscroll", new Value( (double*) d, 2, COPY_ARRAY ) );
	}

void TkListbox::xScrolled( const double *d )
	{
	PostTkEvent( "xscroll", new Value( (double*) d, 2, COPY_ARRAY ) );
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
	static int ret = 0;
	static int setup = 1;

	//
	// There are some *strange* problems with X11R5 + Solaris...
	// after multiple (50+) calls to "have_gui()" (plus other
	// things operations) e.g.:
	//      GO := T
	//      CNT := 0
	//      while (GO) {CNT+:=1;include "tt.g";GO:=have_gui()}
	//      print GO,CNT
	// where "tt.g" is an empty file, "have_gui()" will start
	// returning 'F'. Calling XOpenDisplay(NULL) once is more
	// efficient, and solves this problem, but probably isn't
	// as proper.
	//
	if ( setup )
		{
		if ( (display=XOpenDisplay(NULL)) != NULL )
			{
			ret = 1;
			XCloseDisplay(display);
			}
		setup = 0;
		}

	return ret;
	}

#endif
