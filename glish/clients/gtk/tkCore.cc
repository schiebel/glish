// $Id$
// Copyright (c) 1997,1998 Associated Universities Inc.
//

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "tkCore.h"
#include "tkCanvas.h"

#include <X11/Xlib.h>
#include <string.h>
#include <stdlib.h>
#include "Reporter.h"
#include "Glish/Value.h"
#include "system.h"
#include "comdefs.h"

extern ProxyStore *global_store;
unsigned long TkRadioContainer::count = 0;

unsigned long TkFrame::top_created = 0;
unsigned long TkFrame::tl_count = 0;
unsigned long TkFrame::grab = 0;

char *glishtk_quote_string( charptr str )
	{
	char *ret = (char*) alloc_memory(strlen(str)+3);
	sprintf(ret,"{%s}", str);
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

char *glishtk_nostr( Tcl_Interp *tcl, Tk_Window self, const char *cmd, Value * )
	{
	Tcl_VarEval( tcl, Tk_PathName(self), SP, cmd, 0 );
	return Tcl_GetStringResult(tcl);
	}

char *glishtk_onestr(Tcl_Interp *tcl, Tk_Window self, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Type() == TYPE_STRING )
		{
		const char *str = args->StringPtr(0)[0];
		Tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, " {", str, "}", 0 );
		ret = Tcl_GetStringResult(tcl);
		}
	else
		{
		Tcl_VarEval( tcl, Tk_PathName(self), " cget ", cmd, 0 );
		ret = Tcl_GetStringResult(tcl);
		}

	return ret;
	}

char *glishtk_bitmap(Tcl_Interp *tcl, Tk_Window self, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Type() == TYPE_STRING )
		{
		const char *str = args->StringPtr(0)[0];
		char *bitmap = (char*) alloc_memory(strlen(str)+2);
		sprintf(bitmap," @%s",str);
		Tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, bitmap, 0 );
		free_memory( bitmap );
		}
	else
		{
		Tcl_VarEval( tcl, Tk_PathName(self), " cget ", cmd, 0 );
		ret = Tcl_GetStringResult(tcl);
		if ( *ret == '@' ) ++ret;
		}

	return ret;
	}

char *glishtk_onedim(Tcl_Interp *tcl, Tk_Window self, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Type() == TYPE_STRING )
		Tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, SP, args->StringPtr(0)[0], 0 );
	else if ( args->IsNumeric() && args->Length() > 0 )
		{
		char buf[30];
		sprintf(buf,"%d",args->IntVal());
		Tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, SP, buf, 0 );
		}
	else
		{
		Tcl_VarEval( tcl, Tk_PathName(self), " cget ", cmd, 0 );
		ret = Tcl_GetStringResult(tcl);
		}

	return ret;
	}

char *glishtk_oneint(Tcl_Interp *tcl, Tk_Window self, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Length() > 0 )
		{
		if ( args->IsNumeric() )
			{
			char buf[30];
			sprintf(buf," %d",args->IntVal());
			Tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, buf, 0 );
			}
		else if ( args->Type() == TYPE_STRING )
			Tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, args->StringPtr(0)[0], 0 );
		}
	else
		{
		Tcl_VarEval( tcl, Tk_PathName(self), " cget ", cmd, 0 );
		ret = Tcl_GetStringResult(tcl);
		}

	return ret;
	}

char *glishtk_onedouble(Tcl_Interp *tcl, Tk_Window self, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Length() > 0 )
		{
		if ( args->IsNumeric() )
			{
			char buf[30];
			sprintf(buf," %f",args->DoubleVal());
			Tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, buf, 0 );
			}
		else if ( args->Type() == TYPE_STRING )
			Tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, args->StringPtr(0)[0], 0 );
		}
	else
		{
		Tcl_VarEval( tcl, Tk_PathName(self), " cget ", cmd, 0 );
		ret = Tcl_GetStringResult(tcl);
		}

	return ret;
	}

char *glishtk_onebinary(Tcl_Interp *tcl, Tk_Window self, const char *cmd, const char *ptrue, const char *pfalse,
				Value *args )
	{
	char *ret = 0;

	if ( args->IsNumeric() && args->Length() > 0 )
		Tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, (char*)(args->IntVal() ? ptrue : pfalse), 0 );
	else
		global_store->Error("wrong type, numeric expected");
	
	return ret;
	}

char *glishtk_onebool(Tcl_Interp *tcl, Tk_Window self, const char *cmd, Value *args )
	{
	return glishtk_onebinary(tcl, self, cmd, "true", "false", args);
	}

char *glishtk_oneidx( TkAgent *a, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Type() == TYPE_STRING && args->Length() > 0 )
		Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( args->StringPtr(0)[0] ), 0 );
	else
		global_store->Error("wrong type, string expected");

	return ret;
	}

