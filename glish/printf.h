// $Id$
// Copyright (c) 1998 Associated Universities Inc.
#ifndef printf_h
#define printf_h
#include <stdio.h>
#include "IValue.h"

char *_do_gprintf( int type, char *format, const_args_list *args, int arg_off=1, FILE *file=0, char *&buffer=glish_charptrdummy );

inline char *gprintf( char *format, const_args_list *args, int arg_off=1 )
	{ return _do_gprintf( 1, format, args, arg_off ); }
inline char *gfprintf( FILE *file, char *format, const_args_list *args, int arg_off=1 )
	{ return _do_gprintf( 2, format, args, arg_off, file ); }
inline char *gsprintf( char *&buf, char *format, const_args_list *args, int arg_off=1 )
	{ return _do_gprintf( 3, format, args, arg_off, 0, buf ); }

#endif
