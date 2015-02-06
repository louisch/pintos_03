#ifndef __LIB_KERNEL_FIXED_POINT
#define __LIB_KERNEL_FIXED_POINT

typedef int fixed_point;

static const int TAIL_DIGITS = 14;

#define to_fixed_point(INTEGER) \
    ((fixed_point)(INTEGER << TAIL_DIGITS))

#define to_integer_truncated(FIXED_POINT) \
    FIXED_POINT.value >> TAIL_DIGITS)

#define to_integer_rounded(FIXED_POINT) \
    ((FIXED_POINT.value >= 0) ? FIXED_POINT.value + conversion_factor () / 2  \
                                  : FIXED_POINT.value - conversion_factor () / 2) >> \
    TAIL_DIGITS)

#define to_integer_add(FIXED_POINT_A, FIXED_POINT_B) \
    (FIXED_POINT_A.value + FIXED_POINT_B.value) 

#define fixed_point_subtract(FIXED_POINT_A, FIXED_POINT_B) \
    (FIXED_POINT_A.value - FIXED_POINT_B.value)

#define fixed_point_addi(FIXED_POINT, INT) \
    (FIXED_POINT.value + INT << TAIL_DIGITS)

#define fixed_point_subtracti(FIXED_POINT, INT) \
    (FIXED_POINT.value - INT << TAIL_DIGITS)

#define fixed_point_multiply(FIXED_POINT_A, FIXED_POINT_B) \
    (FIXED_POINT_A.value * FIXED_POINT_B.value >> TAIL_DIGITS)

#define fixed_point_multiplyi(FIXED_POINT, INT) \
    (FIXED_POINT.value * INT)

#define fixed_point_divide(FIXED_POINT_A, FIXED_POINT_B) \
    (((int64_t) FIXED_POINT_A.value) << TAIL_DIGITS / FIXED_POINT_B)

#define fixed_point_dividei(FIXED_POINT, INT) \
    (FIXED_POINT.value / INT)
#endif