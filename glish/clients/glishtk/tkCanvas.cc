// $Id$
// Copyright (c) 1997 Associated Universities Inc.
//
#include "Glish/glish.h"
RCSID("@(#) $Id$")

#include "tkCanvas.h"

#include <string.h>
#include <stdlib.h>
#include "Reporter.h"
#include "Glish/Value.h"
#include "system.h"
#include "comdefs.h"

extern ProxyStore *global_store;

#define GENERATE_TAG(BUFFER,CANVAS,TYPE) 		\
	sprintf(BUFFER,"c%x%s%x",CANVAS->CanvasCount(),TYPE,CANVAS->NewItemCount(TYPE));

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

int TkCanvas::count = 0;

DEFINE_DTOR(TkCanvas)

//
// These variables are made file static to allow them to be shared by
// the canvas functions. This was done to minimize allocations.
//
static int Argv_len = 0;
static char **Arg_name = 0;
static char **Arg_val = 0;
static char **Argv = 0;

#define CANVAS_FUNC_REALLOC(size)							\
	if ( size >= Argv_len )								\
		{									\
		while ( size >= Argv_len ) Argv_len *= 2;				\
		Arg_name = (char**) realloc_memory( Arg_name, Argv_len * sizeof(char*) );\
		Arg_val = (char**) realloc_memory( Arg_val, Argv_len * sizeof(char*) );	\
		Argv = (char**) realloc_memory( Argv, Argv_len * sizeof(char*) );		\
		}

Value *glishtk_StrToInt( char *str )
	{
	int i = atoi(str);
	return new Value( i );
	}

char *glishtk_heightwidth_query(Tcl_Interp *tcl, Tk_Window self, const char *cmd, Value *args )
	{
	static char buf[256];

	if ( args->Type() == TYPE_STRING )
		tcl_VarEval( tcl, Tk_PathName(self), SP, cmd, SP, args->StringPtr(0)[0], 0 );
	else if ( args->Length() > 0 && args->IsNumeric() )
		{
		char buf[30];
		sprintf(buf," %d",args->IntVal());
		tcl_VarEval( tcl, Tk_PathName(self), SP, cmd, buf, 0 );
		}

	tcl_VarEval( tcl, "winfo ", &cmd[1], SP, Tk_PathName(self), 0 );
	int width = atoi(Tcl_GetStringResult(tcl));
	tcl_VarEval( tcl, Tk_PathName(self), " cget -borderwidth", 0 );
	int bdwidth = atoi(Tcl_GetStringResult(tcl));
	sprintf( buf, "%d", width - 2*bdwidth - 4 );
	
	return buf;
	}

char *glishtk_oneintlist_query(Tcl_Interp *tcl, Tk_Window self, const char *cmd, int howmany, Value *args )
	{
	char *event_name = "one int list function";
	if ( args->Length() >= howmany )
		{
		static int len = 4;
		static char *buf = (char*) alloc_memory( sizeof(char)*(len*128) );
		static char elem[128];

		if ( ! howmany )
			howmany = args->Length();

		while ( howmany > len )
			{
			len *= 2;
			buf = (char *) realloc_memory(buf, len * sizeof(char) * 128);
			}

		EXPRINIT( event_name)
		buf[0] = '\0';
		for ( int x=0; x < howmany; x++ )
			{
			EXPRINT( v, event_name )
			sprintf(elem,"%d ",v);
			strcat(buf,elem);
			EXPR_DONE( v )
			}

		tcl_VarEval( tcl, Tk_PathName(self), " config ", cmd, " {", buf, "}", 0 );
		}

	tcl_VarEval( tcl, Tk_PathName(self), " cget ", cmd, 0 );
	return Tcl_GetStringResult(tcl);
	}


