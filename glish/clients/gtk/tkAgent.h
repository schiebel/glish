// $Id$
// Copyright (c) 1997,1998 Associated Universities Inc.
//
#ifndef tkagent_h_
#define tkagent_h_

#include "tk.h"
#include "Glish/Proxy.h"
#include "Glish/Queue.h"

#if ! defined(HAVE_TCL_GETSTRINGRESULT)
#define Tcl_GetStringResult(tcl) (tcl)->result
#endif

extern int TkHaveGui();

class TkAgent;
class TkCanvas;
class TkFrame;

glish_declare(PList,TkAgent);
typedef PList(TkAgent) tkagent_list;

class TkProc;
glish_declare(PDict,TkProc);
typedef PDict(TkProc) tkprochash;

//###  Function to do Argv Eval
extern int tcl_ArgEval( Tcl_Interp *interp, int argc, char *argv[] );

//###  Function to Make Callbacks
extern char *glishtk_make_callback( Tcl_Interp*, Tcl_CmdProc*, ClientData data, char *out=0 );

//###  scrollbar callback
extern char *glishtk_scrolled_update(Tcl_Interp*, Tk_Window, const char *cmd, Value *args);

//###  Callback Procs
typedef char *(*TkEventProc)(Tcl_Interp*, Tk_Window, const char *, Value*);
typedef char *(*TkOneParamProc)(Tcl_Interp*, Tk_Window, const char *, const char *, Value *);
typedef char *(*TkTwoParamProc)(Tcl_Interp*, Tk_Window, const char *, const char *, const char *, Value *);
typedef char *(*TkOneIntProc)(Tcl_Interp*, Tk_Window, const char *, int, Value *);
typedef char *(*TkTwoIntProc)(Tcl_Interp*, Tk_Window, const char *, const char *, int, Value *);
typedef char *(*TkEventAgentProc)(TkAgent*, const char *, Value*);
typedef char *(*TkEventAgentProc2)(TkAgent*, const char *, const char *, Value*);
typedef char *(*TkEventAgentProc3)(TkAgent*, const char *, const char *, const char *, Value*);
typedef Value *(*TkStrToValProc)( char * );

class glishtk_event;
glish_declare(PQueue,glishtk_event);

class TkAgent : public Proxy {
    public:
	TkAgent( ProxyStore *s );
	~TkAgent();

	virtual charptr NewName( Tk_Window parent=0 ) const;
	virtual charptr IndexCheck( charptr );

	int IsValid() { return self != 0; }
	virtual void UnMap();
	Tk_Window Self() { return self; }
	Tcl_Interp *Interp() { return tcl; }
	const Tk_Window Self() const { return self; }

	virtual const char **PackInstruction();
	virtual int CanExpand() const;

	// Used to post events which *originate* from Tk. This is important
	// because of scrollbar/canvas initialization, i.e. the Tk events
	// must be queued and sent after the "whenever"s are in place.
	void PostTkEvent( const char *, Value * );

	static int GlishEventsHeld() { return hold_glish_events; }
	static void FlushGlishEvents();

	static void Version( ProxyStore *p, Value *v );
	static void HaveGui( ProxyStore *p, Value *v );
	static void HoldEvents( ProxyStore *p=0, Value *v=0 );
	static void ReleaseEvents( ProxyStore *p=0, Value *v=0 );
	static int DoOneTkEvent( int flags, int hold_wait = 0 );
	static int DoOneTkEvent( );
	static void SetBitmapPath( ProxyStore *p, Value *v );
	static void dLoad( ProxyStore *p, Value *v );
	static void SetDloadPath( ProxyStore *p, Value *v );

	// For some widgets, they must be enabled before an action is performed
	// otherwise widgets which are disabled will not even accept changes
	// from a script.
	virtual void EnterEnable();
	virtual void ExitEnable();

	static Value *GetError() { return last_error; }
	static void SetError(Value*);

