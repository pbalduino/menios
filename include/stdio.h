#ifndef _INCLUDE_STDIO_H_
#define _INCLUDE_STDIO_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdio_constants.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

struct __sFile {
  int           fd;
  unsigned int  flags;
  unsigned char*buffer;
  size_t        buffer_size;
  size_t        buffer_pos;
  size_t        buffer_end;
  off_t         offset;
  int           error_number;
  int           last_op;
};

typedef struct __sFile FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int getchar();

char* gets(char* str);

int putchar(int character);

int puts(const char* str);

__attribute__ ((format (printf, 1, 2)))
int printf(const char* format, ...);
int vprintf(const char *format, va_list arg);
int scanf(const char *format, ...);
int vscanf(const char* format, va_list arg);
int sscanf(const char* str, const char* format, ...);
int vsscanf(const char* str, const char* format, va_list arg);
int fscanf(FILE* stream, const char* format, ...);
int vfscanf(FILE* stream, const char* format, va_list arg);
int vfprintf(FILE* stream, const char* format, va_list arg);

int sprintf(char *str, const char *format, ...);
int svprintf(char *str, const char *format, va_list arg);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list arg);
long ftell(FILE* stream);
int fseek(FILE* stream, long offset, int whence);
int remove(const char* path);
int rename(const char* oldpath, const char* newpath);
int fflush(FILE* stream);

FILE* fopen(const char *filename, const char *mode);
int   fclose(FILE *stream);
int   fprintf(FILE *stream, const char *format, ...);
int   fputs(const char *text, FILE* file);
int   fputc(int ch, FILE* file);
int fvprintf(FILE *stream, const char *format, va_list arg);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);
int feof(FILE* stream);
void rewind(FILE* stream);

FILE* freopen(const char *filename, const char *mode, FILE *file);

void perror(const char *s);

#endif