char *glishtk_canvas_1toNint(Tcl_Interp *tcl, Tk_Window self, const char *cmd, int howmany, Value *args )
	{
	char *ret = 0;
	char *event_name = "one int list function";

	if ( args->Type() == TYPE_RECORD )
		{
		HASARG( args, > 1 )
		int len = args->Length() < howmany ? args->Length() : howmany;
		CANVAS_FUNC_REALLOC(len+2)
		static char buff[128];
		int argc = 0;

		Argv[argc++] = Tk_PathName(self);
		Argv[argc++] = (char*) cmd;
		EXPRINIT( event_name)
		for ( int i=0; i < len; i++ )
			{
			EXPRINT( v, event_name )
			sprintf(buff,"%d",v);
			Argv[argc++] = strdup(buff);
			EXPR_DONE( v )
			}

		tcl_ArgEval( tcl, argc, Argv );
		ret = Tcl_GetStringResult(tcl);

		for ( int x=0; x < len; x++ )
			free_memory( Argv[x + 2] );
		}
	else if ( args->Length() > 0 && args->IsNumeric() )
		{
		char buf[30];
		sprintf(buf," %d",args->IntVal());
		tcl_VarEval( tcl, Tk_PathName(self), SP, buf, 0 );
		}

	return ret;
	}

char *glishtk_canvas_tagfunc(Tcl_Interp *tcl, Tk_Window self, const char *cmd, const char *subcmd,
				int howmany, Value *args )
	{
	if ( args->Length() <= 0 )
		global_store->Error("zero length value");
	if ( args->Type() == TYPE_RECORD && args->Length() >= howmany )
		{
		recordptr rptr = args->RecordPtr(0);
		const Value *strv = rptr->NthEntry( 0 );
		if ( strv->Type() != TYPE_STRING )
			{
			global_store->Error("wrong type, string expected for argument 1");
			return 0;
			}
		const char *str = strv->StringPtr(0)[0];
		for ( int i=1; i < args->Length(); ++i )
			{
			const Value *val = rptr->NthEntry( i );
			if ( val->Type() == TYPE_STRING )
				if ( subcmd )
					tcl_VarEval( tcl, Tk_PathName(self), SP, cmd, SP, str, SP,
						     subcmd, SP, val->StringPtr(0)[0], 0 );
				else
					tcl_VarEval( tcl, Tk_PathName(self), SP, cmd, SP, str, SP,
						     val->StringPtr(0)[0], 0 );
			}
		}
	else if ( args->Type() == TYPE_STRING && args->Length() >= howmany )
		{
		charptr *ary = args->StringPtr(0);
		if ( args->Length() == 1 )
			tcl_VarEval( tcl, Tk_PathName(self), SP, cmd, SP, ary[0], 0 );
		else
			for ( int i=1; i < args->Length(); ++i )
				if ( subcmd )
					tcl_VarEval( tcl, Tk_PathName(self), SP, cmd, SP, ary[0],
						     SP, subcmd, SP, ary[i], 0 );
				else
					tcl_VarEval( tcl, Tk_PathName(self), SP, cmd, SP, ary[0],
						     SP, ary[i], 0 );
		}

	return 0;
	}


#define POINTFUNC_TAG_APPEND(STR)					\
if ( tagstr_cnt+(int)strlen(STR)+5 >= tagstr_len )			\
	{								\
	while ( tagstr_cnt+(int)strlen(STR)+5 >= tagstr_len ) tagstr_len *= 2; \
	tagstr = (char*) realloc_memory( tagstr, tagstr_len * sizeof(char));\
	}								\
if ( tagstr_cnt ) { strcat(tagstr, " "); tagstr_cnt++; }		\
strcat(tagstr, STR);							\
tagstr_cnt += strlen(STR);

#define POINTFUNC_NAMED_ACTION				 		\
									\
if ( ! val || val ->Type() != TYPE_STRING ||				\
	val->Length() <= 0 )						\
	{								\
	global_store->Error("bad value: one string + n int function");	\
	return 0;							\
	}								\
									\
if ( strcmp(key,"tag") )						\
	{								\
	Arg_name[name_cnt] = (char*) alloc_memory( 			\
			sizeof(char)*(strlen(key)+2) );			\
	sprintf(Arg_name[name_cnt],"-%s",key);				\
	const char *str = ( val->StringPtr(0) )[0];			\
	Arg_val[name_cnt] = (char*) alloc_memory(strlen(str)+3);	\
	sprintf(Arg_val[name_cnt++],"{%s}",str);			\
	}								\
