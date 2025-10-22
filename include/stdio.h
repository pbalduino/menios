#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdio_constants.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct __menios_FILE FILE;

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
int vsprintf(char *str, const char *format, va_list arg);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list arg);
long ftell(FILE* stream);
int fseek(FILE* stream, long offset, int whence);
int remove(const char* path);
int rename(const char* oldpath, const char* newpath);
int fflush(FILE* stream);

FILE* fopen(const char *filename, const char *mode);
FILE* fdopen(int fd, const char* mode);
int   fclose(FILE *stream);
int   fprintf(FILE *stream, const char *format, ...);
int   fputs(const char *text, FILE* file);
int   fputc(int ch, FILE* file);
int   putc(int ch, FILE* file);
int   fgetc(FILE* file);
int   getc(FILE* file);
char* fgets(char* str, int size, FILE* file);
int   ungetc(int ch, FILE* file);
int fvprintf(FILE *stream, const char *format, va_list arg);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);
int feof(FILE* stream);
void rewind(FILE* stream);
int fileno(FILE* stream);

FILE* freopen(const char *filename, const char *mode, FILE *file);

void perror(const char *s);

#endif
