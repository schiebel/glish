// $Id$
// Copyright (c) 1997 Associated Universities Inc.
#ifndef garbage_h_
#define garbage_h_
#include "Glish/Value.h"

class Garbage;
glish_declare(PList,Garbage);
typedef PList(Garbage) garbage_list;

extern int glish_collecting_garbage;

class Garbage {
public:
	Garbage( Value *v ) : value(v), mark(0)
		{ values->append( this ); }
	~Garbage( ) { values->remove( this ); }

	static void init();
	static void finalize();

	static void collect( );
	void tag( ) { mark = 1; }
	int clear( ) { mark = 0; }
	int isTaged( ) { return (int) mark; }

protected:
	static garbage_list *values;
	Value *value;
	char mark;
private:
	void *operator new( size_t ) { return 0; }
	Garbage( const Garbage & ) { }
};

#endif
