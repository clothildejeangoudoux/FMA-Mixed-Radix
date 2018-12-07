#ifndef DECIMAL_HELPER_H
#define DECIMAL_HELPER_H

#ifndef __STDC_WANT_DEC_FP__
#define __STDC_WANT_DEC_FP__ 1
#endif
#include <math.h>

typedef _Decimal64    __decimal_helper_decimal64_t;

void decimal64_to_string(char *, __decimal_helper_decimal64_t);


#endif
