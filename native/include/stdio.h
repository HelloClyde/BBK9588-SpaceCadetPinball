#ifndef SCP_STDIO_H
#define SCP_STDIO_H
#include <stddef.h>
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    int handle,error;
    unsigned char *cache;
    size_t cache_size,position;
    int eof,cache_borrowed;
} FILE;
extern FILE *stderr,*stdout;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
FILE *fopen(const char *,const char *); int fclose(FILE *);
size_t fread(void *,size_t,size_t,FILE *); size_t fwrite(const void *,size_t,size_t,FILE *);
int fgetc(FILE *); int fseek(FILE *,long,int); long ftell(FILE *);
int snprintf(char *,size_t,const char *,...); int vsnprintf(char *,size_t,const char *,va_list);
int sprintf(char *,const char *,...); int fprintf(FILE *,const char *,...);
int printf(const char *,...); int puts(const char *); int putchar(int); int fflush(FILE *);
int sscanf(const char *,const char *,...); int ferror(FILE *); int feof(FILE *);
#ifdef __cplusplus
}
#endif
#endif
