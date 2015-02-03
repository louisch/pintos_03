#ifndef __LIB_KERNEL_FIXED_POINT
#define __LIB_KERNEL_FIXED_POINT

typedef struct
{
  int fixed_point;
} fixed_point;

fixed_point to_fixed_point(int integer);

int to_integer_truncated(fixed_point fixed_point);

int to_integer_rounded(fixed_point fixed_point);

fixed_point add_fixed_points(fixed_point x, fixed_point y);

fixed_point subtract_fixed_points(fixed_point x, fixed_point y);

fixed_point add_fixed_point_to_integer(fixed_point x, int n);

fixed_point subtract_integer_from_fixed_point(fixed_point x, int n);

fixed_point multiply_fixed_points(fixed_point x, fixed_point y);

fixed_point multiply_fixed_point_by_integer(fixed_point x, int n);

fixed_point divide_fixed_points(fixed_point x, fixed_point y);

fixed_point divide_fixed_point_by_integer(fixed_point x, int n);


#endif