// $Id$
// Copyright (c) 1998 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"
#include "Glish/Proxy.h"
#include "Reporter.h"

//
//  Generates Events:
//
//		o  *get-proxy-id* to get an ID for each proxy
//
//  Recieves Event
//
//		o  *proxy* with record [ name=, value=, id= ]
//

class pxy_store_cbinfo {
    public:
	pxy_store_cbinfo( PxyStoreCB cb__, void * data__ ) : cb_(cb__) data_(data__) { }
	PxyStoreCB cb( ) { return cb_; }
	void *data( ) { return data_; }
    private:
	PxyStoreCB cb_;
	void *data_;
};

ProxyStore::ProxyStore( int &argc, char **argv, char *name,
			Client::ShareType multithreaded ) :
		Client( argc, argv, name, multithreaded ) { }
	
ProxyStore::~ProxyStore( )
	{
	loop_over_list( pxlist, i )
		Unref( pxlist[i] );

	IterCookie* c = cbdict.InitForIteration();
	pxy_store_cbinfo *member;
	const char *key;
	while ( (member = cbdict.NextEntry( key, c )) )
		{
		free_memory( (char*) key );
		delete member;
		}
	}

void ProxyStore::Register( const char *string, ProxyStoreCB cb, void *data = 0 )
	{
	char *s = strdup(string);
	pxy_store_cbinfo *old = cbdict.Insert( s, new pxy_store_cbinfo( cb, data ) );
	if ( old )
		{
		free_memory( s );
		delete old;
		}
	}

void ProxyStore::addProxy( Proxy *p )
	{
	double id = getId( );
	if ( id != 0.0 )
		{
		p->setId( id );
		pxlist.append( p );
		}
	}

void ProxyStore::removeProxy( Proxy *p )
	{
	Unref( pxlist.remove( p ) );
	}

double ProxyStore::getId( )
	{
	static Value *v =  create_value(glish_true);

	PostEvent( "*get-proxy-id*", v );
	GlishEvent *reply = NextEvent( );

	if ( ! reply || ! reply->Val()->Type() == TYPE_DOUBLE )
		{
		Error( "problem getting ID" );
		return 0.0;
		}

	return reply->Val()->DoubleVal();
	}

void ProxyStore::Loop( )
	{
	for ( GlishEvent* e; (e = NextEvent()); )
		{
		if ( ! strcmp( e->Name(), "*proxy*" ) )
			{
			Value *rval = e->Val();
			if ( rval->Type() != TYPE_RECORD )
				{
				Error( "bad proxy value" );
				continue;
				}

			const Value *idv = rval->HasRecordElement( "id" );
			const Value *name = rval->HasRecordElement( "name" );
			const Value *val = rval->HasRecordElement( "value" );
			if ( ! id || ! name || ! val || id->Type() != TYPE_DOUBLE ||
			     name->Type() != TYPE_STRING )
				{
				Error( "bad proxy value" );
				continue;
				}

			double id = idv->DoubleVal();
			char *str = name->StringVal();
			int found = 0;
			loop_over_list( pxlist, i )
				if ( pxlist[i]->Id() == id )
					{
					found = 1;
					pxlist[i]->ProcessEvent( str, val, e );
					break;
					}

			if ( ! found )
				Error( "bad proxy id" );

			free_memory( str );
			}
		else if ( pxy_store_cbinfo *cbi = cbdict[e->Name()] )
			(*cbi->cb())( this, e, cbi->data() );
		else
			Unrecognized( );
		}
	}

Proxy::Proxy( ProxyStore *s ) : store(s), id(0.0)
	{
	//
	// store will set our id
	//
	store->addProxy( this );
	}
