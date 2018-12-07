
#include <stdio.h>
#include <stdint.h>
#include <math.h>

/* Get our constants and types */
#include "comparison.h"

/* Get our own header */
#include "comparison-helper.h"

/* Compile me with something like 

    icc -xSSE4.2 -O0 -c comparison-helper.o comparison-helper.c 

    ATTENTION: WE ARE DOING SOME STUFF THAT RELIES ON THE FACT THAT
    THE COMPILER CORRECTLY TRANSLATES CODE THAT WORKS ON SUBNORMALS
    AND THAT USES VOLATILE VARIABLES. FOR ICC THIS MEANS WE MUST USE
    -O0 AS A COMPILER FLAG !!!

*/


/* Compute the correctly rounded binary representation of

    2^E * m
*/
_binary64 composeBinary(int negative, int E, uint64_t m) {
  uint64_t n, nh, nl;
  int F, F1, F2, G;
  volatile _binary64 volatileTemp, volatileTwoM537, sign;
  bin64caster nhcst, nlcst, mantcst, twoF1, twoF2;
  _binary64 res;

  if (negative) sign = -1.0; else sign = 1.0;

  if (m == 0x0ull) {
    return sign * 0.0;
  }

  F = E;
  n = m;
  while (n < 0x8000000000000000ull) {
    n <<= 1;
    F--;
  }

  /* Here, we have to compute 2^F * m 

     with 2^63 <= m <= 2^64 - 1.

     If F > 1023 - 63, we have overflow.
     If F < -1075 - 63, we have complete underflow.

  */
  if (F > 1023 - 63) {
    volatileTemp = 4.1495155688809929585124078636911611510124462322424e180; /* 2^600 */
    return (sign * volatileTemp) * volatileTemp;
  }

  if (F < -1075 - 63) {
    volatileTemp = 2.4099198651028841177407500347125089364310049545099e-181; /* 2^-600 */
    return (sign * volatileTemp) * volatileTemp;    
  }

  /* Check if we have a normal or a subnormal result:

     If F >= -1022 - 63, we have a normal result, otherwise we may
     have a subnormal (or a normal) result.

  */
  if (F >= -1022 - 63) {
    /* Cut n into two pieces of 32 bits */
    nh = n >> 32;
    nl = n & 0x00000000ffffffffull;
    
    /* Produce 2^32 * nh and nl as binary64 numbers */
    nhcst.i = 0x4330000000000000ull | nh;
    nhcst.f -= 4503599627370496.0;
    nhcst.f *= 4294967296.0; /* 2^32 */
    nlcst.i = 0x4330000000000000ull | nl;
    nlcst.f -= 4503599627370496.0;
    
    /* Round 2^32 * nh + nl to a binary64 number */
    mantcst.f = nhcst.f + nlcst.f;

    /* Multiply by 2^F in two steps on a volatile */
    F1 = F >> 1;
    F2 = F - F1;
    twoF1.i = ((uint64_t) (F1 + 1023)) << 52;
    twoF2.i = ((uint64_t) (F2 + 1023)) << 52;

    volatileTemp = mantcst.f * sign;
    volatileTemp *= twoF2.f;
    volatileTemp *= twoF1.f;
    res = volatileTemp;

    return res;
  }

  /* Do subnormal rounding 

     Cut n into two pieces, according to the exponent F.
  */
  
  nh = n >> (-1023 - 52 + 1 - F);
  nl = n & ((1ull << (-1023 - 52 + 1 - F)) - 1ull);

  /* If nl is zero, we have an exact subnormal result */
  if (nl == 0x0ull) {
    nhcst.i = nh;
    res = nhcst.f;
    
    return res * sign;
  }

  /* Here, nl is not zero, normalize it */
  G = 0;
  while (nl < 0x8000000000000000ull) {
    nl <<= 1;
    G--;
  }
  
  /* Make nl hold on 52 bits, integrating any sticky bit */
  nl = (nl >> 12) | ((uint64_t) ((nl & 0xfffull) != 0ull));

  /* Shift the floating-point dot to the right place in nh and nl,
     adapting G. 
  */
  nhcst.i = 0x4330000000000000ull | nh;
  nlcst.i = 0x4330000000000000ull | nl;
  nlcst.f -= 4503599627370496.0;
  G += 1023 + 52 + 12 - 1 + F;
  nlcst.i += ((int64_t) G) << 52;

  /* Perform the fix-point rounding */
  volatileTemp = sign * nhcst.f;
  volatileTemp += sign * nlcst.f;
 
  /* Remove the offset */
  volatileTemp -= sign * 4503599627370496.0;

  /* Multiply by 2^-1074 in two steps. */
  volatileTwoM537 = 2.2227587494850774834427134142705600969125245614649e-162;
  volatileTemp *= volatileTwoM537;

  res = volatileTemp * volatileTwoM537;

  /* Return subnormal result */
  return res;
}

