#include <stdio.h>

#include "fma_types.h"
#include "fma_mixedradix.h"
#include "fma_convert_to_binary_tbl.h"
#include "fma_convert_to_decimal_tbl.h"
#include "fma_rec_tbl.h"

#define STATIC   static
#define INLINE   inline
#define CONST    const
#define RESTRICT __restrict__

#define __MIXED_FMA_ZMIN             ((__mixed_fma_int32_t) (-1192))
#define __MIXED_FMA_FMIN             ((__mixed_fma_int32_t) (-416))
#define __MIXED_FMA_REC_ACC_SIZE     (67)

/* Decomposes a binary64 x into a 

   sign s
   an exponent E
   an dummy exponent F
   a significand m

   such that x = (-1)^s * 2^E * 5^F * m 

   with 2^54 <= m < 2^55 or m = 0 and E = 0

   (setting F to zero all the time)

   and returns a zero value

   or does not touch s, E and m and returns 
   a non-zero value if x is an infinity or a NaN.
*/
STATIC INLINE int __mixed_fma_decompose_binary64(int * RESTRICT s, __mixed_fma_int32_t * RESTRICT E, __mixed_fma_int32_t * RESTRICT F, __mixed_fma_uint64_t * RESTRICT m, __mixed_fma_binary64_t x) {
  __mixed_fma_binary64_caster_t xcst;
  int sign;
  __mixed_fma_int32_t expo;

  /* Touch everyone */
  *s = 0;
  *E = (__mixed_fma_int32_t) 0;
  *F = (__mixed_fma_int32_t) 0;
  *m = (__mixed_fma_uint64_t) 0;
  
  /* Get IEEE754 memory representation */
  xcst.f = x;

  /* Strip off sign */
  sign = !!((int) (xcst.i >> 63));
  xcst.i <<= 1;
  xcst.i >>= 1;

  /* Check for NaN or Inf */
  if ((xcst.i & MIXED_FMA_UINT64_CONST(0x7ff0000000000000)) == MIXED_FMA_UINT64_CONST(0x7ff0000000000000)) {
    /* Input is NaN or Inf */
    return 1;
  }

  /* Input is +/-0, +/- subnormal or +/- normal 

     Write back sign.

  */
  *s = sign;

  /* Set dummy exponent F */
  *F = (__mixed_fma_int32_t) 0;

  /* Set a preliminary exponent */
  expo = (__mixed_fma_int32_t) 0;
  
  /* Check for zero and subnormals */
  if ((xcst.i & MIXED_FMA_UINT64_CONST(0xfff0000000000000)) == MIXED_FMA_UINT64_CONST(0x0000000000000000)) {
    /* Input is zero or subnormal 

       Check for zero 
    */
    if (xcst.i == MIXED_FMA_UINT64_CONST(0x0000000000000000)) {
      /* Input is zero */
      *E = (__mixed_fma_int32_t) 0;
      *m = MIXED_FMA_UINT64_CONST(0);
      return 0;
    }
    /* Input is subnormal, multiply by 2^54, adapt preliminary
       exponent 
    */
    xcst.f *= 18014398509481984.0;
    expo -= (__mixed_fma_int32_t) 54;
  }

  /* The value in xcst.f is normal, expo contains a factor to be applied */
  expo += ((__mixed_fma_int32_t) (xcst.i >> 52)) - ((__mixed_fma_int32_t) (1023 + 52));
  *E = expo - ((__mixed_fma_int32_t) 2);
  *m = ((xcst.i & MIXED_FMA_UINT64_CONST(0x000fffffffffffff)) | MIXED_FMA_UINT64_CONST(0x0010000000000000)) << 2;
  
  return 0;
}

STATIC INLINE unsigned int __mixed_fma_lzc64(__mixed_fma_uint64_t a) {
  unsigned int r;
  __mixed_fma_uint64_t t;
  if ((a) == ((__mixed_fma_uint64_t) 0)) return (unsigned int) 64;
  if (sizeof(__mixed_fma_uint64_t) == sizeof(unsigned int)) {
    return (unsigned int) __builtin_clz(a);
  } else {
    if (sizeof(__mixed_fma_uint64_t) == sizeof(unsigned long)) {
      return (unsigned int) __builtin_clzl(a);
    } else {
      if (sizeof(__mixed_fma_uint64_t) == sizeof(unsigned long long)) {
	return (unsigned int) __builtin_clzll(a);
      } else {
	for (r=0, t=a; ((t & (((__mixed_fma_uint64_t) 1) << 63)) == ((__mixed_fma_uint64_t) 0)); r++, t<<=1);
	return r;
      }
    }
  }
  return (unsigned int) 0;
}

STATIC INLINE unsigned int __mixed_fma_lzc128(__mixed_fma_uint128_t a) {
  __mixed_fma_uint64_t ahi, alo;
  
  ahi = (__mixed_fma_uint64_t) (a >> 64);
  alo = (__mixed_fma_uint64_t) a;

  if (ahi == ((__mixed_fma_uint64_t) 0)) return 64 + __mixed_fma_lzc64(alo);
  return __mixed_fma_lzc64(ahi);
}

STATIC INLINE unsigned int __mixed_fma_lzc256(__mixed_fma_uint256_t * RESTRICT a) {
  int i;
  for (i=3;((i>=0) && (a->l[i] == ((__mixed_fma_uint64_t) 0)));i--);
  return (((unsigned int) (3-i)) << 6) + __mixed_fma_lzc64(a->l[i]);
}

/* Decomposes a decimal64 x into a 

   sign s
   a binary exponent J
   a decimal exponent K
   a significand n

   such that x = (-1)^s * 2^J * 5^K * n

   with 2^54 <= n < 2^55 or n = 0, J = 0 and K = 0

   and returns a zero value

   or does not touch s, J, K and n and returns 
   a non-zero value if x is an infinity or a NaN.
*/
STATIC INLINE int __mixed_fma_decompose_decimal64(int * RESTRICT s, __mixed_fma_int32_t * RESTRICT J, __mixed_fma_int32_t * RESTRICT K, __mixed_fma_uint64_t * RESTRICT n, __mixed_fma_decimal64_t x) {
  __mixed_fma_decimal64_caster_t xcst;
  int sign;
  __mixed_fma_uint64_t mant;
  __mixed_fma_int32_t G;
  unsigned int lzc, sigma;

  /* Touch everyone */
  *s = 0;
  *J = (__mixed_fma_int32_t) 0;
  *K = (__mixed_fma_int32_t) 0;
  *n = (__mixed_fma_uint64_t) 0;
  
  /* Get IEEE754 memory representation */
  xcst.f = x;

  /* Strip off sign */
  sign = !!((int) (xcst.i >> 63));
  xcst.i <<= 1;
  xcst.i >>= 1;

  /* Check for NaN or Inf */
  if ((xcst.i & MIXED_FMA_UINT64_CONST(0x7800000000000000)) == MIXED_FMA_UINT64_CONST(0x7800000000000000)) {
    /* Input is NaN or Inf */
    return 1;
  }

  /* Here input is a zero or non-zero number 

     Write back sign.

  */
  *s = sign;

  /* Get significand */
  mant = (((xcst.i & MIXED_FMA_UINT64_CONST(0x6000000000000000)) != MIXED_FMA_UINT64_CONST(0x6000000000000000)) ? 
	   (xcst.i & MIXED_FMA_UINT64_CONST(0x001fffffffffffff)) : 
	  ((xcst.i & MIXED_FMA_UINT64_CONST(0x0007ffffffffffff)) | MIXED_FMA_UINT64_CONST(0x0020000000000000)));
  if (mant > MIXED_FMA_UINT64_CONST(9999999999999999)) mant = MIXED_FMA_UINT64_CONST(0x0);

  /* Check for zero */
  if (mant == MIXED_FMA_UINT64_CONST(0)) {
    /* The input is zero */
    *n = (__mixed_fma_uint64_t) 0;
    *J = (__mixed_fma_int32_t) 0;
    *K = (__mixed_fma_int32_t) 0;
    return 0;
  }

  /* Get decimal exponent */
  G = ((__mixed_fma_int32_t) (((xcst.i & MIXED_FMA_UINT64_CONST(0x6000000000000000)) != MIXED_FMA_UINT64_CONST(0x6000000000000000)) ? 
			      ((xcst.i & MIXED_FMA_UINT64_CONST(0x7fe0000000000000)) >> 53) : 
			      ((xcst.i & MIXED_FMA_UINT64_CONST(0x1ff8000000000000)) >> 51))) - ((__mixed_fma_int32_t) 398);

  /* Normalize mantissa into 2^54 <= mant < 2^55 */
  lzc = __mixed_fma_lzc64(mant);
  sigma = lzc - ((unsigned int) 9);
  mant <<= sigma;
  
  /* Write back values */
  *J = G - ((__mixed_fma_int32_t) sigma);
  *K = G;
  *n = mant;
  
  return 0;
}

/* Computes 2^LAMBDA * 5^MU * omega = 2^L * 5^M * s * (1 + eps)

   with s such that 2^109 <= s < 2^110 or s = 0
   
   and 2^63 <= omega < 2^64 or omega = 0

   and eps bounded by abs(eps) <= 2^-63.

   We ensure that L, M, LAMBDA and MU satisfy

   -2261 <= L      <= 1938
    -842 <= M      <= 770
   -2215 <= LAMBDA <= 1984
    -842 <= MU     <= 770

    Raises an inexact flag if the operation is not exact.
*/
STATIC INLINE void __mixed_fma_lower_precision_low_prec(__mixed_fma_int32_t * RESTRICT LAMBDA, __mixed_fma_int32_t * RESTRICT MU, __mixed_fma_uint64_t * RESTRICT omega, int *RESTRICT inex_out,
							__mixed_fma_int32_t L, __mixed_fma_int32_t M, __mixed_fma_uint128_t s) {
  __mixed_fma_uint128_t tmp;
  
  *LAMBDA = L + ((__mixed_fma_int32_t) 46);
  *MU = M;

  tmp = s;
  tmp <<= 18;
  *inex_out=(((__mixed_fma_uint64_t) tmp) != 0)?1:0;
  tmp >>= 64;
  *omega = (__mixed_fma_uint64_t) tmp;
}


/* Computes 2^LAMBDA * 5^MU * omega = 2^N * 5^P * t

   with t such that 2^54 <= t < 2^55 or t = 0
   
   and 2^63 <= omega < 2^64 or omega = 0.

   We ensure that N, P, LAMBDA and MU satisfy

   -1130 <= N      <= 969
    -421 <= P      <= 385
   -2215 <= LAMBDA <= 1984
    -842 <= MU     <= 770

*/
STATIC INLINE void __mixed_fma_increase_precision_low_prec(__mixed_fma_int32_t * RESTRICT LAMBDA, __mixed_fma_int32_t * RESTRICT MU, __mixed_fma_uint64_t * RESTRICT omega,
							   __mixed_fma_int32_t N, __mixed_fma_int32_t P, __mixed_fma_uint64_t t) {
  *LAMBDA = N - ((__mixed_fma_int32_t) 9);
  *MU = P;
  *omega = t << 9;
}


/* Computes floor(x * log2(5)) for x in -5000 <= x <= 5000 */
STATIC INLINE __mixed_fma_int32_t __mixed_fma_floor_times_log2_of_5(__mixed_fma_int32_t x) {
  __mixed_fma_int64_t l;
  __mixed_fma_int64_t tmp;

  /* l = floor(2^(48) * log2(5)) */
  l = MIXED_FMA_UINT64_CONST(653564656432238);
  tmp = (__mixed_fma_int64_t) x;
  tmp *= l;
  tmp >>= 48; /* 48 = STUB */

  return (__mixed_fma_int32_t) tmp;
}