char *glishtk_disable_cb( TkAgent *a, const char *cmd, Value *args )
	{
	if ( ! *cmd )
		{
		if ( args->IsNumeric() && args->Length() > 0 )
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
		Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( start ), SP, a->IndexCheck( end ), 0 );
		a->ExitEnable();
		EXPR_DONE( end )
		EXPR_DONE( start )
		}
	else if ( args->Type() == TYPE_STRING )
		{
		a->EnterEnable();
		Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( args->StringPtr(0)[0] ), 0 );
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
			int r = Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP,
					     a->IndexCheck( one ), SP, a->IndexCheck( two ), 0 );
			if ( r == TCL_OK )
				ret->ary[ret->len++] = strdup(Tcl_GetStringResult(a->Interp()));
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
				int r = Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP,
						     a->IndexCheck( idx[i] ), SP, a->IndexCheck( idx[i+1] ), 0 );
				if ( r == TCL_OK )
					ret->ary[ret->len++] = strdup(Tcl_GetStringResult(a->Interp()));
				}
			}
		else
			{
			int r = Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP,
					     a->IndexCheck( (args->StringPtr(0))[0] ), 0);
			if ( r == TCL_OK )
				{
				ret = new strary_ret;
			        ret->len = 1;
				ret->ary = (char**) alloc_memory( sizeof(char*) );
				ret->ary[0] = strdup(Tcl_GetStringResult(a->Interp()));
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
		Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, param, SP,
			     a->IndexCheck( start ), SP, a->IndexCheck( end ), 0 );
		Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), " activate ", a->IndexCheck( end ), 0 );
		EXPR_DONE( end )
		EXPR_DONE( start )
		}
	else if ( args->Type() == TYPE_STRING )
		{
		const char *start = args->StringPtr(0)[0];
		Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, param, SP,
			     a->IndexCheck( start ), 0 );
		Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), " activate ", a->IndexCheck( start ), 0 );
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
		Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( where ), " {", str, "}", 0);
		a->ExitEnable();
		EXPR_DONE( where )
		EXPR_DONE( str )
		}
	else if ( args->Type() == TYPE_STRING )
		{
		a->EnterEnable();
		Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( "end" ), " {", args->StringPtr(0)[0], "}", 0 );
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
		argv[argc++] = Tk_PathName(a->Self());
		argv[argc++] = (char*) cmd;
		if ( param ) argv[argc++] = (char*) a->IndexCheck(param);
		int start = argc;
		for ( int i=0; i < args->Length(); ++i )
			{
			EXPRVAL( val, event_name )
			char *s = val->StringVal( ' ', 0, 1 );
			if ( i != 1 || param )
				{
				argv[argc] = (char*) alloc_memory(strlen(s)+3);
				sprintf(argv[argc],"{%s}", s);
				++argc;
				}
			else
				argv[argc++] = strdup(a->IndexCheck(s));
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
		tcl_ArgEval( a->Interp(), argc, argv );
		if ( param ) Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), " see ", a->IndexCheck(param), 0 );
		a->ExitEnable();
		for ( LOOPDECL i = start; i < argc; ++i )
			free_memory(argv[i]);
		free_memory( argv );
		}
	else if ( args->Type() == TYPE_STRING && param )
		{
		char *s = args->StringVal( ' ', 0, 1 );
		Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, param, " {", s, "}", 0 );
		free_memory(s);
		}
	else
		global_store->Error("wrong arguments");

	Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), " see end", 0 );

	return 0;
	}

char *glishtk_text_tagfunc(Tcl_Interp *tcl, Tk_Window self, const char *cmd, const char *param,
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
	argv[argc++] = Tk_PathName(self);
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
			tcl_ArgEval( tcl, argc+2, argv );
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
			tcl_ArgEval( tcl, argc+1, argv );
			}
		else
			for ( int i=0; i+1 < str_v->Length(); i+=2 )
				{
				argv[argc] = (char*)s[i];
				argv[argc+1] = (char*)s[i+1];
				tcl_ArgEval( tcl, argc+2, argv );
				}

		EXPR_DONE(str_v)
		}

	EXPR_DONE(tag)
	return 0;
	}

char *glishtk_text_configfunc(Tcl_Interp *tcl, Tk_Window self, const char *cmd, const char *param, Value *args )
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
	argv[argc++] = Tk_PathName(self);
	argv[argc++] = (char*) cmd;
	argv[argc++] = (char*) param;
	EXPRSTR(tag, event_name)
	argv[argc++] = (char*) tag;

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
			
			if ( doit ) tcl_ArgEval( tcl, argc+2, argv );
			}
		}
	EXPR_DONE(tag)
	return 0;
	}

char *glishtk_text_rangesfunc( Tcl_Interp *tcl, Tk_Window self, const char *cmd, const char *param,
			      Value *args )
	{
	char *ret = 0;

	if ( args->Type() == TYPE_STRING && args->Length() > 0 )
		{
		Tcl_VarEval( tcl, Tk_PathName(self), SP, cmd, SP, param, SP, args->StringPtr(0)[0], 0 );
		ret = Tcl_GetStringResult(tcl);
		}
	else
		global_store->Error("wrong type, string expected");

	return ret;
	}

char *glishtk_no2str(TkAgent *a, const char *cmd, const char *param, Value * )
	{
	Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, param, 0 );
	return Tcl_GetStringResult(a->Interp());
	}

