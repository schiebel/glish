//======================================================================
// longint.h
//
// $Id$
//
//======================================================================
#ifndef sos_longint_h
#define sos_longint_h
#include <iostream.h>

//
// Indicates if this machine is big endian
//
extern int sos_big_endian;

//
// used to initialize the endian-ness of long_int
//
class long_int_init;
class long_int {
	friend long_int_init;
    public:
	// higher order followed by lower order
	long_int(unsigned int i1, unsigned int i0) : data(data_)
			{ data[LOW] = i0; data[HIGH] = i1; }
	long_int(const long_int &other) : data(data_)
			{ data[LOW] = other.data[LOW]; data[HIGH] = other.data[HIGH]; }
	long_int( unsigned int *d ) : data(d) { }

	unsigned int operator[](int i) const { return data[(i ? HIGH : LOW)]; }
	unsigned int &operator[](int i) { return data[(i ? HIGH : LOW)]; }

	long_int &operator++() { data+=2; return *this; }
	long_int &operator++(int) { data+=2; return *this; }

	void *operator*() const { return data; }

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
	static int LOW;
	static int HIGH;
	unsigned int data_[2];
	unsigned int *data;
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

ostream &operator<<(ostream &, const long_int &);

static class long_int_init {
    public:
	long_int_init();
    private:
	static int initialized;
} sos_long_int_init;
#endif
