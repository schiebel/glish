#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include "Glish/Proxy.h"
#include "tkAgent.h"
#include "tkCanvas.h"
#if defined(TKPGPLOT)
#include "tkPgplot.h"
#endif

class TkStore : public ProxyStore {
    public:
	TkStore( int &argc, char **argv, Client::ShareType multithreaded = NONSHARED );
	void FD_Change( int fd, int add_flag );
	void Loop( );
    protected:
	void addfile( int fd );
	static void fileproc( ClientData data, int );
	int done;
};

void TkStore::addfile( int fd )
	{
	Tk_CreateFileHandler( fd, TK_READABLE, fileproc, (ClientData) this );
	}

void TkStore::FD_Change( int fd, int add_flag )
	{
	if ( add_flag )
		addfile( fd );
	else
		Tk_DeleteFileHandler( fd );
	}

void TkStore::fileproc( ClientData data, int fd )
	{
	TkStore *stor = (TkStore*) data;
	GlishEvent *e = stor->NextEvent();
	if ( ! e )
		stor->done = 1;
	else
		stor->ProcessEvent( e );
	}

TkStore::TkStore( int &argc, char **argv, Client::ShareType multithreaded ) :
				ProxyStore( argc, argv, multithreaded ), done(0)
	{
	SetQuiet();
	fd_set fds;
	FD_ZERO( &fds );
	int num = AddInputMask( &fds );

	for ( int i=0,cnt=0; i < FD_SETSIZE && cnt < num; ++i )
		if ( FD_ISSET( i, &fds ) )
			addfile( i );
	}

void TkStore::Loop( )
	{
	while ( ! done )
		TkAgent::DoOneTkEvent( );
	}

ProxyStore *global_store = 0;
int main( int argc, char** argv )
	{
	TkStore stor( argc, argv );

	global_store = &stor;

	stor.Register( "frame", TkFrame::Create );
	stor.Register( "button", TkButton::Create );
	stor.Register( "scale", TkScale::Create );
	stor.Register( "text", TkText::Create );
	stor.Register( "scrollbar", TkScrollbar::Create );
	stor.Register( "label", TkLabel::Create );
	stor.Register( "entry", TkEntry::Create );
	stor.Register( "message", TkMessage::Create );
	stor.Register( "listbox", TkListbox::Create );
	stor.Register( "canvas", TkCanvas::Create );
#if defined(TKPGPLOT)
	stor.Register( "pgplot", TkPgplot::Create );
#endif
	stor.Register( "version", TkAgent::Version );
	stor.Register( "have_gui", TkAgent::HaveGui );
	stor.Register( "tk_hold", TkAgent::HoldEvents );
	stor.Register( "tk_release", TkAgent::ReleaseEvents );
	stor.Register( "tk_iconpath", TkAgent::SetBitmapPath );

	stor.Loop();
	}

#if defined(TKPGPLOT)
extern "C" int grexec_();
void *grexec__ = grexec_;
#endif