char *glishtk_listbox_insert_action(TkAgent *a, const char *cmd, Value *str_v, charptr where="end" )
	{
	int len = str_v->Length();

	if ( ! len ) return 0;

	char **argv = (char**) alloc_memory( sizeof(char*)*(len+3) );
	charptr *strs = str_v->StringPtr(0);

	argv[0] = Tk_PathName(a->Self());
	argv[1] = (char*) cmd;
	argv[2] = (char*) a->IndexCheck( where );
	int c=0;
	for ( ; c < len; ++c )
		argv[c+3] = (char *) strs[c];
		
	tcl_ArgEval( a->Interp(), c+3, argv );
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
		
		int r = Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( buf ), 0 );

		if ( r == TCL_OK )
			{
			char *v = Tcl_GetStringResult(a->Interp());
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
		int r = Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP,
				     a->IndexCheck( start ), SP, a->IndexCheck( end ), 0 );

		if ( r == TCL_OK ) ret = Tcl_GetStringResult(a->Interp());

		EXPR_DONE( end )
		EXPR_DONE( val )
		}
	else if ( args->Type() == TYPE_STRING )
		{
		int r = Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( args->StringPtr(0)[0] ), 0 );
		if ( r == TCL_OK ) ret = Tcl_GetStringResult(a->Interp());
		}
	else if ( args->Type() == TYPE_INT )
		ret = glishtk_listbox_get_int( a, (char*) cmd, args );
	else
		global_store->Error("invalid argument type");

	return ret;
	}

char *glishtk_listbox_nearest(TkAgent *a, const char *, Value *args )
	{
	char *ret = 0;

	if ( args->IsNumeric() && args->Length() > 0 )
		{
		char ycoord[30];
		sprintf(ycoord,"%d", args->IntVal());
		int r = Tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), " nearest ", ycoord, 0 );
		if ( r == TCL_OK ) ret = Tcl_GetStringResult(a->Interp());
		}
	else
		global_store->Error("wrong type, numeric expected");

	return ret;
	}

char *glishtk_scrolled_update(Tcl_Interp *tcl, Tk_Window self, const char *, Value *val )
	{
	if ( val->Type() != TYPE_STRING || val->Length() != 1 )
		return 0;

	Tcl_VarEval( tcl, Tk_PathName(self), SP, val->StringPtr(0)[0], 0 );
	return 0;
	}

char *glishtk_scrollbar_update(Tcl_Interp *tcl, Tk_Window self, const char *, Value *val )
	{
	if ( val->Type() != TYPE_DOUBLE || val->Length() < 2 )
		{
		global_store->Error("scrollbar update function");
		return 0;
		}

	double *firstlast = val->DoublePtr(0);

	char args[75];
	sprintf( args," set %f %f", firstlast[0], firstlast[1] );
	Tcl_VarEval( tcl, Tk_PathName(self), args, 0 );
	return 0;
	}

char *glishtk_button_state(TkAgent *a, const char *, Value *args )
	{
	char *ret = 0;

	if ( args->IsNumeric() && args->Length() > 0 )
		((TkButton*)a)->State( args->IntVal() ? 1 : 0 );

	ret = ((TkButton*)a)->State( ) ? "T" : "F";
	return ret;
	}