/* Computes floor(x * log2(10)) for x in -5000 <= x <= 5000 */
STATIC INLINE __mixed_fma_int32_t __mixed_fma_floor_times_log2_of_10(__mixed_fma_int32_t x) {
  __mixed_fma_int64_t l;
  __mixed_fma_int64_t tmp;

  /* l = floor(2^(48) * log2(10)) */
  l = MIXED_FMA_UINT64_CONST(935039633142894);
  tmp = (__mixed_fma_int64_t) x;
  tmp *= l;
  tmp >>= 48; /* 48 = STUB */

  return (__mixed_fma_int32_t) tmp;
}

/* Computes floor(x * log10(2)) for x in -5000 <= x <= 5000 */
STATIC INLINE __mixed_fma_int32_t __mixed_fma_floor_times_log10_of_2(__mixed_fma_int32_t x) {
  __mixed_fma_int64_t l;
  __mixed_fma_int64_t tmp;

  /* l = floor(2^(50) * log10(2)) */
  l = MIXED_FMA_UINT64_CONST(338929644074911);
  tmp = (__mixed_fma_int64_t) x;
  tmp *= l;
  tmp >>= 50; /* 50 = STUB */

  return (__mixed_fma_int32_t) tmp;
}

/* Computes 2^GAMMA * v = 2^LAMBDA * 5^MU * omega * (1 + eps)

   with 2^63 <= omega < 2^64 or omega = 0 

   and 2^63 <= v < 2^64 or v = 0

   and eps bounded by abs(eps) <= 2^-60.5.

   The algorithm supposes that 

   -2215 <= LAMBDA <= 1984
    -842 <= MU     <= 770

   and guarantees that 

   -3095 <= GAMMA <= 3772.
*/
STATIC INLINE void __mixed_fma_convert_to_binary_low_prec(__mixed_fma_int32_t * RESTRICT GAMMA, __mixed_fma_uint64_t * RESTRICT v, int *RESTRICT inex_out,
							  __mixed_fma_int32_t LAMBDA, __mixed_fma_int32_t MU, __mixed_fma_uint64_t omega, int inex_in) {
  __mixed_fma_uint64_t t;
  __mixed_fma_uint128_t tmp1, tmp2;
  __mixed_fma_int32_t tmp, mu = MU-__MIXED_FMA_MMIN;

  /* v = floor(2^(-64) * conversionToBinaryTbl3(MU) * omega) */
  t = __mixed_fma_conversionToBinaryTbl3[mu];
  tmp1 = (__mixed_fma_uint128_t) omega;
  tmp2 = (__mixed_fma_uint128_t) t;
  tmp1 = tmp1 * tmp2; // exact
  
  /* Raise the flag if inexact operation */
  *inex_out=(((__mixed_fma_uint64_t) tmp1) != 0)?1:inex_in;
  *v = (__mixed_fma_uint64_t) (tmp1 >> 64);
  
  /* GAMMA = LAMBDA + floor(MU * log2(5)) + 1 */
  tmp = __mixed_fma_floor_times_log2_of_5(MU);
  *GAMMA = LAMBDA + tmp + 1;
}


/* Checks whether (2^L * 5^M * s) / (2^N * 5^P * t)

   is between 1/2 and 2.

   Returns a non-zero value if that ratio is clearly outside [1/2;2] and
   returns a zero value otherwise.

   This means there might be inputs with a ratio outside [1/2;2] for 
   which the algorithm returns a zero value but the algorithm ensures
   that when it returns a non-zero value the ratio is surely outside of
   [1/2;2].
   
*/
STATIC INLINE int __mixed_fma_check_ratio(__mixed_fma_int32_t L, __mixed_fma_int32_t M, __mixed_fma_uint128_t s,
					  __mixed_fma_int32_t N, __mixed_fma_int32_t P, __mixed_fma_uint64_t t) {
  __mixed_fma_int32_t tmp1, tmp2;

  MIXED_FMA_UNUSED_ARG(s);
  MIXED_FMA_UNUSED_ARG(t);

  tmp1 = M-P;
  /* l = floor(2^(48) * log2(5)) */
  tmp2 = __mixed_fma_floor_times_log2_of_5(tmp1);
  
  if ( (((__mixed_fma_int32_t) ((L - N) + tmp2 + 54)) > 1) || (((__mixed_fma_int32_t) ((L - N) + tmp2 + 57)) < -1)) return 1;
   
  return 0; 
}

/* Computes 2^L * 5^M * s = 2^N * 5^P * t

   with t such that 2^54 <= t < 2^55 or t = 0
   
   and 2^109 <= s < 2^110 or s = 0.

   We ensure that N, P, L and M satisfy

   -1130 <= N <= 969
    -421 <= P <= 385
   -1185 <= L <= 914
    -421 <= M <= 385

*/
STATIC INLINE void __mixed_fma_increase_precision_high_prec(__mixed_fma_int32_t * RESTRICT L, __mixed_fma_int32_t * RESTRICT M, __mixed_fma_uint128_t * RESTRICT s,
							    __mixed_fma_int32_t N, __mixed_fma_int32_t P, __mixed_fma_uint64_t t) {
  __mixed_fma_uint128_t tmp;
  
  *L = N - ((__mixed_fma_int32_t) 55);
  *M = P;
  tmp = (__mixed_fma_uint128_t) t;
  tmp <<= 55;
  *s = tmp;
}

STATIC INLINE void __mixed_fma_multiply_array(__mixed_fma_uint64_t * RESTRICT c, CONST __mixed_fma_uint64_t * RESTRICT a, int size_a, CONST __mixed_fma_uint64_t * RESTRICT b, int size_b) {
  int i, j;
  __mixed_fma_uint128_t cs[6];
  __mixed_fma_uint64_t a_elem, b_elem, tmp3;
  __mixed_fma_uint128_t tmp1, tmp2, cin, cout, elem_cs;
  
  for (i=0;i<size_a+size_b;i++) {
    cs[i] = (__mixed_fma_uint128_t) 0;
  }
  for (i=0;i<size_a;i++) {
    a_elem = a[i];
    for (j=0;j<size_b;j++) {
      b_elem = b[j];
      tmp1 = (__mixed_fma_uint128_t) a_elem;
      tmp2 = (__mixed_fma_uint128_t) b_elem;
      tmp2 *= tmp1;
      tmp3 = (__mixed_fma_uint64_t) tmp2;
      tmp1 = (__mixed_fma_uint128_t) tmp3;
      tmp2 >>= 64;
      cs[i + j] += tmp1;
      cs[i + j + 1] += tmp2;
    }
  }
  cin = (__mixed_fma_uint128_t) 0;
  for (i=0;i<size_a+size_b;i++) {
    elem_cs = cs[i];
    elem_cs += cin;
    tmp3 = (__mixed_fma_uint64_t) elem_cs;
    cout = elem_cs >> 64;
    c[i] = tmp3;
    cin = cout;
  }
}

/* Computes z = floor(2^-128 * x * y) */
STATIC INLINE void __mixed_fma_multiply_high_prec(int * RESTRICT inex_out,
						  __mixed_fma_uint256_t * RESTRICT z,
						  __mixed_fma_uint128_t x,
						  CONST __mixed_fma_uint256_t * RESTRICT y) {
  int i;
  __mixed_fma_uint64_t tmp_x[2];
  __mixed_fma_uint64_t tmp_z[6];

  tmp_x[0] = (__mixed_fma_uint64_t) x;
  tmp_x[1] = (__mixed_fma_uint64_t) (x >> 64);
  
  __mixed_fma_multiply_array(tmp_z, tmp_x, 2, y->l, 4);

  for (i=0;i<4;i++) {
    z->l[i] = tmp_z[i + 2];
  }

  *inex_out = ((tmp_z[0] != 0) || (tmp_z[1] != 0))?1:0;
}

/* Normalizes rho that enters as 2^254 <= rho < 2^256 such that
   finally 2^255 <= rho < 2^256 while maintaining the corresponding
   exponent SHA such that in the end 2^SHA * rho is still the same
   value.
*/
STATIC INLINE void __mixed_fma_normalize_high_prec(__mixed_fma_int32_t * RESTRICT SHA, __mixed_fma_uint256_t * RESTRICT rho) {
  int i;
  __mixed_fma_uint64_t cin, cout;

  if (rho->l[3] && (((__mixed_fma_uint64_t) 1) << 63)) return;

  (*SHA)--;
  cin = (__mixed_fma_uint64_t) 0;
  for (i=0;i<4;i++) {
    cout = rho->l[i] >> 63;
    rho->l[i] <<= 1;
    rho->l[i] |= cin;
    cin = cout;
  }
}

/* Normalizes rho that enters as 2^(255-l) <= rho < 2^256 such that
   finally 2^255 <= rho < 2^256 while maintaining the corresponding
   exponent SHA such that in the end 2^SHA * rho is still the same
   value.
*/
STATIC INLINE void __mixed_fma_normalize_high_prec_l(__mixed_fma_int32_t * RESTRICT SHA, __mixed_fma_uint256_t * RESTRICT rho, unsigned int l) {
  int i, j;
  __mixed_fma_uint64_t cin, cout;

  *SHA = *SHA - l;
  
  for (j=3;j>=0;j--) {
    if (l > 64*j) {
      for (i=0;i<j;i++) {
	rho->l[i] = 0;
	l-=64;
      }
      cin = (__mixed_fma_uint64_t) 0;
      for (i=j;i<4;i++) {
	cout = rho->l[i] >> (64-l);
	rho->l[i] <<= l;
	rho->l[i] |= cin;
	cin = cout;
      }
      break;
    }
  }
}


/* Computes rho = 2^-r * rho
   with 2^255 <= rho < 2^256
   and r >= 0, r bounded
*/
STATIC INLINE void __mixed_fma_shift_high_prec_right(int * RESTRICT inex_out, __mixed_fma_uint256_t * RESTRICT rho, __mixed_fma_uint32_t r) {
  int i;
  __mixed_fma_uint64_t cin, cout;

  if (r==0) return;
  
  cin = (__mixed_fma_uint64_t) 0;
  for (i=3;i>=0;i--) {
    cout = rho->l[i] << (64-r);
    rho->l[i] >>= r;
    rho->l[i] |= cin;
    cin = cout;
  }

  *inex_out=((__mixed_fma_uint64_t) (cin >> (64-r)) != 0)?1:0;
}


/* Computes 2^SHA * rho = 2^L * 5^M * s * (1 + eps)

   with 2^109 <= s < 2^110 or s = 0 

   and 2^255 <= rho < 2^256 or rho = 0

   and eps bounded by abs(eps) <= 2^-251.

   The algorithm supposes that 

   -2261 <= L <= 1938
    -842 <= M <= 770

   and guarantees that 

   -4364 <= SHA <= 3579.
*/
STATIC INLINE void __mixed_fma_convert_to_binary_high_prec(__mixed_fma_int32_t * RESTRICT SHA, __mixed_fma_uint256_t * RESTRICT rho, int * RESTRICT inex_out,
							  __mixed_fma_int32_t L, __mixed_fma_int32_t M, __mixed_fma_uint128_t s) {
  __mixed_fma_uint256_t r;
  __mixed_fma_uint128_t c;
   
  *SHA = L-18;
  c =  ((__mixed_fma_uint128_t) s) << 18;

  /* Exact if M is between __MIXED_FMA_CONVERT_TO_BINARY_TBL_EXACT_MIN and __MIXED_FMA_CONVERT_TO_BINARY_TBL_EXACT_MAX */
  r.l[0]=__mixed_fma_conversionToBinaryTbl0[M-__MIXED_FMA_MMIN]; 
  r.l[1]=__mixed_fma_conversionToBinaryTbl1[M-__MIXED_FMA_MMIN]; 
  r.l[2]=__mixed_fma_conversionToBinaryTbl2[M-__MIXED_FMA_MMIN]; 
  r.l[3]=__mixed_fma_conversionToBinaryTbl3[M-__MIXED_FMA_MMIN]; 

  __mixed_fma_multiply_high_prec(inex_out, rho, c, &r);
  *SHA += (__mixed_fma_floor_times_log2_of_5(M) - 127);
  __mixed_fma_normalize_high_prec(SHA, rho); // exact operation
  if ((M< __MIXED_FMA_CONVERT_TO_BINARY_TBL_EXACT_MIN) || (M>__MIXED_FMA_CONVERT_TO_BINARY_TBL_EXACT_MAX)) *inex_out=1;
}


