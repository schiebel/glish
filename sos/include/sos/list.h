// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef list_h_
#define list_h_

// BaseList.h --
//	Interface for class BaseList, current implementation is as an
//	array of ent's.  This implementation was chosen to optimize
//	getting to the ent's rather than inserting and deleting.
//	Also pairs of append's and get's act like push's and pop's
//	and are very efficient.  The only really expensive operations
//	are inserting (but not appending), which requires pushing every
//	element up, and resizing the list, which involves getting new space
//	and moving the data.  Resizing occurs automatically when inserting
//	more elements than the list can currently hold.  Automatic
//	resizing is done one "chunk_size" of elements at a time and
//	always increases the size of the list.  Resizing to zero
//	(or to less than the current value of num_entries)
//	will decrease the size of the list to the current number of
//	elements.  Resize returns the new max_entries.
//
//	Entries must be either a pointer to the data or nonzero data with
//	sizeof(data) <= sizeof(void*).
#include <stdarg.h>
//
// needed for class SosRef
//
#include "sos/ref.h"
#include "sos/generic.h"

typedef void* void_ptr;
typedef void_ptr ent;
typedef void (*PFC)(char*);	// mostly for error handling

class BaseList : public SosRef {
    public:
	~BaseList();

	void clear();		// remove all entries
	int length() const	{ return num_entries; }
	int curlen() const	{ return max_entries; }

    protected:
	int resize( );

	BaseList(int = 0, PFC = 0);
	BaseList(BaseList&);

	void append( ent a )  { if ( num_entries == max_entries ) resize( );
				entry[num_entries++] = a; }

	void insert(ent);	  // add at head of list
	ent remove(ent);	  // delete entry from list
	void insert_nth(int,ent); // add at nth slot in list
	ent remove_nth(int);	  // delete nth entry from list
	ent get();		  // return and remove ent at end of list

	// Return 0 if ent is not in the list, ent otherwise.
	ent is_member(ent) const;

	PFC set_error_handler(PFC = 0);	//reset to default handler if null arg
	ent replace(int, ent);	// replace entry #i with a new value

	// return nth ent of list (do not remove).
	ent operator[]( int i ) const
		{ return i < 0 || i >= num_entries ? 0 : entry[i]; }

	void operator=(BaseList&);

	ent* entry;
	int chunk_size;		// increase size by this amount when necessary
	int max_entries;
	int num_entries;
	PFC error_handler;

	friend class BaseListIterator;
	};

class BaseListIterator {
    public:
	BaseListIterator(BaseList& l)	{ lp = &l; pos = 0; }

	void reset()			{ pos = 0; }
	ent operator()()	
		{
		return (pos < lp->num_entries) ? lp->entry[pos++] : 0;
		}

    protected:
	BaseList* lp;
	int pos;
	};


// List.h -- interface for class List
//	Use:	to get a list of pointers to class foo you should:
//		1) typedef foo* Pfoo; (the macros don't like explicit pointers)
//		2) sos_declare(List,Pfoo); (declare an interest in lists of Pfoo's)
//		3) variables are declared like:
//				List(Pfoo) bar;	(bar is of type list of Pfoo's)
//				ListIterator(Pfoo) next(bar);

// For lists of "type".

// InterViews 3.0 defines List ...
#undef List
#define List(type)			sos_name2(type,List)
#define ListIterator(type)	sos_name2(type,ListIterator)

// For lists of pointers to "type"
#define PList(type)			sos_name2(type,PList)
#define PListIterator(type)	sos_name2(type,PListIterator)

#define Listdeclare(type)						\
struct List(type) : BaseList						\
	{								\
	List(type)(PFC eh =0) : BaseList(0,eh) {}			\
	List(type)(int sz, PFC eh =0) : BaseList(sz,eh) {}		\
	List(type)(List(type)& l) : BaseList((BaseList&)l) {}		\
									\
	void operator=(List(type)& l)					\
		{ BaseList::operator=((BaseList&)l); }			\
	void insert(type a)	{ BaseList::insert(PASTE(type,_to_void)(a)); } \
	void append(type a)	{ BaseList::append(PASTE(type,_to_void)(a)); } \
	type remove(type a)						\
			{ return PASTE(void_to_,type)(BaseList::remove(PASTE(type,_to_void)(a))); } \
	void insert_nth(int n, type a)					\
				{ BaseList::insert_nth(n,PASTE(type,_to_void)(a)); } \
	type remove_nth(int n)	{ return PASTE(void_to_,type)(BaseList::remove_nth(n)); } \
	type get()		{ return PASTE(void_to_,type)(BaseList::get()); } \
	type replace(int i, type new_type)				\
		{ return PASTE(void_to_,type)(BaseList::replace(i,PASTE(type,_to_void)(new_type))); } \
	type is_member(type e)						\
		{ return PASTE(void_to_,type)(BaseList::is_member(PASTE(type,_to_void)(e))); } \
	PFC set_error_handler(PFC eh =0)				\
		{ return BaseList::set_error_handler(eh); }		\
									\
	type operator[](int i) const					\
		{ return PASTE(void_to_,type)(BaseList::operator[](i)); } \
	};								\
									\
struct ListIterator(type) : BaseListIterator				\
	{								\
	ListIterator(type)(List(type)& l)				\
		: BaseListIterator((BaseList&)l) {}			\
	void reset()		{ BaseListIterator::reset(); }		\
	type operator()()						\
		{ return PASTE(void_to_,type)(BaseListIterator::operator()()); } \
	}

#define PListdeclare(type)						\
struct PList(type) : BaseList						\
	{								\
	PList(type)(type* ...);						\
	PList(type)(PFC eh =0) : BaseList(0,eh) {}			\
	PList(type)(int sz, PFC eh =0) : BaseList(sz,eh) {}		\
	PList(type)(PList(type)& l) : BaseList((BaseList&)l) {}		\
									\
	void operator=(PList(type)& l)					\
		{ BaseList::operator=((BaseList&)l); }			\
	void insert(type* a)	{ BaseList::insert(ent(a)); }		\
	void append(type* a)	{ BaseList::append(ent(a)); }		\
	type* remove(type* a)						\
		{ return (type*)BaseList::remove(ent(a)); }		\
	void insert_nth(int n, type* a)					\
				{ BaseList::insert_nth(n,ent(a)); }	\
	type* remove_nth(int n)	{ return (type*)(BaseList::remove_nth(n)); }\
	type* get()		{ return (type*)BaseList::get(); }	\
	type* operator[](int i) const					\
		{ return (type*)(BaseList::operator[](i)); }		\
	type* replace(int i, type* new_type)				\
		{ return (type*)BaseList::replace(i,ent(new_type)); }	\
	type* is_member(type* e)					\
		{ return (type*)BaseList::is_member(ent(e)); }		\
	PFC set_error_handler(PFC eh =0)				\
		{ return BaseList::set_error_handler(eh); }		\
	};								\
									\
struct PListIterator(type) : BaseListIterator				\
	{								\
	PListIterator(type)(PList(type)& l)				\
		: BaseListIterator((BaseList&)l) {}			\
	void reset()		{ BaseListIterator::reset(); }		\
	type* operator()()						\
		{ return (type*)(BaseListIterator::operator()()); }	\
	}

#endif /* list_h_ */
