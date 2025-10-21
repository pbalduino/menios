#ifndef MENIOS_INCLUDE_MATH_H
#define MENIOS_INCLUDE_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

#define HUGE_VAL   (__builtin_huge_val())
#define HUGE_VALF  (__builtin_huge_valf())
#define HUGE_VALL  (__builtin_huge_vall())
#define INFINITY   (__builtin_inf())
#define NAN        (__builtin_nan(""))

#define signbit(x) __builtin_signbit(x)

double fabs(double x);
float fabsf(float x);
long double fabsl(long double x);
double ldexp(double x, int exp);
float ldexpf(float x, int exp);
long double ldexpl(long double x, int exp);
double frexp(double x, int* exp);
float frexpf(float x, int* exp);
long double frexpl(long double x, int* exp);
double modf(double x, double* iptr);
float modff(float x, float* iptr);
long double modfl(long double x, long double* iptr);

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_MATH_H */