/* Compute the correctly rounded decimal representation of

   10^expo * mant

*/
_Decimal64 composeDecimal(int negative, int expo, uint64_t mant) {
  int E, G, H;
  uint64_t K, m, f;
  volatile _Decimal64 volatileTemp, sign, volatileTemp2;
  dec64caster loader;
  uint64_t n, L, r, q, z, y, M, t, S, Ebiased, res;
  _Decimal64 h, d, s;

  if (mant <= 9999999999999999ull) {
    /* Here, we can hope to inject the mantissa directly. */
    E = expo;
    m = mant;
    K = 1ull;
    f = 0ull;
  } else {
    /* Here, we have to cut the mantissa */
    m = mant / 10000ull;
    f = mant % 10000ull;
    E = expo + 4;
    K = 10000ull;
  }

  /* Now we have to ompute the correctly rounded decimal
     representation of
     
     10^E * (m + 1/K * f)
     
     where K is an integer power of 10 and m and f satisfy

      0 <= m <= 10^16 - 1
      0 <= f <= K - 1.

  */

  if (negative) {
    loader.i = 0xb1c0000000000001ull;
    sign = loader.f;
  } else {
    loader.i = 0x31c0000000000001ull;
    sign = loader.f;    
  }

  /* Check if we have clear overflow */
  if (E > 384 - (16 - 1) + 16 + 1) {
    /* Here, we have clear overflow */
    loader.i = 0x48e38d7ea4c68000ull; /* 10^200 */
    volatileTemp = loader.f;
    return (volatileTemp * sign) * volatileTemp;
  }

  /* Check if we have clear underflow */
  if (E < -383 - (16 - 1) - 16) {
    /* Here, we have clear underflow but we have to check for zero. */
    if (m == 0x0ull) {
      loader.i = 0x31c0000000000000ull; /* zero with quantum 0 */
      volatileTemp2 = loader.f;      
    } else {
      loader.i = 0x31c0000000000001ull; /* 1 with quantum 0 */
      volatileTemp2 = loader.f;      
    }
    loader.i = 0x18c0000000000001ull; /* 10^-200 */
    volatileTemp = loader.f;
    return (volatileTemp * (volatileTemp2  * sign)) * volatileTemp;
  }
  
  /* Here, we have chances to get into range, i.e. we have no clear
     under- or overflow. 
  */

  /* Check if we have exponent overflow */
  while (E > 384 - (16 - 1)) {
    /* Here, we have exponent overflow with this mantissa.
       
       Check if we can multiply the mantissa by 10 (while adding in
       the next fractional digit) and still stay in range. If yes,
       call ourselves recursively. Otherwise perform overflow
       handling.

    */
    if (f == 0ull) {
      L = K;
      q = 0ull;
      r = f;
    } else {
      L = K / 10ull; /* exact by definition of K */
      q = f / L;
      r = f % L;
    }
    n = 10ull * m + q;
    if (n <= 9999999999999999ull) {
      /* Here, we have done one step to get into range */
      E--;
      m = n;
      K = L;
      f = r;
    } else {
      /* Handle clear overflow and return */
      loader.i = 0x48e38d7ea4c68000ull; /* 10^200 */
      volatileTemp = loader.f;
      return (volatileTemp * sign) * volatileTemp;
    }
  }

  /* Check if we have exponent underflow */
  while (E < -383 - (16 - 1)) {
    /* Here, we have exponent underflow with this mantissa 

       We divide the mantissa by 10, integrating the rest into the
       fractional part. We do so in order to come into range.
       
    */
    n = m / 10ull;
    q = m % 10ull;
    L = K * 10ull;
    r = q * K + f;
    E++;
    m = n;
    K = L;
    f = r;
  }

  /* Here, we have E in the representable range 

     -383 - (16 - 1) <= E <= 384 - (16 - 1)

     Check if we have a non-zero fractional part or not.

  */
  if (f != 0x0ull) {
    /* The fractional part is non-zero, we have to re-quantize in the
       hope that the rounding gets exact and in order to minimize the
       relative rounding error.
    */
    G = E;
    z = m;
    y = f;
    M = K;
    while ((z < 1000000000000000ull) && (G > -383 - (16 - 1))) {
      if (y == 0ull) 
	break;
      M = M / 10ull; /* exact by definition of M */
      q = y / M;
      y = y % M;
      z = 10ull * z + q;
      G--;
    }

    if (y != 0x0ull) {
      /* We have to round 10^G * (z + 1/M * y) 

	 As we have brought G already into range and z is normalized, 
	 this rounding can be replaced by a surrogate rounding of

	 (10^15 + a) + b

	 where a is either 2 or 3, depending on the parity of z
	 and b is either 0.25, 0.5 or 0.75, depending on 
	 the interval y/M is in.
      */
      if (z & 1ull) {
	/* z is odd, load 10^15 + 3 */
	loader.i = 0x31c38d7ea4c68003ull;
	h = loader.f;      
      } else {
	/* z is even, load 10^15 + 2 */
	loader.i = 0x31c38d7ea4c68002ull;
	h = loader.f;      
      }
      
      /* Determine now d one of 0.25, 0.5 or 0.75 depending on y/M 

	 If y/M < 0.5, load 0.25
	 If y/M = 0.5, load 0.5
         If y/M > 0.5, load 0.75

	 Let H be H = 0.5 * M. 

	 Remark: the division of M by 2 is exact by definition of M.

      */
      H = M >> 1; 
      if (y < H) {
	/* Load 0.25 */
	loader.i = 0x3180000000000019ull;
	d = loader.f;      
      } else {
	if (y > H) {
	  /* Load 0.75 */
	  loader.i = 0x318000000000004bull;
	  d = loader.f;      
	} else {
	  /* Load 0.5 */
	  loader.i = 0x31a0000000000005ull;
	  d = loader.f;      
	}
      }

      /* Now compute (h + d) - h, integrating the sign first.

	 The first operation does the appropriate rounding, the second
	 one is Sterbenz-exact.
      */
      volatileTemp = (sign * h) + (sign * d);
      s = sign * (volatileTemp - (sign * h));

      /* Now, s is either 0 or 1, depending if we rounded up or down. 

	 As the IEEE 754-2008 standard fixes precise rules for the
	 computation of the quantum of the result of additions and
	 subtractions, we know that when s is 1, this 1 will be
	 encoded as 0x31c0000000000001.

	 So we can easily compute an integer representation of s; call
	 it S.

      */
      loader.f = s;
      if (loader.i == 0x31c0000000000001ull) S = 1ull; else S = 0ull;
      
      /* Now reflect the surrogate rounding on the actual mantissa
	 z. 

	 Attention: we have to check for post-rounding overflow.

      */
      H = G;
      t = z + S;
      
      if (t > 9999999999999999ull) {
	/* Here rounding has brought us into the next decade */
	t = 1000000000000000ull;
	H++;
      }
      
      /* Check if we got overflow as a result of rounding into the
	 next decade 
      */
      if (H > 384 - (16 - 1)) {
	/* Handle overflow and return */
	loader.i = 0x48e38d7ea4c68000ull; /* 10^200 */
	volatileTemp = loader.f;
	return (volatileTemp * sign) * volatileTemp;
      }
    } else {
      /* The fractional part has become zero and the rounding hence
	 exact 
      */
      H = G;
      t = z;
    }
  } else {
    /* The fractional part is zero, so the rounding is exact */
    H = E;
    t = m;
  }

  /* Here, we have to encode 10^H * t, knowing that 

     0 <= t <= 10^16 - 1
    
     and 

     -383 - (16 - 1) <= H <= 384 - (16 - 1).

     We start by computing the biased exponent.

  */

  Ebiased = (uint64_t) (H + 398);
  
  /* Decide if we have to use the first or second encoding form 
     for the exponent and the significand.

     The appropriate encoding depends on the length of the
     significand.

     If t holds on 53 bits, i.e. t < 2^53, the first encoding must be
     used, otherwise the second encoding.

  */
  if (t < 0x0020000000000000ull) {
    /* Use the first encoding */
    res = (Ebiased << 53) | t;
  } else {
    /* Use the second encoding */
    res = 0x6000000000000000ull 
      | (Ebiased << 51) 
      | (t & 0x0007ffffffffffffull);
  }

  /* Integrate the sign */
  if (negative) {
    res |= 0x8000000000000000ull;
  }

  /* Return the encoded result */
  loader.i = res;
  return loader.f;  
}

