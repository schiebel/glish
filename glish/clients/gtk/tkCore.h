// $Id$
// Copyright (c) 1997,1998 Associated Universities Inc.
//
#ifndef tkcore_h_
#define tkcore_h_

#include "tk.h"
#include "Glish/Proxy.h"
#include "Queue.h"
#include "config.h"
#include "tkAgent.h"

//###  Functions for Converting Between Strings to Values
// Split a string up into an array of strings with each newline character
extern Value *glishtk_splitnl( char * );
// turn the string into a value
extern Value *glishtk_str( char * );
// Split a string up into an array of ints with each space character
extern Value *glishtk_splitsp_int( char * );
// Split a string up into an array of strings with each space character
// note the array which is returned will be trashed by subsequent calls.
extern char **glishtk_splitsp_str_( char *, int & );
extern Value *glishtk_splitsp_str( char * );

//###  Functions for Invoking Tk Commands For Callbacks
extern char *glishtk_nostr(Tcl_Interp*, Tk_Window, const char *cmd, Value *args);
extern char *glishtk_onestr(Tcl_Interp*, Tk_Window, const char *cmd, Value *args);
extern char *glishtk_onedim(Tcl_Interp*, Tk_Window, const char *cmd, Value *args);
extern char *glishtk_oneint(Tcl_Interp*, Tk_Window, const char *cmd, Value *args);
extern char *glishtk_onebinary(Tcl_Interp*, Tk_Window, const char *cmd, const char *ptrue, const char *pfalse,
						Value *args);
extern char *glishtk_onebool(Tcl_Interp*, Tk_Window, const char *cmd, Value *args);
extern char *glishtk_oneintlist(Tcl_Interp*, Tk_Window, const char *cmd, int howmany, Value *args);
extern char *glishtk_oneidx(TkAgent *, const char *cmd, Value *args);
extern char *glishtk_oneortwoidx(TkAgent *, const char *cmd, Value *args);
extern char *glishtk_strandidx(TkAgent *, const char *cmd, Value *args);
extern char *glishtk_strwithidx(TkAgent *, const char *cmd, const char *param,
						Value *args);
extern char *glishtk_no2str(TkAgent *, const char *cmd, const char *param,
						Value *args);

extern Value *glishtk_strtoint( char *str );

extern char *glishtk_scrollbar_update(Tcl_Interp*, Tk_Window, const char *cmd, Value *args);

//### Event handlers common to Frame and tkCore widgets
extern char *glishtk_bind(TkAgent *agent, const char *, Value *args );
extern char *glishtk_disable_cb( TkAgent *a, const char *cmd, Value *args );

class TkButton : public TkRadioContainer {
    public:
	enum button_type { PLAIN, RADIO, CHECK, MENU };

	TkButton( ProxyStore *, TkFrame *, charptr label, charptr type_, charptr padx,
		  charptr pady, int width, int height, charptr justify, charptr font,
		  charptr relief, charptr borderwidth, charptr foreground,
		  charptr background, int disabled, const Value *val, charptr anchor,
		  charptr fill, charptr bitmap, TkRadioContainer *group );
	TkButton( ProxyStore *, TkButton *, charptr label, charptr type_, charptr padx,
		  charptr pady, int width, int height, charptr justify, charptr font,
		  charptr relief, charptr borderwidth, charptr foreground,
		  charptr background, int disabled, const Value *val, charptr bitmap,
		  TkRadioContainer *group );

	void UnMap();

	unsigned char State() const;
	void State(unsigned char s);

	TkButton *Parent() { return menu; }
	Tk_Window Menu() { return type == MENU ? menu_base : menu ? menu->Menu() : 0; }
	int IsMenu() { return type == MENU; }
	int IsMenuEntry() { return menu != 0; }

	void Add(TkButton *item) {  entry_list.append(item); }
	void Remove(TkButton *item) { entry_list.remove(item); }
	const char *Index( ) const { return menu_index ? menu_index : ""; }

	void ButtonPressed( );
	static void Create( ProxyStore *, Value * );