/* Computes (-1)^s 2^C * g = (2^GAMMA1 * v1 +/- 2^GAMMA2 * v2)(1 + eps)

   with a "far path" addition algorithm,

   with 2^63 <= v1 < 2^64 or v1 = 0 and 2^63 <= v2 < 2^64 or v2 = 0

   and 2^63 <= g < 2^64

   and eps bounded by abs(eps) <= 2^-63.

   The algorithm supposes that 

   -4171 <= GAMMA1 <= 3772
   -4171 <= GAMMA2 <= 3772

   and guarantees that
   
   -4174 <= C <= 3836.
*/
STATIC INLINE void __mixed_fma_far_path_addition(__mixed_fma_int32_t * RESTRICT C, __mixed_fma_uint64_t * RESTRICT g, int * RESTRICT s, int *RESTRICT inex_out,
						 __mixed_fma_int32_t GAMMA1, __mixed_fma_uint64_t v1, int s1, int inex_in1, __mixed_fma_int32_t GAMMA2, __mixed_fma_uint64_t v2, int s2, int inex_in2) {
  unsigned int sigma;
  __mixed_fma_int32_t tmp_exp;
  __mixed_fma_uint64_t tmp_mant;
  int tmp_s, tmp_inex;
  __mixed_fma_uint128_t tmp_v1, tmp_v2, tmp_g;

  /* Cases where one of the operand is zero */
  if (v1 == 0) {
    *g = v2;
    *C = GAMMA2;
    *s = s2;
    *inex_out = inex_in2;
    return;
  }
  if (v2 == 0) {
    *g = v1;
    *C = GAMMA1;
    *s = s1;
    *inex_out = inex_in1;
    return;
  }
  
  /* Order T1 and T2 such that T1 >= T2 */
  if ((GAMMA1 < GAMMA2) || ((GAMMA1 == GAMMA2) && (v1 < v2)) ) {
    tmp_exp = GAMMA2;
    GAMMA2 = GAMMA1;
    GAMMA1 = tmp_exp;
    tmp_mant = v2;
    v2 = v1;
    v1 = tmp_mant;
    tmp_s = s2;
    s2 = s1;
    s1 = tmp_s;
    tmp_inex = inex_in2;
    inex_in2 = inex_in1;
    inex_in1 = tmp_inex;
  } 

  /* If GAMMA1 >= GAMMA2 + 64 */
  if (GAMMA1 >= (GAMMA2 + ((__mixed_fma_int32_t) 64))) {
    /* Absorption of T2*/
    *g = v1;
    *C = GAMMA1;
    *s = s1;
    *inex_out = inex_in1;
    return;
  }
  
  /* Compute T1 +/- T2 on 128 bit significands variables */
  tmp_v1 = (__mixed_fma_uint128_t) v1;
  tmp_v2 = (__mixed_fma_uint128_t) v2;

  /* Align T1 with T2 and harmonize the exponent */
  tmp_exp = GAMMA1 - GAMMA2;
  tmp_v1 <<= tmp_exp;
  GAMMA1 = GAMMA2; /* Useless */

  /* Determine the sign and add or subtract */
  *s = s1;
  *inex_out = inex_in1 | inex_in2;
  if (s1 == s2) {
    tmp_g = (__mixed_fma_uint128_t) (tmp_v1 + tmp_v2); 
  } else {
    tmp_g = (__mixed_fma_uint128_t) (tmp_v1 - tmp_v2); 
  }
        
  /* Normalize with leading zero count */
  sigma = __mixed_fma_lzc128(tmp_g); /* with 0 <= sigma <= 66 */ 
  GAMMA2 -= sigma;
  tmp_g <<= sigma; 

  /* Set the result */
  if (((__mixed_fma_uint64_t) tmp_g) != 0) *inex_out |= 1;
  tmp_g >>= 64;
  *g = (__mixed_fma_uint64_t) tmp_g;
  *C = GAMMA2 + ((__mixed_fma_uint32_t) 64);
}

/* Compare rho1 and rho2

   with 2^255 <= rho1 < 2^256 or rho1 = 0 and 2^255 <= rho2 < 2^256 or rho2 = 0

   Returns 
   - a negative value if rho1 < rho2
   - a zero value if rho1 = rho2
   - a positive value if rho1 > rho2   
*/ 
STATIC INLINE int __mixed_fma_comparison_high_prec(__mixed_fma_uint256_t* RESTRICT rho1, __mixed_fma_uint256_t* RESTRICT rho2) {
  int i;

  for (i=3;i>=0;i--) {
    if (rho1->l[i] > rho2->l[i]) return 1;
    if (rho1->l[i] < rho2->l[i]) return -1;
  }
  
  return 0;
}

/* Compare rho1 a 256 unsigned int and zero

   Returns 
   - a positive value if rho1 == 0
   - a zero value if rho1 != 0
*/ 
STATIC INLINE int __mixed_fma_is_zero_high_prec(__mixed_fma_uint256_t* RESTRICT rho1) {
  int i;

  for (i=3;i>=0;i--) {
    if (rho1->l[i] != 0) return 0;
  }
  
  return 1;
}

/* Computes g = rho1 - rho2
   
   with rho1 >= rho2
   and
   with 2^255 <= rho1 < 2^256 or rho1 = 0 and 2^255 <= rho2 < 2^256 or rho2 = 0

   and 2^255 <= g < 2^256 or g = 0 
*/ 
STATIC INLINE void __mixed_fma_subtraction_high_prec(__mixed_fma_uint256_t* RESTRICT g,
						    __mixed_fma_uint256_t* RESTRICT rho1, __mixed_fma_uint256_t* RESTRICT rho2) {
  int i;
  __mixed_fma_uint64_t cin, cout, r1, r2, tmp;

  cin = (__mixed_fma_uint64_t) 0;
  cout = (__mixed_fma_uint64_t) 0;
  for (i=0;i<4;i++) {
    r1 = rho1->l[i] + cin;
    r2 = rho2->l[i];
    if (r2>r1) {
      tmp = r1;
      r1 = r2;
      r2 = tmp;
      cout = (__mixed_fma_uint64_t) 1;
    }
    g->l[i] = r1-r2;
    cin = cout;
  }
  
}

/* Computes +/- 2^C * g  = (2^SHA1 * rho1 - 2^SHA2 * rho2)(1 + eps)

   with a "near path" addition algorithm,

   with 2^255 <= rho1 < 2^256 or rho1 = 0 and 2^255 <= rho2 < 2^256 or rho2 = 0

   and 2^63 <= g < 2^64 or g = 0

   and eps bounded by abs(eps) <= 2^.

   The algorithm supposes that 

   -4364 <= SHA1 <= 3579
   -4364 <= SHA2 <= 3579

   and guarantees that

   -2296 <= C <= 3773.
*/ 
STATIC INLINE void __mixed_fma_near_path_subtraction(__mixed_fma_int32_t * RESTRICT C, __mixed_fma_uint64_t * RESTRICT g, int * RESTRICT s, int *RESTRICT inex_out,
						     __mixed_fma_int32_t SHA1, __mixed_fma_uint256_t* RESTRICT rho1, int s1, int inex_in1, __mixed_fma_int32_t SHA2, __mixed_fma_uint256_t* RESTRICT rho2, int s2, int inex_in2) {
  __mixed_fma_uint32_t tmp_exp;
  __mixed_fma_uint256_t tmp_mant,tmp_g;
  int tmp_s, tmp_inex, i, sign_switch=0;
  unsigned int l;

  /* Cases where one of the operand is zero */
  if (__mixed_fma_is_zero_high_prec(rho1)) {
    *g = rho2->l[3];
    *C = SHA2;
    *s = !s2;
    *inex_out = inex_in2;
    return;
  }
  if (__mixed_fma_is_zero_high_prec(rho2)) {
    *g = rho1->l[3];
    *C = SHA1;
    *s = s1;
    *inex_out = inex_in1;
    return;
  }

  /* Order T1 and T2 such that T1 >= T2 */
  if (SHA1 < SHA2) {
    tmp_exp = SHA2;
    SHA2 = SHA1;
    SHA1 = tmp_exp;
    tmp_s = s2;
    s2 = s1;
    s1 = tmp_s;
    sign_switch=1;
    tmp_inex = inex_in2;
    inex_in2 = inex_in1;
    inex_in1 = tmp_inex;
    tmp_mant = *rho2;
    rho2 = rho1;
    rho1 = &tmp_mant;
  } 

  /* Save the exponent */
  *C = SHA1;
  *inex_out = inex_in1;
  
  /* Align T1 and T2 and shift T2 to the right */
  tmp_exp = SHA1 - SHA2;
  __mixed_fma_shift_high_prec_right(inex_out, rho2,tmp_exp);
  *inex_out |= inex_in2;

  /* Order rho1 and rho2 such that rho1 >= rho2 */
  if (__mixed_fma_comparison_high_prec(rho1, rho2) < 0) {
    if (!sign_switch) {
      tmp_s = s2;
      s2 = s1;
      s1 = tmp_s;
    }
    tmp_mant = *rho2;
    rho2 = rho1;
    rho1 = &tmp_mant;
  }
  
  /* Subtraction rho1 - rho2 */
  __mixed_fma_subtraction_high_prec(&tmp_g, rho1, rho2); // exact

  /* Adapt the exponent to the normalized significand */
  l = __mixed_fma_lzc256(&tmp_g);
  __mixed_fma_normalize_high_prec_l(C,&tmp_g,l); // exact

  /* Set the result */
  /* for(i=3;((i>=0)&&(tmp_g.l[3-i]==0));i--); */
  /* *C += ((__mixed_fma_uint32_t) (64*i)); */
  *g = tmp_g.l[3];
  *s = s1;
  *C += (192);
  for (i=2;i>=0;i--) {
    if (rho1->l[i] != 0) *inex_out = 1;
  }
}

/* Computes z = floor(2^-64 * x * y) */
STATIC INLINE void __mixed_fma_multiply_high_prec2(__mixed_fma_uint128_t * RESTRICT z,
						   int *RESTRICT inex_out,
						   __mixed_fma_uint64_t x,
						   __mixed_fma_uint64_t * RESTRICT y,
						   int inex_in) {
  __mixed_fma_uint64_t tmp_z[3];
   
  __mixed_fma_multiply_array(tmp_z, &x, 1, y, 2);

  *z = ((__mixed_fma_uint128_t) tmp_z[2]) << 64;
  (*z) += tmp_z[1];

  *inex_out = (tmp_z[0] != 0)?1:inex_in;
}

