//======================================================================
// sos/sos.h
//
// $Id$
//
//======================================================================
#ifndef sos_sos_h
#define sos_sos_h

//
// stub, later these functions will be defined
//
#include <stdlib.h>
#define alloc_memory malloc
#define realloc_memory realloc
#define free_memory free
#define alloc_zero_memory(SIZE) calloc(SIZE,1)
typedef unsigned char byte;

//
// Force insertion of rcsid
//
#if ! defined(RCSID)
#if ! defined(NO_RCSID)
#if defined(__STDC__) || defined(__ANSI_CPP__) || defined(__hpux)
#define UsE_PaStE(b) UsE__##b##_
#define PASTE(a,b) a##b
#else
#define UsE_PaStE(b) UsE__/**/b/**/_
#define PASTE(a,b) a/**/b
#endif
#if defined(__cplusplus)
#define UsE(x) inline void UsE_PaStE(x)(char *) { UsE_PaStE(x)(x); }
#else
#define UsE(x) static void UsE_PaStE(x)(char *d) { UsE_PaStE(x)(x); }
#endif
#define RCSID(str)                      \
        static char *rcsid_ = str;      \
        UsE(rcsid_)
#else
#define RCSID(str)
#endif
#endif

#endif