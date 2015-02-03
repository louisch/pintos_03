#include "fixed_point.h"

#include <stdint.h>

static const int TAIL_DIGITS = 14;

static int CONVERSION_FACTOR = 1 << TAIL_DIGITS;

fixed_point
to_fixed_point(int integer)
{
	fixed_point returned_point;
	returned_point.value = integer*CONVERSION_FACTOR;
	return returned_point;
}

int to_integer_truncated(fixed_point fixed_point)
{
	return fixed_point.value/CONVERSION_FACTOR;
}

int to_integer_rounded(fixed_point fixed_point)
{
  return (fixed_point.value >= 0) ? fixed_point.value + (CONVERSION_FACTOR/2)
                                  : fixed_point.value - (CONVERSION_FACTOR/2);
}

fixed_point add_fixed_points(fixed_point x, fixed_point y)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = x.value + y.value;
  return new_fixed_point;
}

fixed_point subtract_fixed_points(fixed_point x, fixed_point y)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = x.value - y.value;
  return new_fixed_point;
}

fixed_point add_fixed_point_to_integer(fixed_point x, int n)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = x.value + (n*CONVERSION_FACTOR);
  return new_fixed_point;
}

fixed_point subtract_integer_from_fixed_point(fixed_point x, int n)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = x.value - (n*CONVERSION_FACTOR);
  return new_fixed_point;
}

fixed_point multiply_fixed_points(fixed_point x, fixed_point y)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = (((int_64_t) x.value) * y.value) / CONVERSION_FACTOR;
  return new_fixed_point;
}

fixed_point multiply_fixed_point_by_integer(fixed_point x, int n)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = x.value * n;
  return new_fixed_point;
}

fixed_point divide_fixed_points(fixed_point x, fixed_point y)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = ((int_64_t) x.value) * CONVERSION_FACTOR / y.value;
  return new_fixed_point
}

fixed_point divide_fixed_point_by_integer(fixed_point x, int n)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = x.value / n;
  return new_fixed_point;
}