// $Id$
// Copyright (c) 1998 Associated Universities Inc.
#ifndef proxy_h
#define proxy_h

#include "Glish/Client.h"

class Proxy;
class ProxyStore;
glish_declare(PList,Proxy);
typedef PList(Proxy) proxy_list;

typedef void (*PxyStoreCB)( ProxyStore *, GlishEvent *, void * );
class pxy_store_cbinfo;
glish_declare(PDict,pxy_store_cbinfo);
typedef PDict(pxy_store_cbinfo) pxy_store_cbdict;

class ProxyStore : public Client {
friend class Proxy;
    public:
	ProxyStore( int &argc, char **argv,
		    Client::ShareType multithreaded = NONSHARED );

	~ProxyStore( );

	virtual void Register( const char *string, PxyStoreCB cb, void *data = 0 );

	virtual void Loop( );

    protected:
	virtual void addProxy( Proxy * );
	virtual void removeProxy( Proxy * );
	char *TakePending( ) { char *t=pending_reply; pending_reply=0; return t; }
	double getId( );

	proxy_list       pxlist;
	pxy_store_cbdict cbdict;
};

class Proxy : public GlishRef {
friend class ProxyStore;
    public:

	Proxy( ProxyStore * );

	virtual void ProcessEvent( const char *name, const Value *val ) = 0;

	double Id( ) const { return id; }

	void SendCtor( const char *name = "new" );

	void Unrecognized( )	{ store->Unrecognized( id ); }

	void Error( const char* msg ) { store->Error( msg, id ); }
	void Error( const char* fmt, const char* arg )
				{ store->Error( fmt, arg, id ); }

	void PostEvent( const GlishEvent* event, const EventContext &context = glish_ec_dummy )
				{ store->PostEvent( event, context ); }
	void PostEvent( const char* event_name, const Value* event_value,
			const EventContext &context = glish_ec_dummy )
				{ store->PostEvent( event_name, event_value, context, id ); }

	void PostEvent( const char* event_name, const char* event_value,
			const EventContext &context = glish_ec_dummy )
				{ store->PostEvent( event_name, event_value, context, id ); }

	void PostEvent( const char* event_name, const char* event_fmt, const char* arg1,
			const EventContext &context = glish_ec_dummy )
				{ store->PostEvent( event_name, event_fmt, arg1, context, id ); }
	void PostEvent( const char* event_name, const char* event_fmt, const char* arg1,
			const char* arg2, const EventContext &context = glish_ec_dummy )
				{ store->PostEvent( event_name, event_fmt, arg1, arg2, context, id ); }

	void Reply( const Value* event_value ) { store->Reply( event_value, id ); }

	int ReplyPending() const { return store->ReplyPending(); }

    protected:
	void setId( double );
	ProxyStore *store;
	double id;
};

#endif
