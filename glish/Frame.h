// $Header$

#ifndef frame_h
#define frame_h

#include "Glish/Object.h"
#include "Expr.h"

class IValue;

class Frame : public GlishObject {
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

    protected:
	int size;
	IValue** values;
	IValue* missing;
	scope_type scope;
	};

#endif /* frame_h */
