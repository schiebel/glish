// $Id$
// Copyright (c) 1998 Associated Universities Inc.
#ifndef printf_h
#define printf_h
#include <stdio.h>
#include "IValue.h"

int gsprintf( char **&out, char *format, const_args_list *args, const char *&error=glish_charptrdummy, int arg_off=1 );

#endif