uint64_t myatoull(char *str) {
  uint64_t res;
  char *curr;

  res = 0;
  for (curr=str; *curr != '\0'; curr++) {
    if ((*curr < '0') || (*curr > '9')) break;
    res = 10ull * res + ((uint64_t) (*curr - '0'));
  }

  return res;
}

int classifyBinary(_binary64 x) {
  bin64caster xcst;
  int negative;

  xcst.f = x;
  
  negative = ((xcst.i & 0x8000000000000000ull) != 0x0ull);
  xcst.i &= 0x7fffffffffffffffull;

  if (xcst.i > 0x7ff0000000000000ull) {
    /* sNaN or qNaN */
    if (xcst.i & 0x0008000000000000ull) return BINARY64_CLASS_QNAN;
    return BINARY64_CLASS_SNAN;
  }
  
  if (xcst.i == 0x7ff0000000000000ull) {
    /* +/- Inf */
    if (negative) return BINARY64_CLASS_NEG_INF;
    return BINARY64_CLASS_POS_INF;
  }

  if (xcst.i >= 0x0010000000000000ull) {
    /* Normal */
    if (negative) return BINARY64_CLASS_NEG_NORMAL;
    return BINARY64_CLASS_POS_NORMAL;
  } 

  if (xcst.i == 0x0000000000000000ull) {
    /* Zero */
    if (negative) return BINARY64_CLASS_NEG_ZERO;
    return BINARY64_CLASS_POS_ZERO;
  }
  
  /* Subnormal */
  if (negative) return BINARY64_CLASS_NEG_SUBNORMAL;
  return BINARY64_CLASS_POS_SUBNORMAL;
}

