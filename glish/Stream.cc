#include "Glish/Stream.h"
#include "system.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream.h>

#define DEFINE_FUNCS(MACRO)	\
MACRO(float)			\
MACRO(double)			\
MACRO(int)			\
MACRO(long)			\
MACRO(short)			\
MACRO(char)			\
MACRO(unsigned int)		\
MACRO(unsigned long)		\
MACRO(unsigned short)		\
MACRO(unsigned char)		\
MACRO(void*)			\
MACRO(char*)

int OStream::reset() { return 0; }

OStream &OStream::operator<<( OStream &(*f)(OStream&) )
	{
	return (*f)(*this);
	}

char *DBuf::tmpbuf = 0;
DBuf::DBuf(unsigned int s)
	{
	if ( ! tmpbuf ) tmpbuf = new char[512];
	size_ = s ? s : 1;
	buf = new char[size_ + 1];
	len_ = 0;
	}

DBuf::~DBuf() { if ( buf ) delete buf; }

#define DEFINE_APPEND(TYPE)						\
int DBuf::put( TYPE v, const char *format )				\
	{								\
	sprintf(tmpbuf,format,v);					\
	int l = strlen(tmpbuf);						\
	if ( len_ + l >= size_ )					\
		{							\
		while ( len_ + l >= size_ ) size_ *= 2;			\
		buf = (char*) realloc_memory( (void*) buf, size_ + 1 );	\
		}							\
	memcpy(&buf[len_],tmpbuf,l);					\
	len_ += l;							\
	return l;							\
	}

DEFINE_FUNCS(DEFINE_APPEND)

void DBuf::reset() { len_ = 0; }

#define SOSTREAM_PUT(TYPE)			\
OStream &SOStream::operator<<( TYPE v )		\
	{					\
	buf.put(v);				\
	return *this;				\
	}

DEFINE_FUNCS(SOSTREAM_PUT)

int SOStream::reset()
	{
	buf.reset();
	return 1;
	}

OStream &endl(OStream &s)
	{
	s << "\n";
	return s;
	}

#define PROXYSTREAM_PUT(TYPE)			\
OStream &ProxyStream::operator<<( TYPE v )	\
	{					\
	s << (v);				\
	return *this;				\
	}

DEFINE_FUNCS(PROXYSTREAM_PUT)

