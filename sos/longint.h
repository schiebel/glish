//======================================================================
// longint.h
//
// $Id$
// $Revision 1.1.1.1  1996/04/01  19:37:23  dschieb $
//
//======================================================================
#ifndef sos_longint_h
#define sos_longint_h
#include <iostream.h>

class long_int {
    public:
	// higher order followed by lower order
	long_int(unsigned int i1, unsigned int i0) : LOW(0), HIGH(1)
			{ data[LOW] = i0; data[HIGH] = i1; }
	long_int(const long_int &other) : LOW(0), HIGH(1)
			{ data[LOW] = other.data[LOW]; data[HIGH] = other.data[HIGH]; }

	unsigned int operator[](int i) const { return data[(i ? HIGH : LOW)]; }
	unsigned int &operator[](int i) { return data[(i ? HIGH : LOW)]; }

	long_int &operator=(const long_int &other)  { data[LOW] = other.data[LOW];
						      data[HIGH] = other.data[HIGH]; return *this; }
	long_int &operator|=(const long_int &other) { data[LOW] |= other.data[LOW];
						      data[HIGH] |= other.data[HIGH]; return *this; }
	long_int &operator&=(const long_int &other) { data[LOW] &= other.data[LOW];
						      data[HIGH] &= other.data[HIGH]; return *this; }

	long_int &operator|=(unsigned int other)    { data[LOW] |= other; return *this; }
	long_int &operator&=(unsigned int other)    { data[LOW] &= other; return *this; }

	int operator==( const long_int &other ) const { return data[LOW] == other.data[LOW] && data[HIGH] == other.data[HIGH]; }
	
    private:
	const int LOW;
	const int HIGH;
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
