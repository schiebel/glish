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
#include <ctype.h>
#include "Glish/Value.h"
#include "system.h"
#include "comdefs.h"

extern Value *glishtk_valcast( char * );

extern ProxyStore *global_store;

unsigned long TkFrameP::grab = 0;

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
	tcl_VarEval( tcl, Tk_PathName(self), SP, cmd, (char *)NULL );
	return Tcl_GetStringResult(tcl);
	}

char *glishtk_onestr(Tcl_Interp *tcl, Tk_Window self, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Type() == TYPE_STRING )
		{
		const char *str = args->StringPtr(0)[0];
		tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, " {", str, "}", (char *)NULL );
		ret = Tcl_GetStringResult(tcl);
		}
	else
		{
		tcl_VarEval( tcl, Tk_PathName(self), " cget ", cmd, (char *)NULL );
		ret = Tcl_GetStringResult(tcl);
		}

	return ret;
	}

char *glishtk_bitmap( TkProxy *a, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Type() == TYPE_STRING && args->Length() > 0 )
		{
		const char *str = args->StringPtr(0)[0];
		char *expanded = a->which_bitmap(str);
		if ( expanded )
			{
			char *bitmap = (char*) alloc_memory(strlen(expanded)+3);
			sprintf(bitmap," @%s",expanded);
			tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), " config ", cmd, bitmap, (char *)NULL );
			free_memory( expanded );
			free_memory( bitmap );
			}
		}
	else
		{
		tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), " cget ", cmd, (char *)NULL );
		ret = Tcl_GetStringResult(a->Interp());
		if ( *ret == '@' ) ++ret;
		}

	return ret;
	}

char *glishtk_onedim(Tcl_Interp *tcl, Tk_Window self, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Type() == TYPE_STRING )
		tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, SP, args->StringPtr(0)[0], (char *)NULL );
	else if ( args->IsNumeric() && args->Length() > 0 )
		{
		char buf[30];
		sprintf(buf,"%d",args->IntVal());
		tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, SP, buf, (char *)NULL );
		}
	else
		{
		tcl_VarEval( tcl, Tk_PathName(self), " cget ", cmd, (char *)NULL );
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
			tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, buf, (char *)NULL );
			}
		else if ( args->Type() == TYPE_STRING )
			tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, args->StringPtr(0)[0], (char *)NULL );
		}
	else
		{
		tcl_VarEval( tcl, Tk_PathName(self), " cget ", cmd, (char *)NULL );
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
			tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, buf, (char *)NULL );
			}
		else if ( args->Type() == TYPE_STRING )
			tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, args->StringPtr(0)[0], (char *)NULL );
		}
	else
		{
		tcl_VarEval( tcl, Tk_PathName(self), " cget ", cmd, (char *)NULL );
		ret = Tcl_GetStringResult(tcl);
		}

	return ret;
	}

char *glishtk_onebinary(Tcl_Interp *tcl, Tk_Window self, const char *cmd, const char *ptrue, const char *pfalse,
				Value *args )
	{
	char *ret = 0;

	if ( args->IsNumeric() && args->Length() > 0 )
		tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, " ", (char*)(args->IntVal() ? ptrue : pfalse), (char *)NULL );
	else
		global_store->Error("wrong type, numeric expected");
	
	return ret;
	}

char *glishtk_onebool(Tcl_Interp *tcl, Tk_Window self, const char *cmd, Value *args )
	{
	return glishtk_onebinary(tcl, self, cmd, "true", "false", args);
	}

char *glishtk_oneidx( TkProxy *a, const char *cmd, Value *args )
	{
	char *ret = 0;

	if ( args->Type() == TYPE_STRING && args->Length() > 0 )
		tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( args->StringPtr(0)[0] ), (char *)NULL );
	else
		global_store->Error("wrong type, string expected");

	return ret;
	}

char *glishtk_disable_cb( TkProxy *a, const char *cmd, Value *args )
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

char *glishtk_oneortwoidx(TkProxy *a, const char *cmd, Value *args )
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
		tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( start ), SP, a->IndexCheck( end ), (char *)NULL );
		a->ExitEnable();
		EXPR_DONE( end )
		EXPR_DONE( start )
		}
	else if ( args->Type() == TYPE_STRING )
		{
		a->EnterEnable();
		tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( args->StringPtr(0)[0] ), (char *)NULL );
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
char *glishtk_oneortwoidx_strary(TkProxy *a, const char *cmd, Value *args )
	{
	char *event_name = "one-or-two index array function";

	strary_ret *ret = 0;

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_RECORD )
		{
		HASARG( args, >= 2 )
		EXPRINIT( event_name )

		ret = new strary_ret;
		ret->ary = (char**) alloc_memory( sizeof(char*)*((int)(args->Length()/2)) );
		ret->len = 0;
		for ( int i=0; i+1 < args->Length(); i+=2 )
			{
			EXPRSTR(one, event_name)
			EXPRSTR(two, event_name)
			int r = tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP,
					     a->IndexCheck( one ), SP, a->IndexCheck( two ), (char *)NULL );
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
				int r = tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP,
						     a->IndexCheck( idx[i] ), SP, a->IndexCheck( idx[i+1] ), (char *)NULL );
				if ( r == TCL_OK )
					ret->ary[ret->len++] = strdup(Tcl_GetStringResult(a->Interp()));
				}
			}
		else
			{
			int r = tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP,
					     a->IndexCheck( (args->StringPtr(0))[0] ), (char *)NULL);
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

char *glishtk_listbox_select(TkProxy *a, const char *cmd, const char *param,
			      Value *args )
	{
	char *ret = 0;
	char *event_name = "listbox select function";

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_RECORD )
		{
		HASARG( args, > 1 )
		EXPRINIT( event_name)
		EXPRSTR( start, event_name )
		EXPRSTR( end, event_name )
		tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, param, SP,
			     a->IndexCheck( start ), SP, a->IndexCheck( end ), (char *)NULL );
		tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), " activate ", a->IndexCheck( end ), (char *)NULL );
		EXPR_DONE( end )
		EXPR_DONE( start )
		}
	else if ( args->Type() == TYPE_STRING )
		{
		const char *start = args->StringPtr(0)[0];
		tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, param, SP,
			     a->IndexCheck( start ), (char *)NULL );
		tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), " activate ", a->IndexCheck( start ), (char *)NULL );
		}

	return ret;
	}

char *glishtk_strandidx(TkProxy *a, const char *cmd, Value *args )
	{
	char *ret = 0;
	char *event_name = "string-and-index function";

	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	else if ( args->Type() == TYPE_RECORD )
		{
		HASARG( args, > 1 )
		EXPRINIT( event_name)
		EXPRVAL( strv, event_name );
		EXPRSTR( where, event_name )
		char *str = strv->StringVal( ' ', 0, 1 );
		a->EnterEnable();
		tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( where ), " {", str, "}", (char *)NULL);
		a->ExitEnable();
		free_memory( str );
		EXPR_DONE( where )
		EXPR_DONE( strv )
		}
	else if ( args->Type() == TYPE_STRING )
		{
		a->EnterEnable();
		tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( "end" ), " {", args->StringPtr(0)[0], "}", (char *)NULL );
		a->ExitEnable();
		}
	else
		global_store->Error("wrong type, string expected");

	return ret;
	}

char *glishtk_text_append(TkProxy *a, const char *cmd, const char *param,
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
		if ( param ) tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), " see ", a->IndexCheck(param), (char *)NULL );
		a->ExitEnable();
		for ( LOOPDECL i = start; i < argc; ++i )
			free_memory(argv[i]);
		free_memory( argv );
		}
	else if ( args->Type() == TYPE_STRING && param )
		{
		char *s = args->StringVal( ' ', 0, 1 );
		const char *p = a->IndexCheck(param);
		tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, p, " {", s, "}", (char *)NULL );
		tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), " see ", p, (char *)NULL );
		free_memory(s);
		}
	else
		global_store->Error("wrong arguments");

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
		tcl_VarEval( tcl, Tk_PathName(self), SP, cmd, SP, param, SP, args->StringPtr(0)[0], (char *)NULL );
		ret = Tcl_GetStringResult(tcl);
		}
	else
		global_store->Error("wrong type, string expected");

	return ret;
	}

char *glishtk_no2str(TkProxy *a, const char *cmd, const char *param, Value * )
	{
	tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, param, (char *)NULL );
	return Tcl_GetStringResult(a->Interp());
	}

char *glishtk_listbox_insert_action(TkProxy *a, const char *cmd, Value *str_v, charptr where="end" )
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
		argv[c+3] = glishtk_quote_string(strs[c]);

	tcl_ArgEval( a->Interp(), c+3, argv );
	for ( c=0; c < len; ++c ) free_memory(argv[c+3]);
	free_memory( argv );
	return "";
	}

char *glishtk_listbox_insert(TkProxy *a, const char *cmd, Value *args )
	{
	char *event_name = "listbox insert function";

	if ( args->Length() <= 0 )
		return "";
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

char *glishtk_listbox_get_int(TkProxy *a, const char *cmd, Value *val )
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
		
		int r = tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( buf ), (char *)NULL );

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

char *glishtk_listbox_get(TkProxy *a, const char *cmd, Value *args )
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
		int r = tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP,
				     a->IndexCheck( start ), SP, a->IndexCheck( end ), (char *)NULL );

		if ( r == TCL_OK ) ret = Tcl_GetStringResult(a->Interp());

		EXPR_DONE( end )
		EXPR_DONE( val )
		}
	else if ( args->Type() == TYPE_STRING )
		{
		int r = tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), SP, cmd, SP, a->IndexCheck( args->StringPtr(0)[0] ), (char *)NULL );
		if ( r == TCL_OK ) ret = Tcl_GetStringResult(a->Interp());
		}
	else if ( args->Type() == TYPE_INT )
		ret = glishtk_listbox_get_int( a, (char*) cmd, args );
	else
		global_store->Error("invalid argument type");

	return ret;
	}

char *glishtk_listbox_nearest(TkProxy *a, const char *, Value *args )
	{
	char *ret = 0;

	if ( args->IsNumeric() && args->Length() > 0 )
		{
		char ycoord[30];
		sprintf(ycoord,"%d", args->IntVal());
		int r = tcl_VarEval( a->Interp(), Tk_PathName(a->Self()), " nearest ", ycoord, 0 );
		if ( r == TCL_OK ) ret = Tcl_GetStringResult(a->Interp());
		}
	else
		global_store->Error("wrong type, numeric expected");

	return ret;
	}

char *glishtk_scrollbar_update(Tcl_Interp *tcl, Tk_Window self, const char *, Value *val )
	{
	if ( ! val->IsNumeric() || val->Length() < 2 )
		{
		global_store->Error("scrollbar update function");
		return 0;
		}

	char args[75];
	sprintf( args," set %f %f", val->DoubleVal(1), val->DoubleVal(2) );
	tcl_VarEval( tcl, Tk_PathName(self), args, (char *)NULL );
	return 0;
	}

char *glishtk_button_state(TkProxy *a, const char *, Value *args )
	{
	char *ret = 0;

	if ( args->IsNumeric() && args->Length() > 0 )
		((TkButton*)a)->State( args->IntVal() ? 1 : 0 );

	ret = ((TkButton*)a)->State( ) ? "T" : "F";
	return ret;
	}

