#include "Glish/Proxy.h"
#include "Glish/glishtk.h"
#include "mkWidgets.h"
#include "tkCore.h"
#include "comdefs.h"

int MkWidget::initialized = 0;
int MkTab::count = 0;

const char *glishmk_tab_addtab( TkProxy *agent, const char *, Value *args )
	{
	const char *event_name = "tab add tab function";
	MkTab *tab = (MkTab*) agent;

	const char *name = 0;
	const char *background = "lightgrey";
	const char *row = "0";

	if ( args->Type( ) == TYPE_STRING )
		{
		name = args->StringPtr(0)[0];
		}
	else
		{
		EXPRINIT( agent, event_name )
		HASARG( agent, args, >= 1 )
		EXPRSTRVAL( agent, nameval, event_name )
		name = nameval->StringPtr(0)[0];
		if ( args->Length() > 1 )
			{
			EXPRSTRVAL( agent, rowval, event_name );
			row = rowval->StringPtr(0)[0];
			if ( args->Length() > 2 )
				{
				EXPRSTRVAL( agent, backgroundval, event_name );
				background = backgroundval->StringPtr(0)[0];
				}
			}
		}

	static char tabname[256];
	GENERATE_TAG(tabname, tab, "tab")

	TkFrame *frame = new TkFrameP(tab->seq(),tab,"flat","top","0","0","0","none",background,tab->Width( ),tab->Height( ),tabname);

	if ( frame && frame->IsValid( ) )
		{
		//
		// Is this single extra "pack" sufficient???
		//
		tcl_VarEval( agent, "pack ", Tk_PathName(frame->Self()), (char*)NULL );

		char *text = glishtk_quote_string( name );
		tcl_VarEval( agent, Tk_PathName(tab->Self()), " insert ", tabname, " ", row, " -text ", text, " -window ", Tk_PathName(frame->Self()), (char*)NULL );
		free_memory( text );
		EXPR_DONE( name );

		if ( frame )
			{
			tab->Add( tabname, frame );
			frame->SendCtor("newtk",tab);
			}
		}

	else if ( frame )
		{
		Value *err = frame->GetError();
		if ( err )
			{
			tab->Error( err );
			Unref( err );
			}
		else
			tab->Error( "tk widget creation failed" );
		delete frame;
		}
		
	return 0;
	}

void MkWidgets_init( ProxyStore *store )
	{
// 	store->Register( "combobox", MkCombobox::Create );
	store->Register( "tabbox", MkTab::CreateContainer );
	store->Register( "tab", MkTab::CreateTab );
	}

MkWidget::MkWidget( ProxyStore *s ) : TkProxy( s )
	{
	if ( ! initialized )
		{
#ifdef TCLSCRIPTDIRP
		tcl_VarEval( this, "lappend auto_path ", TCLSCRIPTDIRP, (char*) NULL );
#endif
#ifdef TCLSCRIPTDIR
		tcl_VarEval( this, "lappend auto_path ", TCLSCRIPTDIR, (char*) NULL );
#endif
		tcl_VarEval( this, "package require mkWidgets", (char*) NULL );
		initialized = 1;
		}
	}

MkTab::MkTab( ProxyStore *s, TkFrame *frame_, charptr width_, charptr height_ ) : MkWidget( s ), tabcount(0)
	{
	agent_ID = "<graphic:tabbox>";
	frame = frame_;
	char *argv[12];

	count += 1;

	width = string_dup( width_ );
	height = string_dup( height_ );

	int c = 0;
	argv[c++] = (char*) "tabcontrol";
	argv[c++] = (char*) NewName(frame->Self());
	argv[c++] = (char*) "-width";
	argv[c++] = (char*) "auto";

	tcl_ArgEval( this, c, argv );

	char *ctor_error = Tcl_GetStringResult(tcl);
	if ( ctor_error && *ctor_error && *ctor_error != '.' ) HANDLE_CTOR_ERROR(ctor_error);

	self = Tk_NameToWindow( tcl, argv[1], root );

	frame->AddElement( this );
	frame->Pack();
	procs.Insert("addtab", new TkProc( this, "", glishmk_tab_addtab, glishtk_tkcast ));
	}

void MkTab::Raise( const char *tag )
	{
	tcl_VarEval( this, Tk_PathName(Self( )), " invoke ", tag, (char*)NULL );
	}

void MkTab::Add( const char *tag, TkProxy *proxy )
	{
	elements.Insert( string_dup(tag), proxy );
	}

