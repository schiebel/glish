// $Id$
// Copyright (c) 1997,1998 Associated Universities Inc.
//

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <X11/Xlib.h>
#include <string.h>
#include <stdlib.h>
#include "Reporter.h"
#include "Glish/Value.h"
#include "system.h"
#include "tkCore.h"
#include "tkCanvas.h"
#include "comdefs.h"

extern ProxyStore *global_store;

int TkProxy::root_unmapped = 0;
Tk_Window TkProxy::root = 0;
Tcl_Interp *TkProxy::tcl = 0;

PQueue(glishtk_event) *TkProxy::tk_queue = 0;
int TkProxy::hold_tk_events = 0;
int TkProxy::hold_glish_events = 0;
Value *TkProxy::last_error = 0;
Value *TkProxy::bitmap_path = 0;
Value *TkProxy::dload_path = 0;
int TkProxy::widget_index = 0;

unsigned long TkFrame::count = 0;

int tcl_ArgEval( Tcl_Interp *interp, int argc, char *argv[] )
	{
	if ( argc < 1 ) return TCL_ERROR;

	static char *buf = 0;
	static int blen = 0;

	if ( ! blen )
		{
		blen = 1024;
		buf = (char*) alloc_memory( blen );
		}

	int clen = 0;
	int len = 0;

#define ARGEVAL_ADD(what)					\
	len = strlen(what);					\
	if ( clen + len + 2 >=  blen )				\
		{						\
		while ( clen + len + 2 >=  blen ) blen *= 2;	\
		buf = (char*) realloc_memory( buf, blen );	\
		}						\
	memcpy( &buf[clen], what, len );			\
	clen += len;

#define ARGEVAL_SP   buf[clen++] = ' ';
#define ARGEVAL_ZERO buf[clen] = '\0';

	ARGEVAL_ADD(argv[0])
	for ( register int i = 1; i < argc; ++i )
		{
		ARGEVAL_SP
		ARGEVAL_ADD(argv[i] )
		}

	ARGEVAL_ZERO
	return Tcl_Eval( interp, buf );
	}

char *glishtk_make_callback( Tcl_Interp *tcl, Tcl_CmdProc *cmd, ClientData data, char *out )
	{
	static int index = 0;
	static char buf[100];
	if ( ! out ) out = buf;
	sprintf( out, "gtkcb%x", ++index );
	Tcl_CreateCommand( tcl, out, cmd, data, 0 );
	return out;
	}

char *glishtk_winfo(Tcl_Interp *tcl, Tk_Window self, const char *cmd, Value * )
	{
	Tcl_VarEval( tcl, "winfo ", cmd, SP, Tk_PathName(self), 0 );
	return Tcl_GetStringResult(tcl);
	}