	void SetMap( int do_map, int toplevel );
	int DontMap( ) const { return dont_map; }

	virtual void Disable( );
	virtual void Enable( int force = 1 );

	void BindEvent(const char *event, Value *rec);

	virtual Tk_Window TopLevel( );

	int IsPseudo();

	const char *AgentID() const { return agent_ID; }

	void ProcessEvent( const char *name, Value *val );

    protected:
	void do_pack( int argc, char **argv)
		{ Tk_PackCmd( root, tcl, argc, argv ); }

	static void init_tk( int visible_root=1 );
	tkprochash procs;
	static int root_unmapped;
	static Tk_Window root;
	static Tcl_Interp *tcl;
	Tk_Window self;
	TkFrame *frame;

	static int hold_tk_events;
	static int hold_glish_events;
	static PQueue(glishtk_event) *tk_queue;

	// For keeping track of last error
	static Value *last_error;
	static Value *bitmap_path;
	static Value *dload_path;

	// locate the bitmap file
	char *which_bitmap( const char * );
	// locate the shared object file
	static char *which_shared_object( const char* filename );

	unsigned int enable_state;

	int dont_map;

	unsigned int disable_count;

	const char *agent_ID;
	};

class TkRadioContainer : public TkAgent {
    public:
	TkRadioContainer( ProxyStore *s ) : TkAgent(s), radio_id(0), id(++count) { }
	unsigned long RadioID() const { return radio_id; }
	void RadioID( unsigned long id_ ) { radio_id = id_; }
	unsigned long Id() const { return id; }
    private:
	static unsigned long count;
	unsigned long radio_id;
	unsigned long id;
};	

class TkFrame : public TkRadioContainer {
    public:
	TkFrame( ProxyStore *s, charptr relief_, charptr side_, charptr borderwidth,
		  charptr padx_, charptr pady_, charptr expand_, charptr background,
		  charptr width, charptr height, charptr cursor, charptr title,
		  charptr icon, int new_cmap, TkAgent *tlead_, charptr tpos_ );
	TkFrame( ProxyStore *s, TkFrame *frame_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_,
		  charptr background, charptr width, charptr height, charptr cursor,
		  int new_cmap );
	TkFrame( ProxyStore *s, TkCanvas *canvas_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_,
		  charptr background, charptr width, charptr height, const char *tag_ );

	static unsigned long TopLevelCount() { return tl_count; }

	// Called when the frame is killed via the window manager
	void KillFrame( );
	void ResizeEvent( );
	void LeaderMoved( );

	char *SetSide( Value * );
	char *SetPadx( Value * );
	char *SetPady( Value * );
	char *SetExpand( Value * );
	char *GetTag( Value * );

	char *SetIcon( Value * );

	char *Grab( int global_scope=0 );
	char *Release( );
	char *GrabCB( Value * );
	char *ReleaseCB( Value * );
	char *FontsCB( Value * );

	char *Raise( Value * );
	char *Title( Value * );

	void Pack();
	void PackSpecial( TkAgent * );
	void AddElement( TkAgent *obj ) { elements.append(obj); }
	void RemoveElement( TkAgent *obj );
	void UnMap();

	static void Create( ProxyStore *, Value * );
	~TkFrame();

	const char *Expand() const { return expand; }

	const char **PackInstruction();
	int CanExpand() const;

	int ExpandNum(const TkAgent *except=0, unsigned int grtOReqt = 0) const;

	int NumChildren() const { return elements.length() - 1; }

	void Disable( );
	void Enable( int force = 1 );

	Tk_Window TopLevel();

    protected:
	char *side;
	char *padx;
	char *pady;
	char *expand;
	char *tag;
	TkCanvas *canvas;
  	tkagent_list elements;
	static unsigned long top_created;
	static unsigned long tl_count;
	static unsigned long grab;

	char is_tl;
	Tk_Window pseudo;

