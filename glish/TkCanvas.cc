// $Header$
#include "Glish/glish.h"
RCSID("@(#) $Id$")
#ifdef GLISHTK

#include "TkCanvas.h"

#include <string.h>
#include <stdlib.h>
#include "Rivet/rivet.h"
#include "Reporter.h"
#include "Sequencer.h"
#include "IValue.h"
#include "Expr.h"

#include <iostream.h>

#define GENERATE_TAG(BUFFER,CANVAS,TYPE) 		\
	sprintf(BUFFER,"c%lx%s%lx",CANVAS->CanvasCount(),TYPE,CANVAS->NewItemCount(TYPE));

int TkCanvas::count = 0;

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

#define EXPRINTVAL(var,EVENT)                                           \
        Expr *var##_expr_ = (*args)[c++]->Arg();                        \
        const IValue *var = var##_expr_ ->ReadOnlyEval();               \
	const IValue *var##_val_ = var;					\
        if ( ! var || ! var ->IsNumeric() || var ->Length() <= 0 )      \
                {                                                       \
                error->Report("bad value for ", EVENT);                 \
                var##_expr_ ->ReadOnlyDone(var);                        \
                return 0;                                   		\
                }

#define EXPRINT(var,EVENT)                                              \
        int var = 0;                                                    \
	EXPRINTVAL(var##_val_,EVENT)                                    \
	Expr *var##_expr_ = var##_val__expr_;				\
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

DEFINE_DTOR(TkCanvas)

//
// These variables are made file static to allow them to be shared by
// the canvas functions. This was done to minimize allocations.
//
static int argv_len = 64;
static char **arg_name = new char*[argv_len];
static char **arg_val = new char*[argv_len];
static char **argv = new char*[argv_len];

#define CANVAS_FUNC_REALLOC(size)							\
	if ( size >= argv_len )								\
		{									\
		while ( size >= argv_len ) argv_len *= 2;				\
		arg_name = (char**) realloc( arg_name, argv_len * sizeof(char*) );	\
		arg_val = (char**) realloc( arg_val, argv_len * sizeof(char*) );	\
		argv = (char**) realloc( argv, argv_len * sizeof(char*) );		\
		}

IValue *glishtk_StrToInt( char *str )
	{
	int i = atoi(str);
	return new IValue( i );
	}

char *glishtk_heightwidth_query(Rivetobj self, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one dim function";
	char buf[256];
	if ( args->length() > 0 )
		{
		int c = 0;
		EXPRDIM( dim, event_name )
		rivet_set( self, (char*) cmd, (char*) dim );
		EXPR_DONE( dim )
		}

	ret = rivet_va_func(self, (int (*)()) Tk_WinfoCmd, &cmd[1], rivet_path(self), 0);
	int width = atoi(ret);
	ret = rivet_va_cmd(self, "cget", "-borderwidth", 0);
	int bdwidth = atoi(ret);
	sprintf( buf, "%d", width - 2*bdwidth - 4 );
	
	return buf;
	}

char *glishtk_oneintlist_query(Rivetobj self, const char *cmd, int howmany, parameter_list *args,
				int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one int list function";
	if ( args->length() >= howmany )
		{
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

		rivet_set( self, (char*) cmd, buf );
		}

	ret = rivet_get( self, (char*) cmd );
	return ret;
	}


char *glishtk_canvas_1toNint(Rivetobj self, const char *cmd, int howmany, parameter_list *args,
				int is_request, int log )
	{
	char *ret = 0;
	char *event_name = "one int list function";
	HASARG( args, >= 1 )
	int len = args->length() < howmany ? args->length() : howmany;
	CANVAS_FUNC_REALLOC(len+2)
	static char buff[128];
	int argc = 0;

	argv[argc++] = 0;
	argv[argc++] = (char*) cmd;
	int c = 0;
	for ( int i=0; i < len; i++ )
		{
		EXPRINT( v, event_name )
		sprintf(buff,"%d",v);
		argv[argc++] = strdup(buff);
		EXPR_DONE( v )
		}

	rivet_cmd( self, argc, argv );
	ret = (char*) self->interp->result;

	for ( int x=0; x < len; x++ )
		delete argv[x + 2];

	return ret;
	}

char *glishtk_canvas_tagfunc(Rivetobj self, const char *cmd, const char *subcmd,
				int howmany, parameter_list *args, int is_request, int log )
	{
	char *event_name = "tag function";
	if ( args->length() <= 0 )
		return 0;
	CANVAS_FUNC_REALLOC(5)
	int c = 0;
	EXPRSTRVAL(str_v, event_name)
	int argc = 0;
	argv[argc++] = 0;
	argv[argc++] = (char*) cmd;
	argv[argc++] = (char*)(str_v->StringPtr(0)[0]);
	if ( subcmd )
		argv[argc++] = (char*) subcmd;
	if ( str_v->Length() > 1 && str_v->Length() >= howmany )
		for ( int i=1; i < str_v->Length(); i++ )
			{
			argv[argc] = (char*)(str_v->StringPtr(0)[i]);
			rivet_cmd(self, argc+1, argv);
			}
	else if ( args->length() > 1 && args->length() >= howmany )
		{
		for (int i=c; i < args->length(); i++)
			{
			EXPRSTR(str, event_name)
			argv[argc] = (char*)str;
			rivet_cmd(self, argc+1, argv);
			EXPR_DONE(str)
			}
		}
	else if ( howmany == 1 )
		rivet_cmd( self, argc, argv );

	EXPR_DONE(str_v)
	return 0;
	}

char *glishtk_canvas_pointfunc(TkAgent *agent_, const char *cmd, const char *param, parameter_list *args,
				int is_request, int log )
	{
	char buf[50];
	char *ret = 0;
	int rows, cols;
	char *event_name = "one string + n int function";
	TkCanvas *agent = (TkCanvas*)agent_;
	HASARG( args, > 0 )
	static char tag[256];
	static int tagstr_len = 512;
	static char *tagstr = new char[tagstr_len];
	int tagstr_cnt = 0;
	int name_cnt = 0;

	tagstr[0] = '\0';
	int c = 0;
	int elements = 0;
	EXPRVAL(val,event_name)
	int argc = 3;
	const Value *shape_val = val->ExistingAttribute( "shape" );
	if ( (shape_val == false_value || shape_val->Length() == 1 || 
	     ! shape_val->IsNumeric()) && val->Length() == 1 )
		{
	        c = 0;
		elements = (*args).length();
		CANVAS_FUNC_REALLOC(elements*2+argc+2)

#define POINTFUNC_TAG_APPEND(STR)					\
if ( tagstr_cnt+strlen(STR)+5 >= tagstr_len )				\
	{								\
	while ( tagstr_cnt+strlen(STR)+5 >= tagstr_len ) tagstr_len *= 2; \
	tagstr = (char*) realloc( tagstr, tagstr_len * sizeof(char));	\
	}								\
if ( tagstr_cnt ) { strcat(tagstr, " "); tagstr_cnt++; }		\
strcat(tagstr, STR);							\
tagstr_cnt += strlen(STR);

#define POINTFUNC_NAMED_ACTION				 	\
if ( strcmp((*args)[c]->Name(),"tag") )				\
	{							\
	arg_name[name_cnt] = new char[strlen((*args)[c]->Name())+2]; \
	sprintf(arg_name[name_cnt],"-%s",(*args)[c]->Name());	\
	EXPRSTR( str, event_name )				\
	arg_val[name_cnt++] = strdup(str);			\
	EXPR_DONE( str )					\
	}							\
else								\
	{							\
	EXPRSTRVAL(str_v,event_name)				\
	char *str = str_v->StringVal();				\
	POINTFUNC_TAG_APPEND(str)				\
	delete str;						\
	EXPR_DONE( str_v )					\
	}

		for (int i = 0; i < (*args).length(); i++)
			if ( (*args)[c]->Name() )
				{
				POINTFUNC_NAMED_ACTION
				}
			else
				{
				EXPRINT2( str, event_name )
				argv[argc++] = strdup(str);
				EXPR_DONE( str )
				}
		}
	else if ( shape_val != false_value && shape_val->IsNumeric() &&
		  shape_val->Length() == 2 && (cols = shape_val->IntVal(2)) == 2 &&
		  val->IsNumeric() )
		{
		rows = shape_val->IntVal();
		elements = rows*2;
		CANVAS_FUNC_REALLOC( elements+argc+2+(*args).length()*2 )
		Value *newval = copy_value(val);
		newval->Polymorph(TYPE_INT);
		int *ip = newval->IntPtr(0);
		int i;
		for ( i=0; i < rows; i++)
			{
			sprintf(buf,"%d",ip[i]);
			argv[argc++] = strdup(buf);
			sprintf(buf,"%d",ip[i+rows]);
			argv[argc++] = strdup(buf);
			}
		Unref(newval);
		for ( i = c; i < (*args).length(); i++)
			if ( (*args)[c]->Name() )
				{
				POINTFUNC_NAMED_ACTION
				}
			else
				c++;
		}
	else if ( val->Length() > 1 && val->IsNumeric() )
		{
		Value *newval = copy_value(val);
		elements = val->Length();
		CANVAS_FUNC_REALLOC(elements+argc+2+(*args).length()*2)
		newval->Polymorph(TYPE_INT);
		int *ip = newval->IntPtr(0);
		int i;
		for (i=0; i < val->Length(); i++)
			{
			sprintf(buf,"%d",ip[i]);
			argv[argc++] = strdup(buf);
			}
		Unref(newval);
		for (i = c; i < (*args).length(); i++)
			if ( (*args)[c]->Name() )
				{
				POINTFUNC_NAMED_ACTION
				}
			else
				c++;
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
		argv[argc++] = arg_name[x];
		argv[argc++] = arg_val[x];
		}

	argv[0] = 0;
	argv[1] = (char*) cmd;
	argv[2] = (char*) param;
	argv[argc++] = (char*) "-tag";
	argv[argc++] = (char*) tagstr;

	ret = (char*) rivet_cmd( agent->Self(), argc, argv );

	for (int j=3; j < argc - 2; j++)
		delete argv[j];

	return tag;
	}

char *glishtk_canvas_delete(Rivetobj self, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *event_name = "canvas delete function";
	int c = 0;
	if ( args->length() > 0 )
		for (int i = 0; i < (*args).length(); i++)
			{
			EXPRSTR( tag, event_name )
			rivet_va_cmd( self, "delete", tag, 0 );
			EXPR_DONE( tag )
			}
	else
		{
		rivet_va_cmd( self, "addtag", "*NUKEM-ALL", "all", 0 );
		return rivet_va_cmd( self, "delete", "*NUKEM-ALL", 0 );
		}
	}

char *glishtk_canvas_move(Rivetobj self, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *event_name = "canvas move function";
	int c = 0;
	if ( args->length() >= 3 )
		{
		EXPRSTR( tag, event_name )
		EXPRINT2( xshift, event_name )
		EXPRINT2( yshift, event_name )
		rivet_va_cmd( self, "move", tag, xshift, yshift, 0 );
		EXPR_DONE( yshift )
		EXPR_DONE( xshift )
		EXPR_DONE( tag )
		}
	else if ( args->length() >= 2 )
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
				delete delta;
			}
		rivet_va_cmd( self, "move", tag, xshift, yshift, 0 );
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
			tk_event_name(strdup(tk_event)),
			event_name(strdup(event)),
			canvas(c) { tag = tag_arg ? strdup(tag_arg) : 0; }
	~glishtk_canvas_bindinfo()
		{
		if ( tag ) delete tag;
		delete tk_event_name;
		delete event_name;
		}
	};