char *glishtk_menu_onestr(TkProxy *a, const char *cmd, Value *args )
	{
	TkButton *Self = (TkButton*)a;
	TkButton *Parent = Self->Parent();
	char *ret = 0;

	if ( args->Type() == TYPE_STRING && args->Length() > 0 )
		tcl_VarEval( a->Interp(), Tk_PathName(Parent->Menu()), " entryconfigure ",
			     Self->Index(), SP, cmd, " {", args->StringPtr(0)[0], "}", (char *)NULL );
	else if ( args->Length() == 0 )
		{
		tcl_VarEval( a->Interp(), Tk_PathName(Parent->Menu()),
			     " entrycget ", Self->Index(), SP, cmd, (char *)NULL );
		ret = Tcl_GetStringResult(a->Interp());
		}
	else
		global_store->Error("wrong type, string expected");

	return ret;
	}

char *glishtk_menu_onebinary(TkProxy *a, const char *cmd, const char *ptrue, const char *pfalse,
				Value *args )
	{
	TkButton *Self = (TkButton*)a;
	TkButton *Parent = Self->Parent();
	char *ret = 0;

	if ( args->IsNumeric() && args->Length() > 0 )
		tcl_VarEval( a->Interp(), Tk_PathName(Parent->Menu()), " entryconfigure ", Self->Index(), SP,
			     cmd, SP, (args->IntVal() ? ptrue : pfalse), (char *)NULL );
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
	TkProxy *agent;
	char *event_name;
	char *tk_event_name;
	glishtk_bindinfo( TkProxy *c, const char *event, const char *tk_event );
	~glishtk_bindinfo()
		{
		free_memory( tk_event_name );
		free_memory( event_name );
		free_memory( id );
		}

	char *id;
	static unsigned int bind_count;
	};

unsigned int glishtk_bindinfo::bind_count = 0;
glishtk_bindinfo::glishtk_bindinfo( TkProxy *c, const char *event, const char *tk_event ) :
			agent(c), event_name(strdup(event)),
			tk_event_name(strdup(tk_event))
	{
	char buf[30];
	sprintf( buf, "gtkbb%x", ++bind_count );
	id = strdup( buf );
	}

glish_declare(PList,glishtk_bindinfo);
typedef PList(glishtk_bindinfo) glishtk_bindlist;
glish_declare(PDict,glishtk_bindlist);
typedef PDict(glishtk_bindlist) glishtk_bindtable;

int glishtk_bindcb( ClientData data, Tcl_Interp *, int, char *argv[] )
	{
	static char *event_names[] =
	  {
	    "", "", "KeyPress", "KeyRelease", "ButtonPress", "ButtonRelease",
	    "MotionNotify", "EnterNotify", "LeaveNotify", "FocusIn", "FocusOut",
	    "KeymapNotify", "Expose", "GraphicsExpose", "NoExpose", "VisibilityNotify",
	    "CreateNotify", "DestroyNotify", "UnmapNotify", "MapNotify", "MapRequest",
	    "ReparentNotify", "ConfigureNotify", "ConfigureRequest", "GravityNotify",
	    "ResizeRequest", "CirculateNotify", "CirculateRequest", "PropertyNotify",
	    "SelectionClear", "SelectionRequest", "SelectionNotify", "ColormapNotify",
	    "ClientMessage", "MappingNotify"
	  };

	glishtk_bindlist *list = (glishtk_bindlist*) data;
	glishtk_bindinfo *info = (*list)[0];
	if ( ! info ) return TCL_ERROR;

	recordptr rec = create_record_dict();

	int *dpt = (int*) alloc_memory( sizeof(int)*2 );
	dpt[0] = atoi(argv[1]);
	dpt[1] = atoi(argv[2]);
	rec->Insert( strdup("device"), new Value( dpt, 2 ) );

	int type = atoi(argv[4]);
	if ( type >= 2 && type <= 34 )
		rec->Insert( strdup("type"), new Value( event_names[type] ) );
	else
		rec->Insert( strdup("type"), new Value( "unknown" ) );

	rec->Insert( strdup("code"), new Value(atoi(argv[3])) );
	if ( argv[5][0] != '?' )
		{
		// KeyPress/Release event
		rec->Insert( strdup("sym"), new Value(argv[5]) );
		rec->Insert( strdup("key"), new Value(argv[6]) );
		}
	rec->Insert( strdup("id"), 0 );

	Value *val = new Value( rec );
	loop_over_list( (*list), x )
		{
		glishtk_bindinfo *i = (*list)[x];
		Unref( (Value*) rec->Insert("id", new Value(i->id)) );
		i->agent->BindEvent( i->event_name, val );
		}
	Unref( val );

	return TCL_OK;
	}

static glishtk_bindtable *glishtk_table = 0;
static name_hash *glishtk_untable = 0;
char *glishtk_bind(TkProxy *agent, const char *, Value *args )
	{
	char *event_name = "agent bind function";
	EXPRINIT( event_name)

	if ( args->Length() >= 2 )
		{
		EXPRSTR( button, event_name )
		EXPRSTR( event, event_name )

		tcl_VarEval( agent->Interp(), "bind ", Tk_PathName(agent->Self()), SP, button, (char *)NULL );
		const char *current = Tcl_GetStringResult(agent->Interp());
		char last_buffer[50];
		char *last = 0;
		if ( current && *current )
			{
			last = last_buffer;
			while ( *current && ! isspace(*current) )
				*last++ = *current++;
			*last = '\0';
			last = last != last_buffer ? last_buffer : 0;
			}

		glishtk_bindinfo *binfo = new glishtk_bindinfo(agent, event, button);

		if ( ! glishtk_table ) glishtk_table = new glishtk_bindtable;

		glishtk_bindlist *list = 0;
		if ( ! last || ! (list = (*glishtk_table)[last]) )
			{
			list = new glishtk_bindlist;
			char *cback = last = glishtk_make_callback(agent->Interp(), glishtk_bindcb, list);
			(*glishtk_table).Insert( strdup(cback), list );
			tcl_VarEval( agent->Interp(), "bind ", Tk_PathName(agent->Self()), SP, button,
				     " {", cback, " %x %y %b %T %K %A}", (char *)NULL );
			}

		list->append(binfo);

		if ( ! glishtk_untable ) glishtk_untable = new name_hash;
		(*glishtk_untable).Insert( strdup(binfo->id), strdup(last) );

		EXPR_DONE( event )
		EXPR_DONE( button )

		return binfo->id;
		}

	return 0;
	}

char *glishtk_unbind(TkProxy *agent, const char *, Value *args )
	{
	char *event_name = "agent unbind function";
	if ( args->Type() == TYPE_STRING && args->Length() >= 1 )
		{
		char *cback = 0;
		charptr name = args->StringPtr(0)[0];

		if ( glishtk_untable && (cback = (*glishtk_untable)[name]) )
			{
			free_memory( (*glishtk_untable).Remove(name) );
			glishtk_bindlist *list = 0;
			if ( glishtk_table && (list = (*glishtk_table)[cback]) )
				{
				glishtk_bindinfo *info = 0;
				loop_over_list( (*list), x )
					{
					if ( ! strcmp((*list)[x]->id,name) )
						{
						info = (*list).remove_nth(x);
						break;
						}
					}

				if ( info )
					{
					if ( ! (*list).length() )
						{
						free_memory( (*glishtk_table).Remove(cback) );
						tcl_VarEval( agent->Interp(), "bind ", Tk_PathName(agent->Self()), SP,
							     info->tk_event_name, " {}", (char *)NULL );
						Unref(list);
						}
					delete info;
					}
				}
			free_memory(cback);
			}

		else if ( glishtk_table )
			{
			tcl_VarEval( agent->Interp(), "bind ", Tk_PathName(agent->Self()), SP, name, (char *)NULL );

			const char *current = Tcl_GetStringResult(agent->Interp());
			char last_buffer[50];
			char *last = 0;
			if ( current && *current )
				{
				last = last_buffer;
				while ( *current && ! isspace(*current) )
					*last++ = *current++;
				*last = '\0';
				last = last != last_buffer ? last_buffer : 0;
				}

			glishtk_bindlist *list = 0;
			if ( last && *last && (list = (*glishtk_table)[last]) )
				{
				free_memory( (*glishtk_table).Remove(last) );
				glishtk_bindinfo *info = (*list)[0];
				if ( info )
					tcl_VarEval( agent->Interp(), "bind ", Tk_PathName(agent->Self()), SP,
						     info->tk_event_name, " {}", (char *)NULL );
				loop_over_list( (*list), x )
					delete (*list)[x];
				(*list).clear();
				if ( info ) Unref(list);
				}
			}

		}

	return 0;
	}


int glishtk_delframe_cb( ClientData data, Tcl_Interp *, int, char *[] )
	{
	((TkFrameP*)data)->KillFrame();
	return TCL_OK;
	}

char *glishtk_width(Tcl_Interp *, Tk_Window self, const char *, Value * )
	{
	return (char*) new Value( Tk_Width(self) );
	}

char *glishtk_height(Tcl_Interp *, Tk_Window self, const char *, Value * )
	{
	return (char*) new Value( Tk_Height(self) );
	}

