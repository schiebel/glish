#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "Glish/glishtk.h"
#include "Glish/Proxy.h"
#include "tkUtil.h"

class TkStore : public ProxyStore {
    public:
	TkStore( int &argc, char **argv, Client::ShareType multithreaded = NONSHARED );
	void FD_Change( int fd, int add_flag );
	void Loop( );

	const char *GetOption( const char * ) const;

    protected:
	int focus_follows;
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

void TkStore::fileproc( ClientData data, int )
	{
	TkStore *stor = (TkStore*) data;
	GlishEvent *e = stor->NextEvent();

	if ( ! e )
		stor->done = 1;
	else
		stor->ProcessEvent( e );

	while ( stor->QueuedEvents() )
		{
		e = stor->NextEvent();

		if ( e )
			stor->ProcessEvent( e );
		else
			{
			stor->done = 1;
			break;
			}
		}
	}

TkStore::TkStore( int &argc, char **argv, Client::ShareType multithreaded ) :
				ProxyStore( argc, argv, multithreaded ), focus_follows(1), done(0)
	{
	for ( int i=1; i < argc; ++i )
		{
		if ( ! strcmp( argv[i], "-focus" ) )
			if ( ++i < argc )
				if ( ! strcmp( argv[i], "click" ) )
					focus_follows = 0;
		}

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
		TkProxy::DoOneTkEvent( );
	}

const char *TkStore::GetOption( const char *op ) const
	{
	if ( ! strcmp( op, "focus" ) )
		{
		static char follow[] = "follows";
		static char click[] = "click";
		return focus_follows ? follow : click;
		}
	return 0;
	}

int main( int argc, char** argv )
	{
	void *handle = 0;
	typedef void (*InitFunc)( ProxyStore * );
	if ( ! (handle = dlopen( "GlishTk.so", RTLD_NOW | RTLD_GLOBAL )) )
		{
		const char *error = dlerror( );
		if ( ! error )
			perror( "Error:" );
		else
			fprintf( stderr, "%s\n", error );
		fprintf( stderr, "Couldn't open shared object: \"GlishTk.so\"\n" );
		return 1;
		}

	
	InitFunc func = (InitFunc) dlsym( handle, "GlishTk_init" );
	if ( ! func )
		{
		const char *error = dlerror( );
		if ( ! error )
			perror( "Error:" );
		else
			fprintf( stderr, "%s\n", error );
		fprintf( stderr, "Couldn't find initialization function: \"GlishTk_init\"\n" );
		return 1;
		}

	TkStore stor( argc, argv );
	(*func)( &stor );

	stor.Loop();

        return 0;
	}
