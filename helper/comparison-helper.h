#ifndef COMPARISON_HELPER_H
#define COMPARISON_HELPER_H

#include <stdio.h>
#include <stdint.h>
#ifndef __STDC_WANT_DEC_FP__
#define __STDC_WANT_DEC_FP__ 1
#endif
#include <math.h>

typedef double _binary64;

#define BINARY64_CLASS_QNAN          0
#define BINARY64_CLASS_NEG_INF       1
#define BINARY64_CLASS_NEG_NORMAL    2
#define BINARY64_CLASS_NEG_SUBNORMAL 3
#define BINARY64_CLASS_NEG_ZERO      4
#define BINARY64_CLASS_POS_ZERO      5
#define BINARY64_CLASS_POS_SUBNORMAL 6
#define BINARY64_CLASS_POS_NORMAL    7
#define BINARY64_CLASS_POS_INF       8
#define BINARY64_CLASS_SNAN          9

#define BINARY64_NUMBER_CLASSES     10


#define DECIMAL64_CLASS_QNAN         0
#define DECIMAL64_CLASS_NEG_INF      1
#define DECIMAL64_CLASS_NEG_NUMBER   2
#define DECIMAL64_CLASS_NEG_ZERO     3
#define DECIMAL64_CLASS_POS_ZERO     4
#define DECIMAL64_CLASS_POS_NUMBER   5
#define DECIMAL64_CLASS_POS_INF      6
#define DECIMAL64_CLASS_SNAN         7

#define DECIMAL64_NUMBER_CLASSES     8


#define PAIR_HARD 0
#define PAIR_EASY 1
#define PAIR_OPPSIGNS 2
#define PAIR_SPECIAL 3


#define EXTRA_CLASS_SPECIAL                  0
#define EXTRA_CLASS_OPPOSITE_SIGN            1
#define EXTRA_CLASS_NORMAL_SAME_SIGN_EASY    2
#define EXTRA_CLASS_SUBNORMAL_SAME_SIGN_EASY 3
#define EXTRA_CLASS_NORMAL_SAME_SIGN_HARD    4
#define EXTRA_CLASS_SUBNORMAL_SAME_SIGN_HARD 5

#define EXTRA_CLASSES 6



_binary64 composeBinary(int negative, int E, uint64_t m);
_Decimal64 composeDecimal(int negative, int expo, uint64_t mant);

int classifyBinary(_binary64 x);
int classifyDecimal(_Decimal64 x);

int classifyCase(_binary64 x, _Decimal64 y);

char *binaryClassToName(int class);
char *decimalClassToName(int class);

char *extraClassToName(int class);
int classifyExtraClass(int binClass, int decClass, int easy);

#endif /* ifdef COMPARISON_HELPER_H*/