else									\
	{								\
	char *str = val->StringVal();					\
	POINTFUNC_TAG_APPEND(str)					\
	free_memory( str );						\
	}

char *glishtk_canvas_pointfunc(TkProxy *agent_, const char *cmd, const char *param, Value *args )
	{
	char buf[50];
	int rows;
	char *event_name = "one string + n int function";
	TkCanvas *agent = (TkCanvas*)agent_;
	HASARG( args, > 0 )
	static char tag[256];
	static int tagstr_len = 512;
	static char *tagstr = (char*) alloc_memory( sizeof(char)*tagstr_len );
	int tagstr_cnt = 0;
	int name_cnt = 0;

	tagstr[0] = '{';
	tagstr[1] = '\0';
	EXPRINIT( event_name)
	int elements = 0;
	EXPRVAL(val,event_name)
	int argc = 3;
	const Value *shape_val = val->ExistingAttribute( "shape" );
	if ( (shape_val == false_value || shape_val->Length() == 1 || 
	     ! shape_val->IsNumeric()) && val->Length() == 1 )
		{
	        c = 0;
		elements = (*args).Length();
		CANVAS_FUNC_REALLOC(elements*2+argc+2)

		for (int i = 0; i < (*args).Length(); i++)
			{
			EXPRVAL(val,event_name)
			if ( strncmp( key, "arg", 3 ) )
				{
				POINTFUNC_NAMED_ACTION
				}
			else
				{
			        char buf[30];
				charptr str = 0;
				if ( ! val || val->Length() <= 0 )
					{
					global_store->Error("bad value: one string + n int function");
					return 0;
					}

				if ( val->IsNumeric() )
					{
					int iv = val->IntVal();
					str = buf;
					sprintf(buf,"%d",iv);
					}
				else if ( val->Type() == TYPE_STRING )
					str = ( val->StringPtr(0) )[0];
				else
					{
					global_store->Error("bad value: one string + n int function");
					return 0;
					}
				Argv[argc++] = strdup(str);
				}
			}
		}
	else if ( shape_val != false_value && shape_val->IsNumeric() &&
		  shape_val->Length() == 2 && shape_val->IntVal(2) == 2 &&
		  val->IsNumeric() )
		{
		rows = shape_val->IntVal();
		elements = rows*2;
		CANVAS_FUNC_REALLOC( elements+argc+2+(*args).Length()*2 )
		Value *newval = copy_value(val);
		newval->Polymorph(TYPE_INT);
		int *ip = newval->IntPtr(0);
		int i;
		for ( i=0; i < rows; i++)
			{
			sprintf(buf,"%d",ip[i]);
			Argv[argc++] = strdup(buf);
			sprintf(buf,"%d",ip[i+rows]);
			Argv[argc++] = strdup(buf);
			}
		Unref(newval);
		for (i = c; i < (*args).Length(); i++)
			{
			EXPRVAL(val,event_name)
			if ( strncmp( key, "arg", 3 ) )
				{
				POINTFUNC_NAMED_ACTION
				}
			else
				c++;
			}
		}
	else if ( val->Length() > 1 && val->IsNumeric() )
		{
		Value *newval = copy_value(val);
		elements = val->Length();
		CANVAS_FUNC_REALLOC(elements+argc+2+(*args).Length()*2)
		newval->Polymorph(TYPE_INT);
		int *ip = newval->IntPtr(0);
		int i;
		for (i=0; i < val->Length(); i++)
			{
			sprintf(buf,"%d",ip[i]);
			Argv[argc++] = strdup(buf);
			}
		Unref(newval);
		for (i = c; i < (*args).Length(); i++)
			{
			EXPRVAL(val,event_name)
			if ( strncmp( key, "arg", 3 ) )
				{
				POINTFUNC_NAMED_ACTION
				}
			else
				c++;
			}
		}
	else
		{
		EXPR_DONE(val)
		return 0;
		}

	EXPR_DONE(val)
	GENERATE_TAG(tag,agent,param)

	POINTFUNC_TAG_APPEND(tag)

	for ( int x=0; x < name_cnt; x++ )
		{
		Argv[argc++] = Arg_name[x];
		Argv[argc++] = Arg_val[x];
		}

	Argv[0] = Tk_PathName( agent->Self() );
	Argv[1] = (char*) cmd;
	Argv[2] = (char*) param;
	Argv[argc++] = (char*) "-tag";
	strcat( tagstr, "}" );
	Argv[argc++] = (char*) tagstr;

	tcl_ArgEval( agent->Interp(), argc, Argv );

	for (int j=3; j < argc - 2; j++)
		free_memory( Argv[j] );

	return tag;
	}

