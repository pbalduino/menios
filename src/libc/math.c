#include <math.h>
#include <float.h>
#include <stdint.h>

#ifndef MENIOS_KERNEL

double fabs(double x) {
  union {
    double d;
    uint64_t u;
  } value = { x };
  value.u &= 0x7FFFFFFFFFFFFFFFULL;
  return value.d;
}

float fabsf(float x) {
  union {
    float f;
    uint32_t u;
  } value = { x };
  value.u &= 0x7FFFFFFFU;
  return value.f;
}

long double fabsl(long double x) {
#if LDBL_MANT_DIG == 53
  return (long double)fabs((double)x);
#elif LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384
  union {
    long double ld;
    struct {
      uint64_t mantissa;
      uint16_t exponent;
    } __attribute__((packed)) parts;
  } value = { x };
  value.parts.exponent &= (uint16_t)0x7FFFu;
  return value.ld;
#else
  union {
    long double ld;
    unsigned char bytes[sizeof(long double)];
  } value = { x };
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  value.bytes[sizeof(long double) - 1] &= (unsigned char)0x7F;
#else
  value.bytes[0] &= (unsigned char)0x7F;
#endif
  return value.ld;
#endif
}

double ldexp(double x, int exp) {
  return __builtin_ldexp(x, exp);
}

float ldexpf(float x, int exp) {
  return __builtin_ldexpf(x, exp);
}

long double ldexpl(long double x, int exp) {
  return __builtin_ldexpl(x, exp);
}

double frexp(double x, int* exp) {
  return __builtin_frexp(x, exp);
}

float frexpf(float x, int* exp) {
  return __builtin_frexpf(x, exp);
}

long double frexpl(long double x, int* exp) {
  return __builtin_frexpl(x, exp);
}

double modf(double x, double* iptr) {
  return __builtin_modf(x, iptr);
}

float modff(float x, float* iptr) {
  return __builtin_modff(x, iptr);
}

long double modfl(long double x, long double* iptr) {
  return __builtin_modfl(x, iptr);
}

#endif