int canvas_buttoncb(Rivetobj canvas, XEvent *xevent, ClientData assoc, int keysym, int callbacktype)
	{
	glishtk_canvas_bindinfo *info = (glishtk_canvas_bindinfo*) assoc;
	int dummy;
	recordptr rec = create_record_dict();
	if ( info->tag )
		rec->Insert( strdup("tag"), new IValue( info->tag ) );
	static char buff[256];
	int *wpt = new int[2];
	wpt[0] = xevent->xkey.x;
	wpt[1] = xevent->xkey.y;
	rec->Insert( strdup("wpoint"), new IValue( wpt, 2 ) );
	int *cpt = new int[2];
	sprintf(buff,"%d",xevent->xkey.x);
	char *ret = (char*) rivet_va_cmd(canvas, "canvasx", buff, 0);
	cpt[0] = atoi(ret);
	sprintf(buff,"%d",xevent->xkey.y);
	ret = (char*) rivet_va_cmd(canvas, "canvasy", buff, 0);
	cpt[1] = atoi(ret);
	rec->Insert( strdup("cpoint"), new IValue( cpt, 2 ) );
	if ( xevent->type == KeyPress )
		rec->Insert( strdup("key"), new IValue( rivet_expand_event(canvas, "A", xevent, keysym, &dummy) ) );
	info->canvas->ButtonEvent(info->event_name, new IValue( rec ) );
	return TCL_OK;
	}