char *glishtk_menu_onestr(TkAgent *a, const char *cmd, Value *args )
	{
	TkButton *Self = (TkButton*)a;
	TkButton *Parent = Self->Parent();
	char *ret = 0;

	if ( args->Type() == TYPE_STRING && args->Length() > 0 )
		Tcl_VarEval( a->Interp(), Tk_PathName(Parent->Menu()), " entryconfigure ", Self->Index(), SP,
			     cmd, " {", args->StringPtr(0)[0], "}", 0 );
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

	if ( args->IsNumeric() && args->Length() > 0 )
		Tcl_VarEval( a->Interp(), Tk_PathName(Parent->Menu()), " entryconfigure ", Self->Index(), SP,
			     cmd, SP, (args->IntVal() ? ptrue : pfalse), 0 );
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

int glishtk_bindcb( ClientData data, Tcl_Interp *, int, char *argv[] )
	{
	glishtk_bindinfo *info = (glishtk_bindinfo*) data;
	recordptr rec = create_record_dict();

	int *dpt = (int*) alloc_memory( sizeof(int)*2 );
	dpt[0] = atoi(argv[1]);
	dpt[1] = atoi(argv[2]);
	rec->Insert( strdup("device"), new Value( dpt, 2 ) );
	rec->Insert( strdup("code"), new Value(atoi(argv[3])) );
	if ( argv[4][0] != '?' )
		{
		// KeyPress/Release event
		rec->Insert( strdup("sym"), new Value(argv[4]) );
		rec->Insert( strdup("key"), new Value(argv[5]) );
		}

	info->agent->BindEvent( info->event_name, new Value( rec ) );
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
		glishtk_bindinfo *binfo = new glishtk_bindinfo(agent, event, button);

		Tcl_VarEval( agent->Interp(), "bind ", Tk_PathName(agent->Self()), SP, button, " {",
			     glishtk_make_callback(agent->Interp(), glishtk_bindcb, binfo), " %x %y %b %K %A}", 0 );

		EXPR_DONE( event )
		EXPR_DONE( button )
		}

	return 0;
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
			Tcl_VarEval( tcl, Tk_PathName(Menu()), " delete ", Index(), 0 );
			}
		else
			{
			Tk_DestroyWindow( self );
			}
		}

	else if ( menu )
		{
		menu->Remove(this);
		Tcl_VarEval( tcl, Tk_PathName(Menu()), " delete ", Index(), 0 );
		}

	else if ( self )
		{
		Tcl_VarEval( tcl, Tk_PathName(self), " config -command \"\"", 0 );
		Tk_DestroyWindow( self );
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

int buttoncb( ClientData data, Tcl_Interp *, int, char *[] )
	{
	((TkButton*)data)->ButtonPressed();
	return TCL_OK;
	}

void TkButton::EnterEnable()
	{
	if ( ! enable_state && disable_count )
		{
		enable_state++;
		if ( frame )
			Tcl_VarEval( tcl, Tk_PathName(self), " config -state normal", 0 );
		else
			Tcl_VarEval( tcl, Tk_PathName(Parent()->Menu()), " entryconfigure ", Index(), " -state normal", 0 );
		}
	}

void TkButton::ExitEnable()
	{
	if ( enable_state && --enable_state == 0 )
		if ( frame )
			Tcl_VarEval( tcl, Tk_PathName(self), " config -state disabled", 0 );
		else
			Tcl_VarEval( tcl, Tk_PathName(Parent()->Menu()), " entryconfigure ", Index(), " -state disabled", 0 );
	}

void TkButton::Disable( )
	{
	disable_count++;
	if ( frame )
		Tcl_VarEval( tcl, Tk_PathName(self), " config -state disabled", 0 );
	else
		Tcl_VarEval( tcl, Tk_PathName(Parent()->Menu()), " entryconfigure ", Index(), " -state disabled", 0 );
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
		Tcl_VarEval( tcl, Tk_PathName(self), " config -state normal", 0 );
	else
		Tcl_VarEval( tcl, Tk_PathName(Parent()->Menu()), " entryconfigure ", Index(), " -state normal", 0 );
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

	int c = 0;
	argv[c++] = 0;
	argv[c++] = (char*) NewName(frame->Self());

	if ( type == RADIO )
		{
		sprintf(var_name,"%s%lx",type_,radio->Id());
		argv[c++] = "-variable";
		argv[c++] = var_name;
		sprintf(val_name,"BVaLuE%lx",Id());
		argv[c++] = "-value";
		argv[c++] = val_name;
		}

	if ( type == CHECK )
		{
		sprintf(var_name,"%s%lx",type_,Id());
		argv[c++] = "-variable";
		argv[c++] = var_name;
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
		argv[c++] = glishtk_quote_string(label);
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
		argv[c++] = glishtk_make_callback( tcl, buttoncb, this );
		}

	switch ( type )
		{
		case RADIO:
			argv[0] = "radiobutton";
			tcl_ArgEval( tcl, c, argv );
			self = Tk_NameToWindow( tcl, argv[1], root );
			break;
		case CHECK:
			argv[0] = "checkbutton";
			tcl_ArgEval( tcl, c, argv );
			self = Tk_NameToWindow( tcl, argv[1], root );
			break;
		case MENU:
			argv[0] = "menubutton";
			tcl_ArgEval( tcl, c, argv );
			self = Tk_NameToWindow( tcl, argv[1], root );
			if ( ! self )
				HANDLE_CTOR_ERROR("Rivet creation failed in TkButton::TkButton")
			argv[0] = "menu";
			argv[1] = (char*) NewName(self);
			argv[2] = "-tearoff";
			argv[3] = "0";
			tcl_ArgEval( tcl, 4, argv );
			menu_base = Tk_NameToWindow( tcl, argv[1], root );
			if ( ! menu_base )
				HANDLE_CTOR_ERROR("Rivet creation failed in TkButton::TkButton")
			Tcl_VarEval( tcl, Tk_PathName(self), " config -menu ", Tk_PathName(menu_base), 0 );
			break;
		default:
			argv[0] = "button";
			tcl_ArgEval( tcl, c, argv );
			self = Tk_NameToWindow( tcl, argv[1], root );
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

	procs.Insert("anchor", new TkProc("-anchor", glishtk_onestr, glishtk_str));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	procs.Insert("bitmap", new TkProc("-bitmap", glishtk_bitmap, glishtk_str));
	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("disabled", new TkProc(this, "", glishtk_disable_cb));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("height", new TkProc("-height", glishtk_onedim, glishtk_strtoint));
	procs.Insert("justify", new TkProc("-justify", glishtk_onestr, glishtk_str));
	procs.Insert("padx", new TkProc("-padx", glishtk_onedim, glishtk_strtoint));
	procs.Insert("pady", new TkProc("-pady", glishtk_onedim, glishtk_strtoint));
	procs.Insert("state", new TkProc(this, "", glishtk_button_state, glishtk_strtobool));
	procs.Insert("text", new TkProc("-text", glishtk_onestr, glishtk_str));
	procs.Insert("width", new TkProc("-width", glishtk_onedim, glishtk_strtoint));
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
	argv[0] = 0;		// not available yet for cascaded menues
	argv[1] = "add";
	argv[2] = 0;

	if ( type == RADIO )
		{
		sprintf(var_name,"%s%lx",type_,radio->Id());
		argv[c++] = "-variable";
		argv[c++] = var_name;
		sprintf(val_name,"BVaLuE%lx",Id());
		argv[c++] = "-value";
		argv[c++] = val_name;
		}

	if ( type == CHECK )
		{
		sprintf(var_name,"%s%lx",type_,Id());
		argv[c++] = "-variable";
		argv[c++] = var_name;
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
		argv[c++] = glishtk_quote_string(label);
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
	argv[c++] = glishtk_make_callback( tcl, buttoncb, this );

	switch ( type )
		{
		case RADIO:
			argv[0] = Tk_PathName(Menu());
			argv[2] = "radio";
			tcl_ArgEval( tcl, c, argv);
			self = Menu();
			break;
		case CHECK:
			argv[0] = Tk_PathName(Menu());
			argv[2] = "check";
			tcl_ArgEval( tcl, c, argv);
			self = Menu();
			break;
		case MENU:
			{
			char *av[10];
			av[0] = "menu";
			av[1] = (char*) NewName(menu->Menu());
			av[2] = "-tearoff";
			av[3] = "0";
			tcl_ArgEval( tcl, 4, av );
			self = menu_base = Tk_NameToWindow( tcl, av[1], root );
			if ( ! menu_base )
				HANDLE_CTOR_ERROR("Rivet creation failed in TkButton::TkButton")
			argv[0] = Tk_PathName(menu->Menu());
			argv[2] = "cascade";
			argv[c++] = "-menu";
			argv[c++] = Tk_PathName(self);
			tcl_ArgEval( tcl, c, argv );
			}
			break;
		default:
			argv[0] = Tk_PathName(Menu());
			argv[2] = "command";
			tcl_ArgEval( tcl, c, argv);
			self = Menu();
			break;
		}

	if ( bitmap ) free_memory(bitmap);

	value = val ? copy_value( val ) : 0;
#ifdef GGC
	if ( value ) sequencer->RegisterValue( value );
#endif

	Tcl_VarEval( tcl, Tk_PathName(menu->Menu()), " index last", 0 );
	menu_index = strdup( Tcl_GetStringResult(tcl) );

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

void TkButton::Create( ProxyStore *s, Value *args )
	{
	TkButton *ret=0;

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
		Tcl_SetVar( tcl, var_name, "", TCL_GLOBAL_ONLY );
		}
	else
		{
		EnterEnable();
		dont_invoke_button = 1;
		if ( frame )
			Tcl_VarEval( tcl, Tk_PathName(self), " invoke", 0 );
		else if ( menu )
			Tcl_VarEval( tcl, Tk_PathName(Menu()), " invoke ", Index(), 0 );
		dont_invoke_button = 0;
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

Tk_Window TkButton::TopLevel( )
	{
	return frame ? frame->TopLevel() : menu ? menu->TopLevel() : 0;
	}

DEFINE_DTOR(TkScale)

void TkScale::UnMap()
	{
	if ( self ) Tcl_VarEval( tcl, Tk_PathName(self), " -command \"\"", 0 );
	TkAgent::UnMap();
	}

unsigned int TkScale::scale_count = 0;
int scalecb( ClientData data, Tcl_Interp *, int, char *argv[] )
	{
	((TkScale*)data)->ValueSet( atof(argv[1]) );
	return TCL_OK;
	}

char *glishtk_scale_value(TkAgent *a, const char *, Value *args )
	{

	if ( args->IsNumeric() && args->Length() > 0 )
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
	argv[0] = "scale";
	argv[1] = (char*) NewName(frame->Self());
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
	if ( text && *text )
		{
		argv[c++] = "-label";
		argv[c++] = glishtk_quote_string(text);
		}
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

	tcl_ArgEval( tcl, c, argv );
	self = Tk_NameToWindow( tcl, argv[1], root );

	// Can't set command as part of initialization...
	Tcl_VarEval( tcl, Tk_PathName(self), " config -command ", glishtk_make_callback( tcl, scalecb, this ), 0 );

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkScale::TkScale")

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);
	else if ( fill_ && ! fill_[0] )
		fill = (orient && ! strcmp(orient,"vertical")) ? strdup("y") :
				(orient && ! strcmp(orient,"horizontal")) ? strdup("x") : 0;

	frame->AddElement( this );
	frame->Pack();

	if ( value < from || value > to ) value = from;
	SetValue(value);

	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	procs.Insert("end", new TkProc("-to", glishtk_onedouble, glishtk_strtofloat));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("length", new TkProc("-length", glishtk_onedim, glishtk_strtoint));
	procs.Insert("orient", new TkProc("-orient", glishtk_onestr, glishtk_str));
	procs.Insert("resolution", new TkProc("-resolution", glishtk_onedouble));
	procs.Insert("start", new TkProc("-from", glishtk_onedouble, glishtk_strtofloat));
	procs.Insert("text", new TkProc("-label", glishtk_onestr, glishtk_str));
	procs.Insert("value", new TkProc(this, "", glishtk_scale_value));
	procs.Insert("width", new TkProc("-width", glishtk_onedim, glishtk_strtoint));
	}

void TkScale::ValueSet( double d )
	{
	if ( ! discard_event )
		PostTkEvent( "value", new Value( d ) );
	discard_event = 0;
	}

void TkScale::SetValue( double d )
	{
	if ( d >= from_ && d <= to_ )
		{
		char val[256];
		sprintf(val,"%g",d);
		Tcl_VarEval( tcl, Tk_PathName(self), " set ", val, 0 );
		}
	}

void TkScale::Create( ProxyStore *s, Value *args )
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
		Tcl_VarEval( tcl, Tk_PathName(self), " config -xscrollcommand \"\"", 0 );
		Tcl_VarEval( tcl, Tk_PathName(self), " config -yscrollcommand \"\"", 0 );
		}

	TkAgent::UnMap();
	}

int text_yscrollcb( ClientData data, Tcl_Interp *, int, char *argv[] )
	{
	double firstlast[2];
	firstlast[0] = atof(argv[1]);
	firstlast[1] = atof(argv[2]);
	((TkText*)data)->yScrolled( firstlast );
	return TCL_OK;
	}

int text_xscrollcb( ClientData data, Tcl_Interp *, int, char *argv[] )
	{
	double firstlast[2];
	firstlast[0] = atof(argv[1]);
	firstlast[1] = atof(argv[2]);
	((TkText*)data)->xScrolled( firstlast );
	return TCL_OK;
	}

#define DEFINE_ENABLE_FUNCS(CLASS)				\
void CLASS::EnterEnable()					\
	{							\
	if ( ! enable_state ) 					\
		{						\
		Tcl_VarEval( tcl, Tk_PathName(self), " cget -state", 0 ); \
		const char *curstate = Tcl_GetStringResult(tcl); \
		if ( ! strcmp("disabled", curstate ) )		\
			{					\
			enable_state++;				\
			Tcl_VarEval( tcl, Tk_PathName(self),	\
				     " config -state normal", 0 ); \
			}					\
		}						\
	}							\
								\
void CLASS::ExitEnable()					\
	{							\
	if ( enable_state && --enable_state == 0 )		\
		Tcl_VarEval( tcl, Tk_PathName(self),		\
			     " config -state disabled", 0 );	\
	}							\
								\
void CLASS::Disable( )						\
	{							\
	disable_count++;					\
	Tcl_VarEval( tcl, Tk_PathName(self),			\
		     " config -state disabled", 0 );		\
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
	Tcl_VarEval( tcl, Tk_PathName(self),			\
		     " config -state disabled", 0 );		\
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

	const char *nme = NewName(frame->Self());
	Tcl_VarEval( tcl, "text ", nme, 0 );
	self = Tk_NameToWindow( tcl, (char*) nme, root );

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkText::TkText")

	char width_[30];
	char height_[30];
	sprintf(width_,"%d",width);
	sprintf(height_,"%d",height);

	int c = 0;
	argv[c++] = Tk_PathName(self);
	argv[c++] = "config";
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

	char ys[100];
	argv[c++] = "-yscrollcommand";
	argv[c++] = glishtk_make_callback( tcl, text_yscrollcb, this, ys );
	argv[c++] = "-xscrollcommand";
	argv[c++] = glishtk_make_callback( tcl, text_xscrollcb, this );

	tcl_ArgEval( tcl, c, argv );

	if ( text[0] )
		Tcl_VarEval( tcl, Tk_PathName(self), " insert end ", text, 0 );

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("addtag", new TkProc("tag", "add", glishtk_text_tagfunc));
	procs.Insert("append", new TkProc(this, "insert", "end", glishtk_text_append));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	procs.Insert("config", new TkProc("tag", "configure", glishtk_text_configfunc));
	procs.Insert("delete", new TkProc(this, "delete", glishtk_oneortwoidx));
	procs.Insert("deltag", new TkProc("tag", "delete", glishtk_text_rangesfunc));
	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("disabled", new TkProc(this, "", glishtk_disable_cb));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("get", new TkProc(this, "get", glishtk_oneortwoidx_strary, glishtk_strary_to_value));
	procs.Insert("height", new TkProc("-height", glishtk_onedim, glishtk_strtoint));
	procs.Insert("insert", new TkProc(this, "insert", 0, glishtk_text_append));
	procs.Insert("prepend", new TkProc(this, "insert", "start", glishtk_text_append));
	procs.Insert("ranges", new TkProc("tag", "ranges", glishtk_text_rangesfunc, glishtk_splitsp_str));
	procs.Insert("see", new TkProc(this, "see", glishtk_oneidx));
	procs.Insert("view", new TkProc("", glishtk_scrolled_update));
	procs.Insert("width", new TkProc("-width", glishtk_onedim, glishtk_strtoint));
	procs.Insert("wrap", new TkProc("-wrap", glishtk_onestr, glishtk_str));
	}

void TkText::Create( ProxyStore *s, Value *args )
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
	if ( self ) Tcl_VarEval( tcl, Tk_PathName(self), " -command \"\"", 0 );
	TkAgent::UnMap();
	}

int scrollbarcb( ClientData data, Tcl_Interp *tcl, int argc, char *argv[] )
	{
	char buf[256];
	int vert = 0;
	Tcl_VarEval( tcl, Tk_PathName(((TkScrollbar*)data)->Self()), " cget -orient", 0 );
	charptr res = Tcl_GetStringResult(tcl);
	if ( *res == 'v' ) vert = 1;

	if ( argc == 4 )
		if ( vert )
			sprintf( buf, "yview %s %s %s", argv[1], argv[2], argv[3] );
		else
			sprintf( buf, "xview %s %s %s", argv[1], argv[2], argv[3] );
	else if ( argc == 3 )
		if ( vert )
			sprintf( buf, "yview %s %s", argv[1], argv[2] );
		else
			sprintf( buf, "xview %s %s", argv[1], argv[2] );
	else
		if ( vert )
			sprintf( buf, "yview moveto 0.001" );
		else
			sprintf( buf, "xview moveto 0.001" );

	((TkScrollbar*)data)->Scrolled( new Value( buf ));
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

	int c = 0;
	argv[c++] = "scrollbar";
	argv[c++] = (char*) NewName(frame->Self());
	argv[c++] = "-orient";
	argv[c++] = (char*) orient;
	argv[c++] = "-width";
	argv[c++] = width_;
	argv[c++] = "-command";
	argv[c++] = glishtk_make_callback( tcl, scrollbarcb, this );

	tcl_ArgEval( tcl, c, argv );
	self = Tk_NameToWindow( tcl, argv[1], root );
	
	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkScrollbar::TkScrollbar")

	// Setting foreground and background colors at creation
	// time kills the goose
	Tcl_VarEval( tcl, Tk_PathName(self), " -bg ", background, 0 );
	Tcl_VarEval( tcl, Tk_PathName(self), " -fg ", foreground, 0 );

	frame->AddElement( this );
	frame->Pack();

// 	rivet_scrollbar_set( self, 0.0, 1.0 );

	procs.Insert("view", new TkProc("", glishtk_scrollbar_update));
	procs.Insert("orient", new TkProc("-orient", glishtk_onestr, glishtk_str));
	procs.Insert("width", new TkProc("-width", glishtk_onedim, glishtk_strtoint));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	}

const char **TkScrollbar::PackInstruction()
	{
	static char *ret[7];
	ret[0] = "-fill";
	Tcl_VarEval( tcl, Tk_PathName(self), " cget -orient", 0 );
	char *orient = Tcl_GetStringResult(tcl);
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

void TkScrollbar::Create( ProxyStore *s, Value *args )
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

	int c = 0;
	argv[c++] = "label";
	argv[c++] = (char*) NewName(frame->Self());
	argv[c++] = "-text";
	argv[c++] = glishtk_quote_string(text);
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

	tcl_ArgEval( tcl, c, argv );
	self = Tk_NameToWindow( tcl, argv[1], root );

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkLabel::TkLabel")

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("anchor", new TkProc("-anchor", glishtk_onestr, glishtk_str));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("justify", new TkProc("-justify", glishtk_onestr, glishtk_str));
	procs.Insert("padx", new TkProc("-padx", glishtk_onedim, glishtk_strtoint));
	procs.Insert("pady", new TkProc("-pady", glishtk_onedim, glishtk_strtoint));
	procs.Insert("text", new TkProc("-text", glishtk_onestr, glishtk_str));
	procs.Insert("width", new TkProc("-width", glishtk_oneint, glishtk_strtoint));

//	procs.Insert("height", new TkProc("-height", glishtk_oneint, glishtk_strtoint));
	}

void TkLabel::Create( ProxyStore *s, Value *args )
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
	if ( self ) Tcl_VarEval( tcl, Tk_PathName(self), " config -xscrollcommand \"\"", 0 );
	TkAgent::UnMap();
	}

