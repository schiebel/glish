#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "Glish/Proxy.h"

class ProxyA : public Proxy {
    public:
	ProxyA( ProxyStore *s );
	static void Create( ProxyStore *s, GlishEvent *e, void *data );
	void ProcessEvent( const char *name, const Value *val );
};

ProxyA::ProxyA( ProxyStore *s ) : Proxy(s) { }

void ProxyA::Create( ProxyStore *s, GlishEvent *e, void *data )
	{
	ProxyA *np = new ProxyA( s );
	cerr << "(1a)--> " << np->Id() << endl;
	np->SendCtor("newtp");
	cerr << "(1b)--> " << np->Id() << endl;
	}

void ProxyA::ProcessEvent( const char *name, const Value *val )
	{
	cerr << "(2)--> " << name << endl;
	}

int main( int argc, char** argv )
	{
	ProxyStore stor( argc, argv );

	stor.Register( "create-a", ProxyA::Create );

	stor.Loop();
	}
