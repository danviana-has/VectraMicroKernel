#ifndef VECTRA_TYPES_H
#define VECTRA_TYPES_H

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef uint32_t           size_t;
typedef int32_t            ssize_t;
typedef uint32_t           uintptr_t;
typedef int32_t            intptr_t;

#define NULL ((void*)0)

#ifndef __bool_true_false_are_defined
#define __bool_true_false_are_defined 1
#ifndef __cplusplus
#ifndef bool
typedef _Bool bool;
#define true 1
#define false 0
#endif
#endif
#endif

#endif /* VECTRA_TYPES_H */