#define GEOM_GET(WHAT)								\
	tcl_VarEval( tcl, "winfo ", #WHAT, SP, Tk_PathName(tlead), (char *)NULL );		\
	int WHAT = atoi(Tcl_GetStringResult(tcl));


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
const char *glishtk_popup_geometry( Tcl_Interp *tcl, Tk_Window tlead, charptr pos )
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

void glishtk_popup_adjust_dim_cb( ClientData clientData, XEvent *ptr)
	{
	// This dimension (width and height) adjustment was necessary
	// because there were time when the geometry manager would be
	// stuck in oscillations between satisfying the requested
	// height and the needed height. This happened upon entering
	// the popup. This means that the popup won't shrink in size
	// if things are removed... probably OK... This happened
	// with the aips++ combobox...
	if ( ptr->xany.type == ConfigureNotify )
		{
		Tk_Window self = ((TkProxy*)clientData)->Self();
		Tcl_Interp *tcl = ((TkProxy*)clientData)->Interp();
		
		tcl_VarEval( tcl, Tk_PathName(self), " cget -width", (char *)NULL );
		int req_width = atoi(Tcl_GetStringResult(tcl));
		tcl_VarEval( tcl, Tk_PathName(self), " cget -height", (char *)NULL );
		int req_height = atoi(Tcl_GetStringResult(tcl));

		char buf[40];
		if ( Tk_Width(self) > req_width )
			{
			sprintf( buf, "%d", Tk_Width(self) );
			tcl_VarEval( tcl, Tk_PathName(self), " configure -width ", buf, (char *)NULL );
			}
		if ( Tk_Height(self) > req_height )
			{
			sprintf( buf, "%d", Tk_Height(self) );
			tcl_VarEval( tcl, Tk_PathName(self), " configure -height ", buf, (char *)NULL );
			}
		}
	}

void glishtk_resizeframe_cb( ClientData clientData, XEvent *eventPtr)
	{
	if ( eventPtr->xany.type == ConfigureNotify )
		{

		Tk_Window self = ((TkProxy*)clientData)->Self();
		Tcl_Interp *tcl = ((TkProxy*)clientData)->Interp();
		
		tcl_VarEval( tcl, Tk_PathName(self), " cget -width", (char *)NULL );
		tcl_VarEval( tcl, Tk_PathName(self), " cget -height", (char *)NULL );

		TkFrameP *f = (TkFrameP*) clientData;
		f->ResizeEvent();
		}
	}

void glishtk_moveframe_cb( ClientData clientData, XEvent *eventPtr)
	{
	if ( eventPtr->xany.type == ConfigureNotify )
		{
		TkFrameP *f = (TkFrameP*) clientData;
		f->LeaderMoved();
		}
	}

char *glishtk_agent_map(TkProxy *a, const char *cmd, Value *)
	{
	a->SetMap( cmd[0] == 'M' ? 1 : 0, cmd[1] == 'T' ? 1 : 0 );
	return 0;
	}


void TkFrameP::Disable( )
	{
	loop_over_list( elements, i )
		if ( elements[i] != this )
			elements[i]->Disable( );
	}

void TkFrameP::Enable( int force )
	{
	loop_over_list( elements, i )
		if ( elements[i] != this )
			elements[i]->Enable( force );
	}

TkFrameP::TkFrameP( ProxyStore *s, charptr relief_, charptr side_, charptr borderwidth, charptr padx_,
		    charptr pady_, charptr expand_, charptr background, charptr width, charptr height,
		    charptr cursor, charptr title, charptr icon, int new_cmap, TkProxy *tlead_, charptr tpos_,
		    charptr hlcolor, charptr hlbackground, charptr hlthickness ) :
		  TkFrame( s ), side(0), padx(0), pady(0), expand(0), tag(0), canvas(0),
		  topwin( 0 ), reject_first_resize(1), tlead(tlead_), tpos(0), unmapped(0),
		  icon(0)

	{
	char *argv[20];

	char *background_ = glishtk_quote_string(background);
	char *hlcolor_ = glishtk_quote_string(hlcolor,0);
	char *hlbackground_ = glishtk_quote_string(hlbackground,0);

	agent_ID = "<graphic:frame>";

	if ( ! root )
		HANDLE_CTOR_ERROR("Frame creation failed, check DISPLAY environment variable.")

	int c = 0;
	argv[c++] = "toplevel";
	argv[c++] = (char*) NewName();
	argv[c++] = "-borderwidth";
	argv[c++] = "0";
	argv[c++] = "-width";
	argv[c++] = (char*) width;
	argv[c++] = "-height";
	argv[c++] = (char*) height;
	argv[c++] = "-background";
	argv[c++] = (char*) background_;
	if ( new_cmap )
		{
		argv[c++] = "-colormap";
		argv[c++] = "new";
		}

	tcl_ArgEval( tcl, c, argv );
	char *ctor_error = Tcl_GetStringResult(tcl);
	if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);

	topwin = Tk_NameToWindow( tcl, argv[1], root );
	if ( title && title[0] )
		tcl_VarEval( tcl, "wm title ", Tk_PathName( topwin ), " {", title, "}", (char *)NULL );

	if ( tlead )
		{
		if ( tlead->Self() )
			{
			Ref( tlead );
			tpos = strdup(tpos_);
			}
		else
			HANDLE_CTOR_ERROR("Frame creation failed, bad transient leader");

		tcl_VarEval( tcl, "wm transient ", Tk_PathName(topwin), SP,
			     Tk_PathName(tlead->Self()), (char *)NULL );
		tcl_VarEval( tcl, "wm overrideredirect ", Tk_PathName(topwin), " true", (char *)NULL );

		const char *geometry = glishtk_popup_geometry( tcl, tlead->Self(), tpos );
		tcl_VarEval( tcl, "wm geometry ", Tk_PathName(topwin), SP, geometry, (char *)NULL );

		Tk_Window top = tlead->TopLevel();
		Tk_CreateEventHandler(top, StructureNotifyMask, glishtk_moveframe_cb, this );
		}

	side = strdup(side_);
	padx = strdup(padx_);
	pady = strdup(pady_);
	expand = strdup(expand_);

	c = 0;
	argv[c++] = "frame";
	argv[c++] = (char*) NewName( topwin );

	argv[c++] = "-relief";
	argv[c++] = (char*) relief_;
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-width";
	argv[c++] = (char*) width;
	argv[c++] = "-height";
	argv[c++] = (char*) height;
	argv[c++] = "-background";
	argv[c++] = (char*) background_;
	if ( cursor && *cursor )
		{
		argv[c++] = "-cursor";
		argv[c++] = (char*) cursor;
		}
	if ( hlcolor_ && *hlcolor_ )
		{
		argv[c++] = "-highlightcolor";
		argv[c++] = (char*) hlcolor_;
		}
	if ( hlbackground_ && *hlbackground_ )
		{
		argv[c++] = "-highlightbackground";
		argv[c++] = (char*) hlbackground_;
		}
	if ( hlthickness && *hlthickness )
		{
		argv[c++] = "-highlightthickness";
		argv[c++] = (char*) hlthickness;
		}

	tcl_ArgEval( tcl, c, argv );
	ctor_error = Tcl_GetStringResult(tcl);
	if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);

	self = Tk_NameToWindow( tcl, argv[1], root );

	free_memory(background_);
	free_memory(hlcolor_);
	free_memory(hlbackground_);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkFrameP::TkFrameP")

	if ( title && title[0] )
		tcl_VarEval( tcl, "wm title ", Tk_PathName( self ), " {", title, "}", (char *)NULL );

	tcl_VarEval( tcl, "wm protocol ", Tk_PathName(topwin), " WM_DELETE_WINDOW ",
		     glishtk_make_callback( tcl, glishtk_delframe_cb, this ), (char *)NULL );

	if ( icon && strlen( icon ) )
		{
		char *expanded = which_bitmap(icon);
		if ( expanded )
			{
			char *icon_ = (char*) alloc_memory(strlen(expanded)+3);
			sprintf(icon_," @%s",expanded);
			tcl_VarEval( tcl, "wm iconbitmap ", Tk_PathName(topwin), icon_, (char *)NULL);
			free_memory( expanded );
			free_memory( icon_ );
			}
		}

	//
	// Clearing the height/width of toplevel frames fixes problems
	// with configuring the widget. When setting the cursor, for
	// example, the frame & children go crazy resizing themselves.
	//
// 	rivet_clear_frame_dims( self );
	AddElement( this );

	if ( frame )
		{
		frame->AddElement( this );
		frame->Pack();
		}
	else
		Pack();

	procs.Insert("bind", new FmeProc(this, "", glishtk_bind, glishtk_str));
	procs.Insert("unbind", new FmeProc(this, "", glishtk_unbind));
	procs.Insert("cursor", new FmeProc("-cursor", glishtk_onestr, glishtk_str));
	procs.Insert("disable", new FmeProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new FmeProc( this, "0", glishtk_disable_cb ));
	procs.Insert("expand", new FmeProc( this, &TkFrameP::SetExpand, glishtk_str ));
	procs.Insert("fonts", new FmeProc( this, &TkFrameP::FontsCB, glishtk_valcast ));
	procs.Insert("grab", new FmeProc( this, &TkFrameP::GrabCB ));
	procs.Insert("height", new FmeProc("", glishtk_height, glishtk_valcast));
	procs.Insert("icon", new FmeProc( this, &TkFrameP::SetIcon, glishtk_str ));
	procs.Insert("map", new FmeProc(this, "MT", glishtk_agent_map));
	procs.Insert("padx", new FmeProc( this, &TkFrameP::SetPadx, glishtk_strtoint ));
	procs.Insert("pady", new FmeProc( this, &TkFrameP::SetPady, glishtk_strtoint ));
	procs.Insert("raise", new FmeProc( this, &TkFrameP::Raise ));
	procs.Insert("release", new FmeProc( this, &TkFrameP::ReleaseCB ));
	procs.Insert("side", new FmeProc( this, &TkFrameP::SetSide, glishtk_str ));
	procs.Insert("title", new FmeProc( this, &TkFrameP::Title ));
	procs.Insert("unmap", new FmeProc(this, "UT", glishtk_agent_map));
	procs.Insert("width", new FmeProc("", glishtk_width, glishtk_valcast));

	Tk_CreateEventHandler( self, StructureNotifyMask, glishtk_popup_adjust_dim_cb, this );
	if ( ! tlead )
		Tk_CreateEventHandler( self, StructureNotifyMask, glishtk_resizeframe_cb, this );

	size[0] = Tk_ReqWidth(self);
	size[1] = Tk_ReqHeight(self);
	}

TkFrameP::TkFrameP( ProxyStore *s, TkFrame *frame_, charptr relief_, charptr side_,
		    charptr borderwidth, charptr padx_, charptr pady_, charptr expand_, charptr background,
		    charptr width, charptr height, charptr cursor, int new_cmap,
		    charptr hlcolor, charptr hlbackground, charptr hlthickness ) : TkFrame( s ),
		  side(0), padx(0), pady(0), expand(0), tag(0), canvas(0), topwin( 0 ),
		  reject_first_resize(0), tlead(0), tpos(0), unmapped(0), icon(0)

	{
	char *argv[22];
	frame = frame_;

	char *background_ = glishtk_quote_string(background);
	char *hlcolor_ = glishtk_quote_string(hlcolor,0);
	char *hlbackground_ = glishtk_quote_string(hlbackground,0);

	agent_ID = "<graphic:frame>";

	if ( ! root )
		HANDLE_CTOR_ERROR("Frame creation failed, check DISPLAY environment variable.")

	if ( ! frame || ! frame->Self() ) return;

	side = strdup(side_);
	padx = strdup(padx_);
	pady = strdup(pady_);
	expand = strdup(expand_);

	int c = 0;
	argv[c++] = "frame";
	argv[c++] = (char*) NewName(frame->Self());

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
	argv[c++] = (char*) background_;
	if ( cursor && *cursor )
		{
		argv[c++] = "-cursor";
		argv[c++] = (char*) cursor;
		}
	if ( hlcolor_ && *hlcolor_ )
		{
		argv[c++] = "-highlightcolor";
		argv[c++] = (char*) hlcolor_;
		}
	if ( hlbackground_ && *hlbackground_ )
		{
		argv[c++] = "-highlightbackground";
		argv[c++] = (char*) hlbackground_;
		}
	if ( hlthickness && *hlthickness )
		{
		argv[c++] = "-highlightthickness";
		argv[c++] = (char*) hlthickness;
		}

	tcl_ArgEval( tcl, c, argv );
	char *ctor_error = Tcl_GetStringResult(tcl);
	if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);

	self = Tk_NameToWindow( tcl, argv[1], root );

	free_memory(background_);
	free_memory(hlcolor_);
	free_memory(hlbackground_);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkFrameP::TkFrameP")

	AddElement( this );

	if ( frame )
		{
		frame->AddElement( this );
		frame->Pack();
		}
	else
		Pack();

	procs.Insert("padx", new FmeProc( this, &TkFrameP::SetPadx, glishtk_strtoint ));
	procs.Insert("pady", new FmeProc( this, &TkFrameP::SetPady, glishtk_strtoint ));
	procs.Insert("expand", new FmeProc( this, &TkFrameP::SetExpand, glishtk_str ));
	procs.Insert("side", new FmeProc( this, &TkFrameP::SetSide, glishtk_str ));
	procs.Insert("grab", new FmeProc( this, &TkFrameP::GrabCB ));
	procs.Insert("fonts", new FmeProc( this, &TkFrameP::FontsCB, glishtk_valcast ));
	procs.Insert("release", new FmeProc( this, &TkFrameP::ReleaseCB ));
	procs.Insert("cursor", new FmeProc("-cursor", glishtk_onestr, glishtk_str));
	procs.Insert("map", new FmeProc(this, "MC", glishtk_agent_map));
	procs.Insert("unmap", new FmeProc(this, "UC", glishtk_agent_map));
	procs.Insert("bind", new FmeProc(this, "", glishtk_bind, glishtk_str));
	procs.Insert("unbind", new FmeProc(this, "", glishtk_unbind));

	procs.Insert("width", new FmeProc("", glishtk_width, glishtk_valcast));
	procs.Insert("height", new FmeProc("", glishtk_height, glishtk_valcast));

	procs.Insert("disable", new FmeProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new FmeProc( this, "0", glishtk_disable_cb ));
	}

