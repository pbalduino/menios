#ifndef MENIOS_INCLUDE_CTYPE_H
#define MENIOS_INCLUDE_CTYPE_H

#include <stddef.h>

int isalnum(int c);
int isalpha(int c);
int isdigit(int c);
int isxdigit(int c);
int islower(int c);
int isupper(int c);
int isprint(int c);
int iscntrl(int c);
int ispunct(int c);
int isspace(int c);

int tolower(int c);
int toupper(int c);
#endif
