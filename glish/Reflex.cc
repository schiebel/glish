// $Id$
// Copyright (c) 2000 Associated Universities Inc.

#include <stdio.h>
#include "Reflex.h"

ReflexPtrBase::~ReflexPtrBase( )
	{
	if ( obj != 0 )
		obj->PointerGone( this );
	}

void ReflexPtrBase::ObjGone( )
	{
	obj = 0;
	}

ReflexPtrBase::ReflexPtrBase( ReflexObj *p) : obj(p)
	{
	if ( obj ) obj->AddPointer( this );
	}

ReflexPtrBase::ReflexPtrBase( ReflexPtrBase &p ) : obj(p.obj)
	{
	if ( obj ) obj->AddPointer( this );
	}

ReflexPtrBase &ReflexPtrBase::operator=( ReflexObj *o )
	{
	ReflexObj *tobj = obj;
	obj = o;
	if ( obj ) obj->AddPointer( this );
	if ( tobj ) tobj->PointerGone( this );
	}

ReflexPtrBase &ReflexPtrBase::operator=( ReflexPtrBase &p )
	{
	ReflexObj *tobj = obj;
	obj = p.obj;
	if ( obj ) obj->AddPointer( this );
	if ( tobj ) tobj->PointerGone( this );
	}

ReflexObj::~ReflexObj( )
	{
	for ( int i=0; i < ptrs.length(); ++i )
		ptrs[i]->ObjGone( );
	}

void ReflexObj::PointerGone( ReflexPtrBase *p )
	{
	ptrs.remove(p);
	}

void ReflexObj::AddPointer( ReflexPtrBase *p )
	{
	ptrs.append(p);
	}

ReflexPtr(CycleNode) *node_list::remove(CycleNode *a)
	{
	int i = 0;
	for ( ; i < num_entries && a != ((ReflexPtr(CycleNode)*)entry[i])->ptr(); i++ );
	return remove_nth(i);
	}

// ReflexPtr(CycleNode) *node_list::replace( CycleNode *Old, ReflexPtr(CycleNode) *New)
// 	{
// 	int i = 0;
// 	for ( ; i < num_entries && Old != entry[i]->ptr(); i++ );

// 	if ( i >= 0 && i < num_entries )
// 		{
// 		ReflexPtr(CycleNode) *oldp = entry[i];
// 		entry[i] = New;
// 		return oldp;
// 		}

// 	return 0;
// 	}

ReflexPtr(CycleNode) *node_list::is_member(const CycleNode *e)
	{
	ReflexPtr(CycleNode) *ret = 0;
	for ( int i = 0; i < length() && ! ret; i++ )
		{
		CycleNode *cur = ((ReflexPtr(CycleNode)*)entry[i])->ptr();
		if ( ! cur ) continue;
		if ( cur->isList( ) )
			{
			static cyclenode_list been_there;
			if ( ! been_there.is_member( cur ) )
				{
				//
				// How do we get these cycles?!!
				//
				been_there.append( cur );
				ret = ((NodeList*)cur)->is_member( e );
				been_there.remove( cur );
				}
			}
		else if ( e == cur )
			ret = (ReflexPtr(CycleNode)*)entry[i];
		}
	return ret;
	}

int ReflexObj::isList( ) const { return 0; }

void NodeList::prune( CycleNode *r )
	{
	ReflexPtr(CycleNode) *x = list.remove(r);
	if ( x ) pruned.append(x);
	}

void NodeList::append( NodeList *root_list )
	{
 	Ref( root_list );
	list.append( new ReflexPtr(CycleNode)( root_list ) );
	}

void NodeList::append( CycleNode *node )
	{
	list.append( new ReflexPtr(CycleNode)( node ) );
	}

int NodeList::SoftDelete( )
	{
	return 1;
	}

int NodeList::isList( ) const
	{
	return 1;
	}

NodeList::~NodeList( )
	{
	loop_over_list( list, i )
		if ( ! list[i]->isNull( ) && (*list[i])->isList( ) )
			list[i]->unref( );

	loop_over_list( pruned, j )
		if ( ! pruned[j]->isNull( ) && (*pruned[j])->isList( ) )
			pruned[j]->unref( );
	}
