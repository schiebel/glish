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
		TkProxy::DoOneTkEvent( );
	}

int main( int argc, char** argv )
	{
	void *handle = 0;
	typedef void (*InitFunc)( ProxyStore * );
	if ( ! (handle = dlopen( "/home/drs/dev/glish/glish/clients/glishtk/i686-unknown-linux/shared/GlishTk.so", RTLD_NOW | RTLD_GLOBAL )) )
		{
		const char *error = dlerror( );
		if ( ! error )
			perror( "Error:" );
		else
			fprintf( stderr, "%s\n", error );
		fprintf( stderr, "Couldn't open shared object: \"GlishTk.so\"\t\t=>0x%x\n",handle );
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