/* Computes 10^H * 2^-10 * q = 2^C * g * (1 + eps)

   with 10^15 * 2^10 <= q <= (10^16 - 1) * 2^11 < 2^64 or q = 0 

   and 2^63 <= g < 2^64  or g = 0

   and eps bounded by abs(eps) <= 2^-59.82.

   The algorithm supposes that 

   -4174 <= C <= 3836

   and guarantees that 

   -1253 <= H <= 1159.
*/
STATIC INLINE void __mixed_fma_convert_to_decimal(__mixed_fma_int32_t * RESTRICT H, __mixed_fma_uint64_t * RESTRICT q, int *RESTRICT inex_out,
						  __mixed_fma_int32_t C,  __mixed_fma_uint64_t g, int inex_in) {
  __mixed_fma_int32_t alpha;
  __mixed_fma_uint128_t b, d, tmp_b;
  __mixed_fma_uint64_t tblT[2], tmp_tbl, tmp_d;
  
  /* Compute H = floor((C+63)*log10(2)) - 15 + mu(C,g)
     
     with mu(C,g) is in {0,1} such that
     mu(C,g) = 0 if g <= g*(C)
               1 if g >= g*(C) + 1
  
     with g*(C) = ceil(10^(1+floor((C+63)*log10(2))-C*log10(2))) - 1
     and g*(C) <= 2^64 - 1
  */
  *H = __mixed_fma_floor_times_log10_of_2((__mixed_fma_int32_t ) (C+63)) - 15;
  tmp_tbl = __mixed_fma_conversionToDecimalTblGstar[C-__MIXED_FMA_CMIN];
  if (g > tmp_tbl) {
    (*H)++;
  }
  
  /* Compute q such that
        q = floor(2^-62 * d)
	d = floor(2^-alpha(H,C) * b)
	b = floor(2^-64 * t(H) * g)

     with t(H) = floor(10^-H * 2^(floor(H * log2(10)) + 127))
     and alpha(H,C) = floor(H * log2(10)) - C - 9
  */
  alpha = __mixed_fma_floor_times_log2_of_10(*H) - C;
  alpha -= (__mixed_fma_int32_t) 9;

  /* Exact if H is between __MIXED_FMA_CONVERT_TO_DECIMAL_TBL_EXACT_MIN and __MIXED_FMA_CONVERT_TO_DECIMAL_TBL_EXACT_MAX */
  tblT[0] = __mixed_fma_conversionToDecimalTblT0[(*H)-__MIXED_FMA_HMIN]; 
  tblT[1] = __mixed_fma_conversionToDecimalTblT1[(*H)-__MIXED_FMA_HMIN]; 
  *inex_out=(((*H)< __MIXED_FMA_CONVERT_TO_DECIMAL_TBL_EXACT_MIN) || ((*H)>__MIXED_FMA_CONVERT_TO_DECIMAL_TBL_EXACT_MAX))?1:inex_in;
  __mixed_fma_multiply_high_prec2(&b, inex_out, g, tblT, inex_in);

  tmp_b = (__mixed_fma_uint128_t) (b << (128 - alpha));
  *inex_out=((__mixed_fma_uint64_t) (tmp_b >> (128-alpha)) != 0)?1:*inex_out;
  d = (__mixed_fma_uint128_t) (b >> alpha);

  tmp_d = (__mixed_fma_uint64_t) (d << 2);
  *inex_out=(((__mixed_fma_uint64_t) (tmp_d >> 2)) != 0)?1:*inex_out;
  
  *q = (__mixed_fma_uint64_t) (d >> 62);
}

/* Computes round((-1)^s * 2^C * g) where round() is the current
   binary rounding to binary64 and sets all appropriate IEEE754 flags and only the
   appropriate flags.

   If g is non-zero, the value g is guaranteed to be bounded by
   
   2^63 <= g < 2^64

   When g is zero, round() applies the appropriate rounding rules
   for (-1)^s * 1.0 - (-1)^s * 1.0 as asked for IEEE754-2008.

*/
STATIC INLINE __mixed_fma_binary64_t __mixed_fma_final_round_binary(int s, __mixed_fma_int32_t C, __mixed_fma_uint64_t g) {
  volatile __mixed_fma_binary64_t tmp;
  __mixed_fma_int32_t Z, T, U, beta, J, J1, J2;
  __mixed_fma_uint64_t q, r;
  unsigned int alpha, lzc;
  __mixed_fma_binary64_t a, b, c, d, res, qq, rr;
  __mixed_fma_binary64_caster_t acst, bcst, ccst, dcst;

  /* Check if g is zero */
  if (g == ((__mixed_fma_uint64_t) 0)) {
    /* g is zero

       Load (-1)^s on a volatile variable.
    */
    if (s) {
      tmp = -1.0;
    } else {
      tmp = 1.0;
    }
    
    /* Return (-1)^s - (-1)^s */
    return tmp - tmp;
  }

  /* Here, g is non-zero.
     
     We have to return (-1)^s * 2^C * g = (-1)^s * 2^Z * g

     We start by setting Z = C and then clamping Z as follows:

     * if Z > 1200, we set Z = 1200, this yields to overflow
     * if Z < -1200, we set Z = -1200, this yields to full underflow

  */
  Z = C;
  if (Z > ((__mixed_fma_int32_t) 1200)) Z = (__mixed_fma_int32_t) 1200;
  if (Z < ((__mixed_fma_int32_t) -1200)) Z = (__mixed_fma_int32_t) -1200;

  /* Now compute T = max(-1074, Z + 11) and alpha = T - Z

     We then have two cases:

     (i)  If Z + 11 > -1074, then T = max(-1074, Z + 11) = Z + 11 and hence alpha = 11 and T = Z + alpha = Z + 11 > -1074
     (ii) If Z + 11 <= -1074, then T = max(-1074, Z + 11) = -1074 and hence alpha = T - Z = T - (Z + 11) + 11 >= -1074 + 1074 + 11 = 11

     Hence

     (-1)^s * 2^Z * g = 2^T * q + 2^Z * r

     with

     q = floor(2^-alpha * g)

     r = g - 2^alpha * q

     As alpha >= 11, q holds on 53 bits.

     We also compute q and r.
     
  */
  T = Z + ((__mixed_fma_int32_t) 11);
  if (T < ((__mixed_fma_int32_t) -1074)) T = (__mixed_fma_int32_t) -1074;
  alpha = (unsigned int) (T - Z);
  if (alpha > 63) {
    q = (__mixed_fma_uint64_t) 0;
    r = g;
  } else {
    q = g >> alpha;
    r = g - (q << alpha);
  }

  /* Now compute U = min(800, T) and beta = U - T

     We then have two cases:

     (i)  If T < 800, then U = min(800, T) = T < 800 and beta = 0
     (ii) If T >= 800, then U = min(800, T) = 800 and beta = U - T = U - 800

     Hence

     (-1)^s * 2^Z * g = (-1)^s * 2^beta * (2^U * q + 2^(Z - beta) * r) = 2^beta * ((-1)^s * 2^U * q + (-1)^s * 2^J * r)

     where -1074 <= U <= 800 and hence (-1)^s * 2^U * q representable as a binary64.

     We also compute J.

  */
  U = T;
  if (U > ((__mixed_fma_int32_t) 800)) U = (__mixed_fma_int32_t) 800;
  beta = U - T;
  J = Z - beta;

  /* Now compute J1 = floor(J / 2) and J2 = J - J1.

     Hence we have to return

     2^beta * (((-1)^s * 2^U * q) + ((-1)^s * 2^J1) * (2^J2 * r))

     J1 and J2 are bounded by something like

     -700 <= J1 <= 700
     -700 <= J2 <= 700.

  */
  J1 = J / 2;
  J2 = J - J1;

  /* Now normalize r into 2^63 <= r < 2^64 (or r = 0), modifying J2 in accordance. */
  lzc = __mixed_fma_lzc64(r);
  r <<= lzc;
  J2 -= (__mixed_fma_int32_t) lzc;

  /* Now divide r by 2^11, integrating possible sticky bits. Adjust J2 again.

     After this operation, we are sure that 2^J2 * r holds on a normal binary64.

  */
  r = (r >> 11) | ((__mixed_fma_uint64_t) (!!((r - ((r >> 11) << 11)) != ((__mixed_fma_uint64_t) 0))));
  J2 += (__mixed_fma_int32_t) 11;

  /* We have to return

     2^beta * (((-1)^s * 2^U * q) + ((-1)^s * 2^J1) * (2^J2 * r))

     We start by computing

     (i)    the normal binary64   a = (-1)^s * 2^U * q
     (ii)   the normal binary64   b = (-1)^s * 2^J1
     (iii)  the normal binary64   c = 2^J2 * r
     (iv)   the normal binary64   d = 2^beta.

     We then execute

     round(d * round(a + b * c))

     with the current binary rounding mode. This rounds correctly and
     sets all necessary flags.

  */
  qq = (__mixed_fma_binary64_t) q;
  rr = (__mixed_fma_binary64_t) r;
  acst.i = ((__mixed_fma_uint64_t) (U + ((__mixed_fma_int32_t) 1023))) << 52;
  acst.i |= ((__mixed_fma_uint64_t) (!!s)) << 63;
  bcst.i = ((__mixed_fma_uint64_t) (J1 + ((__mixed_fma_int32_t) 1023))) << 52;
  bcst.i |= ((__mixed_fma_uint64_t) (!!s)) << 63;
  ccst.i = ((__mixed_fma_uint64_t) (J2 + ((__mixed_fma_int32_t) 1023))) << 52;
  dcst.i = ((__mixed_fma_uint64_t) (beta + ((__mixed_fma_int32_t) 1023))) << 52;
  a = acst.f * qq;
  b = bcst.f;
  c = ccst.f * rr;
  d = dcst.f;

  /* Perform the final rounding */
  res = d * __builtin_fma(b, c, a);
  
  /* Return the result */
  return res;
}

/* Computes round((-1)^s * 10^H * 2^-10 * q) where round() is the current
   decimal rounding to decimal64 and sets all appropriate IEEE754 flags and only the
   appropriate flags.

   If q is non-zero, the value q is guaranteed to be bounded by

   10^15 * 2^10 <= q < 10^16 * 2^10

   When q is zero, round() applies the appropriate rounding rules 
   for (-1)^s * 1.0DD - (-1)^s * 1.0DD as asked for IEEE754-2008.

*/
STATIC INLINE __mixed_fma_decimal64_t __mixed_fma_final_round_decimal(int s, __mixed_fma_int32_t H, __mixed_fma_uint64_t q) {
  volatile __mixed_fma_decimal64_t tmp;
  __mixed_fma_int32_t Z;
  __mixed_fma_decimal128_t qq, tenToZTimesFiveToTen, r;
  __mixed_fma_decimal128_caster_t tmpcst;
  __mixed_fma_decimal64_t res;
  
  /* Check if q is zero */
  if (q == ((__mixed_fma_uint64_t) 0)) {
    /* q is zero 

       Load (-1)^s on a volatile variable.
    */
    if (s) {
      tmp = -1.0DD;
    } else {
      tmp = 1.0DD;
    }
    
    /* Return (-1)^s - (-1)^s */
    return tmp - tmp;
  }

  /* Here, q is non-zero. 
     
     We have to return (-1)^s * 10^H * 2^-10 * q = (-1)^s * 10^(H - 10) * 5^10 * q.

     Let 

     Z = H - 10.

     We clamp Z into -2000 <= Z <= 2000:

     * if Z is greater than 2000, we set Z = 2000, this will lead to overflow.
     * if Z is less than -2000, we set Z = -2000, this will lead to full underflow.

     We load q onto an IEEE754 decimal128 variable, this is exact.
     We load (-1)^s * 10^Z * 5^10 onto an IEEE754 decimal128 variable, this is exact.

     We multiply (-1)^s * 10^Z * 5^10 and q on a decimal128 variable, this is still exact.

     We convert the result to a decimal64 variable, this provokes the desired rounding.

  */
  Z = H - ((__mixed_fma_int32_t) 10);
  if (Z > ((__mixed_fma_int32_t) 2000)) Z = ((__mixed_fma_int32_t) 2000);
  if (Z < ((__mixed_fma_int32_t) -2000)) Z = ((__mixed_fma_int32_t) -2000);

  /* Load q onto a decimal128 */
  qq = (__mixed_fma_decimal128_t) q; /* exact */

  /* Load (-1)^s * 10^Z * 5^10 = (-1)^s * 10^(Z + 6176 - 6176) * * 5^10 onto a decimal128 */
  tmpcst.i = (__mixed_fma_uint128_t) 9765625; /* 5^10 as an integer */
  tmpcst.i |= ((__mixed_fma_uint128_t) (Z + ((__mixed_fma_int32_t) 6176))) << 113;
  tmpcst.i |= (((__mixed_fma_uint128_t) (!!s)) << 127);
  tenToZTimesFiveToTen = tmpcst.f;

  /* Multiply q and 10^Z * 5^10 on a decimal128 */
  r = qq * tenToZTimesFiveToTen; /* exact */

  /* Perform the desired rounding by converting the decimal128 to a decimal64 

     This operation depends on the decimal rounding mode and sets the
     required flags.

  */
  res = (__mixed_fma_decimal64_t) r;

  /* Return the result */
  return res;
}

