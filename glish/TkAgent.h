// $Header$

#ifndef tkagent_h
#define tkagent_h

#ifdef GLISHTK
#include "Agent.h"
#include "BuiltIn.h"

struct _Rivetstruct;
typedef struct _Rivetstruct *Rivetobj;

class TkAgent;
class TkFrame;
declare(PList,TkAgent);
typedef PList(TkAgent) tkagent_list;

class TkProc;
declare(PDict,TkProc);
typedef PDict(TkProc) tkprochash;

//###  Functions for Converting Between Strings to Values
// Split a string up into an array of strings with each newline character
extern IValue *glishtk_splitnl( char * );
// turn the string into a value
extern IValue *glishtk_str( char * );
// Split a string up into an array of ints with each space character
extern IValue *glishtk_splitsp_int( char * );
// Split a string up into an array of strings with each space character
// note the array which is returned will be trashed by subsequent calls.
extern char **glishtk_splitsp_str( char *, int & );

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
typedef char *(*TkOneIntProc)(Rivetobj, const char *, int, parameter_list *, int, int);
typedef char *(*TkTwoParamProc)(Rivetobj, const char *, const char *, const char *, parameter_list *, int, int);
typedef char *(*TkEventAgentProc)(TkAgent*, const char *, parameter_list*, int, int);
typedef char *(*TkEventAgentProc2)(TkAgent*, const char *, const char *, parameter_list*, int, int);
typedef IValue *(*TkStrToValProc)( char * );