char *glishtk_canvas_textfunc(TkProxy *agent_, const char *cmd, const char *param, Value *args )
	{
	if ( args->Type() != TYPE_RECORD )
		return 0;

	recordptr rptr = args->RecordPtr();
	TkCanvas *agent = (TkCanvas*)agent_;

	static char tag[256];
	int tagstr_cnt = 0;
	static int tagstr_len = 512;
	static char *tagstr = (char*) alloc_memory( sizeof(char)*tagstr_len );

	Argv[0] = Tk_PathName( agent->Self() );
	Argv[1] = (char*) cmd;
	Argv[2] = (char*) param;

	GENERATE_TAG(tag,agent,param)

	const Value *val1 = rptr->NthEntry(0);
	const Value *shape_val = val1->ExistingAttribute( "shape" );
	if ( (shape_val == false_value || shape_val->Length() == 1 || 
	     ! shape_val->IsNumeric()) )
		{
		const Value *val2 = rptr->NthEntry(1);
		const Value *text = (*rptr)["text"];
		if ( ! text || text == val1 || text == val2 )
			return 0;

		int max = val1->Length();
		if ( max == 1 )
			max = val2->Length();
		else if ( val2->Length() > 1 && val2->Length() < max )
			max = val2->Length();
		if ( text->Length() < max ) max = text->Length();
		if ( max > 1 && rptr->Length() > 2 )
			{
			for ( int i=2; i < rptr->Length(); ++i )
				{
				int l = rptr->NthEntry(i)->Length();
				if ( l > 1 && l < max ) max = l;
				}
			}

		for ( int i = 0; i < max; ++i )
			{
			int name_cnt = 0;
			int argc = 3;
			tagstr[0] = '{';
			tagstr[1] = '\0';
			for (int j = 0; j < (*rptr).Length(); j++)
				{
				const char *key = 0;
				const Value *val = rptr->NthEntry( j, key );
				if ( strncmp( key, "arg", 3 ) )
					{
					if ( val->Type() == TYPE_STRING )
						{
						if ( strcmp( key, "tag" ) )
							{
							const char *str = val->StringPtr(0)[i<val->Length()?i:0];
							Arg_name[name_cnt] = (char*) alloc_memory(sizeof(char)*(strlen(key)+2) );
							sprintf(Arg_name[name_cnt],"-%s",key);
							Arg_val[name_cnt] = (char*) alloc_memory(strlen(str)+3);
							sprintf(Arg_val[name_cnt++],"{%s}",str);
							}
						else
							{
							const char *str = val->StringPtr(0)[i<val->Length()?i:0];
							POINTFUNC_TAG_APPEND(str)
							}
						}
					}
				else
					{
					if ( val->Type() == TYPE_STRING )
						Argv[argc++] = strdup(val->StringPtr(0)[i<val->Length()?i:0]);
					else if ( val->IsNumeric() )
						{
						Argv[argc] = (char*) alloc_memory( 30 );
						sprintf( Argv[argc++], "%d", val->IntVal(i<val->Length()?i+1:1) );
						}
					}
				}

			for ( int x=0; x < name_cnt; x++ )
				{
				Argv[argc++] = Arg_name[x];
				Argv[argc++] = Arg_val[x];
				}

			POINTFUNC_TAG_APPEND(tag)
			Argv[argc++] = (char*) "-tag";
			strcat( tagstr, "}" );
			Argv[argc++] = (char*) tagstr;

			tcl_ArgEval( agent->Interp(), argc, Argv );

			for (LOOPDECL j=3; j < argc - 2; j++)
				free_memory( Argv[j] );
			}
		}
	else if ( shape_val != false_value && shape_val->IsNumeric() &&
		  shape_val->Length() == 2 && shape_val->IntVal(2) == 2 )
		{
		int rows = shape_val->IntVal();
		int max = rows;
		const Value *text = (*rptr)["text"];
		if ( ! text || text == val1 )
			return 0;

		if ( text->Length() < max ) max = text->Length();
		if ( max > 1 && rptr->Length() > 1 )
			for ( int i=1; i < rptr->Length(); ++i )
				{
				int l = rptr->NthEntry(i)->Length();
				if ( l > 1 && l < max ) max = l;
				}

		for ( int i = 0; i < max; ++i )
			{
			int name_cnt = 0;
			int argc = 3;
			tagstr[0] = '{';
			tagstr[1] = '\0';

			Argv[argc] = (char*) alloc_memory(30);
			sprintf( Argv[argc++], "%d", val1->IntVal(i+1) );
			Argv[argc] = (char*) alloc_memory(30);
			sprintf( Argv[argc++], "%d", val1->IntVal(i+1+rows) );
			for (int j = 1; j < (*rptr).Length(); j++)
				{
				const char *key = 0;
				const Value *val = rptr->NthEntry( j, key );
				if ( strncmp( key, "arg", 3 ) )
					{
					if ( val->Type() == TYPE_STRING )
						{
						if ( strcmp( key, "tag" ) )
							{
							const char *str = val->StringPtr(0)[i<val->Length()?i:0];
							Arg_name[name_cnt] = (char*) alloc_memory(sizeof(char)*(strlen(key)+2) );
							sprintf(Arg_name[name_cnt],"-%s",key);
							Arg_val[name_cnt] = (char*) alloc_memory(strlen(str)+3);
							sprintf(Arg_val[name_cnt++],"{%s}",str);
							}
						else
							{
							const char *str = val->StringPtr(0)[i<val->Length()?i:0];
							POINTFUNC_TAG_APPEND(str)
							}
						}
					}
				else
					{
					if ( val->Type() == TYPE_STRING )
						Argv[argc++] = strdup(val->StringPtr(0)[i<val->Length()?i:0]);
					else if ( val->IsNumeric() )
						{
						Argv[argc] = (char*) alloc_memory( 30 );
						sprintf( Argv[argc++], "%d", val->IntVal(i<val->Length()?i+1:1) );
						}
					}
				}

			for ( int x=0; x < name_cnt; x++ )
				{
				Argv[argc++] = Arg_name[x];
				Argv[argc++] = Arg_val[x];
				}

			POINTFUNC_TAG_APPEND(tag)
			Argv[argc++] = (char*) "-tag";
			strcat( tagstr, "}" );
			Argv[argc++] = (char*) tagstr;

			tcl_ArgEval( agent->Interp(), argc, Argv );

			for (LOOPDECL j=3; j < argc - 2; j++)
				free_memory( Argv[j] );
			}
		}
	return tag;
	}

