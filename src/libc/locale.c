#ifndef MENIOS_KERNEL

#include <limits.h>
#include <locale.h>
#include <string.h>

static char current_locale[8] = "C";

static struct lconv current_localeconv = {
  .decimal_point = ".",
  .thousands_sep = "",
  .grouping = "",
  .int_curr_symbol = "",
  .currency_symbol = "",
  .mon_decimal_point = "",
  .mon_thousands_sep = "",
  .mon_grouping = "",
  .positive_sign = "",
  .negative_sign = "",
  .int_frac_digits = CHAR_MAX,
  .frac_digits = CHAR_MAX,
  .p_cs_precedes = CHAR_MAX,
  .p_sep_by_space = CHAR_MAX,
  .n_cs_precedes = CHAR_MAX,
  .n_sep_by_space = CHAR_MAX,
  .p_sign_posn = CHAR_MAX,
  .n_sign_posn = CHAR_MAX,
  .int_p_cs_precedes = CHAR_MAX,
  .int_p_sep_by_space = CHAR_MAX,
  .int_n_cs_precedes = CHAR_MAX,
  .int_n_sep_by_space = CHAR_MAX,
  .int_p_sign_posn = CHAR_MAX,
  .int_n_sign_posn = CHAR_MAX,
};

char* setlocale(int category, const char* locale) {
  if(category < LC_CTYPE || category > LC_ALL) {
    return NULL;
  }

  if(locale == NULL) {
    return current_locale;
  }

  if(locale[0] == '\0' || strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0) {
    strcpy(current_locale, "C");
    return current_locale;
  }

  return NULL;
}

struct lconv* localeconv(void) {
  return &current_localeconv;
}

#endif
