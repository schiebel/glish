// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef queue_h
#define queue_h

#include "sos/generic.h"

class BaseQueue;

#define Queue(type) name2(type,Queue)
#define PQueue(type) name2(type,PQueue)

class QueueElement {
    protected:
    friend BaseQueue;
	QueueElement( void* element )
		{ elem = element; next = 0; }
	QueueElement* next;
	void* elem;
	};

class BaseQueue {
    public:
	BaseQueue();
	void EnQueue( void* element );
	void* DeQueue();
	int length() const { return num_entries; }

    protected:
	int num_entries;
	QueueElement* head;
	QueueElement* tail;
	};

#define Queuedeclare(type)						\
	class Queue(type) : public BaseQueue {				\
	    public:							\
		void EnQueue( type element )				\
			{ BaseQueue::EnQueue( (void*) element ); }	\
		type DeQueue()						\
			{ return (type) BaseQueue::DeQueue(); }		\
		}

#define PQueuedeclare(type)						\
	class PQueue(type) : public BaseQueue {				\
	    public:							\
		void EnQueue( type* element )				\
			{ BaseQueue::EnQueue( (void*) element ); }	\
		type* DeQueue()						\
			{ return (type*) BaseQueue::DeQueue(); }	\
		}

#endif	/* queue_h */