	const char **PackInstruction();
	int CanExpand() const;

	~TkButton();

	void Disable( );
	void Enable( int force = 1 );

	Tk_Window TopLevel();

	// Enable modification, used to allow glish commands to modify
	// a widget even if it has been disabled for the user.
	void EnterEnable();
	void ExitEnable();

    protected:
	Value *value;

	unsigned char state;		// only used for check buttons
	button_type type;

	TkButton *menu;
	TkRadioContainer *radio;
	Tk_Window menu_base;
	unsigned long next_menu_entry;	// only used for menu buttons
	tkagent_list entry_list;        // only used for menu buttons
	const char *menu_index;

	char *fill;
	int unmapped;
	};

class TkScale : public TkAgent {
    public:
	TkScale ( ProxyStore *, TkFrame *, double from, double to, double value, charptr len,
		  charptr text, double resolution, charptr orient, int width, charptr font,
		  charptr relief, charptr borderwidth, charptr foreground, charptr background,
		  charptr fill );

	// value was set, so generate an event
	void ValueSet( double );
	// set value
	void SetValue( double );
	static void Create( ProxyStore *, Value * );
	~TkScale();

	const char **PackInstruction();
	int CanExpand() const;

	void UnMap();

    protected:
	char *fill;
	unsigned int id;
	static unsigned int scale_count;
	double from_, to_;
	int discard_event;
	};

class TkText : public TkAgent {
    public:
	TkText( ProxyStore *, TkFrame *, int width, int height, charptr wrap,
		charptr font, int disabled, charptr text, charptr relief,
		charptr borderwidth, charptr foreground, charptr background,
		charptr fill);

	charptr IndexCheck( charptr );
	const char **PackInstruction();
	int CanExpand() const;

	static void Create( ProxyStore *, Value * );
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
	TkScrollbar( ProxyStore *, TkFrame *, charptr orient, int width,
		     charptr foreground, charptr background );

	const char **PackInstruction();
	int CanExpand() const;

	static void Create( ProxyStore *, Value * );
	void Scrolled( Value *data );
	~TkScrollbar();

	void UnMap();
	};

class TkLabel : public TkAgent {
    public:
	TkLabel( ProxyStore *, TkFrame *, charptr text,
		 charptr justify, charptr padx, charptr pady, int width,
		 charptr font, charptr relief, charptr borderwidth,
		 charptr foreground, charptr background, charptr anchor, charptr fill );

	static void Create( ProxyStore *, Value * );
	~TkLabel();

	const char **PackInstruction();
	int CanExpand() const;

    protected:
	char *fill;
	};

class TkEntry : public TkAgent {
    public:
	TkEntry( ProxyStore *, TkFrame *, int width,
		 charptr justify, charptr font, charptr relief, 
		 charptr borderwidth, charptr foreground, charptr background,
		 int disabled, int show, int exportselection, charptr fill );

	static void Create( ProxyStore *, Value * );

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
	TkMessage( ProxyStore *, TkFrame *, charptr text, charptr width,
		   charptr justify, charptr font, charptr padx, charptr pady,
		   charptr relief, charptr borderwidth,
		   charptr foreground, charptr background, charptr anchor, charptr fill );

	static void Create( ProxyStore *, Value * );

	~TkMessage();

	const char **PackInstruction();
	int CanExpand() const;

    protected:
	char *fill;

	};

class TkListbox : public TkAgent {
    public:
	TkListbox( ProxyStore *, TkFrame *, int width, int height, charptr mode,
		   charptr font, charptr relief, charptr borderwidth,
		  charptr foreground, charptr background, int exportselection, charptr fill );

	charptr IndexCheck( charptr );

	void yScrolled( const double *firstlast );
	void xScrolled( const double *firstlast );
	void elementSelected( );
  
	static void Create( ProxyStore *, Value * );

	const char **PackInstruction();
	int CanExpand() const;

	~TkListbox();
	void UnMap();

    protected:
	char *fill;
	};

#endif
