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
//		o  *proxy-id* to get an ID for each proxy
//		o  *proxy-create* to create a proxy in the interpreter
//
#define PXGETID		"*proxy-id*"
#define PXCREATE	"*proxy-create*"

class pxy_store_cbinfo {
    public:
	pxy_store_cbinfo( PxyStoreCB cb__, void * data__ ) : cb_(cb__), data_(data__) { }
	PxyStoreCB cb( ) { return cb_; }
	void *data( ) { return data_; }
    private:
	PxyStoreCB cb_;
	void *data_;
};

ProxyStore::ProxyStore( int &argc, char **argv,
			Client::ShareType multithreaded ) :
		Client( argc, argv, multithreaded ) { }
	
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

void ProxyStore::Register( const char *string, PxyStoreCB cb, void *data = 0 )
	{
	char *s = strdup(string);
	pxy_store_cbinfo *old = (pxy_store_cbinfo*) cbdict.Insert( s, new pxy_store_cbinfo( cb, data ) );
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

	PostEvent( PXGETID, v );
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
		if ( e->IsProxy() )
			{
			Value *rval = e->Val();
			if ( rval->Type() != TYPE_RECORD )
				{
				Error( "bad proxy value" );
				continue;
				}

			const Value *idv = rval->HasRecordElement( "id" );
			const Value *val = rval->HasRecordElement( "value" );
			if ( ! idv || ! val || idv->Type() != TYPE_DOUBLE )
				{
				Error( "bad proxy value" );
				continue;
				}

			double id = idv->DoubleVal();
			int found = 0;
			loop_over_list( pxlist, i )
				if ( pxlist[i]->Id() == id )
					{
					found = 1;
					pxlist[i]->ProcessEvent( e->Name(), val );
					break;
					}

			if ( ! found ) Error( "bad proxy id" );
			}
		else if ( pxy_store_cbinfo *cbi = cbdict[e->Name()] )
			(*cbi->cb())( this, e, cbi->data() );
		else
			Unrecognized( );
		}
	}

void Proxy::setId( double i ) { id = i; }

Proxy::Proxy( ProxyStore *s ) : store(s), id(0.0)
	{
	//
	// store will set our id
	//
	store->addProxy( this );
	}

void Proxy::SendCtor( const char *name )
	{
	recordptr rec = create_record_dict();
	rec->Insert( strdup(PXCREATE), create_value(Id()) );
	if ( ReplyPending( ) )
		{
		char *pending = store->TakePending();
		GlishEvent e( pending, create_value(rec) );
		e.SetIsReply( );
		e.SetIsProxy( );
		PostEvent( &e );
		}
	else
		{
		GlishEvent e( name, create_value(rec) );
		e.SetIsProxy( );
		PostEvent( &e );
		}
	}
