#ifndef __LIB_KERNEL_FIXED_POINT
#define __LIB_KERNEL_FIXED_POINT

#include <stdint.h>

typedef int fixed_point;

static const int TAIL_DIGITS = 14;

/*
Macro that represents the number that is applied to integers to
convert them to a fixed point.
*/
#define conversion_factor() \
        (1 << TAIL_DIGITS)

/*
Converts the given integer into a fixed_point.

Note - Since fixed_point is a typedef int there is no type checking.
This allows to_fixed_point to be applied to an integer more than once.
E.g.to_fixed_point(to_fixed_point(INTEGER));
*/
#define to_fixed_point(INTEGER) \
  (((INTEGER) * conversion_factor()))

/*
Converts the given fixed point into an integer rounding down towards
zero.
*/
#define to_integer_truncated(FIXED_POINT) \
  ((FIXED_POINT) / conversion_factor())

/*
Converts the given fixed point into an integer rounding it to the
nearest integer.
*/
#define to_integer_rounded(FIXED_POINT) \
  ((((FIXED_POINT) >= 0) ? ((FIXED_POINT) + conversion_factor() / 2)    \
    : ((FIXED_POINT) - conversion_factor() / 2)) /                      \
   conversion_factor())
/*
Adds two fixeds points together.
*/
#define fixed_point_add(FIXED_POINT_A, FIXED_POINT_B) \
  ((FIXED_POINT_A) + (FIXED_POINT_B))
/*
Subtracts the second point from the first point.
*/
#define fixed_point_subtract(FIXED_POINT_A, FIXED_POINT_B) \
  ((FIXED_POINT_A) - (FIXED_POINT_B))
/*
Adds an integer, which is converted into a fixed_point, to a given
fixed_point.
*/
#define fixed_point_addi(FIXED_POINT, INT) \
  ((FIXED_POINT) + ((INT) * conversion_factor()))
/*
Subtracts an integer, which is converted into a fixed_point, to a
given fixed_point.
*/
#define fixed_point_subtracti(FIXED_POINT, INT) \
  ((FIXED_POINT) - ((INT) * conversion_factor()))
/*
Multiplies two fixes_points.
*/
#define fixed_point_multiply(FIXED_POINT_A, FIXED_POINT_B) \
  ((int64_t)(FIXED_POINT_A) * (FIXED_POINT_B) / conversion_factor())
/*
Multiplies a fixed_point by an integer.
*/
#define fixed_point_multiplyi(FIXED_POINT, INT) \
  ((FIXED_POINT) * (INT))
/*
Divides the first point by the second point.
*/
#define fixed_point_divide(FIXED_POINT_A, FIXED_POINT_B) \
  (((int64_t)(FIXED_POINT_A)) * conversion_factor() / (FIXED_POINT_B))
/*
Divides a given fixed point by an integer.
*/
#define fixed_point_dividei(FIXED_POINT, INT) \
  ((FIXED_POINT) / (INT))

#endif