TkFrameP::TkFrameP( ProxyStore *s, TkCanvas *canvas_, charptr relief_, charptr side_,
		    charptr borderwidth, charptr padx_, charptr pady_, charptr expand_, charptr background,
		    charptr width, charptr height, const char *tag_ ) : TkFrame( s ), side(0),
		  padx(0), pady(0), expand(0), topwin( 0 ), reject_first_resize(0),
		  tlead(0), tpos(0), unmapped(0), icon(0)

	{
	char *argv[12];

	char *background_ = glishtk_quote_string(background);

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

	int c = 0;
	argv[c++] = "frame";
	argv[c++] = (char*) NewName(canvas->Self());
	argv[c++] = "-relief";
	argv[c++] = (char*) relief_;
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-width";
	argv[c++] = (char*) width;
	argv[c++] = "-height";
	argv[c++] = (char*) height;
	argv[c++] = "-background";
	argv[c++] = (char*) background_;

	tcl_ArgEval( tcl, c, argv );
	char *ctor_error = Tcl_GetStringResult(tcl);
	if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);

	self = Tk_NameToWindow( tcl, argv[1], root );

	free_memory(background_);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkFrameP::TkFrameP")

	//
	// Clearing the height/width of toplevel frames fixes problems
	// with configuring the widget. When setting the cursor, for
	// example, the frame & children go crazy resizing themselves.
	//
// 	rivet_clear_frame_dims( self );

	if ( frame )
		{
		frame->AddElement( this );
		frame->Pack();
		}
	else
		Pack();

	procs.Insert("padx", new FmeProc( this, &TkFrameP::SetPadx, glishtk_strtoint ));
	procs.Insert("pady", new FmeProc( this, &TkFrameP::SetPady, glishtk_strtoint ));
	procs.Insert("tag", new FmeProc( this, &TkFrameP::GetTag, glishtk_str ));
	procs.Insert("side", new FmeProc( this, &TkFrameP::SetSide, glishtk_str ));
	procs.Insert("grab", new FmeProc( this, &TkFrameP::GrabCB ));
	procs.Insert("fonts", new FmeProc( this, &TkFrameP::FontsCB, glishtk_valcast ));
	procs.Insert("release", new FmeProc( this, &TkFrameP::ReleaseCB ));
	procs.Insert("cursor", new FmeProc("-cursor", glishtk_onestr, glishtk_str));
	procs.Insert("bind", new FmeProc(this, "", glishtk_bind, glishtk_str));
	procs.Insert("unbind", new FmeProc(this, "", glishtk_unbind));

	procs.Insert("width", new FmeProc("", glishtk_width, glishtk_valcast));
	procs.Insert("height", new FmeProc("", glishtk_height, glishtk_valcast));

	procs.Insert("disable", new FmeProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new FmeProc( this, "0", glishtk_disable_cb ));
	}

void TkFrameP::UnMap()
	{
	if ( unmapped ) return;
	unmapped = 1;

	if ( RefCount() > 0 ) Ref(this);

	if ( self )
		Tk_DeleteEventHandler(self, StructureNotifyMask, glishtk_resizeframe_cb, this );

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
		TkProxy *a = elements.remove_nth( 0 );
		a->UnMap( );
		}

	if ( canvas )
		tcl_VarEval( tcl, Tk_PathName(canvas->Self()), " delete ", tag, (char *)NULL );
	else if ( self )
		Tk_DestroyWindow( self );

	canvas = 0;
	frame = 0;
	self = 0;

	if ( topwin )
		{
		Tk_DestroyWindow( topwin );
		topwin = 0;
		}

	if ( tlead )
		{
		Tk_Window top = tlead->TopLevel();
		if ( top )
			Tk_DeleteEventHandler(top, StructureNotifyMask, glishtk_moveframe_cb, this );
		Unref( tlead );
		tlead = 0;
		}

	if ( RefCount() > 0 ) Unref(this);
	}

TkFrameP::~TkFrameP( )
	{
	if ( frame )
		frame->RemoveElement( this );
	if ( canvas )
		canvas->Remove( this );

	free_memory( side );
	free_memory( padx );
	free_memory( pady );
	free_memory( expand );
	if ( tpos ) free_memory( tpos );

	UnMap();

	if ( tag )
		free_memory( tag );
	}

char *TkFrameP::SetIcon( Value *args )
	{
	if ( args->Type() == TYPE_STRING && args->Length() > 0 )
		{
		const char *iconx = args->StringPtr(0)[0];
		if ( iconx && strlen(iconx) )
			{
			if ( icon ) free_memory(icon);
			icon = strdup(iconx);
			char *icon_ = (char*) alloc_memory(strlen(icon)+3);
			sprintf(icon_," @%s",icon);
			tcl_VarEval( tcl, "wm iconbitmap ", Tk_PathName(topwin), icon_, (char *)NULL );
			free_memory( icon_ );
			}
		}

	return icon ? icon : "";
	}

char *TkFrameP::SetSide( Value *args )
	{
	if ( args->Type() == TYPE_STRING && args->Length() > 0 )
		{
		const char *side_ = args->StringPtr(0)[0];
		if ( side_[0] != side[0] || strcmp(side, side_) )
			{
			free_memory( side );
			side = strdup( side_ );
			Pack();
			}
		}

	return side;
	}

char *TkFrameP::SetPadx( Value *args )
	{
	if ( args->Type() == TYPE_STRING )
		{
		const char *padx_ = args->StringPtr(0)[0];
		if ( padx_[0] != padx[0] || strcmp(padx, padx_) )
			{
			free_memory( padx );
			padx = strdup( padx_ );
			Pack();
			}
		}
	else if ( args->IsNumeric() && args->Length() > 0 )
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

	tcl_VarEval( tcl, Tk_PathName(self), " cget -padx", (char *)NULL );
	return Tcl_GetStringResult(tcl);
	}

char *TkFrameP::SetPady( Value *args )
	{
	if ( args->Type() == TYPE_STRING )
		{
		const char *pady_ = args->StringPtr(0)[0];
		if ( pady_[0] != pady[0] || strcmp(pady, pady_) )
			{
			free_memory( pady );
			pady = strdup( pady_ );
			Pack();
			}
		}
	else if ( args->Length() > 0 && args->IsNumeric() )
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

	tcl_VarEval( tcl, Tk_PathName(self), " cget -pady", (char *)NULL );
	return Tcl_GetStringResult(tcl);
	}

char *TkFrameP::GetTag( Value * )
	{
	return tag;
	}

char *TkFrameP::SetExpand( Value *args )
	{
	if ( args->Type() == TYPE_STRING && args->Length() > 0 )
		{
		const char *expand_ = args->StringPtr(0)[0];
		if ( expand_[0] != expand[0] || strcmp(expand, expand_) )
			{
			free_memory( expand );
			expand = strdup( expand_ );
			Pack();
			}
		}

	return expand;
	}

char *TkFrameP::Grab( int global_scope )
	{
	if ( grab ) return 0;

	if ( global_scope )
		tcl_VarEval( tcl, "grab set -global ", Tk_PathName(self), (char *)NULL );
	else
		tcl_VarEval( tcl, "grab set ", Tk_PathName(self), (char *)NULL );

	grab = Id();
	return "";
	}

char *TkFrameP::GrabCB( Value *args )
	{
	if ( grab ) return 0;

	int global_scope = 0;

	if ( args->Type() == TYPE_STRING && args->Length() > 0 && ! strcmp(args->StringPtr(0)[0],"global") )
		global_scope = 1;

	return Grab( global_scope );
	}

char *TkFrameP::Raise( Value *args )
	{
	TkProxy *agent = 0;
	if ( args->IsAgentRecord( ) && (agent = (TkProxy*) store->GetProxy(args)) )
		tcl_VarEval( tcl, "raise ", Tk_PathName(TopLevel()), SP, Tk_PathName(agent->TopLevel()), (char *)NULL );
	else
		tcl_VarEval( tcl, "raise ", Tk_PathName(TopLevel()), (char *)NULL );

	return "";
	}

char *TkFrameP::Title( Value *args )
	{
	if ( args->Type() == TYPE_STRING )
{
		tcl_VarEval( tcl, "wm title ", Tk_PathName(TopLevel( )), " {", args->StringPtr(0)[0], "}", (char *)NULL );
}
	else
		global_store->Error("wrong type, string expected");

	return "";
	}

char *TkFrameP::FontsCB( Value *args )
	{
	char *wild = "-*-*-*-*-*-*-*-*-*-*-*-*-*-*";
	char **fonts = 0;
	int len = 0;

	if ( args->Type() == TYPE_STRING )
		fonts = XListFonts(Tk_Display(self), args->StringPtr(0)[0], 32768, &len);
	else if ( args->IsNumeric() && args->Length() > 0 )
		fonts = XListFonts(Tk_Display(self), wild, args->IntVal(), &len);
	else if ( args->Type() == TYPE_RECORD )
		{
		EXPRINIT("TkFrameP::FontsCB")
		EXPRSTR( str, "TkFrameP::FontsCB" )
		EXPRINT( l, "TkFrameP::FontsCB" )
		fonts = XListFonts(Tk_Display(self), str, l, &len);
		EXPR_DONE( str )
		EXPR_DONE( l )
		}
	else
		fonts = XListFonts(Tk_Display(self), wild, 32768, &len);

	Value *ret = fonts ? new Value( (charptr*) fonts, len, COPY_ARRAY ) : new Value( glish_false );
	XFreeFontNames(fonts);
	return (char*) ret;
	}

char *TkFrameP::Release( )
	{
	if ( ! grab || grab != Id() ) return 0;

	tcl_VarEval( tcl, "grap release ", Tk_PathName(self), (char *)NULL );

	grab = 0;
	return "";
	}

char *TkFrameP::ReleaseCB( Value * )
	{
	return Release( );
	}

void TkFrameP::PackSpecial( TkProxy *agent )
	{
	const char **instr = agent->PackInstruction();

	int cnt = 0;
	while ( instr[cnt] ) cnt++;

	char **argv = (char**) alloc_memory( sizeof(char*)*(cnt+8) );

	int i = 0;
	argv[i++] = "pack";
	argv[i++] = Tk_PathName( agent->Self() );
	argv[i++] = "-side";
	argv[i++] = side;
	argv[i++] = "-padx";
	argv[i++] = padx;
	argv[i++] = "-pady";
	argv[i++] = pady;

	cnt=0;
	while ( instr[cnt] )
		argv[i++] = (char*) instr[cnt++];

	do_pack(i,argv);
	free_memory( argv );
	}

int TkFrameP::ExpandNum(const TkProxy *except, unsigned int grtOReqt) const
	{
	unsigned int cnt = 0;
	loop_over_list( elements, i )
		{
		if ( (! except || elements[i] != except) &&
		     elements[i] != (TkProxy*) this && elements[i]->CanExpand() )
			cnt++;
		if ( grtOReqt && cnt >= grtOReqt )
			break;
		}
	return cnt;
	}

void TkFrameP::Pack( )
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
				argv[c++] = Tk_PathName(elements[i]->Self());
			}

		if ( c > 1 )
			{
			argv[c++] = "-side";
			argv[c++] = side;
			argv[c++] = "-padx";
			argv[c++] = padx;
			argv[c++] = "-pady";
			argv[c++] = pady;

			do_pack(c,argv);
			}

		if ( frame )
			frame->Pack();

		free_memory( argv );
		}
	}

void TkFrameP::RemoveElement( TkProxy *obj )
	{
	if ( elements.is_member(obj) )
		elements.remove(obj);
	}

void TkFrameP::KillFrame( )
	{
	Value *v = new Value(glish_true);
	PostTkEvent( "killed", v );
	Unref(v);
	UnMap();
	}

