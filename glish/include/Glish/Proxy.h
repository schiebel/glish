// $Id$
// Copyright (c) 1998 Associated Universities Inc.
#ifndef proxy_h
#define proxy_h

#include "Glish/Client.h"

class Proxy;
class ProxyStore;
glish_declare(PList,Proxy);
typedef PList(Proxy) proxy_list;

typedef char *(*PxyStoreCB)( ProxyStore *, GlishEvent *e, void *data );
class pxy_store_cbinfo;
glish_declare(PDict,pxy_store_cbinfo);
typedef PDict(pxy_store_cbinfo) pxy_store_cbdict;

class ProxyStore : public Client {
friend class Proxy;
    public:
	ProxyStore( int &argc, char **argv, char *name,
		    Client::ShareType multithreaded = NONSHARED );

	~ProxyStore( );

	virtual void Register( const char *string, ProxyStoreCB cb, void *data = 0 );

	virtual void Loop( );

    protected:
	virtual void addProxy( Proxy * );
	virtual void removeProxy( Proxy * );
	double getId( );

	proxy_list       pxlist;
	pxy_store_cbdict cbdict;
};

class Proxy : public GlishRef {
friend class ProxyStore;
    public:

	Proxy( ProxyStore * );

	virtual ProcessEvent( const char *name, const Value *val, GlishEvent *e );

	double Id() const { return id; }

    protected:
	virtual setId( double );
	ProxyStore *store;
	double id;
};

#endif
