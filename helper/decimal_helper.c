
#include "decimal_helper.h"
#include <stdint.h>

typedef union {
  uint64_t                       i;
  __decimal_helper_decimal64_t   f;
} __decimal_helper_decimal64_caster_t;

static inline void __mystrcpy(char * restrict out, const char * restrict in) {
  const char *currin;
  char *currout;

  for (currin=in,currout=out;*currin!='\0';currin++,currout++) {
    *currout = *currin;
  }
  *currout = '\0';
}

static inline int __mystrlen(const char * str) {
  int i;
  const char *curr;

  for (i=0,curr=str;*curr!='\0';curr++,i++);
  return i;
}

static inline void __mystrconcat(char * restrict str, const char * restrict a, const char * restrict b) {
  int l;

  l = __mystrlen(a);
  __mystrcpy(str, a);
  __mystrcpy(str + l, b);
}


static inline void uint64_to_string(char *str, uint64_t x) {
  uint64_t d, r;
  char tmp[128];
  char *curr, *currout;

  if (x == ((uint64_t) 0)) {
    __mystrcpy(str, "0");
    return;
  }

  for (r=x,curr=tmp;r!=((uint64_t) 0);r/=(uint64_t) 10,curr++) {
    d = r % ((uint64_t) 10);
    *curr = '0' + ((char) d);
  }
  for (currout=str,curr--;curr>=tmp;curr--,currout++) {
    *currout = *curr;
  }
  *currout = '\0';
}

static inline void int32_to_string(char * str, int32_t x) {
  int64_t X;
  char *curr;
  uint64_t XX;

  curr = str;
  
  X = (int64_t) x;
  if (X < ((int64_t) 0)) {
    *curr = '-';
    X = -X;
  } else {
    *curr = '+';
  }
  curr++;
  XX = (uint64_t) X;
  
  uint64_to_string(curr, XX);
}

void decimal64_to_string(char *str, __decimal_helper_decimal64_t x) {
  __decimal_helper_decimal64_caster_t xcst;
  int32_t G;
  uint64_t mant;
  int sign;
  char *curr;
  char mantstr[128];
  char expostr[128];
  char tmp1[128];
  char tmp2[128];

  /* Initialize output */
  curr = str;
  
  /* Get IEEE754 memory representation */
  xcst.f = x;

  /* Strip off sign */
  sign = !!((int) (xcst.i >> 63));
  xcst.i <<= 1;
  xcst.i >>= 1;

  /* Write sign */
  if (sign) {
    *curr = '-';
  } else {
    *curr = '+';
  }
  curr++;
  
  /* Check for NaN or Inf */
  if ((xcst.i & UINT64_C(0x7800000000000000)) == UINT64_C(0x7800000000000000)) {
    /* Check for Inf */
    if ((xcst.i & UINT64_C(0x7c00000000000000)) == UINT64_C(0x7800000000000000)) {
      /* Inf */
      __mystrcpy(curr, "inf");
      return;
    }
    __mystrcpy(curr, "nan");
    return;
  }

  /* Here input is a zero or non-zero number 

     Get the significand first.

  */

  mant = (((xcst.i & UINT64_C(0x6000000000000000)) != UINT64_C(0x6000000000000000)) ? 
	   (xcst.i & UINT64_C(0x001fffffffffffff)) : 
	  ((xcst.i & UINT64_C(0x0007ffffffffffff)) | UINT64_C(0x0020000000000000)));
  if (mant > UINT64_C(9999999999999999)) mant = UINT64_C(0x0);

  /* Check for zero */
  if (mant == UINT64_C(0)) {
    /* The input is zero */
    __mystrcpy(curr, "0");
    return;
  }

  /* Get decimal exponent */
  G = ((int32_t) (((xcst.i & UINT64_C(0x6000000000000000)) != UINT64_C(0x6000000000000000)) ? 
		  ((xcst.i & UINT64_C(0x7fe0000000000000)) >> 53) : 
		  ((xcst.i & UINT64_C(0x1ff8000000000000)) >> 51))) - ((int32_t) 398);

  /* Convert the components */
  uint64_to_string(mantstr, mant);
  int32_to_string(expostr, G);

  /* Compose the result */
  __mystrconcat(tmp1, mantstr, "E");
  __mystrconcat(tmp2, tmp1, expostr);

  /* Write the result */
  __mystrcpy(curr, tmp2);
}

