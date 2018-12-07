
#include "reference.h"
#include <stdint.h>
#include <gmp.h>

typedef _Decimal128    __ref_decimal128_t;

typedef union {
  uint64_t            i;
  __ref_binary64_t    f;
} __ref_binary64_caster_t;

typedef union {
  __uint128_t         i;
  __ref_decimal128_t  f;
} __ref_decimal128_caster_t;


static inline void __ref_uint64_to_mpz(mpz_t res, uint64_t x) {
  int i;
  uint64_t r;
  unsigned long int a;
  mpz_t A;

  mpz_init(A);
  mpz_set_ui(res,0u);
  for (i=0,r=x;i<4;i++,r>>=16) {
    a = (unsigned long int) (r & ((uint64_t) 0xffff));
    mpz_set_ui(A, a);
    mpz_mul_2exp(A, A, 16*i);
    mpz_add(res, res, A);
  }
  mpz_clear(A);
}

static inline void __ref_uint64_to_mpq(mpq_t res, uint64_t x) {
  mpz_t tmp;

  mpz_init(tmp);
  __ref_uint64_to_mpz(tmp, x);
  mpq_set_z(res, tmp);
  mpz_clear(tmp);
}

static inline void __ref_pow_to_mpq(mpq_t res, unsigned long int radix, long int exponent) {
  unsigned long int E;

  mpq_set_si(res, 1, 1u);
  if (exponent < ((long int) 0)) {
    /* exponent <= -1 */
    E = ((unsigned long int) (-(exponent + ((long int) 1)))) + ((unsigned long int) 1);
    mpz_ui_pow_ui(mpq_denref(res), radix, E);
  } else {
    /* exponent >= 0 */
    E = (unsigned long int) exponent;
    mpz_ui_pow_ui(mpq_numref(res), radix, E);
  }
}

static inline int __ref_binary64_to_mpq(mpq_t res, __ref_binary64_t x) {
  int E, s;
  __ref_binary64_t m, X;
  uint64_t M;
  mpq_t expo, mq;
  
  /* Check for NaN */
  if (!(x == x)) {
    return 1;
  }

  /* Check for Inf */
  if ((x > 0x1.fffffffffffffp1023) || (x < -0x1.fffffffffffffp1023)) {
    return 1;
  }

  /* Check for zero */
  if (x == 0.0) {
    mpq_set_si(res, 0, 1u);
    return 0;
  }

  /* Here, x is a non-zero finite binary64 FP number 
     
     Start by stripping off the sign.

  */
  if (x < 0.0) {
    s = 1;
    X = -x;
  } else {
    s = 0;
    X = x;
  }

  /* Set x = 2^E * m and normalize m into 2^52 <= m < 2^53 */
  for (m=X,E=0;m<0x1p52;m*=2.0,E--);
  for (;m>=0x1p53;m*=0.5,E++);

  /* A binary64 number m s.t. 2^52 <= m < 2^53 is an integer */
  M = (uint64_t) m;
 
  /* Get M represented as an mpq_t */
  mpq_init(mq);
  __ref_uint64_to_mpq(mq, M);

  /* Integrate sign */
  if (s) {
    mpq_neg(mq,mq);
  }
  
  /* Get 2^E represented as an mpq_t */
  mpq_init(expo);
  __ref_pow_to_mpq(expo, 2u, E);
  
  /* Set result */
  mpq_mul(res, expo, mq);

  /* Clear temporaries */
  mpq_clear(expo);
  mpq_clear(mq);

  /* Signal that we had a finite number */
  return 0;
}