void TkFrameP::ResizeEvent( )
	{
	if ( reject_first_resize )
		reject_first_resize = 0;
	else
		{
		recordptr rec = create_record_dict();

		rec->Insert( strdup("old"), new Value( size, 2, COPY_ARRAY ) );
		size[0] = Tk_Width(self);
		size[1] = Tk_Height(self);
		rec->Insert( strdup("new"), new Value( size, 2, COPY_ARRAY ) );

		Value *v = new Value( rec );
		PostTkEvent( "resize", v );
		Unref(v);
		}
	}

void TkFrameP::LeaderMoved( )
	{
	if ( ! tlead ) return;

	const char *geometry = glishtk_popup_geometry( tcl, tlead->Self(), tpos );
	tcl_VarEval( tcl, "wm geometry ", Tk_PathName(topwin), SP, geometry, (char *)NULL );
	tcl_VarEval( tcl, "raise ", Tk_PathName(topwin), (char *)NULL );
	}

void TkFrameP::Create( ProxyStore *s, Value *args )
	{
	TkFrameP *ret = 0;

	if ( args->Length() != 19 )
		InvalidNumberOfArgs(19);

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
	SETSTR( hlcolor )
	SETSTR( hlbackground )
	SETDIM( hlthickness )

	if ( parent->Type() == TYPE_BOOL )
		{
		TkProxy *tl = (TkProxy*)(tlead->IsAgentRecord() ? global_store->GetProxy(tlead) : 0);

		if ( tl && strncmp( tl->AgentID(), "<graphic:", 9 ) )
			{
			SETDONE
			global_store->Error("bad transient leader");
			return;
			}

		ret =  new TkFrameP( s, relief, side, borderwidth, padx, pady, expand, background,
				     width, height, cursor, title, icon, new_cmap, (TkProxy*) tl, tpos,
				     hlcolor, hlbackground, hlthickness );
		}
	else
		{
		TkProxy *agent = (TkProxy*)global_store->GetProxy(parent);
		if ( agent && ! strcmp("<graphic:frame>", agent->AgentID()) )
			ret =  new TkFrameP( s, (TkFrame*)agent, relief,
					     side, borderwidth, padx, pady, expand, background,
					     width, height, cursor, new_cmap,
					     hlcolor, hlbackground, hlthickness );
		else
			{
			SETDONE
			global_store->Error("bad parent type");
			return;
			}
		}

	CREATE_RETURN
	}

const char *TkFrameP::Side() const
	{
	return side;
	}

const char **TkFrameP::PackInstruction()
	{
	static char *ret[5];
	int c = 0;

	if ( strcmp(expand,"none") )
		{
		ret[c++] = "-fill";
		ret[c++] = expand;

		if ( ! canvas && (! frame || ! strcmp(expand,"both") ||
			! strcmp(expand,"x") && (! strcmp(frame->Side(),"left") || 
						 ! strcmp(frame->Side(),"right"))  ||
			! strcmp(expand,"y") && (! strcmp(frame->Side(),"top") || 
						 ! strcmp(frame->Side(),"bottom"))) )
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

int TkFrameP::CanExpand() const
	{
	return ! canvas && (! frame || ! strcmp(expand,"both") ||
		! strcmp(expand,"x") && (! strcmp(frame->Side(),"left") || 
					 !strcmp(frame->Side(),"right")) ||
		! strcmp(expand,"y") && (! strcmp(frame->Side(),"top") || 
					 ! strcmp(frame->Side(),"bottom")) );
	}

Tk_Window TkFrameP::TopLevel( )
	{
	return frame ? frame->TopLevel() : canvas ? canvas->TopLevel() : topwin;
	}

Value *FmeProc::operator()(Tcl_Interp *tcl, Tk_Window s, Value *arg)
	{
	char *val = 0;

	if ( fproc && agent )
		val = (((TkFrameP*)agent)->*fproc)( arg);
	else
		return TkProc::operator()( tcl, s, arg );

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
			TkProxy *a = entry_list.remove_nth( 0 );
			a->UnMap( );
			}

		if ( menu )
			{
			menu->Remove(this);
			tcl_VarEval( tcl, Tk_PathName(Menu()), " delete ", Index(), (char *)NULL );
			}
		else
			{
			Tk_DestroyWindow( self );
			}
		}

	else if ( menu )
		{
		menu->Remove(this);
		tcl_VarEval( tcl, Tk_PathName(Menu()), " delete ", Index(), (char *)NULL );
		}

	else if ( self )
		{
		tcl_VarEval( tcl, Tk_PathName(self), " config -command \"\"", (char *)NULL );
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

	if ( fill ) free_memory(fill);
	if ( menu_index ) free_memory(menu_index);
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
			tcl_VarEval( tcl, Tk_PathName(self), " config -state normal", (char *)NULL );
		else
			tcl_VarEval( tcl, Tk_PathName(Parent()->Menu()), " entryconfigure ", Index(), " -state normal", (char *)NULL );
		}
	}

void TkButton::ExitEnable()
	{
	if ( enable_state && --enable_state == 0 )
		if ( frame )
			tcl_VarEval( tcl, Tk_PathName(self), " config -state disabled", (char *)NULL );
		else
			tcl_VarEval( tcl, Tk_PathName(Parent()->Menu()), " entryconfigure ", Index(), " -state disabled", (char *)NULL );
	}

void TkButton::Disable( )
	{
	disable_count++;
	if ( frame )
		tcl_VarEval( tcl, Tk_PathName(self), " config -state disabled", (char *)NULL );
	else
		tcl_VarEval( tcl, Tk_PathName(Parent()->Menu()), " entryconfigure ", Index(), " -state disabled", (char *)NULL );
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
		tcl_VarEval( tcl, Tk_PathName(self), " config -state normal", (char *)NULL );
	else
		tcl_VarEval( tcl, Tk_PathName(Parent()->Menu()), " entryconfigure ", Index(), " -state normal", (char *)NULL );
	}

TkButton::TkButton( ProxyStore *s, TkFrame *frame_, charptr label_, charptr type_,
		    charptr padx, charptr pady, int width, int height, charptr justify,
		    charptr font, charptr relief, charptr borderwidth, charptr foreground,
		    charptr background, int disabled, const Value *val, charptr anchor,
		    charptr fill_, charptr bitmap_, TkFrame *group,
		    charptr hlcolor, charptr hlbackground, charptr hlthickness )
			: TkFrame( s ), value(0), state(0), menu(0), radio(group),
			  menu_base(0),  next_menu_entry(0), menu_index(0), fill(0), unmapped(0)
	{
	type = PLAIN;
	frame = frame_;
	char *argv[44];

	char *foreground_ = glishtk_quote_string(foreground);
	char *background_ = glishtk_quote_string(background);
	char *hlcolor_ = glishtk_quote_string(hlcolor,0);
	char *hlbackground_ = glishtk_quote_string(hlbackground,0);

	agent_ID = "<graphic:button>";

	if ( ! frame || ! frame->Self() ) return;

	char width_[30];
	char height_[30];
	char var_name[256];
	char val_name[256];
	char *bitmap = 0;
	char *label = 0;

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
		label = glishtk_quote_string(label_);
		argv[c++] = label;
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
	argv[c++] = (char*) foreground_;
	argv[c++] = "-bg";
	argv[c++] = (char*) background_;
	argv[c++] = "-state";
	argv[c++] = disabled ? "disabled" : "normal";
	if ( disabled ) disable_count++;

	if ( hlcolor_ && *hlcolor_ )
		{
		argv[c++] = "-highlightcolor";
		argv[c++] = (char*) hlcolor_;
		}
	if ( hlbackground_ && *hlbackground_ )
		{
		argv[c++] = "-highlightbackground";
		argv[c++] = (char*) hlbackground_;
		}
	if ( hlthickness && *hlthickness )
		{
		argv[c++] = "-highlightthickness";
		argv[c++] = (char*) hlthickness;
		}

	if ( type != MENU )
		{
		argv[c++] = "-command";
		argv[c++] = glishtk_make_callback( tcl, buttoncb, this );
		}

	char *ctor_error = 0;
	switch ( type )
		{
		case RADIO:
			argv[0] = "radiobutton";
			tcl_ArgEval( tcl, c, argv );
			ctor_error = Tcl_GetStringResult(tcl);
			if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);
			self = Tk_NameToWindow( tcl, argv[1], root );
			break;
		case CHECK:
			argv[0] = "checkbutton";
			tcl_ArgEval( tcl, c, argv );
			ctor_error = Tcl_GetStringResult(tcl);
			if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);
			self = Tk_NameToWindow( tcl, argv[1], root );
			break;
		case MENU:
			argv[0] = "menubutton";
			tcl_ArgEval( tcl, c, argv );
			ctor_error = Tcl_GetStringResult(tcl);
			if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);
			self = Tk_NameToWindow( tcl, argv[1], root );
			if ( ! self )
				HANDLE_CTOR_ERROR("Rivet creation failed in TkButton::TkButton")
			argv[0] = "menu";
			argv[1] = (char*) NewName(self);
			argv[2] = "-tearoff";
			argv[3] = "0";
			tcl_ArgEval( tcl, 4, argv );
			ctor_error = Tcl_GetStringResult(tcl);
			if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);
			menu_base = Tk_NameToWindow( tcl, argv[1], root );
			if ( ! menu_base )
				HANDLE_CTOR_ERROR("Rivet creation failed in TkButton::TkButton")
			tcl_VarEval( tcl, Tk_PathName(self), " config -menu ", Tk_PathName(menu_base), (char *)NULL );
			break;
		default:
			argv[0] = "button";
			tcl_ArgEval( tcl, c, argv );
			ctor_error = Tcl_GetStringResult(tcl);
			if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);
			self = Tk_NameToWindow( tcl, argv[1], root );
			break;
		}

	if ( bitmap ) free_memory(bitmap);
	if ( label ) free_memory(label);
	free_memory(background_);
	free_memory(foreground_);
	free_memory(hlcolor_);
	free_memory(hlbackground_);

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
	procs.Insert("bind", new TkProc(this, "", glishtk_bind, glishtk_str));
	procs.Insert("unbind", new TkProc(this, "", glishtk_unbind));
	procs.Insert("bitmap", new TkProc(this, "-bitmap", glishtk_bitmap, glishtk_str));
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

TkButton::TkButton( ProxyStore *s, TkButton *frame_, charptr label_, charptr type_,
		    charptr /*padx*/, charptr /*pady*/, int width, int height, charptr /*justify*/,
		    charptr font, charptr /*relief*/, charptr /*borderwidth*/, charptr foreground,
		    charptr background, int disabled, const Value *val, charptr bitmap_,
		    TkFrame *group, charptr hlcolor, charptr hlbackground, charptr hlthickness )
			: TkFrame( s ), value(0), state(0), radio(group),
			  menu_base(0), next_menu_entry(0), menu_index(0), fill(0), unmapped(0)
	{
	type = PLAIN;

	menu = frame_;
	frame = 0;

	agent_ID = "<graphic:button>";

	if ( ! frame_->IsMenu() )
		HANDLE_CTOR_ERROR("internal error with creation of menu entry")

	if ( ! menu || ! menu->Self() ) return;

	char *argv[44];

	char width_[30];
	char height_[30];
	char var_name[256];
	char val_name[256];
	char *label = 0;
	char *bitmap = 0;

	char *foreground_ = glishtk_quote_string(foreground);
	char *background_ = glishtk_quote_string(background);
	char *hlcolor_ = glishtk_quote_string(hlcolor,0);
	char *hlbackground_ = glishtk_quote_string(hlbackground,0);

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
		label = glishtk_quote_string(label_);
		argv[c++] = label;
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
	argv[c++] = (char*) foreground_;
	argv[c++] = "-background";
	argv[c++] = (char*) background_;
	argv[c++] = "-state";
	argv[c++] = disabled ? "disabled" : "normal";
	if ( disabled ) disable_count++;

	if ( hlcolor_ && *hlcolor_ )
		{
		argv[c++] = "-activeforeground";
		argv[c++] = (char*) hlcolor_;
		}
	if ( hlbackground_ && *hlbackground_ )
		{
		argv[c++] = "-activebackground";
		argv[c++] = (char*) hlbackground_;
		}

	argv[c++] = "-command";
	argv[c++] = glishtk_make_callback( tcl, buttoncb, this );

	char *ctor_error = 0;
	switch ( type )
		{
		case RADIO:
			argv[0] = Tk_PathName(Menu());
			argv[2] = "radio";
			tcl_ArgEval( tcl, c, argv);
			ctor_error = Tcl_GetStringResult(tcl);
			if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);
			self = Menu();
			break;
		case CHECK:
			argv[0] = Tk_PathName(Menu());
			argv[2] = "check";
			tcl_ArgEval( tcl, c, argv);
			ctor_error = Tcl_GetStringResult(tcl);
			if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);
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
			ctor_error = Tcl_GetStringResult(tcl);
			if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);
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
			ctor_error = Tcl_GetStringResult(tcl);
			if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);
			self = Menu();
			break;
		}

	if ( bitmap ) free_memory(bitmap);
	if ( label ) free_memory(label);

	value = val ? copy_value( val ) : 0;