char *glishtk_canvas_bind(TkAgent *agent, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *event_name = "canvas bind function";
	int c = 0;
	if ( args->length() >= 3 )
		{
		EXPRSTR( tag, event_name )
		EXPRSTR( button, event_name )
		EXPRSTR( event, event_name )
		glishtk_canvas_bindinfo *binfo = 
			new glishtk_canvas_bindinfo((TkCanvas*)agent, event, button, tag);

		if ( rivet_create_binding(agent->Self(), (char*)tag, (char*)button, (int (*)()) canvas_buttoncb,
					  (ClientData) binfo, 1, 0) == TCL_ERROR )
			{
			error->Report("Error, binding not created.");
			delete binfo;
			}

		EXPR_DONE( event )
		EXPR_DONE( button )
		EXPR_DONE( tag )
		}
	else if ( args->length() >= 2 )
		{
		EXPRSTR( button, event_name )
		EXPRSTR( event, event_name )
		glishtk_canvas_bindinfo *binfo = 
			new glishtk_canvas_bindinfo((TkCanvas*)agent, event, button);

		if ( strcmp(button,"<KeyPress>") )
			{
			if ( rivet_create_binding(agent->Self(), 0, (char*)button, (int (*)()) canvas_buttoncb,
						  (ClientData) binfo, 1, 0) == TCL_ERROR )
				{
				error->Report("Error, binding not created.");
				delete binfo;
				}
			}
		else
			{
			if ( rivet_create_binding(0, "all", (char*)button, (int (*)()) canvas_buttoncb,
						  (ClientData) binfo, 1, 0) == TCL_ERROR )
				{
				error->Report("Error, binding not created.");
				delete binfo;
				}
			}

		EXPR_DONE( event )
		EXPR_DONE( button )
		}

	return 0;
	}

