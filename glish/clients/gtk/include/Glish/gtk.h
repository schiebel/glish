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

class TkProxy;
class TkCanvas;
class TkFrame;

glish_declare(PList,TkProxy);
typedef PList(TkProxy) tkagent_list;

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
typedef char *(*TkEventAgentProc)(TkProxy*, const char *, Value*);
typedef char *(*TkEventAgentProc2)(TkProxy*, const char *, const char *, Value*);
typedef char *(*TkEventAgentProc3)(TkProxy*, const char *, const char *, const char *, Value*);
typedef Value *(*TkStrToValProc)( char * );

class glishtk_event;
glish_declare(PQueue,glishtk_event);

class TkProxy : public Proxy {
    public:
	TkProxy( ProxyStore *s, int init_graphic=1 );
	~TkProxy();

	virtual charptr NewName( Tk_Window parent=0 ) const;
	virtual charptr IndexCheck( charptr );

	virtual int IsValid() const;
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
	static int widget_index;
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
	int is_graphic;
	};

class TkProc {
    public:

	TkProc( TkProxy *a, TkStrToValProc cvt = 0 )
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
	TkProc(TkProxy *a, const char *c, TkEventAgentProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0),
				agent(a), aproc(p), aproc2(0), aproc3(0), iproc(0), iproc1(0), param(0),
				param2(0), convert(cvt), i(0) { }
	TkProc(TkProxy *a, const char *c, const char *x, TkEventAgentProc2 p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0),
				agent(a), aproc(0), aproc2(p), aproc3(0), iproc(0), iproc1(0), param(x),
				param2(0), convert(cvt), i(0) { }
	TkProc(TkProxy *a, const char *c, const char *x, const char *y, TkEventAgentProc3 p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0),
				agent(a), aproc(0), aproc2(0), aproc3(p), iproc(0), iproc1(0), param(x),
				param2(y), convert(cvt), i(0) { }

	virtual Value *operator()(Tcl_Interp*, Tk_Window s, Value *arg);

    protected:
	const char *cmdstr;

	TkEventProc proc;
	TkOneParamProc proc1;
	TkTwoParamProc proc2;

	TkProxy *agent;
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

class TkFrame : public TkProxy {
    public:

	TkFrame( ProxyStore *s ) : TkProxy(s), radio_id(0), id(++count) { }
	unsigned long RadioID() const { return radio_id; }
	void RadioID( unsigned long id_ ) { radio_id = id_; }
	unsigned long Id() const { return id; }

	virtual void AddElement( TkProxy *obj );
	virtual void RemoveElement( TkProxy *obj );
	virtual void Pack();
	virtual const char *Expand() const;
	virtual int NumChildren() const;

	virtual const char *Side() const;
	virtual int ExpandNum(const TkProxy *except=0, unsigned int grtOReqt = 0) const;

    private:

	static unsigned long count;
	unsigned long radio_id;
	unsigned long id;
};
	
typedef void (*WidgetCtor)( ProxyStore *, Value * );
extern void GlishTk_Register( const char *, WidgetCtor );

#endif