static inline int __ref_decimal64_to_mpq(mpq_t res, __ref_decimal64_t x) {
  int E, s;
  __ref_decimal64_t m, X;
  uint64_t M;
  mpq_t expo, mq;
  
  /* Check for NaN */
  if (!(x == x)) {
    return 1;
  }

  /* Check for Inf */
  if ((x > 9.999999999999999E384DD) || (x < -9.999999999999999E384DD)) {
    return 1;
  }

  /* Check for zero */
  if (x == 0.0DD) {
    mpq_set_si(res, 0, 1u);
    return 0;
  }

  /* Here, x is a non-zero finite decimal64 FP number 
     
     Start by stripping off the sign.

  */
  if (x < 0.0DD) {
    s = 1;
    X = -x;
  } else {
    s = 0;
    X = x;
  }

  /* Set x = 10^E * m and normalize m into 10^15 <= m < 10^16 */
  for (m=X,E=0;m<1000000000000000.0DD;m*=10.0DD,E--);
  for (;m>=10000000000000000.0DD;m*=0.1DD,E++);

  /* A decimal64 number m s.t. 10^15 <= m < 10^16 is an integer */
  M = (uint64_t) m;

  /* Get M represented as an mpq_t */
  mpq_init(mq);
  __ref_uint64_to_mpq(mq, M);

  /* Integrate sign */
  if (s) {
    mpq_neg(mq,mq);
  }
  
  /* Get 10^E represented as an mpq_t */
  mpq_init(expo);
  __ref_pow_to_mpq(expo, 10u, E);

  /* Set result */
  mpq_mul(res, expo, mq);
  
  /* Clear temporaries */
  mpq_clear(expo);
  mpq_clear(mq);

  /* Signal that we had a finite number */
  return 0;
}

static inline void __ref_mpq_to_sign_expo_mant_in_radix_and_prec(int *s, int *E, mpq_t m, mpq_t x, unsigned long int radix, int prec) {
  mpq_t X, mm, r, rr, ahi, alo;
  long int expo;

  /* Check for special case zero */
  if (mpq_sgn(x) == 0) {
    *s = 0;
    *E = 0;
    mpq_set(m, x);
    return;
  }
  
  /* Strip off sign */
  mpq_init(X);
  if (mpq_sgn(x) < 0) {
    *s = 1;
    mpq_neg(X, x);
  } else {
    *s = 0;
    mpq_set(X, x);
  }
  
  /* Get a first idea of the exponent */
  expo = ((long int) mpz_sizeinbase(mpq_numref(X), (int) radix)) - ((long int) mpz_sizeinbase(mpq_denref(X), (int) radix));
  
  /* Compute mm = radix^(-expo) * X */
  mpq_init(mm);
  __ref_pow_to_mpq(mm, radix, -expo);
  mpq_mul(mm, mm, X);

  /* Here, we have X = radix^expo * mm 

     Normalize mm into radix^(prec - 1) <= mm < radix^prec

     Start by computing 

     alo = radix^(prec - 1) and
     ahi = radix^prec

  */
  mpq_init(alo);
  mpq_init(ahi);
  mpq_init(r);
  mpq_init(rr);
  __ref_pow_to_mpq(alo, radix, (long int) (prec - 1));
  __ref_pow_to_mpq(ahi, radix, (long int) prec);
  __ref_pow_to_mpq(r, radix, (long int) 1);
  __ref_pow_to_mpq(rr, radix, (long int) -1);

  while (mpq_cmp(mm,alo) < 0) {
    mpq_mul(mm, mm, r);
    expo--;
  }
  while (mpq_cmp(mm,ahi) >= 0) {
    mpq_mul(mm, mm, rr);
    expo++;
  }

  mpq_clear(rr);
  mpq_clear(r);
  mpq_clear(ahi);
  mpq_clear(alo);

  /* Now we have X = radix^expo * mm with radix^(prec - 1) <= mm < radix^prec 

     Set the output values.

  */
  *E = (int) expo;
  mpq_set(m, mm);

  /* Clear the temporaries */
  mpq_clear(mm);
  mpq_clear(X);
}