#ifdef GGC
	if ( value ) sequencer->RegisterValue( value );
#endif

	tcl_VarEval( tcl, Tk_PathName(menu->Menu()), " index last", (char *)NULL );
	menu_index = strdup( Tcl_GetStringResult(tcl) );

	free_memory(background_);
	free_memory(foreground_);
	free_memory(hlcolor_);
	free_memory(hlbackground_);

        menu->Add(this);

	procs.Insert("text", new TkProc(this, "-label", glishtk_menu_onestr, glishtk_str));
	procs.Insert("font", new TkProc(this, "-font", glishtk_menu_onestr, glishtk_str));
	procs.Insert("bitmap", new TkProc(this, "-bitmap", glishtk_bitmap, glishtk_str));
	procs.Insert("background", new TkProc(this, "-background", glishtk_menu_onestr, glishtk_str));
	procs.Insert("foreground", new TkProc(this, "-foreground", glishtk_menu_onestr, glishtk_str));
	procs.Insert("state", new TkProc(this, "", glishtk_button_state, glishtk_strtobool));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind, glishtk_str));
	procs.Insert("unbind", new TkProc(this, "", glishtk_unbind));

	procs.Insert("disabled", new TkProc(this, "", glishtk_disable_cb));
	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));
	}

void TkButton::update_menu_index( int i )
	{
	char buf[64];
	sprintf(buf,"%d",i);
	free_memory(menu_index);
	menu_index = strdup(buf);
	}

void TkButton::Remove( TkButton *item )
	{
	int do_update = 0;
	loop_over_list( entry_list, i )
		{
		if ( do_update )
			((TkButton*)entry_list[i])->update_menu_index(i-1);
		else if ( entry_list[i] == item )
			do_update = 1;
		}

	entry_list.remove(item);
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
		Unref(v);
		}
	else
		dont_invoke_button = 0;
	}

void TkButton::Create( ProxyStore *s, Value *args )
	{
	TkButton *ret=0;

	if ( args->Length() != 22 )
		InvalidNumberOfArgs(22);

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
	SETSTR( hlcolor )
	SETSTR( hlbackground )
	SETDIM( hlthickness )

	TkProxy *agent = (TkProxy*) (global_store->GetProxy(parent));
	TkProxy *grp = (TkProxy*) (global_store->GetProxy(group));
	if ( agent && grp && 
	     ( ! strcmp( grp->AgentID(),"<graphic:button>" ) && ((TkButton*)grp)->IsMenu() ||
	       ! strcmp( grp->AgentID(), "<graphic:frame>") ) )
		{
		if ( ! strcmp( agent->AgentID(), "<graphic:button>") &&
		     ((TkButton*)agent)->IsMenu() )
				ret =  new TkButton( s, (TkButton*)agent, label, type, padx, pady,
						     width, height, justify, font, relief, borderwidth,
						     foreground, background, disabled, val, bitmap,
						     (TkFrame*) grp, hlcolor, hlbackground, hlthickness );
		else if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
			ret =  new TkButton( s, (TkFrame*)agent, label, type, padx, pady, width, height,
					     justify, font, relief, borderwidth, foreground, background,
					     disabled, val, anchor, fill, bitmap, (TkFrame*) grp,
					     hlcolor, hlbackground, hlthickness );
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
			tcl_VarEval( tcl, Tk_PathName(self), " invoke", (char *)NULL );
		else if ( menu )
			tcl_VarEval( tcl, Tk_PathName(Menu()), " invoke ", Index(), (char *)NULL );
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

DEFINE_DTOR(TkScale,)

void TkScale::UnMap()
	{
	if ( self ) tcl_VarEval( tcl, Tk_PathName(self), " -command \"\"", (char *)NULL );
	TkProxy::UnMap();
	}

unsigned int TkScale::scale_count = 0;
int scalecb( ClientData data, Tcl_Interp *, int, char *argv[] )
	{
	((TkScale*)data)->ValueSet( atof(argv[1]) );
	return TCL_OK;
	}

char *glishtk_scale_ends(TkProxy *a, const char *cmd, Value *args )
	{
	char *ret = 0;
	Tcl_Interp *tcl = a->Interp();
	Tk_Window self = a->Self();
	TkScale *s = (TkScale*)a;

	if ( args->Length() > 0 )
		{
		double value = 0;
		if ( args->IsNumeric() )
			{
			char buf[30];
			sprintf(buf," %f",(value=args->DoubleVal()));
			tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, buf, (char *)NULL );
			}
		else if ( args->Type() == TYPE_STRING )
			{
			const char *str =  args->StringPtr(0)[0];
			tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, str, (char *)NULL );
			value = strtod( str, 0 );
			}

		if ( cmd[1] == 't' )
			s->SetEnd( value );
		else
			s->SetStart( value );

		}
	else
		{
		tcl_VarEval( tcl, Tk_PathName(self), " cget ", cmd, (char *)NULL );
		ret = Tcl_GetStringResult(tcl);
		}

	return ret;
	}

char *glishtk_scale_value(TkProxy *a, const char *, Value *args )
	{
	TkScale *s = (TkScale*)a;
	if ( args->IsNumeric() && args->Length() > 0 )
		s->SetValue( args->DoubleVal() );

	tcl_VarEval( s->Interp(), Tk_PathName(s->Self()), " get", (char *)NULL );
	return Tcl_GetStringResult(s->Interp());
	}


#define DEFINE_ENABLE_FUNCS(CLASS)				\
void CLASS::EnterEnable()					\
	{							\
	if ( ! enable_state ) 					\
		{						\
		tcl_VarEval( tcl, Tk_PathName(self), " cget -state", (char *)NULL ); \
		const char *curstate = Tcl_GetStringResult(tcl); \
		if ( ! strcmp("disabled", curstate ) )		\
			{					\
			enable_state++;				\
			tcl_VarEval( tcl, Tk_PathName(self),	\
				     " config -state normal", (char *)NULL ); \
			}					\
		}						\
	}							\
								\
void CLASS::ExitEnable()					\
	{							\
	if ( enable_state && --enable_state == 0 )		\
		tcl_VarEval( tcl, Tk_PathName(self),		\
			     " config -state disabled", (char *)NULL );	\
	}							\
								\
void CLASS::Disable( )						\
	{							\
	disable_count++;					\
	tcl_VarEval( tcl, Tk_PathName(self),			\
		     " config -state disabled", (char *)NULL );	\
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
	tcl_VarEval( tcl, Tk_PathName(self),			\
		     " config -state normal", (char *)NULL );	\
	}


DEFINE_ENABLE_FUNCS(TkScale)

TkScale::TkScale ( ProxyStore *s, TkFrame *frame_, double from, double to, double value, charptr len,
		   charptr text_, double resolution, charptr orient, int width, charptr font,
		   charptr relief, charptr borderwidth, charptr foreground, charptr background,
		   charptr fill_, charptr hlcolor, charptr hlbackground, charptr hlthickness )
			: TkProxy( s ), fill(0), from_(from), to_(to), discard_event(1)
	{
	char var_name[256];
	frame = frame_;
	char *argv[36];
	char from_c[40];
	char to_c[40];
	char resolution_[40];
	char width_[30];
	char *text = 0;
	id = ++scale_count;

	char *foreground_ = glishtk_quote_string(foreground);
	char *background_ = glishtk_quote_string(background);
	char *hlcolor_ = glishtk_quote_string(hlcolor,0);
	char *hlbackground_ = glishtk_quote_string(hlbackground,0);

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
	if ( text_ && *text_ )
		{
		argv[c++] = "-label";
		text = glishtk_quote_string(text_);
		argv[c++] = text;
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
	argv[c++] = (char*) foreground_;
	argv[c++] = "-bg";
	argv[c++] = (char*) background_;

	if ( hlcolor_ && *hlcolor_ )
		{
		argv[c++] = "-activebackground";
		argv[c++] = hlcolor_;
		argv[c++] = "-highlightcolor";
		argv[c++] = hlcolor_;
		}
	if ( hlbackground_ && *hlbackground_ )
		{
		argv[c++] = "-highlightbackground";
		argv[c++] = hlbackground_;
		}
 	if ( hlthickness && *hlthickness )
		{
		argv[c++] = "-highlightthickness";
		argv[c++] = (char*) hlthickness;
		}

	argv[c++] = "-variable";
	argv[c++] = var_name;

	tcl_ArgEval( tcl, c, argv );
	char *ctor_error = Tcl_GetStringResult(tcl);
	if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);

	self = Tk_NameToWindow( tcl, argv[1], root );

	if ( text ) free_memory(text);
	free_memory(background_);
	free_memory(foreground_);
	free_memory(hlcolor_);
	free_memory(hlbackground_);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkScale::TkScale")

	// Can't set command as part of initialization...
	tcl_VarEval( tcl, Tk_PathName(self), " config -command ", glishtk_make_callback( tcl, scalecb, this ), (char *)NULL );

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);
	else if ( fill_ && ! fill_[0] )
		fill = (orient && ! strcmp(orient,"vertical")) ? strdup("y") :
				(orient && ! strcmp(orient,"horizontal")) ? strdup("x") : 0;

	frame->AddElement( this );
	frame->Pack();

	if ( value < from || value > to ) value = from;
	SetValue(value);

	procs.Insert("bind", new TkProc(this, "", glishtk_bind, glishtk_str));
	procs.Insert("unbind", new TkProc(this, "", glishtk_unbind));
	procs.Insert("font", new TkProc("-font", glishtk_onestr, glishtk_str));
	procs.Insert("length", new TkProc("-length", glishtk_onedim, glishtk_strtoint));
	procs.Insert("orient", new TkProc("-orient", glishtk_onestr, glishtk_str));
	procs.Insert("resolution", new TkProc("-resolution", glishtk_onedouble));
	procs.Insert("end", new TkProc(this, "-to", glishtk_scale_ends, glishtk_strtofloat));
	procs.Insert("start", new TkProc(this, "-from", glishtk_scale_ends, glishtk_strtofloat));
	procs.Insert("text", new TkProc("-label", glishtk_onestr, glishtk_str));
	procs.Insert("value", new TkProc(this, "", glishtk_scale_value, glishtk_strtofloat));
	procs.Insert("width", new TkProc("-width", glishtk_onedim, glishtk_strtoint));
	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("disabled", new TkProc(this, "", glishtk_disable_cb));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));
	}