	unsigned char reject_first_resize;

	int size[2];

	TkAgent *tlead;
	char *tpos;
	int unmapped;

	char *icon;
	};

class TkProc {
    public:

	TkProc( TkAgent *a, TkStrToValProc cvt = 0 )
			: cmdstr(0), proc(0), proc1(0), proc2(0),
				agent(a), aproc(0), aproc2(0), aproc3(0), iproc(0), iproc1(0), param(0),
				param2(0), convert(cvt), i(0) { }

	TkProc(const char *c, TkEventProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(p), proc1(0), proc2(0),
				agent(0), aproc(0), aproc2(0), aproc3(0), iproc(0), iproc1(0), param(0),
				param2(0), convert(cvt), i(0) { }
	TkProc(const char *c, const char *x, const char *y, TkTwoParamProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(p),
				agent(0), aproc(0), aproc2(0), aproc3(0), iproc(0), iproc1(0), param(x),
				param2(y), convert(cvt), i(0) { }
	TkProc(const char *c, const char *x, int y, TkTwoIntProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0),
				agent(0), aproc(0), aproc2(0), aproc3(0), iproc(0), iproc1(p), param(x),
				param2(0), convert(cvt), i(y) { }
	TkProc(const char *c, const char *x, TkOneParamProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(p), proc2(0),
				agent(0), aproc(0), aproc2(0), aproc3(0), iproc(0), iproc1(0), param(x),
				param2(0), convert(cvt), i(0) { }
	TkProc(const char *c, int x, TkOneIntProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0),
				agent(0), aproc(0), aproc2(0), aproc3(0), iproc(p), iproc1(0), param(0),
				param2(0), convert(cvt), i(x) { }
	TkProc(TkAgent *a, const char *c, TkEventAgentProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0),
				agent(a), aproc(p), aproc2(0), aproc3(0), iproc(0), iproc1(0), param(0),
				param2(0), convert(cvt), i(0) { }
	TkProc(TkAgent *a, const char *c, const char *x, TkEventAgentProc2 p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0),
				agent(a), aproc(0), aproc2(p), aproc3(0), iproc(0), iproc1(0), param(x),
				param2(0), convert(cvt), i(0) { }
	TkProc(TkAgent *a, const char *c, const char *x, const char *y, TkEventAgentProc3 p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0),
				agent(a), aproc(0), aproc2(0), aproc3(p), iproc(0), iproc1(0), param(x),
				param2(y), convert(cvt), i(0) { }

	virtual Value *operator()(Tcl_Interp*, Tk_Window s, Value *arg);

    protected:
	const char *cmdstr;

	TkEventProc proc;
	TkOneParamProc proc1;
	TkTwoParamProc proc2;

	TkAgent *agent;
	TkEventAgentProc aproc;
	TkEventAgentProc2 aproc2;
	TkEventAgentProc3 aproc3;

	TkOneIntProc iproc;
	TkTwoIntProc iproc1;

	const char *param;
	const char *param2;

	TkStrToValProc convert;

	int i;
	};

class FmeProc : public TkProc {
    public:

	FmeProc(TkFrame *f, char *(TkFrame::*p)(Value*), TkStrToValProc cvt = 0)
			: TkProc(f,cvt), fproc(p) { }

	FmeProc(const char *c, TkEventProc p, TkStrToValProc cvt = 0)
			: TkProc(c,p,cvt), fproc(0) { }

	FmeProc(TkFrame *a, const char *c, TkEventAgentProc p, TkStrToValProc cvt = 0)
			: TkProc(a,c,p,cvt), fproc(0) { }

	virtual Value *operator()(Tcl_Interp*, Tk_Window s, Value *arg);

    protected:
	char *(TkFrame::*fproc)(Value*);
};

typedef void (*WidgetCtor)( ProxyStore *, Value * );
extern void GlishTk_Register( const char *, WidgetCtor );

#endif
