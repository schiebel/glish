// $Header$

#ifndef frame_h
#define frame_h

#include "Glish/Object.h"


class Value;


class Frame : public GlishObject {
    public:
	Frame( int frame_size );
	~Frame();

	Value*& FrameElement( int offset );

	int Size() const	{ return size; }

    protected:
	int size;
	Value** values;
	};


#endif /* frame_h */
