/* Host-only preinclude for testing the pure CPU resource/graphics path. */
#pragma once
#define PCH_H
#include <cstdio>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <vector>
#include <algorithm>
#include <cstring>
#include <string>
#include <cstdlib>
#include <cctype>
#include <map>
#include <unordered_map>
#include <functional>
#include "native_types.h"
typedef char* LPSTR;
typedef const char* LPCSTR;
inline FILE* fopenu(const char* path,const char* mode) { return fopen(path,mode); }
#define assertm(exp,msg) assert(((void)msg,exp))
extern "C" void scp_runtime_panic(const char*);
constexpr char PathSeparator='/';
constexpr float Pi=3.14159265358979323846f;
using std::isnan;
inline float RandFloat() { return static_cast<float>(std::rand()/static_cast<double>(RAND_MAX)); }
template<class T> constexpr int Sign(T value) { return (T(0)<value)-(value<T(0)); }
template<class T> const T& Clamp(const T& x,const T& a,const T& b) { return std::max(a,std::min(x,b)); }
