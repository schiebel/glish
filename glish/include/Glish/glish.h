/* $Header$ */

#ifndef glish_h
#define glish_h

typedef enum { glish_false, glish_true } glish_bool;

typedef const char* string;
typedef unsigned char byte;

#define loop_over_list(list, iterator)	\
	for ( int iterator = 0; iterator < (list).length(); ++iterator )

typedef void (*glish_signal_handler)();

#if ! defined(STAT)
#if defined(DO_STAT)
#define StAt_LiSt_hdn2(w,x) w,x
#define StAt_LiSt_hdn3(w,x,y) w,x,y
#define StAt_LiSt_hdn4(w,x,y,z) w,x,y,z
#define StAt_LiSt_hdn5(w,x,y,z,a) w,x,y,z,a
#define StAt_LiSt_hdn6(w,x,y,z,a,b) w,x,y,z,a,b
#define StAt_LiSt_hdn7(w,x,y,z,a,b,c) w,x,y,z,a,b,c
#define StAt_LiSt_hdn8(w,x,y,z,a,b,c,d) w,x,y,z,a,b,c,d
#define StAt_LiSt_hdn9(w,x,y,z,a,b,c,d,e) w,x,y,z,a,b,c,d,e
#define StAt_LiSt_hdn10(w,x,y,z,a,b,c,d,e,f) w,x,y,z,a,b,c,d,e,f
#define StAt_LiSt_hdn11(w,x,y,z,a,b,c,d,e,f,g) w,x,y,z,a,b,c,d,e,f,g
#define StAt_LiSt_hdn12(w,x,y,z,a,b,c,d,e,f,g,h) w,x,y,z,a,b,c,d,e,f,g,h
#define STAT(MESSAGE) message->Report(MESSAGE,": ", __FILE__, ", ", __LINE__);
#define STAT2(w,x) STAT(StAt_LiSt_hdn(w,x))
#define STAT3(w,x,y) STAT(StAt_LiSt_hdn3(w,x,y))
#define STAT4(w,x,y,z) STAT(StAt_LiSt_hdn4(w,x,y,z))
#define STAT5(w,x,y,z,a) STAT(StAt_LiSt_hdn5(w,x,y,z,a))
#define STAT6(w,x,y,z,a,b) STAT(StAt_LiSt_hdn6(w,x,y,z,a,b))
#define STAT7(w,x,y,z,a,b,c) STAT(StAt_LiSt_hdn7(w,x,y,z,a,b,c))
#define STAT8(w,x,y,z,a,b,c,d) STAT(StAt_LiSt_hdn8(w,x,y,z,a,b,c,d))
#define STAT9(w,x,y,z,a,b,c,d,e) STAT(StAt_LiSt_hdn9(w,x,y,z,a,b,c,d,e))
#define STAT10(w,x,y,z,a,b,c,d,e,f) STAT(StAt_LiSt_hdn10(w,x,y,z,a,b,c,d,e,f))
#define STAT11(w,x,y,z,a,b,c,d,e,f,g) STAT(StAt_LiSt_hdn11(w,x,y,z,a,b,c,d,e,f,g))
#define STAT12(w,x,y,z,a,b,c,d,e,f,g,h) STAT(StAt_LiSt_hdn9(w,x,y,z,a,b,c,d,e,f,g,h))
#else
#define STAT(MESSAGE)
#define STAT2(w,x)
#define STAT3(w,x,y)
#define STAT4(w,x,y,z)
#define STAT5(w,x,y,z,a)
#define STAT6(w,x,y,z,a,b)
#define STAT7(w,x,y,z,a,b,c)
#define STAT8(w,x,y,z,a,b,c,d)
#define STAT9(w,x,y,z,a,b,c,d,e)
#define STAT10(w,x,y,z,a,b,c,d,e,f)
#define STAT11(w,x,y,z,a,b,c,d,e,f,g)
#define STAT12(w,x,y,z,a,b,c,d,e,f,g,h)
#endif
#endif

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
