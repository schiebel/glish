#ifndef common_h_
#define common_h_

#include <ctype.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXT extern
#define I32 int
#define U32 unsigned int
#define U16 unsigned short
#define U8 unsigned char

typedef struct sv SV;
typedef struct regexp REGEXP;
typedef struct pmop PMOP;
typedef struct op OP;

#ifdef __cplusplus
	}
#endif

#include "regx/op.h"

#endif
