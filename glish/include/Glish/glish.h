// $Header$

#ifndef glish_h
#define glish_h

#ifdef SABER
#include <libc.h>
#endif

#define bool glish_bool
typedef enum { glish_false, glish_true } bool;

#ifndef iv_defs_h
// InterViews also defines "false" and "true", but differently.
// If we're not being used with InterViews, create convenient
// aliases for "false" and "true".
#define false glish_false
#define true glish_true
#endif

typedef const char* string;

#define loop_over_list(list, iterator)	\
	for ( int iterator = 0; iterator < (list).length(); ++iterator )

#endif