int entry_returncb( ClientData data, Tcl_Interp *, int, char *[] )
	{
	((TkEntry*)data)->ReturnHit();
	return TCL_OK;
	}

int entry_xscrollcb( ClientData data, Tcl_Interp *, int, char *argv[] )
	{
	double firstlast[2];
	firstlast[0] = atof(argv[1]);
	firstlast[1] = atof(argv[2]);
	((TkEntry*)data)->xScrolled( firstlast );
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

	int c = 0;
	argv[c++] = "entry";
	argv[c++] =  (char*) NewName(frame->Self());
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
	argv[c++] = glishtk_make_callback( tcl, entry_xscrollcb, this );

	tcl_ArgEval( tcl, c, argv );
	self = Tk_NameToWindow( tcl, argv[1], root );

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkEntry::TkEntry")

	Tcl_VarEval( tcl, "bind ", Tk_PathName(self), " <Return> ", glishtk_make_callback( tcl, entry_returncb, this ), 0 );

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	procs.Insert("delete", new TkProc(this, "delete", glishtk_oneortwoidx));
	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("disabled", new TkProc(this, "", glishtk_disable_cb));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));
	procs.Insert("exportselection", new TkProc("-exportselection", glishtk_onebool));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("get", new TkProc("get", glishtk_nostr, glishtk_splitnl));
	procs.Insert("insert", new TkProc(this, "insert", glishtk_strandidx));
	procs.Insert("justify", new TkProc("-justify", glishtk_onestr, glishtk_str));
	procs.Insert("show", new TkProc("-show", glishtk_onebool));
	procs.Insert("view", new TkProc("", glishtk_scrolled_update));
	procs.Insert("width", new TkProc("-width", glishtk_oneint, glishtk_strtoint));
	}

