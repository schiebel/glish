//======================================================================
// sos/str.h
//
// $Id$
//
//======================================================================
#if ! defined(sos_str_h_)
#define sos_str_h_
#include "sos/sos.h"
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

class str_kernel {
public:
	str_kernel( unsigned int size = 0 ) : cnt(1), len(size ? size : 1)
		{ ary = (char**) alloc_zero_memory(len*sizeof(char*)); }
	str_kernel( const char * );

	unsigned int strlen( unsigned int off=0 ) const
		{ return ary[off] ? *((unsigned int*)ary[off]) : 0; }
	unsigned int length() const { return len; }

	char *get( unsigned int off ) { return ary[off] ? &ary[off][4] : 0; }
	void set( unsigned int, const char * );

	const char **raw_getary( ) { return (const char **) ary; }
	char *raw_get( unsigned int off ) { return ary[off]; }
	void raw_set( unsigned int, const char * );

	void ref() { cnt++; }
	unsigned int unref() { return --cnt; }
	unsigned int count() const { return cnt; }

	str_kernel *clone() const;
	void grow( unsigned int );

	~str_kernel();
private:
	unsigned int len;
	unsigned int cnt;
	char **ary;
};

class str_ref {
friend class str;
friend unsigned int strlen( const str_ref & );
public:
	inline operator const char *() const;
	inline str_ref &operator=( const char * );
	inline str_ref &operator=( const str_ref & );
private:
	str_ref( str *s_, unsigned int off_ ) : s(s_), off(off_) { }

	// stubs to prevent these functions from being called
	str_ref() { }
	str_ref( const str_ref &) { }

	str *s;
	unsigned int off;
};

class str {
friend class str_ref;
friend unsigned int strlen( const str_ref & );
public:
	str( unsigned int size = 0 ) : kernel( new str_kernel( size ) ) { }
	str( const char *s ) : kernel( new str_kernel(s) ) { }
	str( const str &s ) : kernel(s.kernel) { kernel->ref(); }

	str &operator=( const str &s )
		{
		if ( s.kernel != kernel )
			{
			if ( ! kernel->unref() ) delete kernel;
			kernel = s.kernel;
			kernel->ref();
			}
		return *this;
		}

	const char *operator[]( unsigned int i ) const
		{ return kernel->get(i); }
	str_ref operator[]( unsigned int i )
		{ return str_ref(this,i); }

	void set( unsigned int off, const char *s )
		{ kernel->set(off, s); }
	const char *get( unsigned int off ) const
		{ return kernel->get( off ); }
	
	const char *raw_get( unsigned int off ) const
		{ return kernel->raw_get( off ); }
	const char **raw_getary( ) const
		{ return kernel->raw_getary( ); }

	//
	// make sure we have a modifiable version
	//
	void mod() { if ( kernel->count() > 1 ) do_copy(); }

	void grow( unsigned int size )
		{ mod(); kernel->grow( size ); }

	unsigned int length() const { return kernel->length(); }
	unsigned int strlen( unsigned int off = 0 ) const { return kernel->strlen(off); }

	~str() { if ( ! kernel->unref() ) delete kernel; }
private:
	void do_copy();
	str_kernel *kernel;
};

inline str_ref::operator const char *() const
	{ return s->kernel->get( off ); }
inline str_ref &str_ref::operator=( const char *v )
	{ s->mod(); s->kernel->set( off, v ); return *this; }
inline str_ref &str_ref::operator=( const str_ref &o )
	{ s->mod(); s->kernel->raw_set( off, o.s->kernel->raw_get(o.off) ); return *this; }

inline unsigned int strlen( const str_ref &ref )
	{ return ref.s->kernel->strlen(ref.off); }

#endif