char *glishtk_canvas_delete(Tcl_Interp *tcl, Tk_Window self, const char *, Value *args )
	{
	if ( args->Type() == TYPE_RECORD )
		{
		recordptr rptr = args->RecordPtr(0);
		for ( int i=0; i < (*rptr).Length(); ++i )
			{
			const Value *val = rptr->NthEntry( i );
			if ( val->Type() == TYPE_STRING )
				tcl_VarEval( tcl, Tk_PathName(self), " delete ", val->StringPtr(0)[0], 0 );
			}
		}
	else if ( args->Type() == TYPE_STRING )
		{
		charptr *ary = args->StringPtr(0);
		for ( int i=0; i < (*args).Length(); ++i )
			tcl_VarEval( tcl, Tk_PathName(self), " delete ", ary[i], 0 );
		}
	else
		{
		tcl_VarEval( tcl, Tk_PathName(self), " addtag nukemallll all", 0 );
		tcl_VarEval( tcl, Tk_PathName(self), " delete nukemallll", 0 );
		}

	return 0;
	}

char *glishtk_canvas_move(Tcl_Interp *tcl, Tk_Window self, const char *, Value *args )
	{
	char *event_name = "canvas move function";
	EXPRINIT( event_name)
	if ( args->Length() >= 3 )
		{
		EXPRSTR( tag, event_name )
		EXPRINT2( xshift, event_name )
		EXPRINT2( yshift, event_name )
		tcl_VarEval( tcl, Tk_PathName(self), " move ", tag, SP, xshift, SP, yshift, 0 );
		EXPR_DONE( yshift )
		EXPR_DONE( xshift )
		EXPR_DONE( tag )
		}
	else if ( args->Length() >= 2 )
		{
		EXPRSTR( tag, event_name )
		EXPRVAL( off,event_name)
		char xshift[128];
		char yshift[128];
		if ( off->IsNumeric() && off->Length() >= 2 )
			{
			int is_copy = 0;
			int *delta = off->CoerceToIntArray(is_copy,2);
			sprintf(xshift,"%d",delta[0]);
			sprintf(yshift,"%d",delta[1]);
			if (is_copy)
				free_memory( delta );
			}
		tcl_VarEval( tcl, Tk_PathName(self), " move ", tag, SP, xshift, SP, yshift, 0 );
		EXPR_DONE( off )
		EXPR_DONE( tag )
		}

	return 0;
	}