class glishtk_event {
    public:
	glishtk_event( TkProxy *a_, const char *n_, Value *v_ ) :
			agent(a_), nme(n_ ? strdup(n_) : strdup(" ")), val(v_)
			{ Ref(agent); Ref(val); }
	void Post();
	~glishtk_event();
	Value *value() { return val; }
    protected:
	TkProxy *agent;
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

Value *TkProc::operator()(Tcl_Interp *tcl, Tk_Window s, Value *arg)
	{
	char *val = 0;

	if ( proc )
		val = (*proc)(tcl, s,cmdstr,arg);
	else if ( proc1 )
		val = (*proc1)(tcl, s,cmdstr,param,arg);
	else if ( proc2 )
		val = (*proc2)(tcl, s,cmdstr,param,param2,arg);
	else if ( aproc != 0 && agent != 0 )
		val = (*aproc)(agent, cmdstr, arg );
	else if ( aproc2 != 0 && agent != 0 )
		val = (*aproc2)(agent, cmdstr, param, arg);
	else if ( aproc3 != 0 && agent != 0 )
		val = (*aproc3)(agent, cmdstr, param, param2, arg);
	else if ( iproc )
		val = (*iproc)(tcl, s, cmdstr, i, arg);
	else if ( iproc1 )
		val = (*iproc1)(tcl, s, cmdstr, param, i, arg);
	else
		return error_value();

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

static int (*glishtk_dflt_xioerror_handler)(Display *) = 0;
int glishtk_xioerror_handler(Display *d)
	{
	glish_cleanup();
	if ( glishtk_dflt_xioerror_handler )
		(*glishtk_dflt_xioerror_handler)(d);
	exit(1);
	return 1;
	}

void TkProxy::init_tk( int visible_root )
	{
	if ( ! root && is_graphic )
		{
		tcl = Tcl_CreateInterp();
		Tcl_Init( tcl );
		Tk_Init(tcl);
		root = Tk_MainWindow(tcl);
		
		glishtk_dflt_xioerror_handler = XSetIOErrorHandler(glishtk_xioerror_handler);
		static char tk_follow[] = "tk_focusFollowsMouse";
		Tcl_Eval(tcl, tk_follow);

		if ( ! visible_root )
			{
			root_unmapped = 1;
			Tcl_VarEval( tcl, "wm withdraw ", Tk_PathName(root), 0 );
			}
		}

	else if ( root_unmapped && visible_root )
		{
		root_unmapped = 0;
		Tcl_VarEval( tcl, "wm deiconify ", Tk_PathName(root), 0 );
		}
	}

void TkProxy::HoldEvents( ProxyStore *, Value * )
	{
	hold_tk_events++;
	}

void TkProxy::ReleaseEvents( ProxyStore *, Value * )
	{
	hold_tk_events--;
	}

void TkProxy::ProcessEvent( const char *name, Value *val )
	{
	if ( ! IsValid() ) return;

	TkProc *proc = procs[name];

	if ( proc != 0 )
		{
		Value *v = (*proc)( tcl, self, val );
		if ( v && ReplyPending() ) Reply( v );
		}
	else
		Error("unknown event");
	}

void TkProxy::EnterEnable() { }
void TkProxy::ExitEnable() { }
int TkProxy::IsValid() const { return self != 0; }

void TkProxy::SetError( Value *v )
	{
	if ( last_error ) Unref(last_error);
	last_error = v;
	}

void TkProxy::PostTkEvent( const char *s, Value *v )
	{
	Ref(this);
	if ( hold_glish_events )
		tk_queue->EnQueue( new glishtk_event( this, s, v ) );
	else
		PostEvent( s, v );
	Unref(this);
	}

void TkProxy::FlushGlishEvents()
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

void TkProxy::Version( ProxyStore *s, Value * )
	{
	Value *tkv = new Value( TK_VERSION );
	attributeptr tka = tkv->ModAttributePtr();
#if defined(TK_PATCH_LEVEL)
        tka->Insert( strdup( "patch" ), new Value(TK_PATCH_LEVEL) );
#endif
	Value *tclv = new Value(TCL_VERSION);
        tka->Insert( strdup( "tcl" ), tclv );
#if defined(TCL_PATCH_LEVEL)
	attributeptr tcla = tclv->ModAttributePtr();
	tcla->Insert( strdup( "patch" ), new Value(TCL_PATCH_LEVEL) );
#endif

	if ( s->ReplyPending() )
		s->Reply( tkv );
	else
		s->PostEvent( "version", tkv );

	Unref(tkv);
	}

void TkProxy::HaveGui( ProxyStore *s, Value * )
	{
	if ( s->ReplyPending() )
		{
		Value val( TkHaveGui() ? glish_true : glish_false );
		s->Reply( &val );
		}
	}


void TkProxy::dLoad( ProxyStore *s, Value *arg )
	{
	char *toload = 0;
	const char *module = 0;
	if ( arg->Type() == TYPE_STRING && arg->Length() >= 1 )
		{
		toload = which_shared_object(arg->StringPtr(0)[0]);
		if ( toload && arg->Length() > 1 )
			module = arg->StringPtr(0)[1];
		}

	if ( toload )
		{
		init_tk(0);
		if ( module )
			{
			if ( Tcl_VarEval( tcl, "load ", toload, " ", module, 0 ) == TCL_ERROR )
				s->Error( Tcl_GetStringResult(tcl) );
			}
		else
			{
			if ( Tcl_VarEval( tcl, "load ", toload, 0 ) == TCL_ERROR )
				s->Error( Tcl_GetStringResult(tcl) );
			}
		}
	else
		s->Error( "Couldn't find object to load" );
	}

void TkProxy::SetDloadPath( ProxyStore *, Value *v )
	{
	if ( v && v->Type() == TYPE_STRING )
		{
		if ( dload_path ) Unref( dload_path );
		dload_path = v;
		Ref( dload_path );
		}
	}

char *TkProxy::which_shared_object( const char* filename )
	{
	charptr *paths = dload_path ? dload_path->StringPtr() : 0;
	int len = dload_path ? dload_path->Length() : 0;

	int sl = strlen(filename);
	int do_pre_post = 1;

	if ( sl > 3 && filename[sl-1] == 'o' && filename[sl-2] == 's' && filename[sl-3] == '.' )
		do_pre_post = 0;

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
			if ( do_pre_post )
				{
				sprintf( directory, "%s/%s.so", paths[i], filename );
				if ( access( directory, R_OK ) == 0 )
					return strdup( directory );
				else
					{
					sprintf( directory, "%s/lib%s.so", paths[i], filename );
					if ( access( directory, R_OK ) == 0 )
						return strdup( directory );
					}
				}
			else
				{
				sprintf( directory, "%s/%s", paths[i], filename );
				if ( access( directory, R_OK ) == 0 )
					return strdup( directory );
				}

	return 0;
	}

int TkProxy::DoOneTkEvent( int flags, int hold_wait )
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

int TkProxy::DoOneTkEvent( )
	{
	int ret = 0;

	if ( hold_tk_events )
		ret = Tk_DoOneEvent( TK_FILE_EVENTS | TK_TIMER_EVENTS );
	else
		ret = Tk_DoOneEvent( 0 );

	return ret;
	}

void TkProxy::SetBitmapPath( ProxyStore *, Value *v )
	{
	if ( v && v->Type() == TYPE_STRING )
		{
		if ( bitmap_path ) Unref( bitmap_path );
		bitmap_path = v;
		Ref( bitmap_path );
		}
	}

char *TkProxy::which_bitmap( const char* filename )
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



charptr TkProxy::NewName( Tk_Window parent ) const
	{
	static char buf[50];

	if ( parent )
		{
		charptr pp = Tk_PathName(parent);
		if ( ! pp || pp[0] == '.' && pp[1] == '\0' )
			sprintf( buf, ".g%x", ++widget_index );
		else
			sprintf( buf, "%s.g%x", pp, ++widget_index );
		}
	else
		sprintf( buf, ".g%x", ++widget_index );

	return buf;
	}

void TkProxy::SetMap( int do_map, int toplevel )
	{
	int dont_map_ = do_map ? 0 : 1;
	if ( dont_map != dont_map_ )
		{
		dont_map = dont_map_;
		if ( ! toplevel )
			{
			if ( dont_map )
				Tcl_VarEval( tcl, "pack forget ", Tk_PathName(self), 0 );
			if ( frame ) frame->Pack();
			}
		else
			{
			Tk_Window win =  TopLevel();
			if ( win )
				{
				if ( dont_map )
					Tcl_VarEval( tcl, "wm withdraw ", Tk_PathName(win), 0 );
				else
					Tcl_VarEval( tcl, "wm deiconify ", Tk_PathName(win), 0 );
				}
			}
		}
	}

TkProxy::TkProxy( ProxyStore *s, int init_graphic ) : Proxy( s ), dont_map( 0 ), disable_count(0)
	{
	agent_ID = "<graphic>";
	enable_state = 0;
	is_graphic = init_graphic;

	self = 0;
	frame = 0;

	if ( tk_queue == 0 )
		tk_queue = new PQueue(glishtk_event)();

	if ( init_graphic )
		{
		init_tk( );

		procs.Insert("background", new TkProc("-bg", glishtk_onestr, glishtk_str));
		procs.Insert("foreground", new TkProc("-fg", glishtk_onestr, glishtk_str));
		procs.Insert("relief", new TkProc("-relief", glishtk_onestr, glishtk_str));
		procs.Insert("borderwidth", new TkProc("-borderwidth", glishtk_onedim, glishtk_strtoint));
		procs.Insert("pixelwidth", new TkProc("width",glishtk_winfo, glishtk_strtoint));
		procs.Insert("pixelheight", new TkProc("height",glishtk_winfo, glishtk_strtoint));
		}
	}


void TkProxy::Disable( )
	{
	disable_count++;
	}

void TkProxy::Enable( int force )
	{
	if ( force ) disable_count = 0;
	else disable_count--;
	}

void TkProxy::UnMap()
	{
	if ( self ) Tk_DestroyWindow( self );

	frame = 0;
	self = 0;
	}

const char **TkProxy::PackInstruction()
	{
	return 0;
	}

charptr TkProxy::IndexCheck( charptr c )
	{
	return c;
	}

int TkProxy::CanExpand() const
	{
	return 0;
	}

TkProxy::~TkProxy( )
	{
	IterCookie* c = procs.InitForIteration();

	TkProc* member;
	const char* key;
	while ( (member = procs.NextEntry( key, c )) )
		delete member;
	}

void TkProxy::BindEvent(const char *event, Value *rec)
	{
	PostTkEvent( event, rec );
	}

Tk_Window TkProxy::TopLevel()
	{
	return frame ? frame->TopLevel() : 0;
	}

int TkProxy::IsPseudo( )
	{
	return 1;
	}

char *glishtk_scrolled_update(Tcl_Interp *tcl, Tk_Window self, const char *, Value *val )
	{
	if ( val->Type() != TYPE_STRING || val->Length() != 1 )
		return 0;

	Tcl_VarEval( tcl, Tk_PathName(self), SP, val->StringPtr(0)[0], 0 );
	return 0;
	}

void GlishTk_Register( const char *str, WidgetCtor ctor )
	{
	if ( global_store )
		global_store->Register( str, ctor );
	}

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

void TkFrame::AddElement( TkProxy *obj ) { exit(1); }
void TkFrame::RemoveElement( TkProxy *obj ) { exit(1); }
void TkFrame::Pack() { exit(1); }
const char *TkFrame::Expand() const { exit(1); }
int TkFrame::NumChildren() const { exit(1); }
const char *TkFrame::Side() const { exit(1); }
int TkFrame::ExpandNum(const TkProxy *except, unsigned int grtOReqt) const { exit(1); }