class TkProc {
    public:
	TkProc(const char *c, TkEventProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(p), proc1(0), proc2(0), fproc(0), frame(0),
				aproc(0), agent(0), aproc2(0), iproc(0), param(0), param2(0),
				convert(cvt), i(0) { }
	TkProc(const char *c, const char *x, const char *y, TkTwoParamProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(p), fproc(0), frame(0),
				aproc(0), agent(0), aproc2(0), iproc(0), param(x), param2(y),
				convert(cvt), i(0) { }
	TkProc(const char *c, const char *x, TkOneParamProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(p), proc2(0), fproc(0), frame(0),
				aproc(0), agent(0), aproc2(0), iproc(0), param(x), param2(0),
				convert(cvt), i(0) { }
	TkProc(const char *c, int x, TkOneIntProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0), fproc(0), frame(0),
				aproc(0), agent(0), aproc2(0), iproc(p), param(0), param2(0),
				convert(cvt), i(x) { }
	TkProc(TkFrame *f, char *(TkFrame::*p)(parameter_list*,int,int), TkStrToValProc cvt = 0)
			: cmdstr(0), proc(0), proc1(0), proc2(0), fproc(p), frame(f),
				aproc(0), agent(0), aproc2(0), iproc(0), param(0), param2(0),
				convert(cvt), i(0) { }
	TkProc(TkAgent *a, const char *c, TkEventAgentProc p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0), fproc(0), frame(0),
				aproc(p), agent(a), aproc2(0), iproc(0), param(0), param2(0),
				convert(cvt), i(0) { }
	TkProc(TkAgent *a, const char *c, const char *x, TkEventAgentProc2 p, TkStrToValProc cvt = 0)
			: cmdstr(c), proc(0), proc1(0), proc2(0), fproc(0), frame(0),
				aproc(0), agent(a), aproc2(p), iproc(0), param(x), param2(0),
				convert(cvt), i(0) { }
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

	TkFrame *frame;
	char *(TkFrame::*fproc)(parameter_list*, int, int);

	TkAgent *agent;
	TkEventAgentProc aproc;
	TkEventAgentProc2 aproc2;

	TkStrToValProc convert;
	};

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
	virtual const char **PackInstruction();
    protected:
	tkprochash procs;
	static Rivetobj root;
	Rivetobj self;
	TkFrame *frame;
	};

class TkFrame : public TkAgent {
    public:
	TkFrame( Sequencer *s, charptr relief_, charptr side_, charptr borderwidth,
		  charptr padx_, charptr pady_, charptr expand_, charptr background,
		  charptr width, charptr height, charptr title );
	TkFrame( Sequencer *s, TkFrame *frame_, charptr relief_, charptr side_,
		  charptr borderwidth, charptr padx_, charptr pady_, charptr expand_,
		  charptr background, charptr width, charptr height );

	char *SetSide( parameter_list *, int, int );
	char *SetPadx( parameter_list *, int, int );
	char *SetPady( parameter_list *, int, int );
	char *SetExpand( parameter_list *, int, int );

	void Pack();
	void PackSpecial( TkAgent * );
	void AddElement( TkAgent *obj ) { elements.append(obj); }
	void RemoveElement( TkAgent *obj );
	void UnMap();
	static TkAgent *Create( Sequencer *, const_args_list *);
	~TkFrame();

	const char **PackInstruction();

    protected:
	char *side;
	char *padx;
	char *pady;
	char *expand;
  	tkagent_list elements;
	static unsigned int tl_cnt;
	char is_tl;
	Rivetobj pseudo;
	};

class TkButton : public TkAgent {
    public:
	TkButton( Sequencer *, TkFrame *, charptr label, charptr padx, charptr pady,
		  int width, int height, charptr justify, charptr font, charptr relief,
		  charptr borderwidth, charptr foreground, charptr background, int disabled );

	void ButtonPressed( );
	static TkAgent *Create( Sequencer *, const_args_list *);
	~TkButton();
	};

class TkScale : public TkAgent {
    public:
	TkScale ( Sequencer *, TkFrame *, int from, int to, charptr len,
		  charptr text, charptr orient, charptr relief, charptr borderwidth,
		  charptr foreground, charptr background );

	void ValueSet( double );
	static TkAgent *Create( Sequencer *, const_args_list *);
	~TkScale();
	};

class TkText : public TkAgent {
    public:
	TkText( Sequencer *, TkFrame *, int width, int height, charptr wrap,
		charptr font, int disabled, charptr text, charptr relief,
		charptr borderwidth, charptr foreground, charptr background,
		charptr fill);

	charptr IndexCheck( charptr );
	const char **PackInstruction();

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

	static TkAgent *Create( Sequencer *, const_args_list *);
	void Scrolled( IValue *data );
	~TkScrollbar();
	};

class TkLabel : public TkAgent {
    public:
	TkLabel( Sequencer *, TkFrame *, charptr text,
		 charptr justify, charptr padx, charptr pady, int width,
		 charptr font, charptr relief, charptr borderwidth,
		 charptr foreground, charptr background );

	static TkAgent *Create( Sequencer *, const_args_list *);
	~TkLabel();
	};

class TkEntry : public TkAgent {
    public:
	TkEntry( Sequencer *, TkFrame *, int width,
		 charptr justify, charptr font, charptr relief, 
		 charptr borderwidth, charptr foreground, charptr background,
		 int disabled, int show, int exportselection );

	static TkAgent *Create( Sequencer *, const_args_list *);

	charptr IndexCheck( charptr );

	void ReturnHit( );
	void xScrolled( const double *firstlast );
	~TkEntry();
	};

class TkMessage : public TkAgent {
    public:
	TkMessage( Sequencer *, TkFrame *, charptr text, charptr width,
		   charptr justify, charptr font, charptr padx, charptr pady,
		   charptr relief, charptr borderwidth,
		   charptr foreground, charptr background );

	static TkAgent *Create( Sequencer *, const_args_list *);

	~TkMessage();
	};

class TkListbox : public TkAgent {
    public:
	TkListbox( Sequencer *, TkFrame *, int width, int height, charptr mode,
		   charptr font, charptr relief, charptr borderwidth,
		  charptr foreground, charptr background, int exportselection );

	charptr IndexCheck( charptr );

	void yScrolled( const double *firstlast );
	void xScrolled( const double *firstlast );
	void elementSelected( );
  
	static TkAgent *Create( Sequencer *, const_args_list *);
	~TkListbox();
	};


#endif
#endif