struct glishtk_canvas_bindinfo
	{
	TkCanvas *canvas;
	char *event_name;
	char *tk_event_name;
	char *tag;
	glishtk_canvas_bindinfo( TkCanvas *c, const char *event, const char *tk_event, const char *tag_arg=0 ) :
			canvas(c), event_name(strdup(event)), tk_event_name(strdup(tk_event))
			{ tag = tag_arg ? strdup(tag_arg) : 0; }
	~glishtk_canvas_bindinfo()
		{
		free_memory( tag );
		free_memory( tk_event_name );
		free_memory( event_name );
		}
	};

int glishtk_canvas_bindcb( ClientData data, Tcl_Interp *tcl, int, char *argv[] )
	{
	glishtk_canvas_bindinfo *info = (glishtk_canvas_bindinfo*) data;
	recordptr rec = create_record_dict();
	Tk_Window self = info->canvas->Self();

	if ( info->tag )
		rec->Insert( strdup("tag"), new Value( info->tag ) );

	int *wpt = (int*) alloc_memory( sizeof(int)*2 );
	tcl_VarEval( tcl, Tk_PathName(self), " canvasx ", argv[1], 0 );
	wpt[0] = atoi(Tcl_GetStringResult(tcl));
	tcl_VarEval( tcl, Tk_PathName(self), " canvasy ", argv[2], 0 );
	wpt[1] = atoi(Tcl_GetStringResult(tcl));
	rec->Insert( strdup("world"), new Value( wpt, 2 ) );

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

	info->canvas->BindEvent( info->event_name, new Value( rec ) );
	return TCL_OK;
	}

char *glishtk_canvas_bind(TkProxy *agent, const char *, Value *args )
	{
	char *event_name = "canvas bind function";
	EXPRINIT( event_name)
	if ( args->Length() >= 3 )
		{
		EXPRSTR( tag, event_name )
		EXPRSTR( button, event_name )
		EXPRSTR( event, event_name )
		glishtk_canvas_bindinfo *binfo = 
			new glishtk_canvas_bindinfo((TkCanvas*)agent, event, button, tag);

		tcl_VarEval( agent->Interp(), Tk_PathName(agent->Self()), " bind ", tag, SP, button, " {",
			     glishtk_make_callback(agent->Interp(), glishtk_canvas_bindcb, binfo), " %x %y %b %K %A}", 0 );
		
		EXPR_DONE( event )
		EXPR_DONE( button )
		EXPR_DONE( tag )
		}
	else if ( args->Length() >= 2 )
		{
		EXPRSTR( button, event_name )
		EXPRSTR( event, event_name )
		glishtk_canvas_bindinfo *binfo = 
			new glishtk_canvas_bindinfo((TkCanvas*)agent, event, button);

		tcl_VarEval( agent->Interp(), "bind ", Tk_PathName(agent->Self()), SP, button, " {",
			     glishtk_make_callback(agent->Interp(), glishtk_canvas_bindcb, binfo), " %x %y %b %K %A}", 0 );

		EXPR_DONE( event )
		EXPR_DONE( button )
		}

	return 0;
	}

