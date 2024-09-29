#ifndef _INCLUDE_STDIO_H_
#define _INCLUDE_STDIO_H_

#include <stdarg.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

struct __sFile {
  int reserved;
};

typedef struct __sFile FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int printf(const char *format, ...);
int scanf(const char *format, ...);

int getchar();

char* gets(char* str);

int putchar(int character);

int puts(const char* str);

__attribute__ ((format (printf, 1, 2)))
int printf(const char* format, ...);
int vprintf(const char *format, va_list arg);

int sprintf(char *str, const char *format, ...);
int svprintf(char *str, const char *format, va_list arg);

FILE* fopen(const char *filename, const char *mode);
int   fclose(FILE *stream);
int   fprintf(FILE *stream, const char *format, ...);
int   fscanf(FILE *stream, const char *format, ...);
int   fputs(const char *text, FILE* file);
int fvprintf(FILE *stream, const char *format, va_list arg);

FILE* freopen(const char *filename, const char *mode, FILE *file);

void perror(const char *s);

#endif