/* Computes (-1)^cs * 2^CT * 5^CF * cm = (-1)^as * 2^AT * 5^AF * am * (-1)^bs * 2^BT * 5^BF * bm 

   Input:  2^54 <= am <= 2^55 - 1 or am = 0
           2^54 <= bm <= 2^55 - 1 or bm = 0

   Output: 2^109 <= cm <= 2^110 - 1 or cm = 0

*/
STATIC INLINE void __mixed_fma_core_mult(int * RESTRICT cs, __mixed_fma_int32_t * RESTRICT CT, __mixed_fma_int32_t * RESTRICT CF, __mixed_fma_uint128_t * RESTRICT cm,
					 int as, __mixed_fma_int32_t AT, __mixed_fma_int32_t AF, __mixed_fma_uint64_t am,
					 int bs, __mixed_fma_int32_t BT, __mixed_fma_int32_t BF, __mixed_fma_uint64_t bm) {
  __mixed_fma_uint128_t tmp1, tmp2;
  __mixed_fma_int32_t tmp3;

  *cs = (!!as) ^ (!!bs);
  tmp3 = AT + BT;
  *CF = AF + BF;
  tmp1 = (__mixed_fma_uint128_t) am;
  tmp2 = (__mixed_fma_uint128_t) bm;
  tmp1 *= tmp2;
  if ((tmp1 < (((__mixed_fma_uint128_t) 1) << 109)) && (tmp1 != ((__mixed_fma_uint128_t) 0))) {
    tmp1 <<= 1;
    tmp3--;
  }
  *CT = tmp3;
  *cm = tmp1;
}

/* Tests if a binary FP number 2^C * g can be rounded correctly
   to a binary64 with any sign and rounding mode.

   Uses the magic bound beta = floor(2^64 * epsilonB / (1+epsilonB))
     
     with |epsilonB| <= 2^-58.74
     
   In input, we have 2^63 <= g <= 2^64 - 1 or g = 0.

   Returns zero if correct rounding is not possible 
   Returns a non-zero value otherwise (when rounding is possible).

   If rounding is not possible, computes f such that 

   2^C * 2^10 * 1/2 * f 

   is the binary rounding boundary closest to 2^C * g
   and 

   2^54 <= f <= 2^55 - 1.

*/
STATIC INLINE int __mixed_fma_can_round_binary(__mixed_fma_uint64_t * RESTRICT f, __mixed_fma_int32_t C, __mixed_fma_uint64_t g, __mixed_fma_int64_t beta) {
  __mixed_fma_uint128_t tmp;
  __mixed_fma_uint64_t u;
  __mixed_fma_int64_t tmp_abs;
    
  /* Compute u = nearest_int(2^-10 * g) * 2^10
     boils down to u = floor(2^-10 * (g + 2^9)) * 2^10
  */
  tmp = (__mixed_fma_uint128_t) g;
  tmp += (__mixed_fma_uint128_t) (1 << 9);
  tmp >>= 10;
  u = (__mixed_fma_uint64_t) (tmp << 10);

  /* Test if  |g - u| >= beta */
  tmp_abs = (__mixed_fma_int64_t) (g-u);
  
  if ( (tmp_abs >= beta) || (tmp_abs <= -beta)) {  

    /* In that case 2^C * g can be rounded correctly to a
       binary64 with any sign and rounding mode.
    */
    return 1;
      
  } else {
    
    /* In that case 2^C * g cannot be correctly rounded.
       Compute f the binary rounding boundary closest to 2^C * g such that
       2^54 <= f <= 2^55 - 1
     */
    *f = (__mixed_fma_uint64_t) tmp;
    return 0;

  }
}

/* Tests if a "decimal" FP number 10^H * 2^-10 * q can be rounded correctly
   to a decimal64 with any sign and rounding mode.

   Uses the magic bound beta.

   In input, we have 10^15 * 2^10 <= q < 10^16 * 2^10 or q = 0.

   Returns zero if correct rounding is not possible 
   Returns a non-zero value otherwise (when rounding is possible).

   If rounding is not possible, computes f such that 

   10^H * 2^-10 * 2^10 * 1/2 * f 

   is the decimal rounding boundary closest to 10^H * 2^-10 * q 
   and

   2 * 10^15 < f < 2*(10^16 - 1)
*/
STATIC INLINE int __mixed_fma_can_round_decimal(__mixed_fma_uint64_t * RESTRICT f, __mixed_fma_int32_t H, __mixed_fma_uint64_t q, __mixed_fma_int64_t beta) {
   __mixed_fma_uint128_t tmp;
   __mixed_fma_uint64_t w;
   __mixed_fma_int64_t tmp_abs;

  /* Compute w = nearest_int(2^-9 * q) * 2^9
     boils down to w = floor(2^-9 * (q + 2^8)) * 2^9
  */
  tmp = (__mixed_fma_uint128_t) q;
  tmp += (__mixed_fma_uint128_t) (1 << 8);
  tmp >>= 9;
  w = (__mixed_fma_uint64_t) (tmp << 9);

  /* Test if  |q - w| >= beta */
  tmp_abs = (__mixed_fma_int64_t) (q - w); 

  if ((tmp_abs >= beta) || (tmp_abs <= -beta)) {  
  
    /* In that case 10^H * 2^-10 * q can be rounded correctly to a
       decimal64 with any sign and rounding mode.
    */
    return 1;

  } else {

    /* In that case 10^H * 2^-10 * q cannot be correctly rounded.
       Compute f the binary rounding boundary closest to 10^H * 2^-10 * q such that
       2^54 <= f <= 2^55 - 1
     */
    *f = (__mixed_fma_uint64_t) tmp;
    return 0;

  }
}


/* Computes the multiplication of 
   
   an accumulator x of size __MIXED_FMA_REC_ACC_SIZE words of 64 bits
   by 
   a scalar y of size 64 bits

   such that
   
   res <- x * y mod 2^(64 * __MIXED_FMA_REC_ACC_SIZE) 
*/
STATIC INLINE void __mixed_fma_rec_mul64(__mixed_fma_uint64_t * RESTRICT res, __mixed_fma_uint64_t * RESTRICT x, __mixed_fma_uint64_t y) {
  int i;
  __mixed_fma_uint128_t X, Y, pp, c;
  __mixed_fma_uint64_t  r;

  Y = (__mixed_fma_uint128_t) y;
  c = (__mixed_fma_uint128_t) 0;
  for (i=0;i<__MIXED_FMA_REC_ACC_SIZE;i++) {
    X = (__mixed_fma_uint128_t) x[i];
    pp = X * Y + c; /* 0 <= X * Y + c <= (2^64 - 1) * (2^64 - 1) + 2^64 - 1 = 2^128 - 2 * 2^64 + 1 + 2^64 - 1 = 2^128 - 2^64 < 2^128 - 1 */
    r = (__mixed_fma_uint64_t) pp;
    c = pp >> 64;
    res[i] = r;
  }
}


/* Computes the shift to the left of 
   
   an accumulator x of size __MIXED_FMA_REC_ACC_SIZE words of 64 bits
   by 
   a scalar k 

   such that
   
   res <- x * 2^k mod 2^(64 * __MIXED_FMA_REC_ACC_SIZE) 
*/
STATIC INLINE void __mixed_fma_rec_shift(__mixed_fma_uint64_t * RESTRICT res, __mixed_fma_uint64_t * RESTRICT x, unsigned int k) {
  unsigned int kk, w, s;
  int i;
  __mixed_fma_uint64_t cin, m, r, cout;

  kk = k;
  if (kk > (__MIXED_FMA_REC_ACC_SIZE << 6)) kk = __MIXED_FMA_REC_ACC_SIZE << 6;
  w = kk >> 6;
  s = kk & ((unsigned int) 0x3f);
  for (i=0;i<((int) w);i++) {
    res[i] = (__mixed_fma_uint64_t) 0;
  }
  for (i=((int) w);i<__MIXED_FMA_REC_ACC_SIZE;i++) {
    res[i] = x[i - ((int) w)];
  }
  if (s != ((unsigned int) 0)) {
    cin = (__mixed_fma_uint64_t) 0;
    m = ((((__mixed_fma_uint64_t) 1) << s) - ((__mixed_fma_uint64_t) 1)) << (((unsigned int) 64) - s);
    for (i=0;i<__MIXED_FMA_REC_ACC_SIZE;i++) {
      r = res[i];
      cout = (r & m) >> (((unsigned int) 64) - s);
      r <<= s;
      r |= cin;
      res[i] = r;
      cin = cout;
    }
  }
}


/* Computes the addition of 
   
   an accumulator x of size __MIXED_FMA_REC_ACC_SIZE words of 64 bits
   by
   an accumulator y of size __MIXED_FMA_REC_ACC_SIZE words of 64 bits

   such that
   
   res <- (x + y) mod 2^(64 * __MIXED_FMA_REC_ACC_SIZE) 
*/
STATIC INLINE void __mixed_fma_rec_add(__mixed_fma_uint64_t * RESTRICT res, __mixed_fma_uint64_t * RESTRICT x, __mixed_fma_uint64_t * RESTRICT y) {
  int i;
  __mixed_fma_uint128_t X, Y, s, c;
  __mixed_fma_uint64_t r;
  
  c = (__mixed_fma_uint128_t) 0;
  for (i=0;i<__MIXED_FMA_REC_ACC_SIZE;i++) {
    X = (__mixed_fma_uint128_t) x[i];
    Y = (__mixed_fma_uint128_t) y[i];
    s = X + Y + c;
    r = (__mixed_fma_uint64_t) s;
    c = s >> 64;
    res[i] = r;
  }
}


/* Computes the subtraction of 
   
   an accumulator x of size __MIXED_FMA_REC_ACC_SIZE words of 64 bits
   by
   an accumulator y of size __MIXED_FMA_REC_ACC_SIZE words of 64 bits

   such that
   
   res <- (x - y) mod 2^(64 * __MIXED_FMA_REC_ACC_SIZE) 
*/
STATIC INLINE void __mixed_fma_rec_sub(__mixed_fma_uint64_t * RESTRICT res, __mixed_fma_uint64_t * RESTRICT x, __mixed_fma_uint64_t * RESTRICT y) {
  int i;
  __mixed_fma_uint128_t X, Y, d, b;
  __mixed_fma_uint64_t r;
  
  b = (__mixed_fma_uint128_t) 0;
  for (i=0;i<__MIXED_FMA_REC_ACC_SIZE;i++) {
    X = (__mixed_fma_uint128_t) x[i];
    Y = (__mixed_fma_uint128_t) y[i];
    d = X - Y - b;
    r = (__mixed_fma_uint64_t) d;
    b = (d >> 64) & ((__mixed_fma_uint128_t) 1);
    res[i] = r;
  }
}

