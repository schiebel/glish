// $Header$
//======================================================================
// longint.h
//
// $Log$
// Revision 1.1  1997/03/04 15:15:00  dschieb
// Initial revision
//
// $Revision 1.1.1.1  1996/04/01  19:37:23  dschieb $
//
//======================================================================
#ifndef sos_longint_h
#define sos_longint_h
#include <iostream.h>

class long_int {
    public:
	// higher order followed by lower order
	long_int(unsigned int i1, unsigned int i0) { data[0] = i0; data[1] = i1; }
	long_int(const long_int &other) { data[0] = other.data[0]; data[1] = other.data[1]; }

	unsigned int operator[](int i) const { return data[(i ? 1 : 0)]; }
	unsigned int &operator[](int i) { return data[(i ? 1 : 0)]; }

	long_int &operator=(const long_int &other)  { data[0] = other.data[0];
						      data[1] = other.data[1]; return *this; }
	long_int &operator|=(const long_int &other) { data[0] |= other.data[0];
						      data[1] |= other.data[1]; return *this; }
	long_int &operator&=(const long_int &other) { data[0] &= other.data[0];
						      data[1] &= other.data[1]; return *this; }

	long_int &operator|=(unsigned int other)    { data[0] |= other; return *this; }
	long_int &operator&=(unsigned int other)    { data[0] &= other; return *this; }

	int operator==( const long_int &other ) const { return data[0] == other.data[0] && data[1] == other.data[1]; }
	
    private:
	unsigned int data[2];
};

#define LONGINT_OP(OP)								\
inline long_int operator ## OP (const long_int &left, const long_int &right)	\
	{ return long_int(left[1] OP right[1], left[0] OP right[0]); }		\
inline long_int operator ## OP (const long_int &left, unsigned int right)	\
	{ return long_int(left[1], left[0] OP right); }				\
inline long_int operator ## OP (unsigned int left, const long_int &right)	\
	{ return long_int(right[1], left OP right[0]); }

LONGINT_OP(|)
LONGINT_OP(&)

inline long_int operator<<( const long_int &left, unsigned int right )
	{ return long_int((left[1] << right) | (left[0] >> (32 - right)), left[0] << right); }
inline long_int operator>>( const long_int &left, unsigned int right )
	{ return long_int(left[1] >> right, (left[1] << (32 - right)) | (left[0] >> right)); }

inline ostream &operator<<(ostream &ios, const long_int &li)
	{ ios.form("0x%08x%08x",li[1],li[0]); return ios; }

#endif
