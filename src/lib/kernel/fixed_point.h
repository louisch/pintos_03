#ifndef __LIB_KERNEL_FIXED_POINT
#define __LIB_KERNEL_FIXED_POINT

#include <stdint.h>

typedef int fixed_point;

static const int TAIL_DIGITS = 14;

#define conversion_factor() \
        (1 << TAIL_DIGITS)        

#define to_fixed_point(INTEGER) \
        ((INTEGER * conversion_factor()))

#define to_integer_truncated(FIXED_POINT) \
        (FIXED_POINT / conversion_factor())

#define to_integer_rounded(FIXED_POINT) \
        (((FIXED_POINT >= 0) ? (FIXED_POINT + conversion_factor() / 2)  \
                             : (FIXED_POINT - conversion_factor() / 2)) / \
        conversion_factor())

#define fixed_point_add(FIXED_POINT_A, FIXED_POINT_B) \
        (FIXED_POINT_A + FIXED_POINT_B) 

#define fixed_point_subtract(FIXED_POINT_A, FIXED_POINT_B) \
        (FIXED_POINT_A - FIXED_POINT_B)

#define fixed_point_addi(FIXED_POINT, INT) \
       (FIXED_POINT + (INT * conversion_factor()))

#define fixed_point_subtracti(FIXED_POINT, INT) \
        (FIXED_POINT - (INT * conversion_factor()))

#define fixed_point_multiply(FIXED_POINT_A, FIXED_POINT_B) \
        (FIXED_POINT_A * FIXED_POINT_B / conversion_factor())

#define fixed_point_multiplyi(FIXED_POINT, INT) \
        (FIXED_POINT * INT)

#define fixed_point_divide(FIXED_POINT_A, FIXED_POINT_B) \
        (((int64_t) FIXED_POINT_A) * conversion_factor() / FIXED_POINT_B)

#define fixed_point_dividei(FIXED_POINT, INT) \
        (FIXED_POINT / INT)

#endif