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
extern Value *glishtk_valcast( char * );

int TkAgent::root_unmapped = 0;
Tk_Window TkAgent::root = 0;
Tcl_Interp *TkAgent::tcl = 0;

PQueue(glishtk_event) *TkAgent::tk_queue = 0;
int TkAgent::hold_tk_events = 0;
int TkAgent::hold_glish_events = 0;
Value *TkAgent::last_error = 0;
Value *TkAgent::bitmap_path = 0;
Value *TkAgent::dload_path = 0;

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

void TkAgent::init_tk( int visible_root )
	{
	if ( ! root )
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

void TkAgent::HoldEvents( ProxyStore *, Value * )
	{
	hold_tk_events++;
	}

void TkAgent::ReleaseEvents( ProxyStore *, Value * )
	{
	hold_tk_events--;
	}

void TkAgent::ProcessEvent( const char *name, Value *val )
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

void TkAgent::Version( ProxyStore *s, Value * )
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

void TkAgent::HaveGui( ProxyStore *s, Value * )
	{
	if ( s->ReplyPending() )
		{
		Value val( TkHaveGui() ? glish_true : glish_false );
		s->Reply( &val );
		}
	}


void TkAgent::dLoad( ProxyStore *s, Value *arg )
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

void TkAgent::SetDloadPath( ProxyStore *, Value *v )
	{
	if ( v && v->Type() == TYPE_STRING )
		{
		if ( dload_path ) Unref( dload_path );
		dload_path = v;
		Ref( dload_path );
		}
	}

char *TkAgent::which_shared_object( const char* filename )
	{
	charptr *paths = dload_path ? dload_path->StringPtr() : 0;
	int len = dload_path ? dload_path->Length() : 0;

	int sl = strlen(filename);
	int do_pre_post = 1;

	if ( sl > 3 && filename[sl] == 'o' && filename[sl-1] == 's' && filename[sl-2] == '.' )
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

void TkAgent::SetBitmapPath( ProxyStore *, Value *v )
	{
	if ( v && v->Type() == TYPE_STRING )
		{
		if ( bitmap_path ) Unref( bitmap_path );
		bitmap_path = v;
		Ref( bitmap_path );
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



charptr TkAgent::NewName( Tk_Window parent ) const
	{
	static int index = 0;
	static char buf[50];

	if ( parent )
		{
		charptr pp = Tk_PathName(parent);
		if ( ! pp || pp[0] == '.' && pp[1] == '\0' )
			sprintf( buf, ".g%x", ++index );
		else
			sprintf( buf, "%s.g%x", pp, ++index );
		}
	else
		sprintf( buf, ".g%x", ++index );

	return buf;
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

TkAgent::TkAgent( ProxyStore *s ) : Proxy( s ), dont_map( 0 ), disable_count(0)
	{
	agent_ID = "<graphic>";
	enable_state = 0;

	if ( tk_queue == 0 )
		tk_queue = new PQueue(glishtk_event)();

	self = 0;
	frame = 0;

	init_tk( );

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
	if ( self ) Tk_DestroyWindow( self );

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

Tk_Window TkAgent::TopLevel()
	{
	return frame ? frame->TopLevel() : 0;
	}

int TkAgent::IsPseudo( )
	{
	return 1;
	}


int glishtk_delframe_cb( ClientData data, Tcl_Interp *, int, char *[] )
	{
	((TkFrame*)data)->KillFrame();
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
	Tcl_VarEval( tcl, "winfo ", #WHAT, SP, Tk_PathName(tlead), 0 );		\
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
		Tk_Window self = ((TkAgent*)clientData)->Self();
		Tcl_Interp *tcl = ((TkAgent*)clientData)->Interp();
		
		Tcl_VarEval( tcl, Tk_PathName(self), " cget -width", 0 );
		int req_width = atoi(Tcl_GetStringResult(tcl));
		Tcl_VarEval( tcl, Tk_PathName(self), " cget -height", 0 );
		int req_height = atoi(Tcl_GetStringResult(tcl));

		char buf[40];
		if ( Tk_Width(self) > req_width )
			{
			sprintf( buf, "%d", Tk_Width(self) );
			Tcl_VarEval( tcl, Tk_PathName(self), " configure -width ", buf, 0 );
			}
		if ( Tk_Height(self) > req_height )
			{
			sprintf( buf, "%d", Tk_Height(self) );
			Tcl_VarEval( tcl, Tk_PathName(self), " configure -height ", buf, 0 );
			}
		}
	}

void glishtk_resizeframe_cb( ClientData clientData, XEvent *eventPtr)
	{
	if ( eventPtr->xany.type == ConfigureNotify )
		{

		Tk_Window self = ((TkAgent*)clientData)->Self();
		Tcl_Interp *tcl = ((TkAgent*)clientData)->Interp();
		
		Tcl_VarEval( tcl, Tk_PathName(self), " cget -width", 0 );
		Tcl_VarEval( tcl, Tk_PathName(self), " cget -height", 0 );

		TkFrame *f = (TkFrame*) clientData;
		f->ResizeEvent();
		}
	}

void glishtk_moveframe_cb( ClientData clientData, XEvent *eventPtr)
	{
	if ( eventPtr->xany.type == ConfigureNotify )
		{
		TkFrame *f = (TkFrame*) clientData;
		f->LeaderMoved();
		}
	}

char *glishtk_agent_map(TkAgent *a, const char *cmd, Value *)
	{
	a->SetMap( cmd[0] == 'M' ? 1 : 0, cmd[1] == 'T' ? 1 : 0 );
	return 0;
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
		  is_tl( 1 ), pseudo( 0 ), reject_first_resize(1), tlead(tlead_), tpos(0), unmapped(0),
		  icon(0)

	{
	char *argv[17];

	agent_ID = "<graphic:frame>";

	if ( ! root )
		HANDLE_CTOR_ERROR("Frame creation failed, check DISPLAY environment variable.")

	tl_count++;

	if ( tlead )
		{
		Ref( tlead );
		tpos = strdup(tpos_);
		}

	if ( top_created )
		{
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
		argv[c++] = (char*) background;

		tcl_ArgEval( tcl, c, argv );
		pseudo = Tk_NameToWindow( tcl, argv[1], root );
		if ( title && title[0] )
			Tcl_VarEval( tcl, "wm title ", Tk_PathName( pseudo ), " {", title, "}", 0 );

		if ( tlead )
			{
			Tcl_VarEval( tcl, "wm transient ", Tk_PathName(pseudo), SP,
				     Tk_PathName(tlead->Self()), 0 );
			Tcl_VarEval( tcl, "wm overrideredirect ", Tk_PathName(pseudo), " true", 0 );

			const char *geometry = glishtk_popup_geometry( tcl, tlead->Self(), tpos );
			Tcl_VarEval( tcl, "wm geometry ", Tk_PathName(pseudo), SP, geometry, 0 );

			Tk_Window top = tlead->TopLevel();
			Tk_CreateEventHandler(top, StructureNotifyMask, glishtk_moveframe_cb, this );
			}
		}
	else
		{
		top_created = 1;
		if ( title && title[0] )
			Tcl_VarEval( tcl, "wm title ", Tk_PathName( root ), " {", title, "}", 0 );

		if ( tlead )
			{
			Tcl_VarEval( tcl, "wm transient ", Tk_PathName(root), Tk_PathName(tlead->Self()), 0 );
			Tcl_VarEval( tcl, "wm overrideredirect ", Tk_PathName(root), " true", 0 );

			const char *geometry = glishtk_popup_geometry( tcl, tlead->Self(), tpos );
			Tcl_VarEval( tcl, "wm geometry ", Tk_PathName(root), geometry, 0 );

			Tk_Window top = tlead->TopLevel();
			Tk_CreateEventHandler(top, StructureNotifyMask, glishtk_moveframe_cb, this );
			}
		}

	side = strdup(side_);
	padx = strdup(padx_);
	pady = strdup(pady_);
	expand = strdup(expand_);

	int c = 0;
	argv[c++] = "frame";
	argv[c++] = (char*) NewName(pseudo ? pseudo : root);

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

	tcl_ArgEval( tcl, c, argv );
	self = Tk_NameToWindow( tcl, argv[1], root );

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkFrame::TkFrame")

	Tcl_VarEval( tcl, "wm protocol ", Tk_PathName(pseudo ? pseudo : root), " WM_DELETE_WINDOW ",
		     glishtk_make_callback( tcl, glishtk_delframe_cb, this ), 0 );

	if ( icon && strlen( icon ) )
		{
		char *expanded = which_bitmap(icon);
		if ( expanded )
			{
			char *icon_ = (char*) alloc_memory(strlen(expanded)+2);
			sprintf(icon_," @%s",expanded);
			Tcl_VarEval( tcl, "wm iconbitmap ", Tk_PathName(pseudo ? pseudo : root), icon_, 0);
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

	procs.Insert("bind", new FmeProc(this, "", glishtk_bind));
	procs.Insert("cursor", new FmeProc("-cursor", glishtk_onestr, glishtk_str));
	procs.Insert("disable", new FmeProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new FmeProc( this, "0", glishtk_disable_cb ));
	procs.Insert("expand", new FmeProc( this, &TkFrame::SetExpand, glishtk_str ));
	procs.Insert("fonts", new FmeProc( this, &TkFrame::FontsCB, glishtk_valcast ));
	procs.Insert("grab", new FmeProc( this, &TkFrame::GrabCB ));
	procs.Insert("height", new FmeProc("", glishtk_height, glishtk_valcast));
	procs.Insert("icon", new FmeProc( this, &TkFrame::SetIcon, glishtk_str ));
	procs.Insert("map", new FmeProc(this, "MT", glishtk_agent_map));
	procs.Insert("padx", new FmeProc( this, &TkFrame::SetPadx, glishtk_strtoint ));
	procs.Insert("pady", new FmeProc( this, &TkFrame::SetPady, glishtk_strtoint ));
	procs.Insert("raise", new FmeProc( this, &TkFrame::Raise ));
	procs.Insert("release", new FmeProc( this, &TkFrame::ReleaseCB ));
	procs.Insert("side", new FmeProc( this, &TkFrame::SetSide, glishtk_str ));
	procs.Insert("title", new FmeProc( this, &TkFrame::Title ));
	procs.Insert("unmap", new FmeProc(this, "UT", glishtk_agent_map));
	procs.Insert("width", new FmeProc("", glishtk_width, glishtk_valcast));

	Tk_CreateEventHandler( self, StructureNotifyMask, glishtk_popup_adjust_dim_cb, this );
	if ( ! tlead )
		Tk_CreateEventHandler( self, StructureNotifyMask, glishtk_resizeframe_cb, this );

	size[0] = Tk_ReqWidth(self);
	size[1] = Tk_ReqHeight(self);
	}

TkFrame::TkFrame( ProxyStore *s, TkFrame *frame_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_, charptr background,
		  charptr width, charptr height, charptr cursor, int new_cmap ) : TkRadioContainer( s ),
		  side(0), padx(0), pady(0), expand(0), tag(0), canvas(0), is_tl( 0 ),
		  pseudo( 0 ), reject_first_resize(0), tlead(0), tpos(0), unmapped(0), icon(0)

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
	argv[c++] = (char*) background;
	if ( cursor && *cursor )
		{
		argv[c++] = "-cursor";
		argv[c++] = (char*) cursor;
		}

	tcl_ArgEval( tcl, c, argv );
	self = Tk_NameToWindow( tcl, argv[1], root );

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

	procs.Insert("padx", new FmeProc( this, &TkFrame::SetPadx, glishtk_strtoint ));
	procs.Insert("pady", new FmeProc( this, &TkFrame::SetPady, glishtk_strtoint ));
	procs.Insert("expand", new FmeProc( this, &TkFrame::SetExpand, glishtk_str ));
	procs.Insert("side", new FmeProc( this, &TkFrame::SetSide, glishtk_str ));
	procs.Insert("grab", new FmeProc( this, &TkFrame::GrabCB ));
	procs.Insert("fonts", new FmeProc( this, &TkFrame::FontsCB, glishtk_valcast ));
	procs.Insert("release", new FmeProc( this, &TkFrame::ReleaseCB ));
	procs.Insert("cursor", new FmeProc("-cursor", glishtk_onestr, glishtk_str));
	procs.Insert("map", new FmeProc(this, "MC", glishtk_agent_map));
	procs.Insert("unmap", new FmeProc(this, "UC", glishtk_agent_map));
	procs.Insert("bind", new FmeProc(this, "", glishtk_bind));

	procs.Insert("width", new FmeProc("", glishtk_width, glishtk_valcast));
	procs.Insert("height", new FmeProc("", glishtk_height, glishtk_valcast));

	procs.Insert("disable", new FmeProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new FmeProc( this, "0", glishtk_disable_cb ));
	}

TkFrame::TkFrame( ProxyStore *s, TkCanvas *canvas_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_, charptr background,
		  charptr width, charptr height, const char *tag_ ) : TkRadioContainer( s ), side(0),
		  padx(0), pady(0), expand(0), is_tl( 0 ), pseudo( 0 ), reject_first_resize(0),
		  tlead(0), tpos(0), unmapped(0), icon(0)

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
	argv[c++] = (char*) background;

	tcl_ArgEval( tcl, c, argv );
	self = Tk_NameToWindow( tcl, argv[1], root );

	if ( ! self )
		HANDLE_CTOR_ERROR("Rivet creation failed in TkFrame::TkFrame")

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

	procs.Insert("padx", new FmeProc( this, &TkFrame::SetPadx, glishtk_strtoint ));
	procs.Insert("pady", new FmeProc( this, &TkFrame::SetPady, glishtk_strtoint ));
	procs.Insert("tag", new FmeProc( this, &TkFrame::GetTag, glishtk_str ));
	procs.Insert("side", new FmeProc( this, &TkFrame::SetSide, glishtk_str ));
	procs.Insert("grab", new FmeProc( this, &TkFrame::GrabCB ));
	procs.Insert("fonts", new FmeProc( this, &TkFrame::FontsCB, glishtk_valcast ));
	procs.Insert("release", new FmeProc( this, &TkFrame::ReleaseCB ));
	procs.Insert("cursor", new FmeProc("-cursor", glishtk_onestr, glishtk_str));
	procs.Insert("bind", new FmeProc(this, "", glishtk_bind));

	procs.Insert("width", new FmeProc("", glishtk_width, glishtk_valcast));
	procs.Insert("height", new FmeProc("", glishtk_height, glishtk_valcast));

	procs.Insert("disable", new FmeProc( this, "1", glishtk_disable_cb ));
	procs.Insert("enable", new FmeProc( this, "0", glishtk_disable_cb ));
	}

void TkFrame::UnMap()
	{
	if ( unmapped ) return;
	unmapped = 1;

	if ( RefCount() > 0 ) Ref(this);

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
		TkAgent *a = elements.remove_nth( 0 );
		a->UnMap( );
		}

	int unmap_root = self && ! pseudo && ! frame && ! canvas;

	if ( canvas )
		Tcl_VarEval( tcl, Tk_PathName(canvas->Self()), " delete ", tag, 0 );
	else if ( self )
		Tk_DestroyWindow( self );

	canvas = 0;
	frame = 0;
	self = 0;

	if ( pseudo )
		{
		Tk_DestroyWindow( pseudo );
		pseudo = 0;
		}

	if ( unmap_root )
		Tk_UnmapWindow( root );

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
	if ( args->Type() == TYPE_STRING && args->Length() > 0 )
		{
		const char *iconx = args->StringPtr(0)[0];
		if ( iconx && strlen(iconx) )
			{
			if ( icon ) free_memory(icon);
			icon = strdup(iconx);
			char *icon_ = (char*) alloc_memory(strlen(icon)+3);
			sprintf(icon_," @%s",icon);
			Tcl_VarEval( tcl, "wm iconbitmap ", Tk_PathName(pseudo ? pseudo : root), icon_, 0 );
			free_memory( icon_ );
			}
		}

	return icon ? icon : "";
	}

char *TkFrame::SetSide( Value *args )
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

char *TkFrame::SetPadx( Value *args )
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

	Tcl_VarEval( tcl, Tk_PathName(self), " cget -padx", 0 );
	return Tcl_GetStringResult(tcl);
	}

char *TkFrame::SetPady( Value *args )
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

	Tcl_VarEval( tcl, Tk_PathName(self), " cget -pady", 0 );
	return Tcl_GetStringResult(tcl);
	}

char *TkFrame::GetTag( Value * )
	{
	return tag;
	}

char *TkFrame::SetExpand( Value *args )
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

char *TkFrame::Grab( int global_scope )
	{
	if ( grab ) return 0;

	if ( global_scope )
		Tcl_VarEval( tcl, "grab set -global ", Tk_PathName(self), 0 );
	else
		Tcl_VarEval( tcl, "grab set ", Tk_PathName(self), 0 );

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
		Tcl_VarEval( tcl, "raise ", Tk_PathName(TopLevel()), SP, Tk_PathName(agent->TopLevel()), 0 );
	else
		Tcl_VarEval( tcl, "raise ", Tk_PathName(TopLevel()), 0 );

	return "";
	}

char *TkFrame::Title( Value *args )
	{
	if ( args->Type() == TYPE_STRING )
		Tcl_VarEval( tcl, "wm title ", Tk_PathName(TopLevel( )), SP, args->StringPtr(0)[0], 0 );
	else
		global_store->Error("wrong type, string expected");

	return "";
	}

char *TkFrame::FontsCB( Value *args )
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
		EXPRINIT("TkFrame::FontsCB")
		EXPRSTR( str, "TkFrame::FontsCB" )
		EXPRINT( l, "TkFrame::FontsCB" )
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

char *TkFrame::Release( )
	{
	if ( ! grab || grab != Id() ) return 0;

	Tcl_VarEval( tcl, "grap release ", Tk_PathName(self), 0 );

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
		size[0] = Tk_Width(self);
		size[1] = Tk_Height(self);
		rec->Insert( strdup("new"), new Value( size, 2, COPY_ARRAY ) );

		PostTkEvent( "resize", new Value( rec ) );
		}
	}

void TkFrame::LeaderMoved( )
	{
	if ( ! tlead ) return;

	const char *geometry = glishtk_popup_geometry( tcl, tlead->Self(), tpos );
	Tcl_VarEval( tcl, "wm geometry ", Tk_PathName(pseudo ? pseudo : root), SP, geometry, 0 );
	Tcl_VarEval( tcl, "raise ", Tk_PathName(pseudo ? pseudo : root), 0 );
	}

void TkFrame::Create( ProxyStore *s, Value *args )
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

Tk_Window TkFrame::TopLevel( )
	{
	return frame ? frame->TopLevel() : canvas ? canvas->TopLevel() :
		pseudo ? pseudo : root;
	}

Value *FmeProc::operator()(Tcl_Interp *tcl, Tk_Window s, Value *arg)
	{
	char *val = 0;

	if ( fproc && agent )
		val = (((TkFrame*)agent)->*fproc)( arg);
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