Value *glishtk_tkcast( char *tk )
	{
	TkProxy *agent = (TkProxy*) tk;
	agent->SendCtor("newtk");
	return 0;
	}

Value *glishtk_valcast( char *val )
	{
        Value *v = (Value*) val;
	return v ? v : error_value();
	}

char *glishtk_canvas_frame(TkProxy *agent, const char *, Value *args )
	{
	char *event_name = "canvas bind function";
	TkCanvas *canvas = (TkCanvas*)agent;
	static char tag[256];
	EXPRINIT( event_name)
	HASARG( args, >= 2 )

	GENERATE_TAG(tag,canvas,"frame")

	EXPRINT2( x, event_name )
	EXPRINT2( y, event_name )
	TkFrame *frame = new TkFrameP(canvas->seq(),canvas,"flat","top","0","0","0","none","lightgrey","15","10",tag);
	tcl_VarEval( agent->Interp(), Tk_PathName(canvas->Self()), " create window ", x, SP, y, " -anchor nw -tag ", tag,
		     " -window ", Tk_PathName(frame->Self()), 0 );
	EXPR_DONE( y )
	EXPR_DONE( x )

	if ( frame )
		{
		canvas->Add(frame);
		frame->SendCtor("newtk", canvas);
		}

	return 0;
	}

int canvas_yscrollcb( ClientData data, Tcl_Interp *, int, char *argv[] )
	{
	double firstlast[2];
	firstlast[0] = atof(argv[1]);
	firstlast[1] = atof(argv[2]);
	((TkCanvas*)data)->yScrolled( firstlast );
	return TCL_OK;
	}

int canvas_xscrollcb( ClientData data, Tcl_Interp *, int, char *argv[] )
	{
	double firstlast[2];
	firstlast[0] = atof(argv[1]);
	firstlast[1] = atof(argv[2]);
	((TkCanvas*)data)->xScrolled( firstlast );
	return TCL_OK;
	}

void TkCanvas::UnMap()
	{
	while ( frame_list.length() )
		{
		TkProxy *a = frame_list.remove_nth( 0 );
		a->UnMap( );
		}

	if ( self )
		tcl_VarEval( tcl, Tk_PathName(self), " config -xscrollcommand \"\"", 0 );

	TkProxy::UnMap();
	}

void TkCanvas::BindEvent( const char *event, Value *rec )
	{
	PostTkEvent( event, rec );
	}