static inline uint64_t __ref_mpz_to_uint64(mpz_t x) {
  uint64_t l, res;
  mpz_t tmp, q, r;
  int i;
  
  /* Check if x is negative or zero. Return zero in this case */
  if (mpz_sgn(x) <= 0) {
    return (uint64_t) 0;
  }

  /* Get the largest admissible value for a uint64_t (2^64 - 1) */
  l = (uint64_t) 0;
  l--;
  mpz_init(tmp);
  __ref_uint64_to_mpz(tmp, l);

  /* If x is larger than 2^64 - 1, return 2^64 - 1 */
  if (mpz_cmp(x, tmp) > 0) {
    mpz_clear(tmp);
    return l;
  }

  /* Here, we know that 0 <= x <= 2^64 - 1 

     Copy x into tmp, then cut tmp into four pieces of 16 bits.

  */
  mpz_set(tmp, x);
  mpz_init(q);
  mpz_init(r);
  res = (uint64_t) 0;
  for (i=0;i<4;i++) {
    mpz_tdiv_q_2exp(q, tmp, 16);
    mpz_tdiv_r_2exp(r, tmp, 16);
    res += (((uint64_t) 1) << (16*i)) * ((uint64_t) (mpz_get_ui(r)));
    mpz_set(tmp, q);
  }
  mpz_clear(r);
  mpz_clear(q);
  mpz_clear(tmp);

  /* Return the result */
  return res;
}

static inline unsigned int __ref_lzc64(uint64_t a) {
  unsigned int r;
  uint64_t t;
  if ((a) == ((uint64_t) 0)) return (unsigned int) 64;
  if (sizeof(uint64_t) == sizeof(unsigned int)) {
    return (unsigned int) __builtin_clz(a);
  } else {
    if (sizeof(uint64_t) == sizeof(unsigned long)) {
      return (unsigned int) __builtin_clzl(a);
    } else {
      if (sizeof(uint64_t) == sizeof(unsigned long long)) {
	return (unsigned int) __builtin_clzll(a);
      } else {
	for (r=0, t=a; ((t & (((uint64_t) 1) << 63)) == ((uint64_t) 0)); r++, t<<=1);
	return r;
      }
    }
  }
  return (unsigned int) 0;
}