int classifyDecimal(_Decimal64 x) {
  dec64caster xcst;
  int negative;
  uint64_t significand;

  xcst.f = x;
    
  if ((xcst.i & 0x7c00000000000000ull) == 0x7c00000000000000ull) {
    /* sNaN or qNaN */
    if ((xcst.i & 0x0200000000000000ull) == 0x0200000000000000ull) {
      /* sNaN */
      return DECIMAL64_CLASS_SNAN;
    }
    /* qNaN */
    return DECIMAL64_CLASS_QNAN;
  }

  negative = ((xcst.i & 0x8000000000000000ull) != 0x0ull);

  if ((xcst.i & 0x7c00000000000000ull) == 0x7800000000000000ull) {
    /* +/- Inf */
    if (negative) return DECIMAL64_CLASS_NEG_INF;
    return DECIMAL64_CLASS_POS_INF;
  }

  significand = (((xcst.i & 0x6000000000000000ull) != 0x6000000000000000ull) ? 
		  (xcst.i & 0x001fffffffffffffull) : 
		  ((xcst.i & 0x0007ffffffffffffull) | 0x0020000000000000ull));
  if (significand > 9999999999999999ull) significand = 0x0ull;
  
  if (significand == 0x0ull) {
    /* +/- Zero */
    if (negative) return DECIMAL64_CLASS_NEG_ZERO;
    return DECIMAL64_CLASS_POS_ZERO;
  }

  /* +/- Number */
  if (negative) return DECIMAL64_CLASS_NEG_NUMBER;
  return DECIMAL64_CLASS_POS_NUMBER;
}

char *binaryClassToName(int class) {
  switch (class) {
  case BINARY64_CLASS_QNAN:
    return "QNAN ";
  case BINARY64_CLASS_NEG_INF:
    return "-INF ";
  case BINARY64_CLASS_NEG_NORMAL:
    return "-NORM";
  case BINARY64_CLASS_NEG_SUBNORMAL:
    return "-SUBN";
  case BINARY64_CLASS_NEG_ZERO:
    return "-ZERO";
  case BINARY64_CLASS_POS_ZERO:
    return "+ZERO";
  case BINARY64_CLASS_POS_SUBNORMAL:
    return "+SUBN";
  case BINARY64_CLASS_POS_NORMAL:
    return "+NORM";
  case BINARY64_CLASS_POS_INF:
    return "+INF ";
  case BINARY64_CLASS_SNAN:
    return "SNAN ";
  default:
    return "UKNWN";
  }
}

