#include "mini_stl.h"
extern "C" void scp_runtime_panic(const char*) __attribute__((noreturn));
void*operator new(size_t n){void*p=malloc(n?n:1);if(!p)scp_runtime_panic("Out of memory");return p;}
void*operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{free(p);}void operator delete[](void*p)noexcept{free(p);}
void operator delete(void*p,size_t)noexcept{free(p);}void operator delete[](void*p,size_t)noexcept{free(p);}
extern "C" {
void*__dso_handle;
void __cxa_pure_virtual(void){scp_runtime_panic("Pure virtual engine call");}
int __cxa_guard_acquire(unsigned long long*g){return !*(unsigned char*)g;}
void __cxa_guard_release(unsigned long long*g){*(unsigned char*)g=1;}
void __cxa_guard_abort(unsigned long long*g){(void)g;}
struct Destructor{void(*fn)(void*);void*object;};
static Destructor destructors[1024];static unsigned destructor_count;
int __cxa_atexit(void(*fn)(void*),void*object,void*dso){(void)dso;if(destructor_count>=1024)return -1;destructors[destructor_count++]={fn,object};return 0;}
static void c_atexit_wrapper(void*v){reinterpret_cast<void(*)(void)>(v)();}
int atexit(void(*fn)(void)){return __cxa_atexit(c_atexit_wrapper,reinterpret_cast<void*>(fn),nullptr);}
extern void(*__init_array_start[])(void);extern void(*__init_array_end[])(void);
extern void(*__ctors_start[])(void);extern void(*__ctors_end[])(void);
void scp_runtime_init(void){
    // MIPS bare-metal GCC uses .ctors rather than .init_array. The legacy
    // convention runs pointers from the end, skipping optional CRT sentinels.
    for(auto p=__ctors_end;p!=__ctors_start;){auto fn=*--p;if(fn&&(uintptr_t)fn!=(uintptr_t)-1)fn();}
    for(auto p=__init_array_start;p!=__init_array_end;++p)if(*p)(*p)();
}
void scp_runtime_release_all(void);
void scp_runtime_fini(void){while(destructor_count){auto d=destructors[--destructor_count];d.fn(d.object);}scp_runtime_release_all();}
}
