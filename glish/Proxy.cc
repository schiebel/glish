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
	pxy_store_cbinfo( PxyStoreCB1 cb, void * data_ ) : cb1(cb), cb2(0), cb3(0), data(data_) { }
	pxy_store_cbinfo( PxyStoreCB2 cb, void * data_ ) : cb1(0), cb2(cb), cb3(0), data(data_) { }
	pxy_store_cbinfo( PxyStoreCB3 cb ) : cb1(0), cb2(0), cb3(cb), data(0) { }
	void invoke( ProxyStore *s, Value *v, GlishEvent *e )
		{ if ( cb1 ) (*cb1)( s, v, e, data );
		  else if ( cb2 ) (*cb2)( s, v, data );
		  else if ( cb3 ) (*cb3)(s,v); }
    private:
	PxyStoreCB1 cb1;
	PxyStoreCB2 cb2;
	PxyStoreCB3 cb3;
	void *data;
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

void ProxyStore::Register( const char *string, PxyStoreCB1 cb, void *data )
	{
	char *s = strdup(string);
	pxy_store_cbinfo *old = (pxy_store_cbinfo*) cbdict.Insert( s, new pxy_store_cbinfo( cb, data ) );
	if ( old )
		{
		free_memory( s );
		delete old;
		}
	}

void ProxyStore::Register( const char *string, PxyStoreCB2 cb, void *data )
	{
	char *s = strdup(string);
	pxy_store_cbinfo *old = (pxy_store_cbinfo*) cbdict.Insert( s, new pxy_store_cbinfo( cb, data ) );
	if ( old )
		{
		free_memory( s );
		delete old;
		}
	}

void ProxyStore::Register( const char *string, PxyStoreCB3 cb )
	{
	char *s = strdup(string);
	pxy_store_cbinfo *old = (pxy_store_cbinfo*) cbdict.Insert( s, new pxy_store_cbinfo( cb ) );
	if ( old )
		{
		free_memory( s );
		delete old;
		}
	}

void ProxyStore::addProxy( Proxy *p )
	{
	ProxyId id(getId( ));

	if ( id != 0 )
		{
		p->setId( id );
		pxlist.append( p );
		}
	}

void ProxyStore::removeProxy( Proxy *p )
	{
	pxlist.remove( p );
	}

const ProxyId ProxyStore::getId( )
	{
	static Value *v =  create_value(glish_true);

	GlishEvent e( (const char*) PXGETID, (const Value*) v );
	e.SetIsProxy( );
	PostEvent( &e );
	GlishEvent *reply = NextEvent( );

	if ( ! reply || reply->Val()->Type() != TYPE_INT ||
	     reply->Val()->Length() != ProxyId::len() )
		{
		return ProxyId();
		}

	return ProxyId(reply->Val()->IntPtr(0));
	}

Proxy *ProxyStore::GetProxy( const ProxyId &proxy_id )
	{
	loop_over_list( pxlist, i )
		if ( pxlist[i]->Id() == proxy_id )
			return pxlist[i];
	return 0;
	}

void ProxyStore::ProcessEvent( GlishEvent *e )
	{
	if ( e->IsProxy() )
		{
		Value *rval = e->Val();
		if ( rval->Type() != TYPE_RECORD )
			{
			Error( "bad proxy value" );
			return;
			}

		const Value *idv = rval->HasRecordElement( "id" );
		Value *val = rval->Field( "value" );
		if ( ! idv || ! val || idv->Type() != TYPE_INT ||
		     idv->Length() != ProxyId::len() )
			{
			Error( "bad proxy value" );
			return;
			}

		ProxyId id(idv->IntPtr(0));
		int found = 0;
		loop_over_list( pxlist, i )
			if ( pxlist[i]->Id() == id )
				{
				found = 1;
				if ( ! strcmp( "terminate", e->Name() ) )
					Unref(pxlist[i]->Done( val ));
				else
					{
					if ( e->IsBundle() )
						{
						if ( val->Type() != TYPE_RECORD )
							{
							Error( "bad event bundle", id );
							return;
							}

						recordptr events = val->RecordPtr(0);
						for ( int x=0; x < events->Length(); ++x )
							{
							const char* bname;
							Value *bval = events->NthEntry( x, bname );
							pxlist[i]->ProcessEvent( &bname[8], bval );
							}
						}
					else
						pxlist[i]->ProcessEvent( e->Name(), val );
					}
				break;
				}

		if ( ! found ) Error( "bad proxy id" );
		}
	else 
		{
		if ( e->IsBundle( ) )
			{
			Value *val = e->Val();
	
			if ( val->Type() != TYPE_RECORD )
				{
				Error( "bad event bundle" );
				return;
				}

			recordptr events = val->RecordPtr(0);
			for ( int x=0; x < events->Length(); ++x )
				{
				const char* bname;
				Value *bval = events->NthEntry( x, bname );
				if ( pxy_store_cbinfo *cbi = cbdict[bname] )
					cbi->invoke( this, bval, e );
				else
					Unrecognized( );
				}
			}
		else if ( pxy_store_cbinfo *cbi = cbdict[e->Name()] )
			cbi->invoke( this, e->Val(), e );
		else
			Unrecognized( );
		}
	}

void ProxyStore::Loop( )
	{
	for ( GlishEvent* e; (e = NextEvent()); )
		ProcessEvent( e );
	}

void Proxy::setId( const ProxyId &i ) { id = i; }

Proxy::Proxy( ProxyStore *s ) : store(s)
	{
	// store will set our id
	store->addProxy( this );
	}

Proxy::~Proxy( )
	{
	store->removeProxy( this );
	}

Proxy *Proxy::Done( Value * )
	{
	return this;
	}

void Proxy::SendCtor( const char *name, Proxy *source )
	{
	recordptr rec = create_record_dict();
	rec->Insert( strdup(PXCREATE), create_value((int*)id.array(),ProxyId::len(),COPY_ARRAY) );
	if ( ! source )
		{
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
	else
		{
		Value *v = create_value(rec);
		if ( ReplyPending( ) )
			source->Reply( v );
		else
			source->PostEvent( name, v );
		Unref(v);
		}
	}
