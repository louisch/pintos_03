#include "fixed_point.h"

#include <stdint.h>

static const int TAIL_DIGITS = 14;

inline int
conversion_factor (void)
{
  return 1 << TAIL_DIGITS;
}

fixed_point
to_fixed_point (int integer)
{
	fixed_point returned_point;
	returned_point.value = integer * conversion_factor ();
	return returned_point;
}

int
to_integer_truncated (fixed_point fixed_point)
{
	return fixed_point.value / conversion_factor ();
}

int
to_integer_rounded (fixed_point fixed_point)
{
  return
    ((fixed_point.value >= 0) ? fixed_point.value + conversion_factor () / 2
                              : fixed_point.value - conversion_factor () / 2) /
    conversion_factor ();
}

fixed_point
fixed_point_add (fixed_point x, fixed_point y)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = x.value + y.value;
  return new_fixed_point;
}

fixed_point
fixed_point_subtract (fixed_point x, fixed_point y)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = x.value - y.value;
  return new_fixed_point;
}

fixed_point
fixed_point_addi (fixed_point x, int n)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = x.value + n * conversion_factor ();
  return new_fixed_point;
}

fixed_point
fixed_point_subtracti(fixed_point x, int n)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = x.value - n * conversion_factor ();
  return new_fixed_point;
}

fixed_point
fixed_point_multiply (fixed_point x, fixed_point y)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = ((int64_t) x.value) * y.value / conversion_factor ();
  return new_fixed_point;
}

fixed_point
fixed_point_multiplyi (fixed_point x, int n)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = x.value * n;
  return new_fixed_point;
}

fixed_point
fixed_point_divide (fixed_point x, fixed_point y)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = ((int64_t) x.value) * conversion_factor () / y.value;
  return new_fixed_point;
}

fixed_point
fixed_point_dividei (fixed_point x, int n)
{
  fixed_point new_fixed_point;
  new_fixed_point.value = x.value / n;
  return new_fixed_point;
}