/* Computes the multiplication of 
   
   an accumulator x of size __MIXED_FMA_REC_ACC_SIZE words of 64 bits
   by
   a scalar y of size 128 bits

   such that
   
   res <- x * y mod 2^(64 * __MIXED_FMA_REC_ACC_SIZE) 
*/
STATIC INLINE void __mixed_fma_rec_mul128(__mixed_fma_uint64_t * RESTRICT res, __mixed_fma_uint64_t * RESTRICT x, __mixed_fma_uint128_t y) {
  __mixed_fma_uint64_t yhi, ylo;
  __mixed_fma_uint64_t xylo[__MIXED_FMA_REC_ACC_SIZE];
  __mixed_fma_uint64_t xyhi[__MIXED_FMA_REC_ACC_SIZE];

  yhi = (__mixed_fma_uint64_t) (y >> 64);
  ylo = (__mixed_fma_uint64_t) y;

  __mixed_fma_rec_mul64(res, x, yhi);
  __mixed_fma_rec_mul64(xylo, x, ylo);
  
  __mixed_fma_rec_shift(xyhi, res, 64);
  __mixed_fma_rec_add(res, xyhi, xylo);
}

/* Computes the exponeniation of 5 by 
   
   a scalar k of size 32 bits

   and stores it into an accumulator of size __MIXED_FMA_REC_ACC_SIZE words of 64 bits
   
   such that
   
   res <- 5^k mod 2^(64 * __MIXED_FMA_REC_ACC_SIZE) for 0 <= k <= 786. 
*/
STATIC INLINE void __mixed_fma_rec_pow5(__mixed_fma_uint64_t * RESTRICT res, __mixed_fma_uint32_t k) {
  int i;

  for (i=0;i<__MIXED_FMA_REC_TBL_SIZE;i++) {
    res[i] = __mixed_fma_rec_tbl5[(int) k].l[i];
  }
  for (i=__MIXED_FMA_REC_TBL_SIZE;i<__MIXED_FMA_REC_ACC_SIZE;i++) {
    res[i] = (__mixed_fma_uint64_t) 0;
  }
}


/* Returns the sign of a long accumulator seen as a two's complement number */
STATIC INLINE int __mixed_fma_rec_sign(__mixed_fma_uint64_t * RESTRICT res) {
  int i;
  
  if ((res[__MIXED_FMA_REC_ACC_SIZE - 1] >> 63) != ((__mixed_fma_uint64_t) 0)) {
    return -1;
  }
  for (i=__MIXED_FMA_REC_ACC_SIZE - 1;i>=0;i--) {
    if (res[i] != ((__mixed_fma_uint64_t) 0)) {
      return 1;
    }
  }
  return 0;
}

/* Computes the sign of 

   alpha' = (-1)^sa * 2^L * 5^M * s + (-1)^sb * 2^N * 5^P * t - 2^FT * 5^FF * f

   and returns 

   -1 if alpha is negative 
    0 if alpha is zero
    1 if alpha is positive.

    The input is guaranteed to be 

    (i)    sa and sb are either 0 or 1
    (ii)  2^109  <= s  <= 2^110 - 1 or s = 0
    (iii)  2^54  <= t  <= 2^55 - 1  or t = 0
    (iv)   2^54  <= f  <= 2^55 - 1
    (v)       0  <= FT <= 2162
    (vi)      0  <= FF <= 786
    (vii)     0  <= L  <= 2282
    (viii)    0  <= M  <= 785
    (ix)      0  <= N  <= 2342
    (x)       0  <= P  <= 786

    Given the bounds on FF, M and P, we know that we need to compute 
    positive or zero integer powers of 5, i.e. 5^k, with 0 <= k <= 786.

    Further, we know that every 5^k with 0 <= k <= 786 holds on 
    ceil(log2(5^786)) = 1826 bits, hence on ceil(1826 / 64) = 29 words of 64 bits.

    Finally, each of the terms 

    (i)   2^L * 5^M * s,

    (ii)  2^N * 5^P * t and

    (iii) 2^FT * 5^FF * f

    holds on 

    (i)   ceil(log2(2^2282 * 5^786 * 2^110)) = 4218 bits
    (ii)  ceil(log2(2^2342 * 5^786 * 2^55))  = 4223 bits
    (iii) ceil(log2(2^2162 * 5^786 * 2^55))  = 4043 bits
    
    So, whatever their sign, they hold on max(4218, 4223, 4043) + 2 = 4225 bits.

    We are sure that we can store 4225 bits on 67 words of 64 bits, leaving 63 "free" bits.

*/ 
STATIC INLINE int __mixed_fma_round_recovery_core(__mixed_fma_uint32_t FT, __mixed_fma_uint32_t FF, __mixed_fma_uint64_t f,
						  int sa, __mixed_fma_uint32_t L, __mixed_fma_uint32_t M, __mixed_fma_uint128_t s,
						  int sb, __mixed_fma_uint32_t N, __mixed_fma_uint32_t P, __mixed_fma_uint128_t t) {
  __mixed_fma_uint64_t op1[__MIXED_FMA_REC_ACC_SIZE];
  __mixed_fma_uint64_t op2[__MIXED_FMA_REC_ACC_SIZE];
  __mixed_fma_uint64_t op3[__MIXED_FMA_REC_ACC_SIZE];

  /* Compute with a long accumulator
     (-1)^sa * 2^L * 5^M * s 
  */
  __mixed_fma_rec_pow5(op1, M);
  __mixed_fma_rec_mul128(op3, op1, s);
  __mixed_fma_rec_shift(op1, op3, L);
  /* TODO add sign */
  
  /* Compute with a long accumulator
     (-1)^sb * 2^N * 5^P * t 
  */
  __mixed_fma_rec_pow5(op2, P);
  __mixed_fma_rec_mul64(op3, op2, t);
  __mixed_fma_rec_shift(op2, op3, N);
  /* TODO add sign */
  
  /* Now compute the addition 
     (-1)^sa * 2^L * 5^M * s + (-1)^sb * 2^N * 5^P * t
  */
  __mixed_fma_rec_add(op3, op1, op2);

  /* Compute with a long accumulator
     2^FT * 5^FF * f 
  */
  __mixed_fma_rec_pow5(op1, FF);
  __mixed_fma_rec_mul64(op2, op1, f);
  __mixed_fma_rec_shift(op1, op2, FT);
  /* TODO add sign */
  
  /* And finally compute the subtraction 
     (-1)^sa * 2^L * 5^M * s + (-1)^sb * 2^N * 5^P * t - 2^FT * 5^FF * f
  */
  __mixed_fma_rec_sub(op2, op3, op1);

  /* Finally, we want to return the sign of op_E */
  return __mixed_fma_rec_sign(op3);
}

/* Computes the sign of 

   alpha = (-1)^sa * 2^L * 5^M * s + (-1)^sb * 2^N * 5^P * t - 2^FT * 5^FF * f

   and returns 

   -1 if alpha is negative 
    0 if alpha is zero
    1 if alpha is positive.

    The input is guaranteed to be 

    (i)    sa and sb are either 0 or 1
    (ii)   2^109 <= s  <= 2^110 - 1 or s = 0
    (iii)  2^54  <= t  <= 2^55 - 1  or t = 0
    (iv)   2^54  <= f  <= 2^55 - 1
    (v)    -1131 <= FT <= 970
    (vi)    -415 <= FF <= 370
    (vii)  -1192 <= L  <= 1090
    (viii)  -415 <= M  <= 369 
    (ix)   -1136 <= N  <= 1150
    (x)     -416 <= P  <= 370

    Given these bounds, we can deduce 

    ZMIN = min(FTmin, Lmin, Nmin) = -1192

    and

    FMIN = min(FFmin, Mmin, Pmin) = -416.

    This means that we can reduce computing the sign of 
    alpha to computing the sign of alpha' where 

    alpha = 2^ZMIN * 5^FMIN * alpha'

    and 

    alpha' = (-1)^sa * 2^L' * 5^M' * s + (-1)^sb * 2^N' * 5^P' * t - 2^FT' * 5^FF' * f

    L'  = L  - ZMIN \in \N
    M'  = M  - FMIN \in \N
    N'  = N  - ZMIN \in \N
    P'  = P  - FMIN \in \N
    FT' = FT - ZMIN \in \N
    FF' = FF - FMIN \in \N

    where 

    0 <= L'  <= 2282
    0 <= M'  <= 785
    0 <= N'  <= 2342
    0 <= P'  <= 786
    0 <= FT' <= 2162
    0 <= FF' <= 786

*/ 
STATIC INLINE int __mixed_fma_round_recovery(__mixed_fma_int32_t FT, __mixed_fma_int32_t FF, __mixed_fma_uint64_t f,
					     int sa, __mixed_fma_int32_t L, __mixed_fma_int32_t M, __mixed_fma_uint128_t s,
					     int sb, __mixed_fma_int32_t N, __mixed_fma_int32_t P, __mixed_fma_uint128_t t) {
  return __mixed_fma_round_recovery_core((__mixed_fma_uint32_t) (FT - __MIXED_FMA_ZMIN),
					 (__mixed_fma_uint32_t) (FF - __MIXED_FMA_FMIN),
					 f,
					 sa,
					 (__mixed_fma_uint32_t) (L - __MIXED_FMA_ZMIN),
					 (__mixed_fma_uint32_t) (M - __MIXED_FMA_FMIN),
					 s,
					 sb,
					 (__mixed_fma_uint32_t) (N - __MIXED_FMA_ZMIN),
					 (__mixed_fma_uint32_t) (P - __MIXED_FMA_FMIN),
					 t);
}


