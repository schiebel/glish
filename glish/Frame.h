// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997,1998 Associated Universities Inc.
#ifndef frame_h
#define frame_h

#include "Glish/Object.h"
#include "Glish/List.h"

class IValue;

// Different scopes to use when resolving identifiers; used by the VarExpr
// and Sequencer classes.
//	LOCAL_SCOPE  --  local to the narrowest block
//      FUNC_SCOPE   --  local to a function
//      GLOBAL_SCOPE --  global to the current name space
//      ANY_SCOPE    --  any scope from the narrowest block to global
//
typedef enum { LOCAL_SCOPE, FUNC_SCOPE, GLOBAL_SCOPE, ANY_SCOPE } scope_type;

class Frame : public GlishObject {
friend class stack_type;
    public:
	Frame( int frame_size, IValue* param_info, scope_type s );
	~Frame();

	IValue*& FrameElement( int offset );

	// Functionality necessary to implement the "missing()" 
	// function; in the future this may need to be extended 
	// with a "ParameterInfo" object.
	const IValue *Missing() const	{ return missing; }

	int Size() const	{ return size; }

	scope_type GetScopeType() const { return scope; }

	const char *Description() const;

	unsigned int CountRefs( recordptr r ) const;
	int CountRefs( Frame *f ) const;

#ifdef GGC
	void TagGC( );
#endif

    protected:
	void clear();
	int size;
	IValue** values;
	IValue* missing;
	scope_type scope;
	};

glish_declare(PList,Frame);
typedef PList(Frame) frame_list;

#endif /* frame_h */