char *decimalClassToName(int class) {
  switch (class) {
  case DECIMAL64_CLASS_QNAN:
    return "QNAN ";
  case DECIMAL64_CLASS_NEG_INF:
    return "-INF ";
  case DECIMAL64_CLASS_NEG_NUMBER:
    return "-NMBR";
  case DECIMAL64_CLASS_NEG_ZERO:
    return "-ZERO";
  case DECIMAL64_CLASS_POS_ZERO:
    return "+ZERO";
  case DECIMAL64_CLASS_POS_NUMBER:
    return "+NMBR";
  case DECIMAL64_CLASS_POS_INF:
    return "+INF ";
  case DECIMAL64_CLASS_SNAN:
    return "SNAN ";
  default:
    return "UKNWN";
  }
}

/* Return non-zero if x <= y is an easy case, zero if it is a hard
   case.  Adapted from the version of compareSignalingLessEqualBinary64Decimal64
   from comparison.c.
*/
int classifyCase(_binary64 x, _Decimal64 y) {
  bin64caster xcst;
  dec64caster ycst;
  int xIsInf, yIsInf, xIsZero, yIsZero, xIsNeg, yIsNeg;
  uint64_t ySignificand;
  int64_t E, F, G, S, H, cTwo19Log2OverLog5, Q;
  uint64_t t; //m, n
  bin64caster temp;
 
  xcst.f = x;
  ycst.f = y;

  /* Handle NaNs first */
  if (((xcst.i & 0x7fffffffffffffffull) > 0x7ff0000000000000ull) ||
      ((ycst.i & 0x7c00000000000000ull) == 0x7c00000000000000ull))
    /* At least one of x or y is NaN. */
    return PAIR_SPECIAL;

  /* Check if x or y are zero. Also determine their signs. */
  xIsNeg = ((xcst.i & 0x8000000000000000ull) != 0x0ull);
  yIsNeg = ((ycst.i & 0x8000000000000000ull) != 0x0ull);
  xIsZero = ((xcst.i & 0x7fffffffffffffffull) == 0x0ull);
  ySignificand = (((ycst.i & 0x6000000000000000ull) != 0x6000000000000000ull) ? 
		  (ycst.i & 0x001fffffffffffffull) : 
		  ((ycst.i & 0x0007ffffffffffffull) | 0x0020000000000000ull));
  if (ySignificand > 9999999999999999ull) ySignificand = 0x0ull;
  yIsZero = (ySignificand == 0x0ull);
  
  if (xIsZero || yIsZero) 
    return PAIR_SPECIAL;

  /* Here, x and y are both neither zero nor NaN. */
  
  /* Now perform a first check based on the sign of the input: 
     if the signs of x and y are different, the result is already known.
  */
  if (xIsNeg != yIsNeg)
    /* Here, the signs of x and y are different. */
    return PAIR_OPPSIGNS;
  
  /* Now check if x or y is +/- Infinity */
  xIsInf = ((xcst.i & 0x7fffffffffffffffull) == 0x7ff0000000000000ull);
  yIsInf = ((ycst.i & 0x7c00000000000000ull) == 0x7800000000000000ull);
  if (xIsInf || yIsInf) 
    return PAIR_SPECIAL;

  /* Here, x and y are both non-zero finite numbers of the same sign. */

  xcst.i &= 0x7fffffffffffffffull;
  E = 0;
  if (xcst.i < 0x0010000000000000ull) {
    /* Handle subnormal numbers */
    E -= 54;
    xcst.f *= 18014398509481984.0; /* Multiply with 2^54 */
  }
  E += ((xcst.i & 0x7ff0000000000000ull) >> 52) - 1023 - 52;
  //m = (xcst.i & 0x000fffffffffffffull) | 0x0010000000000000ull;
  
  G = (((ycst.i & 0x6000000000000000ull) != 0x6000000000000000ull) ? 
       ((ycst.i & 0x7fe0000000000000ull) >> 53) : 
       ((ycst.i & 0x1ff8000000000000ull) >> 51)) - 398;

  t = ySignificand >> 32;
  S = 0;
  if (t == 0ull) {
    t = ySignificand;
    S = 32;
  } 
  temp.i = 0x4330000000000000 | t; /* 0x43300...0 = 2^52 */
  temp.f -= 4503599627370496.0;    /* 2^52 */
  S += (53 - 32) - ((temp.i >> 52) - 1023);
  //n = ySignificand << S;
  F = G - S;

  H = E - F;

  cTwo19Log2OverLog5 = 225799;
  Q = (H * cTwo19Log2OverLog5) >> 19;

  /* Now check if G < Q or G > Q or if we have equality */
  if (Q != G)
    return PAIR_EASY;

  return PAIR_HARD;

}