/* Computes s, C and g such that 

   (-1)^s * 2^C * g 

   rounds to the same binary64 as

   (-1)^as * 2^AT * 5^AF * am * (-1)^bs * 2^BT * 5^BF * bm + (-1)^cs * 2^CT * 5^CF * cm.

   In input we guarantee that as, bs, cs in {0,1} and

   2^54 <= am <= 2^55 - 1 or am = 0
   2^54 <= bm <= 2^55 - 1 or bm = 0
   2^54 <= cm <= 2^55 - 1 or cm = 0

   (however am, bm, cm are never all zero)

   and that 

   -1130 <= AT <= 969
   -1130 <= BT <= 969
   -1130 <= CT <= 969

   as well as 

   -421 <= AF <= 385
   -421 <= BF <= 385
   -421 <= CF <= 385

   In output, we guarantee that s in {0,1} and

   2^63 <= g < 2^64 

   and 

   -4174 <= C <= 3836 

*/
STATIC INLINE void __mixed_fma_core_binary(int * RESTRICT s, __mixed_fma_int32_t * RESTRICT C, __mixed_fma_uint64_t * RESTRICT g,
					   int as, __mixed_fma_int32_t AT, __mixed_fma_int32_t AF, __mixed_fma_uint64_t am,
					   int bs, __mixed_fma_int32_t BT, __mixed_fma_int32_t BF, __mixed_fma_uint64_t bm,
					   int cs, __mixed_fma_int32_t CT, __mixed_fma_int32_t CF, __mixed_fma_uint64_t cm) {

  int tmp_s, ss, alpha_s, inex1, inex2, tmp_inex;
  __mixed_fma_int32_t L, M, LAMBDA1, LAMBDA2, MU1, MU2, GAMMA1, GAMMA2, tmp_L, tmp_M, SHA1, SHA2, tmp_C, FT, FF;
  __mixed_fma_uint64_t v1, v2, omega1, omega2, tmp_g, f, beta;
  __mixed_fma_uint128_t sm, tmp_sm;
  __mixed_fma_uint256_t rho1, rho2;

  /* Multiply
     (-1)^ss * 2^L * 5^M * sm = (-1)^as * 2^AT * 5^AF * am * (-1)^bs * 2^BT * 5^BF * bm
  */
  __mixed_fma_core_mult(&ss, &L, &M, &sm, as, AT, AF, am, bs, BT, BF, bm);
  
  /* Checks whether 
       - it is an addition or a subtraction 
       and 
       - the ratio 2^L * 5^M * sm / 2^ * 5^CF * cm is between 1/2 and 2
   */
  if ((ss == cs) ||  __mixed_fma_check_ratio(L, M, sm, CT, CF, cm)) {
    /* It is an addition 
       or 
       it is a subtraction and the ratio is outside of [1/2;2] */

    /* Conversion of the result of the multiplication 2^L * 5^M * sm
       with 2^109 <= sm < 2^110 or sm = 0
       into a binary number 2^GAMMA1 * v1 * (1 + eps1_1) 
       such that 2^63 <= v1 < 2^64 or v1 = 0
       and eps bounded by abs(eps1_1) <= 2^-60.5
    */
    __mixed_fma_lower_precision_low_prec(&LAMBDA1, &MU1, &omega1, &inex2, L, M, sm);
    __mixed_fma_convert_to_binary_low_prec(&GAMMA1, &v1, &inex1, LAMBDA1, MU1, omega1, inex2);
    
    /* Conversion of the input 2^CT * 5^CF * cm
       with 2^54 <= cm <= 2^55 - 1 or cm = 0
       into a binary number 2^GAMMA2 * v2 * (1 + eps1_2)
       such that 2^63 <= v2 < 2^64 or v2 = 0
       and eps bounded by abs(eps1_2) <= 2^-60.5
    */
    __mixed_fma_increase_precision_low_prec(&LAMBDA2, &MU2, &omega2, CT, CF, cm); // exact
    __mixed_fma_convert_to_binary_low_prec(&GAMMA2, &v2, &inex2, LAMBDA2, MU2, omega2, 0);
    
    /* Far path addition
       (-1)^gs 2^C * g = (2^GAMMA1 * v1 +/- 2^GAMMA2 * v2)(1 + eps1)
       and eps bounded by abs(eps1) <= 2^-63 
    */
    __mixed_fma_far_path_addition(&tmp_C, &tmp_g, &tmp_s, &tmp_inex, GAMMA1, v1, ss, inex1, GAMMA2, v2, cs, inex2);
    
  } else {
    /* It is a subtraction and the ratio is in [1/2;2] */
  
    /* Conversion of the result of the multiplication 2^L * 5^M * sm
       with 2^109 <= sm < 2^110 or sm = 0
       into a binary number 2^SHA1 * rho1 * (1 + eps2_1)
       2^255 <= rho1 < 2^256 or rho1 = 0
       and eps bounded by abs(eps2_1) <= 2^-251
    */
    __mixed_fma_convert_to_binary_high_prec(&SHA1, &rho1, &inex1, L, M, sm);
   
    /* Conversion of the input 2^CT * 5^CF * cm
       with 2^54 <= cm <= 2^55 - 1 or cm = 0 
       into a binary number 2^SHA2 * rho2 * (1 + eps2_2)
       2^255 <= rho2 < 2^256 or rho2 = 0
       and eps2_2 bounded by abs(eps2_2) <= 2^-251     
    */
    __mixed_fma_increase_precision_high_prec(&tmp_L, &tmp_M, &tmp_sm, CT, CF, cm); // exact
    __mixed_fma_convert_to_binary_high_prec(&SHA2, &rho2, &inex2, tmp_L, tmp_M, tmp_sm);

    /* Near path addition
       (-1)^gs 2^C * g = (2^SHA1 * rho1 - 2^SHA2 * rho2)(1 + eps2)
       and eps2 bounded by abs(eps2) <= 2^
    */
    __mixed_fma_near_path_subtraction(&tmp_C, &tmp_g, &tmp_s, &tmp_inex, SHA1, &rho1, ss, inex1, SHA2, &rho2, cs, inex2);
  }
  
  /* Rounding test
     Tests if a binary FP number 2^C * g can be rounded correctly
     to a binary64 with any sign and rounding mode.
     
     Uses the magic bound beta = ceil(2^64 * epsilonB / (1+epsilonB))
     
     with |epsilonB| <= 2^-58.74
     
     In input, we have 2^63 <= g <= 2^64 - 1 or g = 0.
     
     Returns zero if correct rounding is not possible 
     Returns a non-zero value otherwise (when rounding is possible).
     
     If rounding is not possible, computes f such that 
     
     2^C * 2^10 * 1/2 * f 
     
     is the binary rounding boundary closest to 2^C * g
     and 
     
     2^54 <= f <= 2^55 -1.
  */
  beta = MIXED_FMA_INT64_CONST(39);
  if ((tmp_inex == 0) || __mixed_fma_can_round_binary(&f, tmp_C, tmp_g, beta)) {
    /* If rounding is possible */
    *s = tmp_s;
    *C = tmp_C;
    *g = tmp_g;
  } else {
    /* If rounding is not possible */
    /* Computes the sign of 
       
       alpha = (-1)^ss * 2^L * 5^M * sm + (-1)^sc * 2^CT * 5^CF * cm - 2^FT * 5^FF * f
       
       and returns 
       
       -1 if alpha is negative 
       0 if alpha is zero
       1 if alpha is positive.
       
       The input is guaranteed to be 
       
       (i)    sa and sb are either 0 or 1
       (ii)   2^109 <= s  <= 2^110 - 1 or s = 0
       (iii)  2^54  <= t  <= 2^55 - 1  or t = 0
       (iv)   2^54  <= f  <= 2^55 - 1
       (v)    -1131 <= FT <= 970
       (vi)    -415 <= FF <= 370
       (vii)  -2261 <= L  <= 1938 
       (viii)  -842 <= M  <= 770 
       (ix)   -1130 <= CT <= 969
       (x)     -421 <= CF <= 385 
    */
    FT = tmp_C+9;
    FF = 0;
    alpha_s = __mixed_fma_round_recovery(FT, FF, f, ss, L, M, sm, cs, CT, CF, cm);
    if (alpha_s == 0) {
     /* Then (-1)^ss * 2^L * 5^M * sm + (-1)^sc * 2^CT * 5^CF * cm = 2^FT * 5^FF * f */
      *s = tmp_s;
      *g = (__mixed_fma_uint64_t) (f << 10);
      *C = tmp_C;
    } else if (alpha_s < 0) {
      /* Then (-1)^ss * 2^L * 5^M * sm + (-1)^sc * 2^CT * 5^CF * cm < 2^FT * 5^FF * f */
      *s = tmp_s;
      *g = (__mixed_fma_uint64_t) (((__mixed_fma_uint64_t) (f << 10)) - beta);
      *C = tmp_C;
    } else {
      /* Then (-1)^ss * 2^L * 5^M * sm + (-1)^sc * 2^CT * 5^CF * cm > 2^FT * 5^FF * f */
      *s = tmp_s;
      *g = (__mixed_fma_uint64_t) (((__mixed_fma_uint64_t) (f << 10)) + beta);
      *C = tmp_C;
    }
  }
}

/* Computes s, H and q such that 

   (-1)^s * 10^H * 2^-10 * q 

   rounds to the same decimal64 as

   (-1)^as * 2^AT * 5^AF * am * (-1)^bs * 2^BT * 5^BF * bm + (-1)^cs * 2^CT * 5^CF * cm.

   In input we guarantee that as, bs, cs in {0,1} and

   2^54 <= am <= 2^55 - 1 or am = 0
   2^54 <= bm <= 2^55 - 1 or bm = 0
   2^54 <= cm <= 2^55 - 1 or cm = 0

   (however am, bm, cm are never all zero)

   and that 

   -1130 <= AT <= 969
   -1130 <= BT <= 969
   -1130 <= CT <= 969

   as well as 

   -421 <= AF <= 385
   -421 <= BF <= 385
   -421 <= CF <= 385

   In output, we guarantee that s in {0,1} and

   10^15 * 2^10 <= q < 10^16 * 2^10

   and 

   -929 <= H <= 1159 

*/
STATIC INLINE void __mixed_fma_core_decimal(int * RESTRICT s, __mixed_fma_int32_t * RESTRICT H, __mixed_fma_uint64_t * RESTRICT q,
					    int as, __mixed_fma_int32_t AT, __mixed_fma_int32_t AF, __mixed_fma_uint64_t am,
					    int bs, __mixed_fma_int32_t BT, __mixed_fma_int32_t BF, __mixed_fma_uint64_t bm,
					    int cs, __mixed_fma_int32_t CT, __mixed_fma_int32_t CF, __mixed_fma_uint64_t cm) {

  int tmp_s, ss, alpha_s, inex1, inex2, tmp_inex;
  __mixed_fma_int32_t L, M, LAMBDA1, LAMBDA2, MU1, MU2, GAMMA1, GAMMA2, tmp_L, tmp_M, SHA1, SHA2, tmp_C, tmp_H, FF, FT;
  __mixed_fma_uint64_t v1, v2, omega1, omega2, tmp_g, tmp_q, f, beta;
  __mixed_fma_uint128_t sm, tmp_sm;
  __mixed_fma_uint256_t rho1, rho2;

  /* Multiply
     (-1)^ss * 2^L * 5^M * sm = (-1)^as * 2^AT * 5^AF * am * (-1)^bs * 2^BT * 5^BF * bm
  */
  __mixed_fma_core_mult(&ss, &L, &M, &sm, as, AT, AF, am, bs, BT, BF, bm);
  
  /* Checks whether 
       - it is an addition or a subtraction 
       and 
       - the ratio 2^L * 5^M * sm / 2^ * 5^CF * cm is between 1/2 and 2
   */
  if ((ss == cs) ||  __mixed_fma_check_ratio(L, M, sm, CT, CF, cm)) {
    /* It is an addition 
       or 
       it is a subtraction and the ratio is outside of [1/2;2] */
        
    /* Conversion of the result of the multiplication 2^L * 5^M * sm
       with 2^109 <= sm < 2^110 or sm = 0
       into a binary number 2^GAMMA1 * v1 * (1 + eps1_1) 
       such that 2^63 <= v1 < 2^64 or v1 = 0
       and eps bounded by abs(eps1_1) <= 2^-60.5
    */
    __mixed_fma_lower_precision_low_prec(&LAMBDA1, &MU1, &omega1, &inex2, L, M, sm);
    __mixed_fma_convert_to_binary_low_prec(&GAMMA1, &v1, &inex1, LAMBDA1, MU1, omega1, inex2);
    
    /* Conversion of the input 2^CT * 5^CF * cm
       with 2^54 <= cm <= 2^55 - 1 or cm = 0
       into a binary number 2^GAMMA2 * v2 * (1 + eps1_2)
       such that 2^63 <= v2 < 2^64 or v2 = 0
       and eps bounded by abs(eps1_2) <= 2^-60.5
    */
    __mixed_fma_increase_precision_low_prec(&LAMBDA2, &MU2, &omega2, CT, CF, cm); // exact
    __mixed_fma_convert_to_binary_low_prec(&GAMMA2, &v2, &inex2, LAMBDA2, MU2, omega2, 0);
    
    /* Far path addition
       (-1)^gs 2^C * g = (2^GAMMA1 * v1 +/- 2^GAMMA2 * v2)(1 + eps1)
       and eps bounded by abs(eps1) <= 2^-63 
    */
    __mixed_fma_far_path_addition(&tmp_C, &tmp_g, &tmp_s, &tmp_inex, GAMMA1, v1, ss, inex1, GAMMA2, v2, cs, inex2);

  } else {
    /* It is a subtraction and the ratio is in [1/2;2] */
    
    /* Conversion of the result of the multiplication 2^L * 5^M * sm
       with 2^109 <= sm < 2^110 or sm = 0
       into a binary number 2^SHA1 * rho1 * (1 + eps2_1)
       2^255 <= rho1 < 2^256 or rho1 = 0
       and eps bounded by abs(eps2_1) <= 2^-251
    */
    __mixed_fma_convert_to_binary_high_prec(&SHA1, &rho1, &inex1, L, M, sm);
    
    /* Conversion of the input 2^CT * 5^CF * cm
       with 2^54 <= cm <= 2^55 - 1 or cm = 0 
       into a binary number 2^SHA2 * rho2 * (1 + eps2_2)
       2^255 <= rho2 < 2^256 or rho2 = 0
       and eps2_2 bounded by abs(eps2_2) <= 2^-251     
    */
    __mixed_fma_increase_precision_high_prec(&tmp_L, &tmp_M, &tmp_sm, CT, CF, cm); // exact
    __mixed_fma_convert_to_binary_high_prec(&SHA2, &rho2, &inex2, tmp_L, tmp_M, tmp_sm);

    /* Near path addition
       (-1)^gs 2^C * g = (2^SHA1 * rho1 - 2^SHA2 * rho2)(1 + eps2)
       and eps2 bounded by abs(eps2) <= 2^
    */
    __mixed_fma_near_path_subtraction(&tmp_C, &tmp_g, &tmp_s, &tmp_inex, SHA1, &rho1, ss, inex1, SHA2, &rho2, cs, inex2);
  }
  
  /* Computes 10^H * 2^-10 * q = 2^C * g * (1 + eps)
     and eps bounded by abs(eps) <= 2^-59.82.
  */
  __mixed_fma_convert_to_decimal(&tmp_H, &tmp_q, &inex1, tmp_C, tmp_g, tmp_inex);
  
  /* Rounding test
     Tests if a "decimal" FP number 10^H * 2^-10 * q can be rounded correctly
     to a decimal64 with any sign and rounding mode.
     
     Uses the magic bound beta = ceil(10^16 * 2^10 * epsilonD / (1 + epsilonD))
     
     with |epsilonD| < 2^-58.18

     In input, we have 10^15 * 2^10 <= q < 10^16 * 2^10 or q = 0.
     
     Returns zero if correct rounding is not possible 
     Returns a non-zero value otherwise (when rounding is possible).
     
     If rounding is not possible, computes f such that 
     
     10^H * 2^-10 * 2^10 * 1/2 * f 
     
     is the decimal rounding boundary closest to 10^H * 2^-10 * q 
     and
   
     2 * 10^15 < f < 2 * (10^16 - 1).
     
  */
  beta = MIXED_FMA_INT64_CONST(32);
  if ((inex1 == 0) || __mixed_fma_can_round_decimal(&f, tmp_H, tmp_q, beta)) {
    /* If rounding is possible */
    *s = tmp_s;
    *H = tmp_H;
    *q = tmp_q;    
  } else {
    /* If rounding is not possible */
    /* Computes the sign of 
       
       alpha = (-1)^ss * 2^L * 5^M * sm + (-1)^sc * 2^CT * 5^CF * cm - 2^FT * 5^FF * f
       
       and returns 
       
       -1 if alpha is negative 
       0 if alpha is zero
       1 if alpha is positive.
       
       The input is guaranteed to be 
       
       (i)    sa and sb are either 0 or 1
       (ii)   2^109 <= s  <= 2^110 - 1 or s = 0
       (iii)  2^54  <= t  <= 2^55 - 1  or t = 0
       (iv)   2^54  <= f  <= 2^55 - 1
       (v)    -1131 <= FT <= 970
       (vi)    -415 <= FF <= 370
       (vii)  -2261 <= L  <= 1938 
       (viii)  -842 <= M  <= 770 
       (ix)   -1130 <= CT <= 969
       (x)     -421 <= CF <= 385 
    */
    FT = tmp_H-9;
    FF = tmp_H;
    alpha_s = __mixed_fma_round_recovery(FT, FF, f, ss, L, M, sm, cs, CT, CF, cm);
    if (alpha_s == 0) {
      /* Then (-1)^ss * 2^L * 5^M * sm + (-1)^sc * 2^CT * 5^CF * cm = 2^FT * 5^FF * f */
      *s = tmp_s;
      *q = (__mixed_fma_uint64_t) (f << 9);
      *H = tmp_H;
    } else if (alpha_s < 0) {
      /* Then (-1)^ss * 2^L * 5^M * sm + (-1)^sc * 2^CT * 5^CF * cm < 2^FT * 5^FF * f */
      *s = tmp_s;
      *q = (__mixed_fma_uint64_t) (((__mixed_fma_uint64_t) (f << 9)) - beta);
      *H = tmp_H;
    } else {
      /* Then (-1)^ss * 2^L * 5^M * sm + (-1)^sc * 2^CT * 5^CF * cm > 2^FT * 5^FF * f */
      *s = tmp_s;
      *q = (__mixed_fma_uint64_t) (((__mixed_fma_uint64_t) (f << 9)) + beta);
      *H = tmp_H;
    }
  }
}

