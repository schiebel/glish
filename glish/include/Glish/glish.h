/* $Header$ */

#ifndef glish_h
#define glish_h

typedef enum { glish_false, glish_true } glish_bool;

typedef const char* string;
typedef unsigned char byte;

#define loop_over_list(list, iterator)	\
	for ( int iterator = 0; iterator < (list).length(); ++iterator )

typedef void (*glish_signal_handler)();

#if ! defined(RCSID)
#if ! defined(NO_RCSID)
#if defined(__STDC__) || defined(__ANSI_CPP__)
#define UsE_PaStE(b) UsE__##b##_
#else
#define UsE_PaStE(b) UsE__/**/b/**/_
#endif
#if defined(__cplusplus)
#define UsE(x) inline void UsE_PaStE(x)(char *) { UsE_PaStE(x)(x); }
#else
#define UsE(x) static void UsE_PaStE(x)(char *d) { UsE_PaStE(x)(x); }
#endif
#define RCSID(str)			\
	static char *rcsid_ = str;	\
	UsE(rcsid_)
#else
#define RCSID(str)
#endif
#endif

#endif
