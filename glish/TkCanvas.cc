// $Header$
#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "TkCanvas.h"

#ifdef GLISHTK
#include <string.h>
#include <stdlib.h>
#include "Rivet/rivet.h"
#include "Reporter.h"
#include "IValue.h"
#include "Expr.h"

#include <iostream.h>

unsigned long TkCanvas::count = 0;

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

DEFINE_DTOR(TkCanvas)

char *glishtk_canvas_pointfunc(TkAgent *agent_, const char *cmd, const char *param, parameter_list *args,
				int is_request, int log )
	{
	char buf[50];
	char *ret = 0;
	int rows, cols;
	char *event_name = "one string + n int function";
	char tag[256];
	TkCanvas *agent = (TkCanvas*)agent_;
	HASARG( args, > 0 )
	char **argv = 0;
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
		argv = new char*[elements+argc+2];
		for (int i = 0; i < (*args).length(); i++)
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
		argv = new char*[elements+argc+2];
		Value *newval = copy_value(val);
		newval->Polymorph(TYPE_INT);
		int *ip = newval->IntPtr();
		for (int i=0; i < rows; i++)
			{
			sprintf(buf,"%d",ip[i]);
			argv[argc++] = strdup(buf);
			sprintf(buf,"%d",ip[i+rows]);
			argv[argc++] = strdup(buf);
			}
		Unref(newval);
		}
	else if ( val->Length() > 1 && val->IsNumeric() )
		{
		Value *newval = copy_value(val);
		elements = val->Length();
		argv = new char*[elements+argc+2];
		newval->Polymorph(TYPE_INT);
		int *ip = newval->IntPtr();
		for (int i=0; i < val->Length(); i++)
			{
			sprintf(buf,"%d",ip[i]);
			argv[argc++] = strdup(buf);
			}
		Unref(newval);
		}
	else
		{
		EXPR_DONE(val)
		return 0;
		}

	sprintf(tag,"c%lx%s%lx",agent->CanvasCount(),param,agent->NewItemCount(param));

	argv[0] = 0;
	argv[1] = (char*) cmd;
	argv[2] = (char*) param;
	argv[argc++] = (char*) "-tag";
	argv[argc++] = (char*) tag;

	ret = (char*) rivet_cmd( agent->Self(), argc, argv );

	for (int j=3; j < elements + 3; j++)
		delete argv[j];
	delete argv;

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

TkCanvas::TkCanvas( Sequencer *s, TkFrame *frame_, charptr width, charptr height, const Value *region_,
		    charptr relief, charptr borderwidth, charptr background ) : TkAgent( s )
	{
	frame = frame_;
	char *argv[18];
	static char region_str[512];

	int region_is_copy = 0;
	int *region = 0;

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

	frame->AddElement( this );
	frame->Pack();

	procs.Insert("height", new TkProc("-height", glishtk_onedim));
	procs.Insert("width", new TkProc("-width", glishtk_onedim));
	procs.Insert("line", new TkProc(this, "create", "line", glishtk_canvas_pointfunc,glishtk_str));
	procs.Insert("delete", new TkProc("", glishtk_canvas_delete));
	procs.Insert("view", new TkProc("", glishtk_scrolled_update));
	procs.Insert("region", new TkProc("-scrollregion", 4, glishtk_oneintlist));
	}

unsigned long TkCanvas::ItemCount(const char *name) const
	{
	return item_count[name];
	}

unsigned long TkCanvas::NewItemCount(const char *name)
	{
	unsigned long cnt = item_count[name];
	item_count.Insert(name,++cnt);
	return cnt;
	}

void TkCanvas::yScrolled( const double *d )
	{
	CreateEvent( "yscroll", new IValue( (double*) d, 2, COPY_ARRAY ) );
	}

void TkCanvas::xScrolled( const double *d )
	{
	CreateEvent( "xscroll", new IValue( (double*) d, 2, COPY_ARRAY ) );
	}

TkAgent *TkCanvas::Create( Sequencer *s, const_args_list *args_val )
	{
	TkCanvas *ret;

	if ( args_val->length() != 8 )
		return InvalidNumberOfArgs(8);

	int c = 1;
	SETVAL( parent, parent->IsAgentRecord() )
	SETDIM( width )
	SETDIM( height )
	SETVAL( region, region->IsNumeric() )
	SETSTR( relief )
	SETDIM( borderwidth )
	SETSTR( background )

	ret =  new TkCanvas( s, (TkFrame*)parent->AgentVal(), width, height, region, relief, borderwidth, background );

	return ret;
	}      

#endif