__mixed_fma_binary64_t fma_binary64_binary64_binary64_decimal64(__mixed_fma_binary64_t x, __mixed_fma_binary64_t y, __mixed_fma_decimal64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_binary64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_binary64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_decimal64(&sz, &Tz, &Fz, &mz, z);
 
  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_binary64_t) x) * ((__mixed_fma_binary64_t) y) + ((__mixed_fma_binary64_t) z);
  }
 
  __mixed_fma_core_binary(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);
  return __mixed_fma_final_round_binary(ss, EE, mm);
}


__mixed_fma_binary64_t fma_binary64_binary64_decimal64_binary64(__mixed_fma_binary64_t x, __mixed_fma_decimal64_t y, __mixed_fma_binary64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_binary64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_decimal64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_binary64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_binary64_t) x) * ((__mixed_fma_binary64_t) y) + ((__mixed_fma_binary64_t) z);
  }

  __mixed_fma_core_binary(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_binary(ss, EE, mm);
}


__mixed_fma_binary64_t fma_binary64_binary64_decimal64_decimal64(__mixed_fma_binary64_t x, __mixed_fma_decimal64_t y, __mixed_fma_decimal64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_binary64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_decimal64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_decimal64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_binary64_t) x) * ((__mixed_fma_binary64_t) y) + ((__mixed_fma_binary64_t) z);
  }

  __mixed_fma_core_binary(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_binary(ss, EE, mm);
}


__mixed_fma_binary64_t fma_binary64_decimal64_binary64_binary64(__mixed_fma_decimal64_t x, __mixed_fma_binary64_t y, __mixed_fma_binary64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_decimal64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_binary64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_binary64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_binary64_t) x) * ((__mixed_fma_binary64_t) y) + ((__mixed_fma_binary64_t) z);
  }

  __mixed_fma_core_binary(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_binary(ss, EE, mm);
}


__mixed_fma_binary64_t fma_binary64_decimal64_binary64_decimal64(__mixed_fma_decimal64_t x, __mixed_fma_binary64_t y, __mixed_fma_decimal64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_decimal64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_binary64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_decimal64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_binary64_t) x) * ((__mixed_fma_binary64_t) y) + ((__mixed_fma_binary64_t) z);
  }

  __mixed_fma_core_binary(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_binary(ss, EE, mm);
}


__mixed_fma_binary64_t fma_binary64_decimal64_decimal64_binary64(__mixed_fma_decimal64_t x, __mixed_fma_decimal64_t y, __mixed_fma_binary64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_decimal64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_decimal64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_binary64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_binary64_t) x) * ((__mixed_fma_binary64_t) y) + ((__mixed_fma_binary64_t) z);
  }

  __mixed_fma_core_binary(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_binary(ss, EE, mm);
}


__mixed_fma_binary64_t fma_binary64_decimal64_decimal64_decimal64(__mixed_fma_decimal64_t x, __mixed_fma_decimal64_t y, __mixed_fma_decimal64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_decimal64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_decimal64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_decimal64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_binary64_t) x) * ((__mixed_fma_binary64_t) y) + ((__mixed_fma_binary64_t) z);
  }

  __mixed_fma_core_binary(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_binary(ss, EE, mm);
}



__mixed_fma_decimal64_t fma_decimal64_binary64_binary64_binary64(__mixed_fma_binary64_t x, __mixed_fma_binary64_t y, __mixed_fma_binary64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_binary64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_binary64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_binary64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_decimal64_t) x) * ((__mixed_fma_decimal64_t) y) + ((__mixed_fma_decimal64_t) z);
  }

  __mixed_fma_core_decimal(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_decimal(ss, EE, mm);
}


__mixed_fma_decimal64_t fma_decimal64_binary64_binary64_decimal64(__mixed_fma_binary64_t x, __mixed_fma_binary64_t y, __mixed_fma_decimal64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_binary64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_binary64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_decimal64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_decimal64_t) x) * ((__mixed_fma_decimal64_t) y) + ((__mixed_fma_decimal64_t) z);
  }

  __mixed_fma_core_decimal(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_decimal(ss, EE, mm);
}


__mixed_fma_decimal64_t fma_decimal64_binary64_decimal64_binary64(__mixed_fma_binary64_t x, __mixed_fma_decimal64_t y, __mixed_fma_binary64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_binary64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_decimal64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_binary64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_decimal64_t) x) * ((__mixed_fma_decimal64_t) y) + ((__mixed_fma_decimal64_t) z);
  }

  __mixed_fma_core_decimal(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_decimal(ss, EE, mm);
}


__mixed_fma_decimal64_t fma_decimal64_binary64_decimal64_decimal64(__mixed_fma_binary64_t x, __mixed_fma_decimal64_t y, __mixed_fma_decimal64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;
    
  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_binary64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_decimal64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_decimal64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_decimal64_t) x) * ((__mixed_fma_decimal64_t) y) + ((__mixed_fma_decimal64_t) z);
  }

  __mixed_fma_core_decimal(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_decimal(ss, EE, mm);
}


__mixed_fma_decimal64_t fma_decimal64_decimal64_binary64_binary64(__mixed_fma_decimal64_t x, __mixed_fma_binary64_t y, __mixed_fma_binary64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_decimal64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_binary64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_binary64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_decimal64_t) x) * ((__mixed_fma_decimal64_t) y) + ((__mixed_fma_decimal64_t) z);
  }

  __mixed_fma_core_decimal(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_decimal(ss, EE, mm);
}


__mixed_fma_decimal64_t fma_decimal64_decimal64_binary64_decimal64(__mixed_fma_decimal64_t x, __mixed_fma_binary64_t y, __mixed_fma_decimal64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_decimal64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_binary64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_decimal64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_decimal64_t) x) * ((__mixed_fma_decimal64_t) y) + ((__mixed_fma_decimal64_t) z);
  }

  __mixed_fma_core_decimal(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_decimal(ss, EE, mm);
}


__mixed_fma_decimal64_t fma_decimal64_decimal64_decimal64_binary64(__mixed_fma_decimal64_t x, __mixed_fma_decimal64_t y, __mixed_fma_binary64_t z) {
  int spx, spy, spz;
  int sx, sy, sz;
  __mixed_fma_int32_t Tx, Fx, Ty, Fy, Tz, Fz;
  __mixed_fma_uint64_t mx, my, mz;
  int ss;
  __mixed_fma_int32_t EE;
  __mixed_fma_uint64_t mm;

  mx = (__mixed_fma_uint64_t) 0;
  my = (__mixed_fma_uint64_t) 0;
  mz = (__mixed_fma_uint64_t) 0;
  
  spx = __mixed_fma_decompose_decimal64(&sx, &Tx, &Fx, &mx, x);
  spy = __mixed_fma_decompose_decimal64(&sy, &Ty, &Fy, &my, y);
  spz = __mixed_fma_decompose_binary64(&sz, &Tz, &Fz, &mz, z);

  if ((spx || spy || spz) ||
      ((mx == ((__mixed_fma_uint64_t) 0)) &&
       (my == ((__mixed_fma_uint64_t) 0)) &&
       (mz == ((__mixed_fma_uint64_t) 0)))) {
    /* IEEE754-2008 with conversions does The Right Thing* for Inf, NaN or Zero */
    return ((__mixed_fma_decimal64_t) x) * ((__mixed_fma_decimal64_t) y) + ((__mixed_fma_decimal64_t) z);
  }

  __mixed_fma_core_decimal(&ss, &EE, &mm, sx, Tx, Fx, mx, sy, Ty, Fy, my, sz, Tz, Fz, mz);

  return __mixed_fma_final_round_decimal(ss, EE, mm);
}