void TkEntry::ReturnHit( )
	{
	Tcl_VarEval( tcl, Tk_PathName(self), " cget -state", 0 );
	const char *curstate = Tcl_GetStringResult(tcl);
	if ( strcmp("disabled", curstate) )
		{
		Tcl_VarEval( tcl, Tk_PathName(self), " get", 0 );
		Value *ret = new Value( Tcl_GetStringResult(tcl) );
		PostTkEvent( "return", ret );
		}
	}

void TkEntry::xScrolled( const double *d )
	{
	PostTkEvent( "xscroll", new Value( (double*) d, 2, COPY_ARRAY ) );
	}

void TkEntry::Create( ProxyStore *s, Value *args )
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
	argv[0] = "message";
	argv[1] = (char*) NewName( frame->Self() );
	argv[c++] = "-text";
	argv[c++] = glishtk_quote_string(text);
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

	tcl_ArgEval( tcl, c, argv );
	self = Tk_NameToWindow( tcl, argv[1], root );

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkMessage::TkMessage")

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("anchor", new TkProc("-anchor", glishtk_onestr, glishtk_str));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("justify", new TkProc("-justify", glishtk_onestr, glishtk_str));
	procs.Insert("padx", new TkProc("-padx", glishtk_onedim, glishtk_strtoint));
	procs.Insert("pady", new TkProc("-pady", glishtk_onedim, glishtk_strtoint));
	procs.Insert("text", new TkProc("-text", glishtk_onestr, glishtk_str));
	procs.Insert("width", new TkProc("-width", glishtk_onedim, glishtk_strtoint));
	}

