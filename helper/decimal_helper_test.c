#include <stdio.h>
#include <stdlib.h>
#include "decimal_helper.h"

int main(int argc, char **argv) {
  double X;
  __decimal_helper_decimal64_t x;
  char str[128] = { '\0' };
  char *endptr;

  if (argc < 2) return 1;
  X = strtod(argv[1], &endptr);
  if (endptr == argv[1]) return 1;

  x = (__decimal_helper_decimal64_t) X;

  decimal64_to_string(str, x);

  printf("\"%s\" vs. \"%1.50e\" vs. \"%s\"\n", argv[1], X, str);

  return 0;
}

