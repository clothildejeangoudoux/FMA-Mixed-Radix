#ifndef FMA_TESTS_H
#define FMA_TESTS_H

#include "fma_types.h"
#include "fma_mixedradix.h"
#include "comparison-helper.h"
#include "decimal_helper.h"
#include "reference.h"
#include "fma_empty.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <gmp.h>
#include <fenv.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef struct {
  __mixed_fma_binary64_t tmax;
  __mixed_fma_binary64_t tmax_empty;
  __mixed_fma_binary64_t tmax_ref;  
} timeanalytics_t;

typedef struct {
  __mixed_fma_uint64_t tfail_impl_VS_Sollya;
  __mixed_fma_uint64_t tfail_impl_VS_GMP;
  __mixed_fma_uint64_t tfail_Sollya_VS_GMP; 
  __mixed_fma_uint64_t tfail_flags;
} failedtests_t;

typedef struct {
  __mixed_fma_binary64_t expectedResult;
  __mixed_fma_binary64_t computedResult;
  __mixed_fma_binary64_t x;
  __mixed_fma_binary64_t y;
  __mixed_fma_decimal64_t z;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;  
  int decimalRoundingMode;
} __mixed_fma_testvector_binary64_binary64_binary64_decimal64_t;

typedef struct {
  __mixed_fma_binary64_t expectedResult;
  __mixed_fma_binary64_t computedResult;
  __mixed_fma_binary64_t x;
  __mixed_fma_decimal64_t y;
  __mixed_fma_binary64_t z;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;  
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_binary64_binary64_decimal64_binary64_t;

typedef struct {
  __mixed_fma_binary64_t expectedResult;
  __mixed_fma_binary64_t computedResult;
  __mixed_fma_binary64_t x;
  __mixed_fma_decimal64_t y;
  __mixed_fma_decimal64_t z;
  __mixed_fma_binary64_t computeTime;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_binary64_binary64_decimal64_decimal64_t;

typedef struct {
  __mixed_fma_binary64_t expectedResult;
  __mixed_fma_binary64_t computedResult;
  __mixed_fma_decimal64_t x;
  __mixed_fma_binary64_t y;
  __mixed_fma_binary64_t z;
  __mixed_fma_binary64_t computeTime;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_binary64_decimal64_binary64_binary64_t;

typedef struct {
  __mixed_fma_binary64_t expectedResult;
  __mixed_fma_binary64_t computedResult;
  __mixed_fma_decimal64_t x;
  __mixed_fma_binary64_t y;
  __mixed_fma_decimal64_t z;
  __mixed_fma_binary64_t computeTime;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_binary64_decimal64_binary64_decimal64_t;

typedef struct {
  __mixed_fma_binary64_t expectedResult;
  __mixed_fma_binary64_t computedResult;
  __mixed_fma_decimal64_t x;
  __mixed_fma_decimal64_t y;
  __mixed_fma_binary64_t z;
  __mixed_fma_binary64_t computeTime;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_binary64_decimal64_decimal64_binary64_t;

typedef struct {
  __mixed_fma_binary64_t expectedResult;
  __mixed_fma_binary64_t computedResult;
  __mixed_fma_decimal64_t x;
  __mixed_fma_decimal64_t y;
  __mixed_fma_decimal64_t z;
  __mixed_fma_binary64_t computeTime;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_binary64_decimal64_decimal64_decimal64_t;

typedef struct {
  __mixed_fma_decimal64_t expectedResult;
  __mixed_fma_decimal64_t computedResult;
  __mixed_fma_binary64_t x;
  __mixed_fma_binary64_t y;
  __mixed_fma_binary64_t z;
  __mixed_fma_binary64_t computeTime;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_decimal64_binary64_binary64_binary64_t;

typedef struct {
  __mixed_fma_decimal64_t expectedResult;
  __mixed_fma_decimal64_t computedResult;
  __mixed_fma_binary64_t x;
  __mixed_fma_binary64_t y;
  __mixed_fma_decimal64_t z;
  __mixed_fma_binary64_t computeTime;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_decimal64_binary64_binary64_decimal64_t;

typedef struct {
  __mixed_fma_decimal64_t expectedResult;
  __mixed_fma_decimal64_t computedResult;
  __mixed_fma_binary64_t x;
  __mixed_fma_decimal64_t y;
  __mixed_fma_binary64_t z;
  __mixed_fma_binary64_t computeTime;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_decimal64_binary64_decimal64_binary64_t;

typedef struct {
  __mixed_fma_decimal64_t expectedResult;
  __mixed_fma_decimal64_t computedResult;
  __mixed_fma_binary64_t x;
  __mixed_fma_decimal64_t y;
  __mixed_fma_decimal64_t z;
  __mixed_fma_binary64_t computeTime;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_decimal64_binary64_decimal64_decimal64_t;

typedef struct {
  __mixed_fma_decimal64_t expectedResult;
  __mixed_fma_decimal64_t computedResult;
  __mixed_fma_decimal64_t x;
  __mixed_fma_binary64_t y;
  __mixed_fma_binary64_t z;
  __mixed_fma_binary64_t computeTime;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_decimal64_decimal64_binary64_binary64_t;

typedef struct {
  __mixed_fma_decimal64_t expectedResult;
  __mixed_fma_decimal64_t computedResult;
  __mixed_fma_decimal64_t x;
  __mixed_fma_binary64_t y;
  __mixed_fma_decimal64_t z;
  __mixed_fma_binary64_t computeTime;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_decimal64_decimal64_binary64_decimal64_t;

typedef struct {
  __mixed_fma_decimal64_t expectedResult;
  __mixed_fma_decimal64_t computedResult;
  __mixed_fma_decimal64_t x;
  __mixed_fma_decimal64_t y;
  __mixed_fma_binary64_t z;
  __mixed_fma_binary64_t computeTime;
  int beforeFlags;
  int expectedFlags;
  int computedFlags;
  int binaryRoundingMode;
  int decimalRoundingMode;
} __mixed_fma_testvector_decimal64_decimal64_decimal64_binary64_t;

extern int __dfp_get_round();
extern void __dfp_set_round(int);

#endif




