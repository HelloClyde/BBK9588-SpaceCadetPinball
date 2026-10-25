#ifndef SCP_MATH_H
#define SCP_MATH_H
#ifdef __cplusplus
extern "C" {
#endif
float sqrtf(float); double sqrt(double);
float sinf(float); double sin(double); float cosf(float); double cos(double);
float acosf(float); double acos(double); float atan2f(float,float); double atan2(double,double);
float floorf(float); double floor(double); float ceilf(float); double ceil(double);
float fabsf(float); double fabs(double);
float fmodf(float,float); double fmod(double,double);
#ifdef __cplusplus
}
/* Preserve the original C++ float overloads. Without these, physics expressions
 * would silently promote to software double precision on the MIPS target. */
inline float sqrt(float v){return sqrtf(v);}
inline float sin(float v){return sinf(v);}
inline float cos(float v){return cosf(v);}
inline float acos(float v){return acosf(v);}
inline float floor(float v){return floorf(v);}
inline float ceil(float v){return ceilf(v);}
inline float fabs(float v){return fabsf(v);}
inline float fmod(float a,float b){return fmodf(a,b);}
inline float atan2(float a,float b){return atan2f(a,b);}
#endif
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))
#define isnan(x) (__builtin_isnan(x))
#define isfinite(x) (__builtin_isfinite(x))
#endif
