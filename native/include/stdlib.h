#ifndef SCP_STDLIB_H
#define SCP_STDLIB_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RAND_MAX 32767
void *malloc(size_t); void free(void *); void *calloc(size_t,size_t); void *realloc(void *,size_t);
int abs(int); int atoi(const char *); unsigned long strtoul(const char *,char **,int);
int rand(void); void srand(unsigned);
void abort(void) __attribute__((noreturn));
void exit(int) __attribute__((noreturn));
int atexit(void (*)(void));
#ifdef __cplusplus
}
#endif
#endif
