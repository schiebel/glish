// $Id$
// Copyright (c) 1997 Associated Universities Inc.
//
#ifndef tkagent_h
#define tkagent_h

#ifdef GLISHTK
#include "Agent.h"
#include "BuiltIn.h"
#include "Queue.h"

struct _Rivetstruct;
typedef struct _Rivetstruct *Rivetobj;

extern int TkHaveGui();

class TkAgent;
class TkCanvas;
class TkFrame;
#if defined(TKPGPLOT)
class TkPgplot;
#endif
glish_declare(PList,TkAgent);
typedef PList(TkAgent) tkagent_list;

class TkProc;
glish_declare(PDict,TkProc);
typedef PDict(TkProc) tkprochash;

//
// This is somewhat of a hack to notify the TkSelector that a
// glish event of interest has occurred. See TkSelector.cc for
// more information
//
extern void glish_event_posted( int );

//###  Functions for Converting Between Strings to Values
// Split a string up into an array of strings with each newline character
extern IValue *glishtk_splitnl( char * );
// turn the string into a value
extern IValue *glishtk_str( char * );
// Split a string up into an array of ints with each space character
extern IValue *glishtk_splitsp_int( char * );
// Split a string up into an array of strings with each space character
// note the array which is returned will be trashed by subsequent calls.
extern char **glishtk_splitsp_str_( char *, int & );
extern IValue *glishtk_splitsp_str( char * );

//###  Functions for Invoking Rivet Commands For Callbacks
extern char *glishtk_nostr(Rivetobj, const char *cmd, parameter_list *args, int, int);
extern char *glishtk_onestr(Rivetobj, const char *cmd, parameter_list *args, int, int);
extern char *glishtk_onedim(Rivetobj, const char *cmd, parameter_list *args, int, int);
extern char *glishtk_oneint(Rivetobj, const char *cmd, parameter_list *args, int, int);
extern char *glishtk_onebinary(Rivetobj, const char *cmd, const char *ptrue, const char *pfalse,
						parameter_list *args, int, int);
extern char *glishtk_onebool(Rivetobj, const char *cmd, parameter_list *args, int, int);
extern char *glishtk_oneintlist(Rivetobj, const char *cmd, int howmany, parameter_list *args,
						int, int );
extern char *glishtk_oneidx(TkAgent *, const char *cmd, parameter_list *args, int, int);
extern char *glishtk_oneortwoidx(TkAgent *, const char *cmd, parameter_list *args, int, int);
extern char *glishtk_strandidx(TkAgent *, const char *cmd, parameter_list *args, int, int);
extern char *glishtk_strwithidx(TkAgent *, const char *cmd, const char *param,
						parameter_list *args, int, int);
extern char *glishtk_no2str(TkAgent *, const char *cmd, const char *param,
						parameter_list *args, int, int);

extern char *glishtk_scrolled_update(Rivetobj, const char *cmd, parameter_list *args, int, int);
extern char *glishtk_scrollbar_update(Rivetobj, const char *cmd, parameter_list *args, int, int);


//###  Callback Procs
typedef char *(*TkEventProc)(Rivetobj, const char *, parameter_list*, int, int);
typedef char *(*TkOneParamProc)(Rivetobj, const char *, const char *, parameter_list *, int, int);
typedef char *(*TkTwoParamProc)(Rivetobj, const char *, const char *, const char *, parameter_list *, int, int);
typedef char *(*TkOneIntProc)(Rivetobj, const char *, int, parameter_list *, int, int);
typedef char *(*TkTwoIntProc)(Rivetobj, const char *, const char *, int, parameter_list *, int, int);
typedef char *(*TkEventAgentProc)(TkAgent*, const char *, parameter_list*, int, int);
typedef char *(*TkEventAgentProc2)(TkAgent*, const char *, const char *, parameter_list*, int, int);
typedef char *(*TkEventAgentProc3)(TkAgent*, const char *, const char *, const char *, parameter_list*, int, int);
typedef IValue *(*TkStrToValProc)( char * );

#if defined(TKPGPLOT)
#define TKPGI(x) pgproc(x),
#else
#define TKPGI(x)
#endif

