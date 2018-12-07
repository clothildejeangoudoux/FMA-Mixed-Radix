#ifndef COMPARISON_H
#define COMPARISON_H

#include <stdio.h>
#include <stdint.h>
#ifndef __STDC_WANT_DEC_FP__
#define __STDC_WANT_DEC_FP__ 1
#endif
#include <math.h>

typedef double _binary64;

typedef union {
  uint64_t i;
  _binary64 f;
} bin64caster;

typedef union {
  uint64_t i;
  _Decimal64 f;
} dec64caster;

int compareSignalingLessEqualBinary64Decimal64(_binary64 x, _Decimal64 y);
int emptyFunctionBinary64Decimal64(_binary64 x, _Decimal64 y);

#endif /* ifdef COMPARISON_H*/