IValue *glishtk_tkcast( char *tk )
	{
	TkAgent *agent = (TkAgent*) tk;
	return agent ? agent->AgentRecord() : error_ivalue();
	}

IValue *glishtk_valcast( char *val )
	{
        IValue *v = (IValue*) val;
	return v ? v : error_ivalue();
	}

char *glishtk_canvas_frame(TkAgent *agent, const char *cmd, parameter_list *args,
				int is_request, int log )
	{
	char *event_name = "canvas bind function";
	TkCanvas *canvas = (TkCanvas*)agent;
	static char tag[256];
	int c = 0;
	HASARG( args, >= 2 )

	GENERATE_TAG(tag,canvas,"frame")

	EXPRINT2( x, event_name )
	EXPRINT2( y, event_name )
	TkFrame *frame = new TkFrame(canvas->seq(),canvas,"flat","top","0","0","0","none","lightgrey","15","10",tag);
	rivet_va_cmd(canvas->Self(),"create","window",x,y,"-anchor","nw","-tag",tag,"-window",rivet_path(frame->Self()),0);
	EXPR_DONE( y )
	EXPR_DONE( x )

	if ( frame ) canvas->Add(frame);

	return (char*) frame;
	}

int canvas_yscrollcb(Rivetobj button, XEvent *unused1, ClientData assoc, ClientData calldata)
	{
	double *firstlast = (double*)calldata;
	((TkCanvas*)assoc)->yScrolled( firstlast );
	return TCL_OK;
	}

int canvas_xscrollcb(Rivetobj button, XEvent *unused1, ClientData assoc, ClientData calldata)
	{
	double *firstlast = (double*)calldata;
	((TkCanvas*)assoc)->xScrolled( firstlast );
	return TCL_OK;
	}

void TkCanvas::UnMap()
	{
	while ( frame_list.length() )
		{
		TkAgent *a = frame_list.remove_nth( 0 );
		a->UnMap( );
		}

	TkAgent::UnMap();
	}

void TkCanvas::ButtonEvent( const char *event, IValue *rec )
	{
	glish_event_posted(sequencer->NewEvent( this, event, rec ));
	}