void TkScale::ValueSet( double d )
	{
	if ( ! discard_event )
		{
		Value *v = new Value( d );
		PostTkEvent( "value", v );
		Unref(v);
		}
	discard_event = 0;
	}

void TkScale::SetValue( double d )
	{
	discard_event = 1;
	if ( d >= from_ && d <= to_ ||
	     d <= from_ && d >= to_ )
		{
		char val[256];
		sprintf(val,"%g",d);
		tcl_VarEval( tcl, Tk_PathName(self), " set ", val, (char *)NULL );
		}
	}

void TkScale::SetStart( double d ) { to_ = d; }
void TkScale::SetEnd( double d ) { from_ = d; }

void TkScale::Create( ProxyStore *s, Value *args )
	{
	TkScale *ret;

	if ( args->Length() != 18 )
		InvalidNumberOfArgs(18);

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
	SETSTR( hlcolor )
	SETSTR( hlbackground )
	SETDIM( hlthickness )

	TkProxy *agent = (TkProxy*) (global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret = new TkScale( s, (TkFrame*)agent, start, end, value, len, text, resolution, orient, width, font, relief, borderwidth, foreground, background, fill, hlcolor, hlbackground, hlthickness );
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

DEFINE_DTOR(TkText, if ( fill ) free_memory(fill); )

void TkText::UnMap()
	{
	if ( self )
		{
		tcl_VarEval( tcl, Tk_PathName(self), " config -xscrollcommand \"\"", (char *)NULL );
		tcl_VarEval( tcl, Tk_PathName(self), " config -yscrollcommand \"\"", (char *)NULL );
		}

	TkProxy::UnMap();
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


DEFINE_ENABLE_FUNCS(TkText)

TkText::TkText( ProxyStore *s, TkFrame *frame_, int width, int height, charptr wrap,
		charptr font, int disabled, charptr text, charptr relief,
		charptr borderwidth, charptr foreground, charptr background,
		charptr fill_, charptr hlcolor, charptr hlbackground, charptr hlthickness )
			: TkProxy( s ), fill(0)
	{
	frame = frame_;
	char *argv[30];

	char *foreground_ = glishtk_quote_string(foreground);
	char *background_ = glishtk_quote_string(background);
	char *hlcolor_ = glishtk_quote_string(hlcolor,0);
	char *hlbackground_ = glishtk_quote_string(hlbackground,0);

	agent_ID = "<graphic:text>";

	if ( ! frame || ! frame->Self() ) return;

	const char *nme = NewName(frame->Self());
	tcl_VarEval( tcl, "text ", nme, (char *)NULL );
	char *ctor_error = Tcl_GetStringResult(tcl);
	if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);

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
	argv[c++] = (char*) foreground_;
	argv[c++] = "-bg";
	argv[c++] = (char*) background_;
	argv[c++] = "-state";
	argv[c++] = disabled ? "disabled" : "normal";
	if ( disabled ) disable_count++;

	if ( font[0] )
		{
		argv[c++] = "-font";
		argv[c++] = (char*) font;
		}

	if ( hlcolor_ && *hlcolor_ )
		{
		argv[c++] = "-highlightcolor";
		argv[c++] = (char*) hlcolor_;
		}
	if ( hlbackground_ && *hlbackground_ )
		{
		argv[c++] = "-highlightbackground";
		argv[c++] = (char*) hlbackground_;
		}
	if ( hlthickness && *hlthickness )
		{
		argv[c++] = "-highlightthickness";
		argv[c++] = (char*) hlthickness;
		}

	char ys[100];
	argv[c++] = "-yscrollcommand";
	argv[c++] = glishtk_make_callback( tcl, text_yscrollcb, this, ys );
	argv[c++] = "-xscrollcommand";
	argv[c++] = glishtk_make_callback( tcl, text_xscrollcb, this );

	tcl_ArgEval( tcl, c, argv );

	free_memory(background_);
	free_memory(foreground_);
	free_memory(hlcolor_);
	free_memory(hlbackground_);

	if ( text[0] )
		tcl_VarEval( tcl, Tk_PathName(self), " insert end {", text, "}", (char *)NULL );

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("addtag", new TkProc("tag", "add", glishtk_text_tagfunc));
	procs.Insert("append", new TkProc(this, "insert", "end", glishtk_text_append));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind, glishtk_str));
	procs.Insert("unbind", new TkProc(this, "", glishtk_unbind));
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

	if ( args->Length() != 15 )
		InvalidNumberOfArgs(15);

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
	SETSTR( hlcolor )
	SETSTR( hlbackground )
	SETDIM( hlthickness )

	TkProxy *agent = (TkProxy*)(global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		{
		char *text_str = text->StringVal( ' ', 0, 1 );
		ret =  new TkText( s, (TkFrame*)agent, width, height, wrap, font, disabled, text_str, relief, borderwidth, foreground, background, fill, hlcolor, hlbackground, hlthickness );
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
	Value *v = new Value( (double*) d, 2, COPY_ARRAY );
	PostTkEvent( "yscroll", v );
	Unref(v);
	}

void TkText::xScrolled( const double *d )
	{
	Value *v = new Value( (double*) d, 2, COPY_ARRAY );
	PostTkEvent( "xscroll", v );
	Unref(v);
	}

charptr TkText::IndexCheck( charptr s )
	{
	if ( s && s[0] == 's' && ! strcmp(s,"start") )
		return "1.0";
	return s;
	}

STD_EXPAND_PACKINSTRUCTION(TkText)
STD_EXPAND_CANEXPAND(TkText)

DEFINE_DTOR(TkScrollbar,)

void TkScrollbar::EnterEnable()
	{
	}

void TkScrollbar::ExitEnable()
	{
	}

void TkScrollbar::Disable( )
	{
	disable_count++;
	}

void TkScrollbar::Enable( int force )
	{
	if ( disable_count <= 0 ) return;

	if ( force )
		disable_count = 0;
	else
		disable_count--;
	}

void TkScrollbar::UnMap()
	{
	if ( self ) tcl_VarEval( tcl, Tk_PathName(self), " -command \"\"", (char *)NULL );
	TkProxy::UnMap();
	}

int scrollbarcb( ClientData data, Tcl_Interp *tcl, int argc, char *argv[] )
	{
	char buf[256];
	int vert = 0;
	TkScrollbar *sb = (TkScrollbar*)data;

	if ( ! sb->Enabled() )
		return TCL_OK;

	tcl_VarEval( tcl, Tk_PathName(sb->Self()), " cget -orient", (char *)NULL );
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

	Value *v = new Value( buf );
	sb->Scrolled( v );
	Unref(v);

	return TCL_OK;
	}

TkScrollbar::TkScrollbar( ProxyStore *s, TkFrame *frame_, charptr orient,
			  int width, charptr foreground, charptr background, int jump,
			  charptr hlcolor, charptr hlbackground, charptr hlthickness )
				: TkProxy( s )
	{
	frame = frame_;
	char *argv[16];

	char *hlcolor_ = glishtk_quote_string(hlcolor,0);
	char *hlbackground_ = glishtk_quote_string(hlbackground,0);

	agent_ID = "<graphic:scrollbar>";

	if ( ! frame || ! frame->Self() ) return;

	char width_[14];
	sprintf(width_,"%d", width);

	int c = 0;
	argv[c++] = "scrollbar";
	argv[c++] = (char*) NewName(frame->Self());
	argv[c++] = "-orient";
	argv[c++] = (char*) orient;
	argv[c++] = "-width";
	argv[c++] = width_;

	if ( jump )
		{
		argv[c++] = "-jump";
		argv[c++] = "true";
		}

	if ( hlcolor_ && *hlcolor_ )
		{
		argv[c++] = "-highlightcolor";
		argv[c++] = (char*) hlcolor_;
		}
	if ( hlbackground_ && *hlbackground_ )
		{
		argv[c++] = "-highlightbackground";
		argv[c++] = (char*) hlbackground_;
		}
	if ( hlthickness && *hlthickness )
		{
		argv[c++] = "-highlightthickness";
		argv[c++] = (char*) hlthickness;
		}

	argv[c++] = "-command";
	argv[c++] = glishtk_make_callback( tcl, scrollbarcb, this );

	tcl_ArgEval( tcl, c, argv );
	char *ctor_error = Tcl_GetStringResult(tcl);
	if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);

	self = Tk_NameToWindow( tcl, argv[1], root );

	free_memory(hlcolor_);
	free_memory(hlbackground_);
	
	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkScrollbar::TkScrollbar")

	// Setting foreground and background colors at creation
	// time kills the goose
	tcl_VarEval( tcl, Tk_PathName(self), " -bg {", background, "}", (char *)NULL );
	tcl_VarEval( tcl, Tk_PathName(self), " -fg {", foreground, "}", (char *)NULL );

	frame->AddElement( this );
	frame->Pack();

// 	rivet_scrollbar_set( self, 0.0, 1.0 );

	procs.Insert("view", new TkProc("", glishtk_scrollbar_update));
	procs.Insert("orient", new TkProc("-orient", glishtk_onestr, glishtk_str));
	procs.Insert("width", new TkProc("-width", glishtk_onedim, glishtk_strtoint));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind, glishtk_str));
	procs.Insert("unbind", new TkProc(this, "", glishtk_unbind));
	procs.Insert("jump", new TkProc("-jump", glishtk_onebool));
	procs.Insert("disable", new TkProc( this, "1", glishtk_disable_cb ));
	procs.Insert("disabled", new TkProc(this, "", glishtk_disable_cb));
	procs.Insert("enable", new TkProc( this, "0", glishtk_disable_cb ));
	}

