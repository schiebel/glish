#include <stdio.h>
#include "Reflex.h"

int NodeList::ISLIST( )
	{
	return 1;
	}

int node_list::ISLIST( )
	{
	return 0;
	}

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
	if ( obj )
		{
		obj->AddPointer( this );
		}
	}

ReflexPtrBase::ReflexPtrBase( ReflexPtrBase &p ) : obj(p.obj)
	{
	if ( obj )
		{
		obj->AddPointer( this );
		}
	}

ReflexPtrBase &ReflexPtrBase::operator=( ReflexObj *o )
	{
	if ( obj ) obj->PointerGone( this );
	obj = o;
	if ( obj ) obj->AddPointer( this );
	}

ReflexPtrBase &ReflexPtrBase::operator=( ReflexPtrBase &p )
	{
	if ( obj ) obj->PointerGone( this );
	obj = p.obj;
	if ( obj ) obj->AddPointer( this );
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
	int i = 0;
	for ( ; i < length() && e != ((ReflexPtr(CycleNode)*)entry[i])->ptr(); i++ );
	return (i == length()) ? 0 : (ReflexPtr(CycleNode)*)entry[i];
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

	loop_over_list( pruned, i )
		if ( ! pruned[i]->isNull( ) && (*pruned[i])->isList( ) )
			pruned[i]->unref( );
	}
