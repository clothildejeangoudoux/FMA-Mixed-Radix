#include "../src/fma_impl.c"
#include "fma_tests.h"

#include <assert.h>
#include <time.h>

#include "tables/lower_precision_low_prec_tables.h"
#include "tables/increase_precision_low_prec_tables.h"
#include "tables/floor_times_log2_tables.h"
#include "tables/check_ratio.h"

/* Decomposes a binary64 x into a 

   sign s
   an exponent E
   a significand m

   such that x = (-1)^s * 2^E * m 

   with 2^54 <= m < 2^55 or m = 0 and E = 0

   and returns a zero value

   or does not touch s, E and m and returns 
   a non-zero value if x is an infinity or a NaN.
*/
STATIC INLINE void __TEST__mixed_fma_decompose_binary64() {
  int s;
  __mixed_fma_int32_t E;
  __mixed_fma_uint64_t m;

  __mixed_fma_binary64_t x;
   
   /* STUB */
}

STATIC INLINE void __TEST__mixed_fma_lzc64() {
  __mixed_fma_uint64_t a;
  /* STUB */
}

STATIC INLINE void __TEST__mixed_fma_lzc128() {
  __mixed_fma_uint128_t a;
  /* STUB */
}

STATIC INLINE void __TEST__mixed_fma_lzc256() {
  __mixed_fma_uint256_t a;
  /* STUB */
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
STATIC INLINE void __TEST__mixed_fma_decompose_decimal64() {
  int s;
  __mixed_fma_int32_t J, K;
  __mixed_fma_uint64_t n;

  __mixed_fma_decimal64_t x;

  /* STUB */
}

/* /\* Computes 2^LAMBDA * 5^MU * omega = 2^L * 5^M * s * (1 + eps) */

/*    with s such that 2^109 <= s < 2^110 or s = 0 */
   
/*    and 2^63 <= omega < 2^64 or omega = 0 */

/*    and eps bounded by abs(eps) <= 2^-63. */

/*    We ensure that L, M, LAMBDA and MU satisfy */

/*    -2261 <= L <= 1938 */
/*    -842 <= M <= 770 */
/*    -2215 <= LAMBDA <= 1984 */
/*    -842 <= MU <= 770 */

/* *\/ */
/* STATIC INLINE void __TEST__mixed_fma_lower_precision_low_prec() { */
/*    printf("TEST __mixed_fma_increase_precision_low_prec\t"); */
/*   __mixed_fma_int32_t LAMBDA, MU; */
/*   __mixed_fma_uint64_t omega; */

/*   __mixed_fma_int32_t L, M; */
/*   __mixed_fma_uint128_t s; */

  
/*   __mixed_fma_uint128_t i; */
/*   __mixed_fma_uint128_t LOWER_PRECISION_LOW_PREC_S_MIN = ((__mixed_fma_uint128_t) 1) << 109; */
/*   __mixed_fma_uint128_t LOWER_PRECISION_LOW_PREC_S_MAX = (((__mixed_fma_uint128_t) 1) << 110) - ((__mixed_fma_uint128_t) 1); */
/*   __mixed_fma_uint64_t LOWER_PRECISION_LOW_PREC_OMEGA_MIN = ((__mixed_fma_uint64_t) 1) << 63; */
/*   __mixed_fma_uint64_t LOWER_PRECISION_LOW_PREC_OMEGA_MAX = (__mixed_fma_uint128_t) ((((__mixed_fma_uint128_t) 1) << 64) - ((__mixed_fma_uint128_t) 1)); */
/*   int i0, j, k; */

/*   for (j=LOWER_PRECISION_LOW_PREC_L_MIN;j<=LOWER_PRECISION_LOW_PREC_L_MAX;j++) { */
/*     for (k=LOWER_PRECISION_LOW_PREC_M_MIN;k<=LOWER_PRECISION_LOW_PREC_M_MAX;k++) { */
/*       i0=0; */
/*       for (i=(__mixed_fma_uint128_t)LOWER_PRECISION_LOW_PREC_S_MIN;i<=LOWER_PRECISION_LOW_PREC_S_MAX;i+=(((__mixed_fma_uint128_t) 1) << 105)) { */
/* 	__mixed_fma_lower_precision_low_prec(&LAMBDA,&MU,&omega,j,k,i); */

/* 	assert(LAMBDA==__mixed_fma_lower_precision_low_prec_tbl_lambda[j-LOWER_PRECISION_LOW_PREC_L_MIN]); */
/* 	assert(LAMBDA>=LOWER_PRECISION_LOW_PREC_LAMBDA_MIN); */
/* 	assert(LAMBDA<=LOWER_PRECISION_LOW_PREC_LAMBDA_MAX); */
/* 	assert(MU==k); */
/*       	assert(MU>=LOWER_PRECISION_LOW_PREC_MU_MIN); */
/*       	assert(MU<=LOWER_PRECISION_LOW_PREC_MU_MAX); */
/* 	assert(omega==__mixed_fma_lower_precision_low_prec_tbl_omega[i0]); */
/* 	assert(omega>=LOWER_PRECISION_LOW_PREC_OMEGA_MIN); */
/*       	assert(omega<=LOWER_PRECISION_LOW_PREC_OMEGA_MAX); */
/* 	i0++; */
/*       } */
/*     } */
/*   } */

/*   printf("\tPASS\n"); */
/* } */


/* Computes 2^LAMBDA * 5^MU * omega = 2^N * 5^P * t

   with t such that 2^54 <= t < 2^55 or t = 0
   
   and 2^63 <= omega < 2^64 or omega = 0.

   We ensure that N, P, LAMBDA and MU satisfy

   -1130 <= N <= 969
   -421 <= P <= 385
   -2215 <= LAMBDA <= 1984
   -842 <= MU <= 770

*/
STATIC INLINE void __TEST__mixed_fma_increase_precision_low_prec() {
  printf("TEST __mixed_fma_increase_precision_low_prec\t");
  __mixed_fma_int32_t LAMBDA, MU;
  __mixed_fma_uint64_t omega;
  
  __mixed_fma_int32_t N, P;
  __mixed_fma_uint64_t t;

  __mixed_fma_uint64_t i, i0;
  int j, k;

   for (j=INCREASE_PRECISION_LOW_PREC_N_MIN;j<=INCREASE_PRECISION_LOW_PREC_N_MAX;j++) {
    for (k=INCREASE_PRECISION_LOW_PREC_P_MIN;k<=INCREASE_PRECISION_LOW_PREC_P_MAX;k++) {
      i0=0;
      for (i=INCREASE_PRECISION_LOW_PREC_T_MIN;i<=INCREASE_PRECISION_LOW_PREC_T_MAX;i+=(((__mixed_fma_uint64_t) 1) << 50)) {
	__mixed_fma_increase_precision_low_prec(&LAMBDA,&MU,&omega,j,k,i);
	
	assert(LAMBDA==__mixed_fma_increase_precision_low_prec_tbl_lambda[j-INCREASE_PRECISION_LOW_PREC_N_MIN]);
	assert(LAMBDA>=INCREASE_PRECISION_LOW_PREC_LAMBDA_MIN);
	assert(LAMBDA<=INCREASE_PRECISION_LOW_PREC_LAMBDA_MAX);
	assert(MU==k);
      	assert(MU>=INCREASE_PRECISION_LOW_PREC_MU_MIN);
      	assert(MU<=INCREASE_PRECISION_LOW_PREC_MU_MAX);
	assert(omega==__mixed_fma_increase_precision_low_prec_tbl_omega[i0]);
	i0++;
      }  
    }
  }

  printf("\tPASS\n");
}


/* Computes floor(x * log2(5)) for x in -5000 <= x <= 5000 */
STATIC INLINE void __TEST__mixed_fma_floor_times_log2_of_5() {
  printf("TEST __mixed_fma_floor_times_log2_of_5\t");
  __mixed_fma_int32_t res, x;

  for (x=FLOOR_TIMES_LOG2_X_MIN;x<=FLOOR_TIMES_LOG2_X_MAX;x++){
    res = __mixed_fma_floor_times_log2_of_5(x);
    assert(res==__mixed_fma_floor_times_log2_of_5_Tbl[x-FLOOR_TIMES_LOG2_X_MIN]);
  }
  
  printf("\t\tPASS\n");
}

/* Computes floor(x * log2(10)) for x in -5000 <= x <= 5000 */
STATIC INLINE void __TEST__mixed_fma_floor_times_log2_of_10() {
  printf("TEST __mixed_fma_floor_times_log2_of_10\t");
  __mixed_fma_int32_t res, x;

  for (x=FLOOR_TIMES_LOG2_X_MIN;x<=FLOOR_TIMES_LOG2_X_MAX;x++){
    res = __mixed_fma_floor_times_log2_of_10(x);
    assert(res==__mixed_fma_floor_times_log2_of_10_Tbl[x-FLOOR_TIMES_LOG2_X_MIN]);
  }
  
  printf("\t\tPASS\n");
}

/* Computes 2^GAMMA * v = 2^LAMBDA * 5^MU * omega * (1 + eps)

   with 2^63 <= omega < 2^64 or omega = 0 

   and 2^63 <= v < 2^64 or v = 0

   and eps bounded by abs(eps) <= 2^-60.5.

   The algorithm supposes that 

   -2215 <= LAMBDA <= 1984
   -842 <= MU <= 770

   and guarantees that 

   -4171 <= GAMMA <= 3772.
*/
STATIC INLINE void __TEST__mixed_fma_convert_to_binary_low_prec() {
  printf("TEST __mixed_fma_convert_to_binary_low_prec\t");
  __mixed_fma_int32_t GAMMA;
  __mixed_fma_uint64_t v;

  __mixed_fma_int32_t LAMBDA, MU;
  __mixed_fma_uint64_t omega;

  
  __mixed_fma_uint64_t i, i0;
  int j, k;

  omega = ((__mixed_fma_uint64_t) 1) << 53;
  for (j=INCREASE_PRECISION_LOW_PREC_N_MIN;j<=INCREASE_PRECISION_LOW_PREC_N_MAX;j++) {
    for (k=INCREASE_PRECISION_LOW_PREC_P_MIN;k<=INCREASE_PRECISION_LOW_PREC_P_MAX;k++) {
      i0=0;
      for (i=INCREASE_PRECISION_LOW_PREC_T_MIN;i<=INCREASE_PRECISION_LOW_PREC_T_MAX;i+=(((__mixed_fma_uint64_t) 1) << 50)) {
	__mixed_fma_increase_precision_low_prec(&LAMBDA,&MU,&omega,j,k,i);
	
	assert(LAMBDA==__mixed_fma_increase_precision_low_prec_tbl_lambda[j-INCREASE_PRECISION_LOW_PREC_N_MIN]);
	assert(LAMBDA>=INCREASE_PRECISION_LOW_PREC_LAMBDA_MIN);
	assert(LAMBDA<=INCREASE_PRECISION_LOW_PREC_LAMBDA_MAX);
	assert(MU==k);
      	assert(MU>=INCREASE_PRECISION_LOW_PREC_MU_MIN);
      	assert(MU<=INCREASE_PRECISION_LOW_PREC_MU_MAX);
	assert(omega==__mixed_fma_increase_precision_low_prec_tbl_omega[i0]);
	i0++;
      }  
    }
  }

  printf("\tPASS\n");
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
STATIC INLINE void __TEST__mixed_fma_check_ratio() {
  printf("TEST __TEST__mixed_fma_check_ratio\t\t");
  int res;
  
  __mixed_fma_int32_t L, M;
  __mixed_fma_uint128_t s;
  __mixed_fma_int32_t N, P;
  __mixed_fma_uint64_t t;
  
  int i;
  __mixed_fma_int32_t X, Y;

  printf("\n");
  for (i=0;i<CHECK_RATIO_NB_PIRES_CAS;i++){
    
    X = check_ratio_x[i];
    Y = check_ratio_y[i];
    s = (__mixed_fma_uint128_t) check_ratio_s0[i];
    s += ((__mixed_fma_uint128_t) check_ratio_s1[i]) << 64;
    t = check_ratio_t[i];

    printf("X : %d\t Y : %d\t t : %lu\t s : %lu %lu\n",X,Y,t,((__mixed_fma_uint64_t) (s >> 64)),((__mixed_fma_uint64_t) s));
  }

  printf("\tPASS\n");
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
STATIC INLINE void __TEST__mixed_fma_increase_precision_high_prec() {
  __mixed_fma_int32_t L, M;
  __mixed_fma_uint128_t s;
  
  __mixed_fma_int32_t N, P;
  __mixed_fma_uint64_t t;

  /* STUB */
}

STATIC INLINE void __TEST__mixed_fma_multiply_array() {
  __mixed_fma_uint64_t  c, a, b;
  int size_a, size_b;

  /* STUB */
}

/* Computes z = floor(2^-128 * x * y) */
STATIC INLINE void __TEST__mixed_fma_multiply_high_prec() {
  __mixed_fma_uint256_t z;
  
  __mixed_fma_uint128_t x;
  __mixed_fma_uint256_t y;

  /* STUB */
}

/* Normalizes rho that enters as 2^(255-l) <= rho < 2^256 such that
   finally 2^255 <= rho < 2^256 while maintaining the corresponding
   exponent SHA such that in the end 2^SHA * rho is still the same
   value.
*/
STATIC INLINE void __TEST__mixed_fma_normalize_high_prec() {
  __mixed_fma_int32_t SHA;
  __mixed_fma_uint256_t rho;
  
 /* STUB */
}

/* Normalizes rho that enters as 2^(255-l) <= rho < 2^256 such that
   finally 2^255 <= rho < 2^256 while maintaining the corresponding
   exponent SHA such that in the end 2^SHA * rho is still the same
   value.
*/
STATIC INLINE void __TEST__mixed_fma_normalize_high_prec_l() {
  __mixed_fma_int32_t SHA;
  __mixed_fma_uint256_t rho;
  unsigned int l;

  /* STUB */
}


/* Computes rho = 2^r * rho
   with 2^255 <= rho < 2^256
*/
STATIC INLINE void __TEST__mixed_fma_shift_high_prec_right() {
  __mixed_fma_uint256_t rho;
  __mixed_fma_uint32_t r;

  /* STUB */
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
STATIC INLINE void __TEST__mixed_fma_convert_to_binary_high_prec() {
  __mixed_fma_int32_t SHA;
  __mixed_fma_uint256_t rho;

  __mixed_fma_int32_t L;
  __mixed_fma_int32_t M;
  __mixed_fma_uint64_t s;

  /* STUB */
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
   
   -4171 <= C <= 3772.
*/
STATIC INLINE void __TEST__mixed_fma_far_path_addition() {
  __mixed_fma_int32_t C;
  __mixed_fma_uint64_t g;
  int s;

  __mixed_fma_int32_t GAMMA1, GAMMA2;
  __mixed_fma_uint64_t v1, v2;
  int s1, s2;

  /* STUB */
}

/* Compare rho1 and rho2

   with 2^255 <= rho1 < 2^256 or rho1 = 0 and 2^255 <= rho2 < 2^256 or rho2 = 0

   Returns 
   - a negative value if rho1 < rho2
   - a zero value if rho1 = rho2
   - a positive value if rho1 > rho2   
*/ 
STATIC INLINE void __TEST__mixed_fma_comparison_high_prec() {
  __mixed_fma_uint256_t rho1, rho2;

  /* STUB */
}

/* Computes g = rho1 - rho2
   
   with rho1 >= rho2
   and
   with 2^255 <= rho1 < 2^256 or rho1 = 0 and 2^255 <= rho2 < 2^256 or rho2 = 0

   and 2^255 <= g < 2^256 or g = 0 
*/ 
STATIC INLINE void __TEST__mixed_fma_subtraction_high_prec() {
  __mixed_fma_uint256_t g;

  __mixed_fma_uint256_t rho1, rho2;

  /* STUB */
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

   -4364 <= C <= 3579.
*/ 
STATIC INLINE void __TEST__mixed_fma_near_path_subtraction() {
  __mixed_fma_int32_t C;
  __mixed_fma_uint64_t g;
  int s;

  __mixed_fma_int32_t SHA1, SHA2;
  __mixed_fma_uint256_t rho1, rho2;
  int s1, s2;

  /* STUB */
}

/* Computes z = floor(2^-64 * x * y) */
STATIC INLINE void __TEST__mixed_fma_multiply_high_prec2() {
  __mixed_fma_uint128_t z;

  __mixed_fma_uint64_t x;
  __mixed_fma_uint64_t y[2];

  /* STUB */
}

/* Computes 10^H * 2^-10 * q = 2^C * g * (1 + eps)

   with 10^15 * 2^10 <= q <= (10^16 - 1) * 2^11 < 2^64 or q = 0 

   and 2^63 <= g < 2^64  or g = 0

   and eps bounded by abs(eps) <= 2^-59.82.

   The algorithm supposes that 

   -3094 <= C <= 3773

   and guarantees that 

   -928 <= H <= 1140.
*/
STATIC INLINE void __TEST__mixed_fma_convert_to_decimal() {
  __mixed_fma_int32_t H;
  __mixed_fma_uint64_t q;

  __mixed_fma_int32_t C;
  __mixed_fma_uint64_t g;

  /* STUB */
}


/* Computes round((-1)^s * 2^C * g) where round() is the current
   binary rounding to binary64 and sets all appropriate IEEE754 flags and only the
   appropriate flags.

   If g is non-zero, the value g is guaranteed to be bounded by

   2^63 <= g < 2^64

   When g is zero, round() applies the appropriate rounding rules
   for (-1)^s * 1.0 - (-1)^s * 1.0 as asked for IEEE754-2008.

*/
STATIC INLINE void __TEST__mixed_fma_final_round_binary() {
  __mixed_fma_binary64_t res;
  
  int s;
  __mixed_fma_int32_t C;
  __mixed_fma_uint64_t g;

  /* STUB */
}

/* Computes round((-1)^s * 10^H * 2^-10 * q) where round() is the current
   decimal rounding to decimal64 and sets all appropriate IEEE754 flags and only the
   appropriate flags.

   If q is non-zero, the value q is guaranteed to be bounded by

   10^15 * 2^10 <= q < 10^16 * 2^10

   When q is zero, round() applies the appropriate rounding rules
   for (-1)^s * 1.0DD - (-1)^s * 1.0DD as asked for IEEE754-2008.

*/
STATIC INLINE void __TEST__mixed_fma_final_round_decimal() {
  __mixed_fma_decimal64_t res;
  
  int s;
  __mixed_fma_int32_t H;
  __mixed_fma_uint64_t q;

  /* STUB */
}
  
int main(int argc, char **argv) {
  srand(time(NULL)); // initialisation de rand

  int nombre_aleatoire = rand();
  printf("%d \n",nombre_aleatoire);
  
  /* __TEST__mixed_fma_lower_precision_low_prec(); */
  __TEST__mixed_fma_increase_precision_low_prec();
  __TEST__mixed_fma_floor_times_log2_of_5();
  __TEST__mixed_fma_floor_times_log2_of_10();
  __TEST__mixed_fma_check_ratio();
  
  return 0;
}