const char **TkScrollbar::PackInstruction()
	{
	static char *ret[7];
	ret[0] = "-fill";
	tcl_VarEval( tcl, Tk_PathName(self), " cget -orient", (char *)NULL );
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

	if ( args->Length() != 9 )
		InvalidNumberOfArgs(9);

	SETINIT
	SETVAL( parent, parent->IsAgentRecord() )
	SETSTR( orient )
	SETINT( width )
	SETSTR( foreground )
	SETSTR( background )
	SETINT( jump )
	SETSTR( hlcolor )
	SETSTR( hlbackground )
	SETDIM( hlthickness )

	TkProxy *agent = (TkProxy*)(global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret = new TkScrollbar( s, (TkFrame*)agent, orient, width, foreground,
				       background, jump, hlcolor, hlbackground, hlthickness );
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


DEFINE_DTOR(TkLabel,)

TkLabel::TkLabel( ProxyStore *s, TkFrame *frame_, charptr text_, charptr justify,
		  charptr padx, charptr pady, int width_, charptr font, charptr relief,
		  charptr borderwidth, charptr foreground, charptr background,
		  charptr anchor, charptr fill_, charptr hlcolor, charptr hlbackground,
		  charptr hlthickness )
			: TkProxy( s ), fill(0)
	{
	frame = frame_;
	char *argv[30];
	char width[30];
	char *text = 0;

	char *foreground_ = glishtk_quote_string(foreground);
	char *background_ = glishtk_quote_string(background);
	char *hlcolor_ = glishtk_quote_string(hlcolor,0);
	char *hlbackground_ = glishtk_quote_string(hlbackground,0);

	agent_ID = "<graphic:label>";

	if ( ! frame || ! frame->Self() ) return;

	sprintf(width,"%d",width_);

	int c = 0;
	argv[c++] = "label";
	argv[c++] = (char*) NewName(frame->Self());
	argv[c++] = "-text";
	text = glishtk_quote_string(text_);
	argv[c++] = text;
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
	argv[c++] = (char*) foreground_;
	argv[c++] = "-bg";
	argv[c++] = (char*) background_;
	argv[c++] = "-width";
	argv[c++] = width;
	argv[c++] = "-anchor";
	argv[c++] = (char*) anchor;
	if ( hlcolor_ && *hlcolor_ )
		{
		argv[c++] = "-highlightcolor";
		argv[c++] = (char*) hlcolor_;
		}
	if ( hlbackground_ && *hlbackground_ )
		{
		argv[c++] = "-highlightbackground";
		argv[c++] = (char*) hlbackground_;
		}
	if ( hlthickness && *hlthickness )
		{
		argv[c++] = "-highlightthickness";
		argv[c++] = (char*) hlthickness;
		}

	tcl_ArgEval( tcl, c, argv );
	char *ctor_error = Tcl_GetStringResult(tcl);
	if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);

	self = Tk_NameToWindow( tcl, argv[1], root );

	if ( text ) free_memory(text);
	free_memory(background_);
	free_memory(foreground_);
	free_memory(hlcolor_);
	free_memory(hlbackground_);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkLabel::TkLabel")

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("anchor", new TkProc("-anchor", glishtk_onestr, glishtk_str));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind, glishtk_str));
	procs.Insert("unbind", new TkProc(this, "", glishtk_unbind));
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

	if ( args->Length() != 16 )
		InvalidNumberOfArgs(16);

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
	SETSTR( hlcolor )
	SETSTR( hlbackground )
	SETDIM( hlthickness )

	TkProxy *agent = (TkProxy*)(global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret =  new TkLabel( s, (TkFrame*)agent, text, justify, padx, pady, width,
				    font, relief, borderwidth, foreground, background,
				    anchor, fill, hlcolor, hlbackground, hlthickness );
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


DEFINE_DTOR(TkEntry,)

void TkEntry::UnMap()
	{
	if ( self ) tcl_VarEval( tcl, Tk_PathName(self), " config -xscrollcommand \"\"", (char *)NULL );
	TkProxy::UnMap();
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
		  int disabled, int show, int exportselection, charptr fill_,
		  charptr hlcolor, charptr hlbackground, charptr hlthickness)
			: TkProxy( s ), fill(0)
	{
	frame = frame_;
	char *argv[30];
	char width_[30];

	char *foreground_ = glishtk_quote_string(foreground);
	char *background_ = glishtk_quote_string(background);
	char *hlcolor_ = glishtk_quote_string(hlcolor,0);
	char *hlbackground_ = glishtk_quote_string(hlbackground,0);

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
	argv[c++] = (char*) foreground_;
	argv[c++] = "-bg";
	argv[c++] = (char*) background_;
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
	if ( hlcolor_ && *hlcolor_ )
		{
		argv[c++] = "-highlightcolor";
		argv[c++] = (char*) hlcolor_;
		}
	if ( hlbackground_ && *hlbackground_ )
		{
		argv[c++] = "-highlightbackground";
		argv[c++] = (char*) hlbackground_;
		}
	if ( hlthickness && *hlthickness )
		{
		argv[c++] = "-highlightthickness";
		argv[c++] = (char*) hlthickness;
		}
	argv[c++] = "-xscrollcommand";
	argv[c++] = glishtk_make_callback( tcl, entry_xscrollcb, this );

	tcl_ArgEval( tcl, c, argv );
	char *ctor_error = Tcl_GetStringResult(tcl);
	if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);

	self = Tk_NameToWindow( tcl, argv[1], root );

	free_memory(background_);
	free_memory(foreground_);
	free_memory(hlcolor_);
	free_memory(hlbackground_);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkEntry::TkEntry")

	tcl_VarEval( tcl, "bind ", Tk_PathName(self), " <Return> ", glishtk_make_callback( tcl, entry_returncb, this ), (char *)NULL );

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("bind", new TkProc(this, "", glishtk_bind, glishtk_str));
	procs.Insert("unbind", new TkProc(this, "", glishtk_unbind));
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
	tcl_VarEval( tcl, Tk_PathName(self), " cget -state", (char *)NULL );
	const char *curstate = Tcl_GetStringResult(tcl);
	if ( strcmp("disabled", curstate) )
		{
		tcl_VarEval( tcl, Tk_PathName(self), " get", (char *)NULL );
		Value *ret = new Value( Tcl_GetStringResult(tcl) );
		PostTkEvent( "return", ret );
		Unref(ret);
		}
	}

void TkEntry::xScrolled( const double *d )
	{
	Value *v = new Value( (double*) d, 2, COPY_ARRAY );
	PostTkEvent( "xscroll", v );
	Unref(v);
	}

void TkEntry::Create( ProxyStore *s, Value *args )
	{
	TkEntry *ret;

	if ( args->Length() != 15 )
		InvalidNumberOfArgs(15);

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
	SETSTR( hlcolor )
	SETSTR( hlbackground )
	SETDIM( hlthickness )

	TkProxy *agent = (TkProxy*)(global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret =  new TkEntry( s, (TkFrame*)agent, width, justify,
				    font, relief, borderwidth, foreground, background,
				    disabled, show, exp, fill, hlcolor, hlbackground,
				    hlthickness );
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

DEFINE_DTOR(TkMessage,)

TkMessage::TkMessage( ProxyStore *s, TkFrame *frame_, charptr text_, charptr width, charptr justify,
		      charptr font, charptr padx, charptr pady, charptr relief, charptr borderwidth,
		      charptr foreground, charptr background, charptr anchor, charptr fill_,
		      charptr hlcolor, charptr hlbackground, charptr hlthickness )
			: TkProxy( s ), fill(0)
	{
	frame = frame_;
	char *argv[30];
	char *text = 0;

	char *foreground_ = glishtk_quote_string(foreground);
	char *background_ = glishtk_quote_string(background);
	char *hlcolor_ = glishtk_quote_string(hlcolor,0);
	char *hlbackground_ = glishtk_quote_string(hlbackground,0);

	agent_ID = "<graphic:message>";

	if ( ! frame || ! frame->Self() ) return;

	int c = 2;
	argv[0] = "message";
	argv[1] = (char*) NewName( frame->Self() );
	argv[c++] = "-text";
	text = glishtk_quote_string(text_);
	argv[c++] = text;
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
	argv[c++] = (char*) foreground_;
	argv[c++] = "-bg";
	argv[c++] = (char*) background_;
	argv[c++] = "-anchor";
	argv[c++] = (char*) anchor;
	if ( hlcolor_ && *hlcolor_ )
		{
		argv[c++] = "-highlightcolor";
		argv[c++] = (char*) hlcolor_;
		}
	if ( hlbackground_ && *hlbackground_ )
		{
		argv[c++] = "-highlightbackground";
		argv[c++] = (char*) hlbackground_;
		}
	if ( hlthickness && *hlthickness )
		{
		argv[c++] = "-highlightthickness";
		argv[c++] = (char*) hlthickness;
		}

	tcl_ArgEval( tcl, c, argv );
	char *ctor_error = Tcl_GetStringResult(tcl);
	if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);

	self = Tk_NameToWindow( tcl, argv[1], root );

	if ( text ) free_memory(text);
	free_memory(background_);
	free_memory(foreground_);
	free_memory(hlcolor_);
	free_memory(hlbackground_);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkMessage::TkMessage")

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("anchor", new TkProc("-anchor", glishtk_onestr, glishtk_str));
	procs.Insert("bind", new TkProc(this, "", glishtk_bind, glishtk_str));
	procs.Insert("unbind", new TkProc(this, "", glishtk_unbind));
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

	if ( args->Length() != 16 )
		InvalidNumberOfArgs(16);

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
	SETSTR( hlcolor )
	SETSTR( hlbackground )
	SETDIM( hlthickness )

	TkProxy *agent = (TkProxy*)(global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret =  new TkMessage( s, (TkFrame*)agent, text, width, justify, font, padx, pady, relief, borderwidth,
				      foreground, background, anchor, fill, hlcolor, hlbackground, hlthickness );
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

DEFINE_DTOR(TkListbox,)

void TkListbox::UnMap()
	{
	if ( self )
		{
		tcl_VarEval( tcl, Tk_PathName(self), " config -xscrollcommand \"\"", (char *)NULL );
		tcl_VarEval( tcl, Tk_PathName(self), " config -yscrollcommand \"\"", (char *)NULL );
		}

	TkProxy::UnMap();
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
		      charptr foreground, charptr background, int exportselection, charptr fill_,
		      charptr hlcolor, charptr hlbackground, charptr hlthickness )
			: TkProxy( s ), fill(0)
	{
	frame = frame_;
	char *argv[30];
	char width_[40];
	char height_[40];

	char *foreground_ = glishtk_quote_string(foreground);
	char *background_ = glishtk_quote_string(background);
	char *hlcolor_ = glishtk_quote_string(hlcolor,0);
	char *hlbackground_ = glishtk_quote_string(hlbackground,0);

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
	argv[c++] = (char*) foreground_;
	argv[c++] = "-bg";
	argv[c++] = (char*) background_;
	argv[c++] = "-exportselection";
	argv[c++] = exportselection ? "true" : "false";

	if ( hlcolor_ && *hlcolor_ )
		{
		argv[c++] = "-highlightcolor";
		argv[c++] = (char*) hlcolor_;
		}
	if ( hlbackground_ && *hlbackground_ )
		{
		argv[c++] = "-highlightbackground";
		argv[c++] = (char*) hlbackground_;
		}
	if ( hlthickness && *hlthickness )
		{
		argv[c++] = "-highlightthickness";
		argv[c++] = (char*) hlthickness;
		}

	char ys[100];
	argv[c++] = "-yscrollcommand";
	argv[c++] = glishtk_make_callback( tcl, listbox_yscrollcb, this, ys );
	argv[c++] = "-xscrollcommand";
	argv[c++] = glishtk_make_callback( tcl, listbox_xscrollcb, this );

	tcl_ArgEval( tcl, c, argv );
	char *ctor_error = Tcl_GetStringResult(tcl);
	if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);

	self = Tk_NameToWindow( tcl, argv[1], root );

	free_memory(background_);
	free_memory(foreground_);
	free_memory(hlcolor_);
	free_memory(hlbackground_);

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkListbox::TkListbox")

	tcl_VarEval( tcl, "bind ", Tk_PathName(self), " <ButtonRelease-1> ", glishtk_make_callback( tcl, listbox_button1cb, this ), (char *)NULL );

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("bind", new TkProc(this, "", glishtk_bind, glishtk_str));
	procs.Insert("unbind", new TkProc(this, "", glishtk_unbind));
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

	if ( args->Length() != 14 )
		InvalidNumberOfArgs(14);

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
	SETSTR( hlcolor )
	SETSTR( hlbackground )
	SETDIM( hlthickness )

	TkProxy *agent = (TkProxy*)(global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret =  new TkListbox( s, (TkFrame*)agent, width, height, mode, font, relief, borderwidth,
				      foreground, background, exp, fill, hlcolor, hlbackground, hlthickness );
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
	Value *v = new Value( (double*) d, 2, COPY_ARRAY );
	PostTkEvent( "yscroll", v );
	Unref(v);
	}

void TkListbox::xScrolled( const double *d )
	{
	Value *v = new Value( (double*) d, 2, COPY_ARRAY );
	PostTkEvent( "xscroll", v );
	Unref(v);
	}

void TkListbox::elementSelected(  )
	{
	tcl_VarEval( tcl, Tk_PathName(self), " curselection", (char *)NULL );
	Value *v = glishtk_splitsp_int(Tcl_GetStringResult(tcl));
	PostTkEvent( "select", v );
	Unref(v);
	}

STD_EXPAND_PACKINSTRUCTION(TkListbox)
STD_EXPAND_CANEXPAND(TkListbox)