TkCanvas::TkCanvas( Sequencer *s, TkFrame *frame_, charptr width, charptr height, const Value *region_,
		    charptr relief, charptr borderwidth, charptr background, charptr fill_ ) : TkAgent( s ), fill(0)
	{
	frame = frame_;
	char *argv[18];
	static char region_str[512];

	int region_is_copy = 0;
	int *region = 0;

	if ( ! frame || ! frame->Self() ) return;

	if (region_->Length() >= 4)
		region = region_->CoerceToIntArray( region_is_copy, 4 );

	if ( region )
		sprintf(region_str ,"%d %d %d %d", region[0], region[1], region[2], region[3]);

	int c = 2;
	argv[0] = argv[1] = 0;
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
	argv[c++] = "-xscrollcommand";
	argv[c++] = rivet_new_callback((int (*)()) canvas_xscrollcb, (ClientData) this, 0);
	argv[c++] = "-yscrollcommand";
	argv[c++] = rivet_new_callback((int (*)()) canvas_yscrollcb, (ClientData) this, 0);

	if ( region_is_copy )
		delete region;

	self = rivet_create(CanvasClass, frame->Self(), c, argv);
	agent_ID = "<graphic:canvas>";

	if ( ! self )
		fatal->Report("Rivet creation failed in TkCanvas::TkCanvas");

	count++;

	if ( fill_ && fill_[0] && strcmp(fill_,"none") )
		fill = strdup(fill_);

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("height", new TkProc("-height", glishtk_heightwidth_query, glishtk_StrToInt));
	procs.Insert("width", new TkProc("-width", glishtk_heightwidth_query, glishtk_StrToInt));
	procs.Insert("canvasx", new TkProc("canvasx", 2, glishtk_canvas_1toNint, glishtk_StrToInt));
	procs.Insert("canvasy", new TkProc("canvasy", 2, glishtk_canvas_1toNint, glishtk_StrToInt));
	procs.Insert("line", new TkProc(this, "create", "line", glishtk_canvas_pointfunc,glishtk_str));
	procs.Insert("rectangle", new TkProc(this, "create", "rectangle", glishtk_canvas_pointfunc,glishtk_str));
	procs.Insert("poly", new TkProc(this, "create", "poly", glishtk_canvas_pointfunc,glishtk_str));
	procs.Insert("oval", new TkProc(this, "create", "oval", glishtk_canvas_pointfunc,glishtk_str));
	procs.Insert("arc", new TkProc(this, "create", "arc", glishtk_canvas_pointfunc,glishtk_str));
	procs.Insert("text", new TkProc(this, "create", "text", glishtk_canvas_pointfunc,glishtk_str));
	procs.Insert("delete", new TkProc("", glishtk_canvas_delete));
	procs.Insert("view", new TkProc("", glishtk_scrolled_update));
	procs.Insert("move", new TkProc("", glishtk_canvas_move));
	procs.Insert("bind", new TkProc(this, "", glishtk_canvas_bind));
	procs.Insert("frame", new TkProc(this, "", glishtk_canvas_frame, glishtk_tkcast));
	procs.Insert("region", new TkProc("-scrollregion", 4, glishtk_oneintlist_query, glishtk_splitsp_int));
	procs.Insert("addtag", new TkProc("addtag","withtag", 2, glishtk_canvas_tagfunc));
	procs.Insert("tagabove", new TkProc("addtag","above", 2, glishtk_canvas_tagfunc));
	procs.Insert("tagbelow", new TkProc("addtag","below", 2, glishtk_canvas_tagfunc));
	procs.Insert("deltag", new TkProc("dtag", (const char*) 0, 1, glishtk_canvas_tagfunc));
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
	glish_event_posted(sequencer->NewEvent( this, "yscroll", new IValue( (double*) d, 2, COPY_ARRAY ) ));
	}

void TkCanvas::xScrolled( const double *d )
	{
	glish_event_posted(sequencer->NewEvent( this, "xscroll", new IValue( (double*) d, 2, COPY_ARRAY ) ));
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

STD_EXPAND_PACKINSTRUCTION(TkCanvas)
STD_EXPAND_CANEXPAND(TkCanvas)
	  
TkAgent *TkCanvas::Create( Sequencer *s, const_args_list *args_val )
	{
	TkCanvas *ret;

	if ( args_val->length() != 9 )
		return InvalidNumberOfArgs(9);

	int c = 1;
	SETVAL( parent, parent->IsAgentRecord() )
	SETDIM( width )
	SETDIM( height )
	SETVAL( region, region->IsNumeric() )
	SETSTR( relief )
	SETDIM( borderwidth )
	SETSTR( background )
	SETSTR( fill )

	ret =  new TkCanvas( s, (TkFrame*)parent->AgentVal(), width, height, region, relief, borderwidth, background, fill );

	return ret;
	}      

#endif
