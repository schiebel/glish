#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "Glish/Proxy.h"
#include "Reporter.h"

class ProxyA : public Proxy {
    public:
	ProxyA( ProxyStore *s );
	~ProxyA( );
	static void Create( ProxyStore *s, Value *v, GlishEvent *e, void *data );
	void ProcessEvent( const char *name, Value *val );
};

ProxyA::ProxyA( ProxyStore *s ) : Proxy(s) { 	cerr << "hello world " << id << endl; }

ProxyA::~ProxyA( )
	{
	cerr << "bye bye " << id << endl;
	}

void ProxyA::Create( ProxyStore *s, Value *v, GlishEvent *e, void *data )
	{
	message->Report( v );
	ProxyA *np = new ProxyA( s );
	np->SendCtor("newtp");
	}

void ProxyA::ProcessEvent( const char *name, Value *val )
	{
	if ( ReplyPending() )
		Reply( val );
	else
		{
		char buf[1024];
		sprintf(buf, "ProxyA_%s", name);
		PostEvent( buf, val );
		}
	}

int main( int argc, char** argv )
	{
	ProxyStore stor( argc, argv );

	stor.Register( "maka", ProxyA::Create );

	stor.Loop();
	}