TkCanvas::TkCanvas( ProxyStore *s, TkFrame *frame_, charptr width, charptr height, const Value *region_,
		    charptr relief, charptr borderwidth, charptr background, charptr fill_ ) : TkProxy( s ), fill(0)
	{
	frame = frame_;
	char *argv[18];
	static char region_str[512];

	int region_is_copy = 0;
	int *region = 0;

	if ( Argv_len == 0 )
		{
		Argv_len = 64;
		Arg_name = (char**) alloc_memory( sizeof(char*)*Argv_len );
		Arg_val = (char**) alloc_memory( sizeof(char*)*Argv_len );
		Argv = (char**) alloc_memory( sizeof(char*)*Argv_len );
		}

	if ( ! frame || ! frame->Self() ) return;

	if (region_->Length() >= 4)
		region = region_->CoerceToIntArray( region_is_copy, 4 );

	if ( region )
		sprintf(region_str ,"{%d %d %d %d}", region[0], region[1], region[2], region[3]);

	int c = 0;
	argv[c++] = "canvas";
	argv[c++] = (char*) NewName(frame->Self());
	argv[c++] = "-relief";
	argv[c++] = (char*) relief;
	argv[c++] = "-width";
	argv[c++] = (char*) width;
	argv[c++] = "-height";
	argv[c++] = (char*) height;
	if ( region )
		{
		argv[c++] = "-scrollregion";
		argv[c++] = region_str;
		}
	argv[c++] = "-borderwidth";
	argv[c++] = (char*) borderwidth;
	argv[c++] = "-background";
	argv[c++] = (char*) background;

	char ys[100];
	argv[c++] = "-yscrollcommand";
	argv[c++] = glishtk_make_callback( tcl, canvas_yscrollcb, this, ys );
	argv[c++] = "-xscrollcommand";
	argv[c++] = glishtk_make_callback( tcl, canvas_xscrollcb, this );

	if ( region_is_copy )
		free_memory( region );

	tcl_ArgEval( tcl, c, argv );
	self = Tk_NameToWindow( tcl, argv[1], root );

	agent_ID = "<graphic:canvas>";

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkCanvas::TkCanvas")

	count++;

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("addtag", new TkProc("addtag","withtag", 2, glishtk_canvas_tagfunc));
	procs.Insert("arc", new TkProc(this, "create", "arc", glishtk_canvas_pointfunc,glishtk_str));
	procs.Insert("bind", new TkProc(this, "", glishtk_canvas_bind));
	procs.Insert("canvasx", new TkProc("canvasx", 2, glishtk_canvas_1toNint, glishtk_StrToInt));
	procs.Insert("canvasy", new TkProc("canvasy", 2, glishtk_canvas_1toNint, glishtk_StrToInt));
	procs.Insert("delete", new TkProc("", glishtk_canvas_delete));
	procs.Insert("deltag", new TkProc("dtag", (const char*) 0, 1, glishtk_canvas_tagfunc));
	procs.Insert("frame", new TkProc(this, "", glishtk_canvas_frame, glishtk_tkcast));
	procs.Insert("height", new TkProc("-height", glishtk_heightwidth_query, glishtk_StrToInt));
	procs.Insert("line", new TkProc(this, "create", "line", glishtk_canvas_pointfunc,glishtk_str));
	procs.Insert("move", new TkProc("", glishtk_canvas_move));
	procs.Insert("oval", new TkProc(this, "create", "oval", glishtk_canvas_pointfunc,glishtk_str));
	procs.Insert("poly", new TkProc(this, "create", "poly", glishtk_canvas_pointfunc,glishtk_str));
	procs.Insert("rectangle", new TkProc(this, "create", "rectangle", glishtk_canvas_pointfunc,glishtk_str));
	procs.Insert("region", new TkProc("-scrollregion", 4, glishtk_oneintlist_query, glishtk_splitsp_int));
	procs.Insert("tagabove", new TkProc("addtag","above", 2, glishtk_canvas_tagfunc));
	procs.Insert("tagbelow", new TkProc("addtag","below", 2, glishtk_canvas_tagfunc));
	procs.Insert("text", new TkProc(this, "create", "text", glishtk_canvas_textfunc,glishtk_str));
	procs.Insert("view", new TkProc("", glishtk_scrolled_update));
	procs.Insert("width", new TkProc("-width", glishtk_heightwidth_query, glishtk_StrToInt));
	}

int TkCanvas::ItemCount(const char *name) const
	{
	return item_count[name];
	}

int TkCanvas::NewItemCount(const char *name)
	{
	int cnt = item_count[name];
	item_count.Insert(name,++cnt);
	return cnt;
	}

void TkCanvas::yScrolled( const double *d )
	{
	PostTkEvent( "yscroll", new Value( (double*) d, 2, COPY_ARRAY ) );
	}

void TkCanvas::xScrolled( const double *d )
	{
	PostTkEvent( "xscroll", new Value( (double*) d, 2, COPY_ARRAY ) );
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
		return (const char**) ret;		\
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

STD_EXPAND_PACKINSTRUCTION(TkCanvas)
STD_EXPAND_CANEXPAND(TkCanvas)
	  
void TkCanvas::Create( ProxyStore *s, Value *args )
	{
	TkCanvas *ret;

	if ( args->Length() != 8 )
		InvalidNumberOfArgs(8);

	SETINIT
	SETVAL( parent, parent->IsAgentRecord() )
	SETDIM( width )
	SETDIM( height )
	SETVAL( region, region->IsNumeric() )
	SETSTR( relief )
	SETDIM( borderwidth )
	SETSTR( background )
	SETSTR( fill )

	TkProxy *agent = (TkProxy*) (global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret = new TkCanvas( s, (TkFrame*)agent, width, height, region, relief, borderwidth, background, fill );
	else
		{
		SETDONE
		global_store->Error("bad parent type");
		return;
		}

	CREATE_RETURN
	}