class TkProc {
    public:
	TkProc(const char *c, TkEventProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(p), proc1(0), proc2(0), TKPGI(0) fproc(0), frame(0),
				aproc(0), agent(0), aproc2(0), aproc3(0), iproc(0), iproc1(0), param(0),
				param2(0), convert(cvt), i(0) { }
	TkProc(const char *c, const char *x, const char *y, TkTwoParamProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(p), TKPGI(0) fproc(0), frame(0),
				aproc(0), agent(0), aproc2(0), aproc3(0), iproc(0), iproc1(0), param(x),
				param2(y), convert(cvt), i(0) { }
	TkProc(const char *c, const char *x, int y, TkTwoIntProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0), TKPGI(0) fproc(0), frame(0),
				aproc(0), agent(0), aproc2(0), aproc3(0), iproc(0), iproc1(p), param(x),
				param2(0), convert(cvt), i(y) { }
	TkProc(const char *c, const char *x, TkOneParamProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(p), proc2(0), TKPGI(0) fproc(0), frame(0),
				aproc(0), agent(0), aproc2(0), aproc3(0), iproc(0), iproc1(0), param(x),
				param2(0), convert(cvt), i(0) { }
	TkProc(const char *c, int x, TkOneIntProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0), TKPGI(0) fproc(0), frame(0),
				aproc(0), agent(0), aproc2(0), aproc3(0), iproc(p), iproc1(0), param(0),
				param2(0), convert(cvt), i(x) { }
	TkProc(TkFrame *f, char *(TkFrame::*p)(parameter_list*,int,int), TkStrToValProc cvt = 0)
			: cmdstr(0), proc(0), proc1(0), proc2(0), TKPGI(0) fproc(p), frame(f),
				aproc(0), agent(0), aproc2(0), aproc3(0), iproc(0), iproc1(0), param(0),
				param2(0), convert(cvt), i(0) { }
#if defined(TKPGPLOT)
	TkProc(TkPgplot *f, char *(TkPgplot::*p)(parameter_list*,int,int), TkStrToValProc cvt = 0)
			: cmdstr(0), proc(0), proc1(0), proc2(0), TKPGI(p) fproc(0), pgplot(f),
				aproc(0), agent(0), aproc2(0), aproc3(0), iproc(0), iproc1(0), param(0),
				param2(0), convert(cvt), i(0) { }
#endif
	TkProc(TkAgent *a, const char *c, TkEventAgentProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0), TKPGI(0) fproc(0), frame(0),
				aproc(p), agent(a), aproc2(0), aproc3(0), iproc(0), iproc1(0), param(0),
				param2(0), convert(cvt), i(0) { }
	TkProc(TkAgent *a, const char *c, const char *x, TkEventAgentProc2 p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0), TKPGI(0) fproc(0), frame(0),
				aproc(0), agent(a), aproc2(p), aproc3(0), iproc(0), iproc1(0), param(x),
				param2(0), convert(cvt), i(0) { }
	TkProc(TkAgent *a, const char *c, const char *x, const char *y, TkEventAgentProc3 p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0), TKPGI(0) fproc(0), frame(0),
				aproc(0), agent(a), aproc2(0), aproc3(p), iproc(0), iproc1(0), param(x),
				param2(y), convert(cvt), i(0) { }
	IValue *operator()(Rivetobj s, parameter_list*arg, int x, int y);
    protected:
	const char *cmdstr;

	TkEventProc proc;
	TkOneParamProc proc1;
	TkTwoParamProc proc2;

#if defined(TKPGPLOT)
	TkPgplot *pgplot;
	char *(TkPgplot::*pgproc)(parameter_list*, int, int);
#endif
	char *(TkFrame::*fproc)(parameter_list*, int, int);
	TkFrame *frame;

	TkEventAgentProc aproc;
	TkAgent *agent;
	TkEventAgentProc2 aproc2;
	TkEventAgentProc3 aproc3;

	TkOneIntProc iproc;
	TkTwoIntProc iproc1;

	const char *param;
	const char *param2;

	TkStrToValProc convert;

	int i;
	};

class glishtk_event;
glish_declare(PQueue,glishtk_event);

class TkAgent : public Agent {
    public:
	TkAgent( Sequencer *s );
	~TkAgent();

	IValue* SendEvent( const char* event_name, parameter_list* args,
				int is_request, int log );
	virtual IValue* HandleEvent( const char* event_name, parameter_list* args,
				int is_request, int log )
		{ return SendEvent( event_name, args, is_request, log ); }

	virtual IValue *Invoke(TkProc*,parameter_list*arg, int x, int y);

	virtual charptr IndexCheck( charptr );

	IValue *UnrecognizedEvent();
	int IsValid() { return self != 0; }
	virtual void UnMap();
	Rivetobj Self() { return self; }
	const Rivetobj Self() const { return self; }

	virtual const char **PackInstruction();
	virtual int CanExpand() const;

	// Used to post events which *originate* from Tk. This is important
	// because of scrollbar/canvas initialization, i.e. the Tk events
	// must be queued and sent after the "whenever"s are in place.
	void PostTkEvent( const char *, IValue *,
			  int complain_if_no_interest = 0, NotifyTrigger *t=0 );

	static int GlishEventsHeld() { return hold_glish_events; }
	static void FlushGlishEvents();

	static void HoldEvents() { hold_tk_events++; }
	static void ReleaseEvents() { hold_tk_events--; }
	static int DoOneTkEvent( int flags, int hold_wait = 0 );
	static int DoOneTkEvent( );

	// For some widgets, they must be enabled before an action is performed
	// otherwise widgets which are disabled will not even accept changes
	// from a script.
	virtual void EnterEnable();
	virtual void ExitEnable();

	static IValue *GetError() { return last_error; }
	static void SetError(IValue*);

	void SetMap( int do_map, int toplevel );
	int DontMap( ) const { return dont_map; }

	virtual void Disable( );
	virtual void Enable( int force = 1 );

	void BindEvent(const char *event, IValue *rec);

	virtual Rivetobj TopLevel( );

    protected:
	tkprochash procs;
	static Rivetobj root;
	Rivetobj self;
	TkFrame *frame;

	static int hold_tk_events;
	static int hold_glish_events;
	static PQueue(glishtk_event) *tk_queue;

	// For keeping track of last error
	static IValue *last_error;

	unsigned int enable_state;

	int dont_map;

	unsigned int disable_count;
	};

class TkRadioContainer : public TkAgent {
    public:
	TkRadioContainer( Sequencer *s ) : TkAgent(s), radio_id(0), id(++count) { }
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
	TkFrame( Sequencer *s, charptr relief_, charptr side_, charptr borderwidth,
		  charptr padx_, charptr pady_, charptr expand_, charptr background,
		  charptr width, charptr height, charptr cursor, charptr title,
		  charptr icon, int new_cmap );
	TkFrame( Sequencer *s, TkFrame *frame_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_,
		  charptr background, charptr width, charptr height, charptr cursor,
		  int new_cmap );
	TkFrame( Sequencer *s, TkCanvas *canvas_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_,
		  charptr background, charptr width, charptr height, const char *tag_ );

	static unsigned long TopLevelCount() { return tl_count; }

	// Called when the frame is killed via the window manager
	void KillFrame( );
	void ResizeEvent( );

	char *SetSide( parameter_list *, int, int );
	char *SetPadx( parameter_list *, int, int );
	char *SetPady( parameter_list *, int, int );
	char *SetExpand( parameter_list *, int, int );
	char *GetTag( parameter_list *, int, int );

	char *SetIcon( parameter_list *, int, int );

	char *Grab( int global_scope=0 );
	char *Release( );
	char *GrabCB( parameter_list *, int, int );
	char *ReleaseCB( parameter_list *, int, int );
	char *FontsCB( parameter_list *, int, int );

	void Pack();
	void PackSpecial( TkAgent * );
	void AddElement( TkAgent *obj ) { elements.append(obj); }
	void RemoveElement( TkAgent *obj );
	void UnMap();

	static IValue *Create( Sequencer *, const_args_list *);
	~TkFrame();

	const char *Expand() const { return expand; }

	const char **PackInstruction();
	int CanExpand() const;

	int ExpandNum(const TkAgent *except=0, unsigned int grtOReqt = 0) const;

	int NumChildren() const { return elements.length() - 1; }

	void Disable( );
	void Enable( int force = 1 );

	Rivetobj TopLevel();

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
	Rivetobj pseudo;

	unsigned char reject_first_resize;

	int size[2];

	int unmapped;
	};

class TkButton : public TkRadioContainer {
    public:
	enum button_type { PLAIN, RADIO, CHECK, MENU };

	TkButton( Sequencer *, TkFrame *, charptr label, charptr type_, charptr padx,
		  charptr pady, int width, int height, charptr justify, charptr font,
		  charptr relief, charptr borderwidth, charptr foreground,
		  charptr background, int disabled, const IValue *val, charptr anchor,
		  charptr fill, TkRadioContainer *group );
	TkButton( Sequencer *, TkButton *, charptr label, charptr type_, charptr padx,
		  charptr pady, int width, int height, charptr justify, charptr font,
		  charptr relief, charptr borderwidth, charptr foreground,
		  charptr background, int disabled, const IValue *val,
		  TkRadioContainer *group );

	void UnMap();

	unsigned char State() const;
	void State(unsigned char s);

	TkButton *Parent() { return menu; }
	Rivetobj Menu() { return type == MENU ? menu_base : menu ? menu->Menu() : 0; }
	int IsMenu() { return type == MENU; }
	int IsMenuEntry() { return menu != 0; }

	void Add(TkButton *item) {  entry_list.append(item); }
	void Remove(TkButton *item) { entry_list.remove(item); }
	const char *Index( ) const { return menu_index ? menu_index : ""; }

	void ButtonPressed( );
	static IValue *Create( Sequencer *, const_args_list *);

	const char **PackInstruction();
	int CanExpand() const;

	~TkButton();

	void Disable( );
	void Enable( int force = 1 );

    protected:
	IValue *value;

	unsigned char state;		// only used for check buttons
	button_type type;

	TkButton *menu;
	TkRadioContainer *radio;
	Rivetobj menu_base;
	unsigned long next_menu_entry;	// only used for menu buttons
	tkagent_list entry_list;        // only used for menu buttons
	const char *menu_index;

	char *fill;
	int unmapped;
	};

class TkScale : public TkAgent {
    public:
	TkScale ( Sequencer *, TkFrame *, double from, double to, charptr len, charptr text,
		  double resolution, charptr orient, int width, charptr font, charptr relief,
		  charptr borderwidth, charptr foreground, charptr background, charptr fill );

	// value was set, so generate an event
	void ValueSet( double );
	// set value
	void SetValue( double );
	static IValue *Create( Sequencer *, const_args_list *);
	~TkScale();

	const char **PackInstruction();
	int CanExpand() const;

	void UnMap();

    protected:
	char *fill;
	unsigned int id;
	static unsigned int scale_count;
	double from_, to_;
	};

class TkText : public TkAgent {
    public:
	TkText( Sequencer *, TkFrame *, int width, int height, charptr wrap,
		charptr font, int disabled, charptr text, charptr relief,
		charptr borderwidth, charptr foreground, charptr background,
		charptr fill);

	charptr IndexCheck( charptr );
	const char **PackInstruction();
	int CanExpand() const;

	static IValue *Create( Sequencer *, const_args_list *);
	void yScrolled( const double *firstlast );
	void xScrolled( const double *firstlast );
	~TkText();

	// Enable modification, used to allow glish commands to modify
	// a widget even if it has been disabled for the user.
	void EnterEnable();
	void ExitEnable();

	void Disable( );
	void Enable( int force = 1 );

	void UnMap();

    protected:
	char *fill;
	};

class TkScrollbar : public TkAgent {
    public:
	TkScrollbar( Sequencer *, TkFrame *, charptr orient, int width,
		     charptr foreground, charptr background );

	const char **PackInstruction();
	int CanExpand() const;

	static IValue *Create( Sequencer *, const_args_list *);
	void Scrolled( IValue *data );
	~TkScrollbar();

	void UnMap();
	};

class TkLabel : public TkAgent {
    public:
	TkLabel( Sequencer *, TkFrame *, charptr text,
		 charptr justify, charptr padx, charptr pady, int width,
		 charptr font, charptr relief, charptr borderwidth,
		 charptr foreground, charptr background, charptr anchor, charptr fill );

	static IValue *Create( Sequencer *, const_args_list *);
	~TkLabel();

	const char **PackInstruction();
	int CanExpand() const;

    protected:
	char *fill;
	};

class TkEntry : public TkAgent {
    public:
	TkEntry( Sequencer *, TkFrame *, int width,
		 charptr justify, charptr font, charptr relief, 
		 charptr borderwidth, charptr foreground, charptr background,
		 int disabled, int show, int exportselection, charptr fill );

	static IValue *Create( Sequencer *, const_args_list *);

	charptr IndexCheck( charptr );

	void ReturnHit( );
	void xScrolled( const double *firstlast );

	const char **PackInstruction();
	int CanExpand() const;

	~TkEntry();

	void EnterEnable();
	void ExitEnable();

	void Disable( );
	void Enable( int force = 1 );

	void UnMap();

    protected:
	char *fill;

	};

class TkMessage : public TkAgent {
    public:
	TkMessage( Sequencer *, TkFrame *, charptr text, charptr width,
		   charptr justify, charptr font, charptr padx, charptr pady,
		   charptr relief, charptr borderwidth,
		   charptr foreground, charptr background, charptr anchor, charptr fill );

	static IValue *Create( Sequencer *, const_args_list *);

	~TkMessage();

	const char **PackInstruction();
	int CanExpand() const;

    protected:
	char *fill;

	};

class TkListbox : public TkAgent {
    public:
	TkListbox( Sequencer *, TkFrame *, int width, int height, charptr mode,
		   charptr font, charptr relief, charptr borderwidth,
		  charptr foreground, charptr background, int exportselection, charptr fill );

	charptr IndexCheck( charptr );

	void yScrolled( const double *firstlast );
	void xScrolled( const double *firstlast );
	void elementSelected( );
  
	static IValue *Create( Sequencer *, const_args_list *);

	const char **PackInstruction();
	int CanExpand() const;

	~TkListbox();
	void UnMap();

    protected:
	char *fill;
	};


#endif
#endif
