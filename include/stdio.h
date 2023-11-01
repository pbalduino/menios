#ifndef _STDIO_H
#define _STDIO_H_

#include <types.h>

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

FILE* fopen(const char *filename, const char *mode);
int   fclose(FILE *stream);
int   fprintf(FILE *stream, const char *format, ...);
int   fscanf(FILE *stream, const char *format, ...);
int   fputs(const char *text, FILE* file);

FILE* freopen(const char *filename, const char *mode, FILE *file);

#endif
