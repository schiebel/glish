// $Header$

#ifndef frame_h
#define frame_h

#include "Glish/Object.h"
#include "Expr.h"

class Value;

class Frame : public GlishObject {
    public:
	Frame( int frame_size, Value* param_info, scope_type s );
	~Frame();

	Value*& FrameElement( int offset );

	// Functionality necessary to implement the "missing()" 
	// function; in the future this may need to be extended 
	// with a "ParameterInfo" object.
	const Value *Missing() const	{ return missing; }

	int Size() const	{ return size; }

	scope_type GetScope() const { return scope; }

    protected:
	int size;
	Value** values;
	Value* missing;
	scope_type scope;
	};

#endif /* frame_h */
