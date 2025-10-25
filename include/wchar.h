#ifndef MENIOS_INCLUDE_WCHAR_H
#define MENIOS_INCLUDE_WCHAR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int wint_t;

typedef struct {
  unsigned int __state;
} mbstate_t;

#define WEOF ((wint_t)-1)

static inline wint_t btowc(int c) {
  if(c == EOF) {
    return WEOF;
  }
  return (unsigned char)c;
}

static inline int wctob(wint_t wc) {
  if(wc == WEOF || wc > 0xff) {
    return EOF;
  }
  return (int)(unsigned char)wc;
}

static inline size_t mbrtowc(wchar_t* __restrict pwc,
                             const char* __restrict s,
                             size_t n,
                             mbstate_t* __restrict ps) {
  (void)ps;
  if(s == NULL || n == 0) {
    return (size_t)-1;
  }
  if(pwc != NULL) {
    *pwc = (unsigned char)s[0];
  }
  return (s[0] == '\0') ? 0 : 1;
}

static inline size_t wcrtomb(char* __restrict s,
                             wchar_t wc,
                             mbstate_t* __restrict ps) {
  (void)ps;
  if(s == NULL) {
    return 1;
  }
  s[0] = (char)wc;
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* MENIOS_INCLUDE_WCHAR_H */
