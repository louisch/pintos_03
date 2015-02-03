#ifndef __LIB_KERNEL_FIXED_POINT
#define __LIB_KERNEL_FIXED_POINT

typedef struct
{
  int value;
} fixed_point;

fixed_point to_fixed_point(int integer);

int to_integer_truncated(fixed_point fixed_point);

int to_integer_rounded(fixed_point fixed_point);

fixed_point fixed_point_add(fixed_point x, fixed_point y);

fixed_point fixed_point_subtract(fixed_point x, fixed_point y);

fixed_point fixed_point_addi(fixed_point x, int n);

fixed_point fixed_point_subtracti(fixed_point x, int n);

fixed_point fixed_point_multiply(fixed_point x, fixed_point y);

fixed_point fixed_point_multiplyi(fixed_point x, int n);

fixed_point fixed_point_divide(fixed_point x, fixed_point y);

fixed_point fixed_point_dividei(fixed_point x, int n);

#endif