int classifyExtraClass(int binClass, int decClass, int easy) {
  switch (binClass) {
  case BINARY64_CLASS_QNAN:
  case BINARY64_CLASS_NEG_INF:
  case BINARY64_CLASS_POS_INF:
  case BINARY64_CLASS_SNAN:
  case BINARY64_CLASS_NEG_ZERO:
  case BINARY64_CLASS_POS_ZERO:
    return EXTRA_CLASS_SPECIAL;
    break;
  case BINARY64_CLASS_NEG_NORMAL:
  case BINARY64_CLASS_NEG_SUBNORMAL:
  case BINARY64_CLASS_POS_SUBNORMAL:
  case BINARY64_CLASS_POS_NORMAL:
    switch (decClass) {
    case DECIMAL64_CLASS_QNAN:
    case DECIMAL64_CLASS_NEG_INF:
    case DECIMAL64_CLASS_POS_INF:
    case DECIMAL64_CLASS_SNAN:
    case DECIMAL64_CLASS_NEG_ZERO:
    case DECIMAL64_CLASS_POS_ZERO:
      return EXTRA_CLASS_SPECIAL;
      break;
    case DECIMAL64_CLASS_NEG_NUMBER:
      switch (binClass) {
      case BINARY64_CLASS_NEG_NORMAL:
	if (easy) {
	  return EXTRA_CLASS_NORMAL_SAME_SIGN_EASY;
	}
	return EXTRA_CLASS_NORMAL_SAME_SIGN_HARD;
	break;
      case BINARY64_CLASS_NEG_SUBNORMAL:
	if (easy) {
	  return EXTRA_CLASS_SUBNORMAL_SAME_SIGN_EASY;
	}
	return EXTRA_CLASS_SUBNORMAL_SAME_SIGN_HARD;
	break;
      case BINARY64_CLASS_POS_SUBNORMAL:
      case BINARY64_CLASS_POS_NORMAL:
	return EXTRA_CLASS_OPPOSITE_SIGN;
	break;
      default:
	return -1;
      }
      return -1;
      break;
    case DECIMAL64_CLASS_POS_NUMBER:
      switch (binClass) {
      case BINARY64_CLASS_POS_NORMAL:
	if (easy) {
	  return EXTRA_CLASS_NORMAL_SAME_SIGN_EASY;
	}	
	return EXTRA_CLASS_NORMAL_SAME_SIGN_HARD;
	break;
      case BINARY64_CLASS_POS_SUBNORMAL:
	if (easy) {
	  return EXTRA_CLASS_SUBNORMAL_SAME_SIGN_EASY;
	}	
	return EXTRA_CLASS_SUBNORMAL_SAME_SIGN_HARD;
	break;
      case BINARY64_CLASS_NEG_SUBNORMAL:
      case BINARY64_CLASS_NEG_NORMAL:
	return EXTRA_CLASS_OPPOSITE_SIGN;
	break;
      default:
	return -1;
      }
      return -1;
      break;
    default:
      return -1;
    }
    break;
  default:
    return -1;
  }
  return -1;
}

char *extraClassToName(int class) {
  switch (class) {
  case EXTRA_CLASS_SPECIAL:
    return "SPECIAL                 ";
    break;
  case EXTRA_CLASS_OPPOSITE_SIGN:
    return "OPPOSITE SIGN           ";
    break;
  case EXTRA_CLASS_NORMAL_SAME_SIGN_EASY:
    return "SAME SIGN NORMAL EASY   ";
    break;
  case EXTRA_CLASS_SUBNORMAL_SAME_SIGN_EASY:
    return "SAME SIGN SUBNORMAL EASY";
    break;
  case EXTRA_CLASS_NORMAL_SAME_SIGN_HARD:
    return "SAME SIGN NORMAL HARD   ";
    break;
  case EXTRA_CLASS_SUBNORMAL_SAME_SIGN_HARD:
    return "SAME SIGN SUBNORMAL HARD";
    break;
  default:
    return "UNKNOWN                 ";
  }
}