/* Rounds (-1)^s * 2^E * m to a binary64 */
static inline __ref_binary64_t __ref_round_to_binary64(int s, int E, uint64_t m) {
  volatile __ref_binary64_t tmp;
  int32_t Z, T, U, beta, J, J1, J2, C;
  uint64_t q, r, g;
  unsigned int alpha, lzc;
  __ref_binary64_t a, b, c, d, res, qq, rr;
  __ref_binary64_caster_t acst, bcst, ccst, dcst;

  /* Set C = E, g = m */
  C = (int32_t) E;
  g = m;
  
  /* Check if g is zero */
  if (g == ((uint64_t) 0)) {
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
  if (Z > ((int32_t) 1200)) Z = (int32_t) 1200;
  if (Z < ((int32_t) -1200)) Z = (int32_t) -1200;

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
  T = Z + ((int32_t) 11);
  if (T < ((int32_t) -1074)) T = (int32_t) -1074;
  alpha = (unsigned int) (T - Z);
  if (alpha > 63) {
    q = (uint64_t) 0;
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
  if (U > ((int32_t) 800)) U = (int32_t) 800;
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
  lzc = __ref_lzc64(r);
  r <<= lzc;
  J2 -= (int32_t) lzc;

  /* Now divide r by 2^11, integrating possible sticky bits. Adjust J2 again.

     After this operation, we are sure that 2^J2 * r holds on a normal binary64.

  */
  r = (r >> 11) | ((uint64_t) (!!((r - ((r >> 11) << 11)) != ((uint64_t) 0))));
  J2 += (int32_t) 11;

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
  qq = (__ref_binary64_t) q;
  rr = (__ref_binary64_t) r;
  acst.i = ((uint64_t) (U + ((int32_t) 1023))) << 52;
  acst.i |= ((uint64_t) (!!s)) << 63;
  bcst.i = ((uint64_t) (J1 + ((int32_t) 1023))) << 52;
  bcst.i |= ((uint64_t) (!!s)) << 63;
  ccst.i = ((uint64_t) (J2 + ((int32_t) 1023))) << 52;
  dcst.i = ((uint64_t) (beta + ((int32_t) 1023))) << 52;
  a = acst.f * qq;
  b = bcst.f;
  c = ccst.f * rr;
  d = dcst.f;

  /* Perform the final rounding */
  res = d * __builtin_fma(b, c, a);
  
  /* Return the result */
  return res;
}
 
/* Rounds (-1)^s * 10^E * 2^-10 * m to a decimal64 */
static inline __ref_decimal64_t __ref_round_to_decimal64(int s, int E, uint64_t m) {
  volatile __ref_decimal64_t tmp;
  int32_t H, Z;
  __ref_decimal128_t qq, tenToZTimesFiveToTen, r;
  __ref_decimal128_caster_t tmpcst;
  __ref_decimal64_t res;
  uint64_t q;

  /* Set H = E, q = m */
  H = (int32_t) E;
  q = m;
  
  /* Check if q is zero */
  if (q == ((uint64_t) 0)) {
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
  Z = H - ((int32_t) 10);
  if (Z > ((int32_t) 2000)) Z = ((int32_t) 2000);
  if (Z < ((int32_t) -2000)) Z = ((int32_t) -2000);

  /* Load q onto a decimal128 */
  qq = (__ref_decimal128_t) q; /* exact */

  /* Load (-1)^s * 10^Z * 5^10 = (-1)^s * 10^(Z + 6176 - 6176) * * 5^10 onto a decimal128 */
  tmpcst.i = (__uint128_t) 9765625; /* 5^10 as an integer */
  tmpcst.i |= ((__uint128_t) (Z + ((int32_t) 6176))) << 113;
  tmpcst.i |= (((__uint128_t) (!!s)) << 127);
  tenToZTimesFiveToTen = tmpcst.f;

  /* Multiply q and 10^Z * 5^10 on a decimal128 */
  r = qq * tenToZTimesFiveToTen; /* exact */

  /* Perform the desired rounding by converting the decimal128 to a decimal64 

     This operation depends on the decimal rounding mode and sets the
     required flags.

  */
  res = (__ref_decimal64_t) r;

  /* Return the result */
  return res;
}
  
static inline __ref_binary64_t __ref_mpq_to_binary64(mpq_t x) {
  int s, E;
  mpq_t m, twoToTheEleven;
  mpz_t mm, r;
  uint64_t M;

  /* Decompose x into (-1)^s * 2^E * m = x, where 2^52 <= m < 2^53, m a rational! */
  mpq_init(m);
  __ref_mpq_to_sign_expo_mant_in_radix_and_prec(&s, &E, m, x, 2, 53);

  /* Multiply m by 2^11, so that we have x = (-1)^s * 2^E * 2^-11 * m */
  mpq_init(twoToTheEleven);
  __ref_pow_to_mpq(twoToTheEleven, 2u, (long int) 11);
  mpq_mul(m, m, twoToTheEleven);
  mpq_clear(twoToTheEleven);
  E -= 11;
  
  /* Suppose m is m = p/q. Compute two integers mm and r such that 

     mm = floor(p/q) and r = p - mm * q 
     
     We are sure that 2^52 * 2^11 <= mm < 2^53 * 2^11 = 2^64.

  */
  mpz_init(mm);
  mpz_init(r);
  mpz_fdiv_qr(mm, r, mpq_numref(m), mpq_denref(m));

  /* Get M = mm as a uint64_t */
  M = __ref_mpz_to_uint64(mm);

  /* If r is non-zero, set a sticky bit in M */
  if (mpz_sgn(r) != 0) {
    M |= (uint64_t) 1;
  }

  /* Clear the temporaries */
  mpz_clear(r);
  mpz_clear(mm);
  mpq_clear(m);
  
  /* Now we know that (-1)^s * 2^E * M rounds to the same binary64 as x */
  return __ref_round_to_binary64(s, E, M);
}

static inline __ref_decimal64_t __ref_mpq_to_decimal64(mpq_t x) {
  int s, E;
  mpq_t m, twoToTheTen;
  mpz_t mm, r;
  uint64_t M;

  /* Decompose x into (-1)^s * 10^E * m = x, where 10^15 <= m < 10^16, m a rational! */
  mpq_init(m);
  __ref_mpq_to_sign_expo_mant_in_radix_and_prec(&s, &E, m, x, 10, 16);
  
  /* Multiply m by 2^10, so that we have x = (-1)^s * 10^E * 2^-10 * m */
  mpq_init(twoToTheTen);
  __ref_pow_to_mpq(twoToTheTen, 2u, (long int) 10);
  mpq_mul(m, m, twoToTheTen);
  mpq_clear(twoToTheTen);

  /* Suppose m is m = p/q. Compute two integers mm and r such that 

     mm = floor(p/q) and r = p - mm * q 
     
     We are sure that 10^15 * 2^10 <= mm < 10^16 * 2^10 < 2^64.

  */
  mpz_init(mm);
  mpz_init(r);
  mpz_fdiv_qr(mm, r, mpq_numref(m), mpq_denref(m));

  /* Get M = mm as a uint64_t */
  M = __ref_mpz_to_uint64(mm);

  /* If r is non-zero, set a sticky bit in M */
  if (mpz_sgn(r) != 0) {
    M |= (uint64_t) 1;
  }

  /* Clear the temporaries */
  mpz_clear(r);
  mpz_clear(mm);
  mpq_clear(m);
  
  /* Now we know that (-1)^s * 10^E * 2^-10 * M rounds to the same decimal64 as x */
  return __ref_round_to_decimal64(s, E, M);
}

static inline void __ref_fma_core(mpq_t res, mpq_t x, mpq_t y, mpq_t z) {
  mpq_mul(res, x, y);
  mpq_add(res, res, z);
}

__ref_binary64_t ref_fma_binary64_binary64_binary64_decimal64(__ref_binary64_t x, __ref_binary64_t y, __ref_decimal64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_binary64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_binary64_to_mpq(X, x);
  sy = __ref_binary64_to_mpq(Y, y);
  sz = __ref_decimal64_to_mpq(Z, z);
  
  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_binary64_t) x) * ((__ref_binary64_t) y) + ((__ref_binary64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);

  r = __ref_mpq_to_binary64(R);

  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_binary64_t ref_fma_binary64_binary64_decimal64_binary64(__ref_binary64_t x, __ref_decimal64_t y, __ref_binary64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_binary64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_binary64_to_mpq(X, x);
  sy = __ref_decimal64_to_mpq(Y, y);
  sz = __ref_binary64_to_mpq(Z, z);

  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_binary64_t) x) * ((__ref_binary64_t) y) + ((__ref_binary64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_binary64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_binary64_t ref_fma_binary64_binary64_decimal64_decimal64(__ref_binary64_t x, __ref_decimal64_t y, __ref_decimal64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_binary64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_binary64_to_mpq(X, x);
  sy = __ref_decimal64_to_mpq(Y, y);
  sz = __ref_decimal64_to_mpq(Z, z);

  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_binary64_t) x) * ((__ref_binary64_t) y) + ((__ref_binary64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_binary64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_binary64_t ref_fma_binary64_decimal64_binary64_binary64(__ref_decimal64_t x, __ref_binary64_t y, __ref_binary64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_binary64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_decimal64_to_mpq(X, x);
  sy = __ref_binary64_to_mpq(Y, y);
  sz = __ref_binary64_to_mpq(Z, z);

  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_binary64_t) x) * ((__ref_binary64_t) y) + ((__ref_binary64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_binary64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_binary64_t ref_fma_binary64_decimal64_binary64_decimal64(__ref_decimal64_t x, __ref_binary64_t y, __ref_decimal64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_binary64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_decimal64_to_mpq(X, x);
  sy = __ref_binary64_to_mpq(Y, y);
  sz = __ref_decimal64_to_mpq(Z, z);

  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_binary64_t) x) * ((__ref_binary64_t) y) + ((__ref_binary64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_binary64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_binary64_t ref_fma_binary64_decimal64_decimal64_binary64(__ref_decimal64_t x, __ref_decimal64_t y, __ref_binary64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_binary64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_decimal64_to_mpq(X, x);
  sy = __ref_decimal64_to_mpq(Y, y);
  sz = __ref_binary64_to_mpq(Z, z);

  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_binary64_t) x) * ((__ref_binary64_t) y) + ((__ref_binary64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_binary64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_binary64_t ref_fma_binary64_decimal64_decimal64_decimal64(__ref_decimal64_t x, __ref_decimal64_t y, __ref_decimal64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_binary64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_decimal64_to_mpq(X, x);
  sy = __ref_decimal64_to_mpq(Y, y);
  sz = __ref_decimal64_to_mpq(Z, z);

  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_binary64_t) x) * ((__ref_binary64_t) y) + ((__ref_binary64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_binary64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_decimal64_t ref_fma_decimal64_binary64_binary64_binary64(__ref_binary64_t x, __ref_binary64_t y, __ref_binary64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_decimal64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_binary64_to_mpq(X, x);
  sy = __ref_binary64_to_mpq(Y, y);
  sz = __ref_binary64_to_mpq(Z, z);

  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_decimal64_t) x) * ((__ref_decimal64_t) y) + ((__ref_decimal64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_decimal64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_decimal64_t ref_fma_decimal64_binary64_binary64_decimal64(__ref_binary64_t x, __ref_binary64_t y, __ref_decimal64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_decimal64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_binary64_to_mpq(X, x);
  sy = __ref_binary64_to_mpq(Y, y);
  sz = __ref_decimal64_to_mpq(Z, z);

  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_decimal64_t) x) * ((__ref_decimal64_t) y) + ((__ref_decimal64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_decimal64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_decimal64_t ref_fma_decimal64_binary64_decimal64_binary64(__ref_binary64_t x, __ref_decimal64_t y, __ref_binary64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_decimal64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_binary64_to_mpq(X, x);
  sy = __ref_decimal64_to_mpq(Y, y);
  sz = __ref_binary64_to_mpq(Z, z);

  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_decimal64_t) x) * ((__ref_decimal64_t) y) + ((__ref_decimal64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_decimal64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_decimal64_t ref_fma_decimal64_binary64_decimal64_decimal64(__ref_binary64_t x, __ref_decimal64_t y, __ref_decimal64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_decimal64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_binary64_to_mpq(X, x);
  sy = __ref_decimal64_to_mpq(Y, y);
  sz = __ref_decimal64_to_mpq(Z, z);

  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_decimal64_t) x) * ((__ref_decimal64_t) y) + ((__ref_decimal64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_decimal64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_decimal64_t ref_fma_decimal64_decimal64_binary64_binary64(__ref_decimal64_t x, __ref_binary64_t y, __ref_binary64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_decimal64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_decimal64_to_mpq(X, x);
  sy = __ref_binary64_to_mpq(Y, y);
  sz = __ref_binary64_to_mpq(Z, z);

  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_decimal64_t) x) * ((__ref_decimal64_t) y) + ((__ref_decimal64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_decimal64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_decimal64_t ref_fma_decimal64_decimal64_binary64_decimal64(__ref_decimal64_t x, __ref_binary64_t y, __ref_decimal64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_decimal64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_decimal64_to_mpq(X, x);
  sy = __ref_binary64_to_mpq(Y, y);
  sz = __ref_decimal64_to_mpq(Z, z);

  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_decimal64_t) x) * ((__ref_decimal64_t) y) + ((__ref_decimal64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_decimal64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}

__ref_decimal64_t ref_fma_decimal64_decimal64_decimal64_binary64(__ref_decimal64_t x, __ref_decimal64_t y, __ref_binary64_t z) {
  mpq_t X, Y, Z, R;
  int sx, sy, sz;
  __ref_decimal64_t r;

  mpq_init(X);
  mpq_init(Y);
  mpq_init(Z);
  sx = __ref_decimal64_to_mpq(X, x);
  sy = __ref_decimal64_to_mpq(Y, y);
  sz = __ref_binary64_to_mpq(Z, z);
  
  if ((sx || sy || sz) || ((mpq_sgn(X) == 0) && (mpq_sgn(Y) == 0) && (mpq_sgn(Z) == 0))) {
    mpq_clear(Z);
    mpq_clear(Y);
    mpq_clear(X);
    return ((__ref_decimal64_t) x) * ((__ref_decimal64_t) y) + ((__ref_decimal64_t) z);
  }

  mpq_init(R);
  __ref_fma_core(R, X, Y, Z);
  r = __ref_mpq_to_decimal64(R);
  mpq_clear(R);
  mpq_clear(Z);
  mpq_clear(Y);
  mpq_clear(X);
  
  return r;
}


