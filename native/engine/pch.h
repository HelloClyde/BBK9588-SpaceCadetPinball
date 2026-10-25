/* Native platform boundary; engine logic retains upstream implementation. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "../mini_stl.h"
#include "../native_types.h"
typedef char *LPSTR;
typedef const char *LPCSTR;
constexpr char PathSeparator='/';
extern "C" void scp_runtime_panic(const char *);
#define assertm(exp,msg) do { if(!(exp)) scp_runtime_panic(msg); } while(0)
#define assert(exp) assertm(exp,#exp)
inline float RandFloat(){return static_cast<float>(rand()/static_cast<double>(RAND_MAX));}
template<class T> constexpr int Sign(T value){return (T(0)<value)-(value<T(0));}
template<class T> const T& Clamp(const T& n,const T& lo,const T& hi){return std::max(lo,std::min(n,hi));}
inline FILE *fopenu(const char *path,const char *mode){return fopen(path,mode);}
constexpr float Pi=3.14159265358979323846f;