void TkMessage::Create( ProxyStore *s, Value *args )
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
		Tcl_VarEval( tcl, Tk_PathName(self), " config -xscrollcommand \"\"", 0 );
		Tcl_VarEval( tcl, Tk_PathName(self), " config -yscrollcommand \"\"", 0 );
		}

	TkAgent::UnMap();
	}

int listbox_yscrollcb( ClientData data, Tcl_Interp *, int, char *argv[] )
	{
	double firstlast[2];
	firstlast[0] = atof(argv[1]);
	firstlast[1] = atof(argv[2]);
	((TkListbox*)data)->yScrolled( firstlast );
	return TCL_OK;
	}

int listbox_xscrollcb( ClientData data, Tcl_Interp *, int, char *argv[] )
	{
	double firstlast[2];
	firstlast[0] = atof(argv[1]);
	firstlast[1] = atof(argv[2]);
	((TkText*)data)->xScrolled( firstlast );
	return TCL_OK;
	}

int listbox_button1cb( ClientData data, Tcl_Interp*, int, char *[] )
	{
	((TkListbox*)data)->elementSelected();
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

	int c = 0;
	argv[c++] = "listbox";
	argv[c++] = (char*) NewName(frame->Self());
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

	char ys[100];
	argv[c++] = "-yscrollcommand";
	argv[c++] = glishtk_make_callback( tcl, listbox_yscrollcb, this, ys );
	argv[c++] = "-xscrollcommand";
	argv[c++] = glishtk_make_callback( tcl, listbox_xscrollcb, this );

	tcl_ArgEval( tcl, c, argv );
	self = Tk_NameToWindow( tcl, argv[1], root );

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkListbox::TkListbox")

	Tcl_VarEval( tcl, "bind ", Tk_PathName(self), " <ButtonRelease-1> ", glishtk_make_callback( tcl, listbox_button1cb, this ), 0 );

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("bind", new TkProc(this, "", glishtk_bind));
	procs.Insert("clear", new TkProc(this, "select", "clear", glishtk_listbox_select));
	procs.Insert("delete", new TkProc(this, "delete", glishtk_oneortwoidx));
	procs.Insert("exportselection", new TkProc("-exportselection", glishtk_onebool));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("get", new TkProc(this, "get", glishtk_listbox_get, glishtk_splitnl));
	procs.Insert("height", new TkProc("-height", glishtk_oneint, glishtk_strtoint));
	procs.Insert("insert", new TkProc(this, "insert", glishtk_listbox_insert));
	procs.Insert("nearest", new TkProc(this, "", glishtk_listbox_nearest, glishtk_strtoint));
	procs.Insert("mode", new TkProc("-selectmode", glishtk_onestr, glishtk_str));
	procs.Insert("see", new TkProc(this, "see", glishtk_oneidx));
	procs.Insert("select", new TkProc(this, "select", "set", glishtk_listbox_select));
	procs.Insert("selection", new TkProc("curselection", glishtk_nostr, glishtk_splitsp_int));
	procs.Insert("view", new TkProc("", glishtk_scrolled_update));
	procs.Insert("width", new TkProc("-width", glishtk_oneint, glishtk_strtoint));
	}

void TkListbox::Create( ProxyStore *s, Value *args )
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
	Tcl_VarEval( tcl, Tk_PathName(self), " curselection", 0 );
	PostTkEvent( "select", glishtk_splitsp_int(Tcl_GetStringResult(tcl)) );
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

void GlishTk_Register( const char *str, WidgetCtor ctor )
	{
	if ( global_store )
		global_store->Register( str, ctor );
	}
