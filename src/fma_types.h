
#ifndef FMA_TYPES_H
#define FMA_TYPES_H

#include <stdint.h>

#ifndef __STDC_WANT_DEC_FP__
#define __STDC_WANT_DEC_FP__ 1
#endif
#include <math.h>

#define MIXED_FMA_INT32_CONST(x)  INT32_C(x)
#define MIXED_FMA_UINT64_CONST(x)  UINT64_C(x)
#define MIXED_FMA_INT64_CONST(x)  INT64_C(x)
#define MIXED_FMA_UNUSED_ARG(x)    do { (void) (x); } while (0)

#define __MIXED_FMA_REC_TBL_SIZE     (29)

typedef double        __mixed_fma_binary64_t;
typedef _Decimal64    __mixed_fma_decimal64_t;
typedef _Decimal128   __mixed_fma_decimal128_t;

typedef uint32_t      __mixed_fma_uint32_t;
typedef uint64_t      __mixed_fma_uint64_t;
typedef __uint128_t   __mixed_fma_uint128_t;

typedef int32_t       __mixed_fma_int32_t;
typedef int64_t       __mixed_fma_int64_t;
typedef __int128_t    __mixed_fma_int128_t;

/* Represented value = sum_i=0^n-1 l[i] * 2^(64 * i) */
typedef struct {
  __mixed_fma_uint64_t l[4];
} __mixed_fma_uint256_t;

typedef union {
  __mixed_fma_uint64_t      i;
  __mixed_fma_binary64_t    f;
} __mixed_fma_binary64_caster_t;

typedef union {
  __mixed_fma_uint64_t      i;
  __mixed_fma_decimal64_t   f;
} __mixed_fma_decimal64_caster_t;

typedef union {
  __mixed_fma_uint128_t     i;
  __mixed_fma_decimal128_t  f;
} __mixed_fma_decimal128_caster_t;

typedef struct {
  __mixed_fma_uint64_t l[__MIXED_FMA_REC_TBL_SIZE];
} __mixed_fma_rec_tbl_t;


#endif
