// $Header:

#if defined(mips) || defined (masscomp)
#include <string.h>

extern "C" char *strdup( const char *str );

char *strdup( const char *str )
	{
	int str_length = strlen( str );
	char *tmp_str = new char [str_length + 1];

	return strcpy( tmp_str, str );
	}
#endif
