#ifndef _STDDEF_H
#define _STDDEF_H

#include "types.h"

typedef int32_t ptrdiff_t;

#ifndef offsetof
#define offsetof(type, member) ((size_t) &((type *)0)->member)
#endif

#endif
