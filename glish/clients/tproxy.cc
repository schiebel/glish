// $Id$
// Copyright (c) 1997,1998 Associated Universities Inc.
#include <iostream.h>
#include "Glish/Proxy.h"

class ProxyA : public Proxy {
    public:
	ProxyA( ProxyStore *s );
	~ProxyA( );
	static void Create( ProxyStore *s, Value *v, GlishEvent *e, void *data );
	void ProcessEvent( const char *name, Value *val );
};

ProxyA::ProxyA( ProxyStore *s ) : Proxy(s)
	{ cerr << "Created a ProxyA: " << id << endl; }

ProxyA::~ProxyA( )
	{ cerr << "Deleted a ProxyA: " << id << endl; }

void ProxyA::Create( ProxyStore *s, Value *v, GlishEvent *e, void *data )
	{ 
	cerr << "In ProxyA::Create" << endl;
	ProxyA *np = new ProxyA( s );
	np->SendCtor("newtp");
	}

void ProxyA::ProcessEvent( const char *name, Value *val )
	{
	Value *result = new Value(id.id());
	if ( ReplyPending() )
		Reply( result );
	else
		PostEvent( name, result );
	Unref( result );
	}

int main( int argc, char** argv )
	{
        ProxyStore stor( argc, argv );
	stor.Register( "make", ProxyA::Create );
	stor.Loop();
        return 0;
	}