void MkTab::Remove( const char *tag )
	{
	TkProxy *old = (TkProxy*) elements.Insert( tag, this );
	if ( old )
		{
		old->UnMap( );
		tcl_VarEval( this, Tk_PathName(this->Self( )), " delete ", tag, (char*)NULL );
		}
	}

void MkTab::UnMap()
	{
	IterCookie* c = elements.InitForIteration();
	TkProxy *member;
	const char *key;
	while ( (member = elements.NextEntry( key, c )) )
		{
		if ( member != this )
			{
			free_memory( (void*) key );
			member->UnMap( );
			}
		}
	elements.Clear( );
	}

MkTab::~MkTab( )
	{
	UnMap( );
	if ( width ) free_memory( width );
	if ( height ) free_memory( height );
	}

void MkTab::CreateContainer( ProxyStore *s, Value *args )
	{
	MkTab *ret = 0;

	if ( args->Length() != 3 )
		InvalidNumberOfArgs(3);

	SETINIT
	SETVAL( parent, parent->IsAgentRecord() )
	SETDIM( width )
	SETDIM( height )

	TkProxy *agent = (TkProxy*) (global_store->GetProxy(parent));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:frame>") )
		ret = new MkTab( s, (TkFrame*)agent, width, height );

	CREATE_RETURN
	}

void MkTab::CreateTab( ProxyStore *s, Value *args )
	{
	TkFrameP *ret = 0;
	if ( args->Length() != 11 )
		InvalidNumberOfArgs(11);

	SETINIT
	SETVAL( container, container->IsAgentRecord() )
	SETSTR( text_ )
	SETSTR( side )
	SETINT( row_ )
	char row[10];
	sprintf( row, " %d ", row_ >= 0 && row_ < 5 ? row_ : 0 );
	SETSTR( justify )
	SETDIM( padx )
	SETDIM( pady )
	SETSTR( font )
	SETINT( width_ )
	char width[30];
	sprintf( width, "%d", width_ );
	SETSTR( foreground )
	SETSTR( background )

	TkProxy *agent = (TkProxy*)(s->GetProxy(container));
	if ( agent && ! strcmp( agent->AgentID(), "<graphic:tabbox>") )
		{
		MkTab *tab = (MkTab*) agent;

		static char tabname[256];
		GENERATE_TAG(tabname, tab, "tab")

		TkFrame *frame = new TkFrameP(s,tab,"flat",side,"0",padx,pady,"none",
					      "lightgrey",tab->Width( ),tab->Height( ),tabname);

		if ( frame && frame->IsValid( ) )
			{
			//
			// Is this single extra "pack" sufficient???
			//
			tcl_VarEval( agent, "pack ", Tk_PathName(frame->Self()), (char*)NULL );

			char *text = glishtk_quote_string( text_ );

			int c = 0;
			char *argv[30];
			argv[c++] = Tk_PathName(tab->Self());
			argv[c++] = (char*) "insert";
			argv[c++] = (char*) tabname;
			argv[c++] = (char*) row;
			argv[c++] = (char*) "-text";
			argv[c++] = (char*) text;
			argv[c++] = (char*) "-window";
			argv[c++] = Tk_PathName(frame->Self());
			argv[c++] = (char*) "-justify";
			argv[c++] = (char*) justify;
			argv[c++] = (char*) "-padx";
			argv[c++] = (char*) padx;
			argv[c++] = (char*) "-pady";
			argv[c++] = (char*) pady;
			if ( font && *font )
				{
				argv[c++] = (char*) "-font";
				argv[c++] = (char*) font;
				}
			if ( width_ > 0 )
				{
				argv[c++] = (char*) "-width";
				argv[c++] = (char*) width;
				}
			argv[c++] = (char*) "-foreground";
			argv[c++] = (char*) foreground;
			argv[c++] = (char*) "-background";
			argv[c++] = (char*) background;

			tcl_ArgEval( tab, c, argv );

			char *ctor_error = Tcl_GetStringResult(tcl);
			Tcl_ResetResult(tcl);

			if ( ctor_error && *ctor_error && *ctor_error != '.' &&
			     strcmp(tabname,ctor_error) )
				{
				s->Error( ctor_error );
				delete frame;
				}

			else
				{
				tab->Add( tabname, frame );
				frame->SendCtor("newtk");
				}

			free_memory( text );
			}
		else if ( frame )
			{
			Value *err = frame->GetError();
			if ( err )
				{
				s->Error( err );
				Unref( err );
				}
			else
				s->Error( "tk widget creation failed" );

			delete frame;
			}
		else
			s->Error( "tk widget creation failed" );
		}
	else
		s->Error( "invalid tab container" );
	}
