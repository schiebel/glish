// $Header$

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
class TkPGPLOT;
#endif
declare(PList,TkAgent);
typedef PList(TkAgent) tkagent_list;

class TkProc;
declare(PDict,TkProc);
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
	TkProc(TkPGPLOT *f, char *(TkPGPLOT::*p)(parameter_list*,int,int), TkStrToValProc cvt = 0)
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
	const char *param;
	const char *param2;
	int i;
	TkEventProc proc;
	TkOneParamProc proc1;
	TkTwoParamProc proc2;
	TkOneIntProc iproc;
	TkTwoIntProc iproc1;

	TkFrame *frame;
	char *(TkFrame::*fproc)(parameter_list*, int, int);
#if defined(TKPGPLOT)
	TkPGPLOT *pgplot;
	char *(TkPGPLOT::*pgproc)(parameter_list*, int, int);
#endif

	TkAgent *agent;
	TkEventAgentProc aproc;
	TkEventAgentProc2 aproc2;
	TkEventAgentProc3 aproc3;

	TkStrToValProc convert;
	};

class glishtk_event;
declare(PQueue,glishtk_event);

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
	static int QueuedEvents() { return hold_events; }
	static int InitialHold() { return initial_hold; }
	static void HoldEvents() { hold_events++; }
	static void ReleaseEvents();

    protected:
	tkprochash procs;
	static Rivetobj root;
	Rivetobj self;
	TkFrame *frame;

	static int hold_events;
	static int initial_hold;
	static PQueue(glishtk_event) *tk_queue;
	};

class TkFrame : public TkAgent {
    public:
	TkFrame( Sequencer *s, charptr relief_, charptr side_, charptr borderwidth,
		  charptr padx_, charptr pady_, charptr expand_, charptr background,
		  charptr width, charptr height, charptr cursor, charptr title );
	TkFrame( Sequencer *s, TkFrame *frame_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_,
		  charptr background, charptr width, charptr height, charptr cursor);
	TkFrame( Sequencer *s, TkCanvas *canvas_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_,
		  charptr background, charptr width, charptr height, const char *tag_ );

	static unsigned long TopLevelCount() { return tl_count; }

	// Called when the frame is killed via the window manager
	void KillFrame( );

	char *SetSide( parameter_list *, int, int );
	char *SetPadx( parameter_list *, int, int );
	char *SetPady( parameter_list *, int, int );
	char *SetExpand( parameter_list *, int, int );
	char *GetTag( parameter_list *, int, int );

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

	unsigned long Count() const { return frame_count; }
	unsigned long Id() const { return id; }

	static TkAgent *Create( Sequencer *, const_args_list *);
	~TkFrame();

	const char *Expand() const { return expand; }

	const char **PackInstruction();
	int CanExpand() const;

	unsigned long RadioID() const { return radio_id; }
	void RadioID( unsigned long id_ ) { radio_id = id_; }

	int ExpandNum(const TkAgent *except=0, unsigned int grtOReqt = 0) const;

	int NumChildren() const { return elements.length() - 1; }
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
	static unsigned long frame_count;
	static unsigned long grab;
	unsigned long id;
	char is_tl;
	Rivetobj pseudo;

	unsigned long radio_id;
	};

class TkButton : public TkAgent {
    public:
	enum button_type { PLAIN, RADIO, CHECK, MENU };

	TkButton( Sequencer *, TkFrame *, charptr label, charptr type_, charptr padx,
		  charptr pady, int width, int height, charptr justify, charptr font,
		  charptr relief, charptr borderwidth, charptr foreground,
		  charptr background, int disabled, const IValue *val, charptr fill );
	TkButton( Sequencer *, TkButton *, charptr label, charptr type_, charptr padx,
		  charptr pady, int width, int height, charptr justify, charptr font,
		  charptr relief, charptr borderwidth, charptr foreground,
		  charptr background, int disabled, const IValue *val );

	void UnMap();

	unsigned long Count() const { return button_count; }
	unsigned char State() const;
	void State(unsigned char s);
	unsigned long Id() const { return id; }

	TkButton *Parent() { return menu; }
	Rivetobj Menu() { return menu ? menu->Menu() : menu_base; }
	int IsMenu() { return type == MENU; }
	int IsMenuEntry() { return menu != 0; }
	unsigned long RadioID() const { return radio_id; }
	void RadioID( unsigned long id ) { radio_id = id; }
	void Add(TkButton *item) {  entry_list.append(item); }
	void Remove(TkButton *item) { entry_list.remove(item); }
	int Index(TkButton *) const;

	void ButtonPressed( );
	static TkAgent *Create( Sequencer *, const_args_list *);

	const char **PackInstruction();
	int CanExpand() const;

	~TkButton();
    protected:
	static unsigned long button_count;
	IValue *value;

	unsigned char state;		// only used for check buttons
	button_type type;
	unsigned long id;

	TkButton *menu;
	Rivetobj menu_base;
	unsigned long next_menu_entry;	// only used for menu buttons
	unsigned long radio_id;		// only used for menu buttons
	tkagent_list entry_list;	// only used for menu buttons

	char *fill;
	};

class TkScale : public TkAgent {
    public:
	TkScale ( Sequencer *, TkFrame *, int from, int to, charptr len,
		  charptr text, charptr orient, charptr relief, charptr borderwidth,
		  charptr foreground, charptr background, charptr fill );

	void ValueSet( double );
	static TkAgent *Create( Sequencer *, const_args_list *);
	~TkScale();

	const char **PackInstruction();
	int CanExpand() const;

    protected:
	char *fill;
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

	static TkAgent *Create( Sequencer *, const_args_list *);
	void yScrolled( const double *firstlast );
	void xScrolled( const double *firstlast );
	~TkText();
    protected:
	char *fill;
	};

class TkScrollbar : public TkAgent {
    public:
	TkScrollbar( Sequencer *, TkFrame *, charptr orient, 
		     charptr foreground, charptr background );

	const char **PackInstruction();
	int CanExpand() const;

	static TkAgent *Create( Sequencer *, const_args_list *);
	void Scrolled( IValue *data );
	~TkScrollbar();
	};

class TkLabel : public TkAgent {
    public:
	TkLabel( Sequencer *, TkFrame *, charptr text,
		 charptr justify, charptr padx, charptr pady, int width,
		 charptr font, charptr relief, charptr borderwidth,
		 charptr foreground, charptr background, charptr fill );

	static TkAgent *Create( Sequencer *, const_args_list *);
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

	static TkAgent *Create( Sequencer *, const_args_list *);

	charptr IndexCheck( charptr );

	void ReturnHit( );
	void xScrolled( const double *firstlast );

	const char **PackInstruction();
	int CanExpand() const;

	~TkEntry();
    protected:
	char *fill;

	};

class TkMessage : public TkAgent {
    public:
	TkMessage( Sequencer *, TkFrame *, charptr text, charptr width,
		   charptr justify, charptr font, charptr padx, charptr pady,
		   charptr relief, charptr borderwidth,
		   charptr foreground, charptr background, charptr fill );

	static TkAgent *Create( Sequencer *, const_args_list *);

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
  
	static TkAgent *Create( Sequencer *, const_args_list *);

	const char **PackInstruction();
	int CanExpand() const;

	~TkListbox();

    protected:
	char *fill;
	};


#endif
#endif
