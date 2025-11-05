#ifndef __LAMA_RUNTIME__
#define __LAMA_RUNTIME__

#include "runtime_common.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#define WORD_SIZE (CHAR_BIT * sizeof(ptrt))

_Noreturn void failure (char *s, ...);

extern aint Lread ();
extern aint Lwrite (aint n);
extern aint Ls__Infix_43(void *p, void *q);   // +
extern aint Ls__Infix_45(void *p, void *q);   // -
extern aint Ls__Infix_42(void *p, void *q);   // *
extern aint Ls__Infix_47(void *p, void *q);   // /
extern aint Ls__Infix_37(void *p, void *q);   // %
extern aint Ls__Infix_60(void *p, void *q);   // <
extern aint Ls__Infix_6061(void *p, void *q); // <=
extern aint Ls__Infix_62(void *p, void *q);   // >
extern aint Ls__Infix_6261(void *p, void *q); // >=
extern aint Ls__Infix_6161(void *p, void *q); // ==
extern aint Ls__Infix_3361(void *p, void *q); // !=
extern aint Ls__Infix_3838(void *p, void *q); // &&
extern aint Ls__Infix_3333(void *p, void *q); // !!

#endif
