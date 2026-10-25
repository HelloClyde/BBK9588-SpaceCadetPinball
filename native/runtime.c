/* SPDX-License-Identifier: MIT
 * Freestanding libc services through the verified BBK 9588 SDK. */
#include "bda_memory.h"
#include "bda_filesystem.h"
#include "sdk_compat.h"
#include "bda_dialogs.h"
#include "bda_time.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdarg.h>

char scp_runtime_log[8192];
static size_t log_size;
static uint32_t *allocation_head;
void scp_runtime_log_reset(void){log_size=0;scp_runtime_log[0]=0;}
static void log_append(const char *s,size_t n){
    if(n>sizeof(scp_runtime_log)-1-log_size)n=sizeof(scp_runtime_log)-1-log_size;
    memcpy(scp_runtime_log+log_size,s,n);log_size+=n;scp_runtime_log[log_size]=0;
}
void *malloc(size_t n){
    uint32_t *p;if(n>0xfffffff0u)return 0;
    p=bda_alloc(n+16);if(!p||(uintptr_t)p==0xffffffffu)return 0;
    p[0]=(uint32_t)n;p[1]=0x53435031u;p[2]=0;p[3]=(uint32_t)(uintptr_t)allocation_head;
    if(allocation_head)allocation_head[2]=(uint32_t)(uintptr_t)p;
    allocation_head=p;return p+4;
}
void free(void *v){
    uint32_t *p,*before,*after;if(!v)return;p=(uint32_t*)v-4;
    before=(uint32_t*)(uintptr_t)p[2];after=(uint32_t*)(uintptr_t)p[3];
    if(before)before[3]=p[3];else allocation_head=after;
    if(after)after[2]=p[2];p[1]=0;bda_free(p);
}
void scp_runtime_release_all(void){while(allocation_head)free(allocation_head+4);}
void *calloc(size_t n,size_t size){void*p;if(size&&n>0xffffffffu/size)return 0;p=malloc(n*size);if(p)memset(p,0,n*size);return p;}
void *realloc(void*v,size_t n){void*p;size_t old;if(!v)return malloc(n);if(!n){free(v);return 0;}old=((uint32_t*)v)[-4];p=malloc(n);if(p){memcpy(p,v,old<n?old:n);free(v);}return p;}
void *memset(void*v,int c,size_t n){unsigned char*p=v;while(n--)*p++=(unsigned char)c;return v;}
void *memcpy(void*v,const void*s,size_t n){unsigned char*p=v;const unsigned char*q=s;while(n--)*p++=*q++;return v;}
void *memmove(void*v,const void*s,size_t n){unsigned char*p=v;const unsigned char*q=s;if(p<q)return memcpy(v,s,n);while(n){--n;p[n]=q[n];}return v;}
int memcmp(const void*a,const void*b,size_t n){const unsigned char*p=a,*q=b;while(n--){int d=*p++-*q++;if(d)return d;}return 0;}
void *memchr(const void*v,int c,size_t n){const unsigned char*p=v;while(n--){if(*p==(unsigned char)c)return (void*)p;++p;}return 0;}
size_t strlen(const char*s){const char*p=s;while(*p)++p;return (size_t)(p-s);}
int strcmp(const char*a,const char*b){while(*a&&*a==*b){++a;++b;}return (unsigned char)*a-(unsigned char)*b;}
int strncmp(const char*a,const char*b,size_t n){while(n--){int d=(unsigned char)*a-(unsigned char)*b;if(d||!*a)return d;++a;++b;}return 0;}
char *strcpy(char*d,const char*s){char*p=d;while((*p++=*s++));return d;}
char *strncpy(char*d,const char*s,size_t n){char*p=d;while(n&&*s){*p++=*s++;--n;}while(n--)*p++=0;return d;}
char *strcat(char*d,const char*s){strcpy(d+strlen(d),s);return d;}
char *strncat(char*d,const char*s,size_t n){char*p=d+strlen(d);while(n--&&*s)*p++=*s++;*p=0;return d;}
char *strchr(const char*s,int c){do{if(*s==(char)c)return(char*)s;}while(*s++);return 0;}
char *strrchr(const char*s,int c){const char*p=0;do{if(*s==(char)c)p=s;}while(*s++);return(char*)p;}
char *strstr(const char*s,const char*q){size_t n=strlen(q);if(!n)return(char*)s;while(*s){if(!strncmp(s,q,n))return(char*)s;++s;}return 0;}
int tolower(int c){return c>='A'&&c<='Z'?c+32:c;}int toupper(int c){return c>='a'&&c<='z'?c-32:c;}
int abs(int v){return v<0?-v:v;}
static unsigned random_state=1;
void srand(unsigned seed){random_state=seed;}
int rand(void){random_state=random_state*214013u+2531011u;return (int)((random_state>>16)&32767u);}
unsigned long strtoul(const char*s,char**end,int base){
    unsigned long v=0;int sign=0;const char*start=s;
    while(*s==' '||*s=='\t'||*s=='\r'||*s=='\n')++s;
    if(*s=='-'||*s=='+')sign=*s++=='-';
    if((!base||base==16)&&s[0]=='0'&&(s[1]=='x'||s[1]=='X')){base=16;s+=2;}
    if(!base)base=*s=='0'?8:10;
    if(base<2||base>36){if(end)*end=(char*)start;return 0;}
    while(*s){unsigned d=*s>='0'&&*s<='9'?(unsigned)(*s-'0'):*s>='A'&&*s<='Z'?(unsigned)(*s-'A'+10):*s>='a'&&*s<='z'?(unsigned)(*s-'a'+10):255u;if(d>=(unsigned)base)break;v=v*base+d;++s;}
    if(end)*end=(char*)s;return sign?0-v:v;
}
int atoi(const char*s){return(int)strtoul(s,0,10);}

static FILE console={0,0,0,0,0,0,0};FILE*stderr=&console,*stdout=&console;
static unsigned char *resource_pack;
static size_t resource_pack_size;
static unsigned resource_count;
static unsigned resource_u32(const unsigned char *p){return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static int resource_name_equal(const char *a,const char *b){
    while(*a&&*b){if(tolower((unsigned char)*a)!=tolower((unsigned char)*b))return 0;++a;++b;}return *a==*b;
}
static const char *resource_basename(const char *path){const char *base=path;for(;*path;++path)if(*path=='/'||*path=='\\')base=path+1;return base;}
void scp_resource_unmount(void){if(resource_pack)free(resource_pack);resource_pack=0;resource_pack_size=0;resource_count=0;}
int scp_resource_mount(const char *path){
    char normalized[512];int handle,bytes,read;unsigned char *data;unsigned count;
    if(resource_pack||!path)return 0;
    size_t n=strlen(path);if(n>=sizeof(normalized))return 0;
    for(size_t i=0;i<=n;++i)normalized[i]=path[i]=='/'?'\\':path[i];
    handle=bda_fs_fopen_raw(normalized,"rb");if(!bda_fs_file_is_valid(handle))return 0;
    bytes=bda_fs_seek_raw(handle,0,SEEK_END);
    if(bytes<16||bytes>4*1024*1024||bda_fs_seek_raw(handle,0,SEEK_SET)!=0){bda_fs_close_raw(handle);return 0;}
    data=malloc((size_t)bytes);if(!data){bda_fs_close_raw(handle);return 0;}
    read=bda_fs_read_raw(handle,data,(size_t)bytes);bda_fs_close_raw(handle);
    if(read!=bytes||memcmp(data,"SCPAK01\0",8)){free(data);return 0;}
    count=resource_u32(data+8);
    if(count>128||16u+count*40u>(unsigned)bytes){free(data);return 0;}
    unsigned payload_begin=16u+count*40u;
    for(unsigned i=0;i<count;++i){
        const unsigned char *entry=data+16u+i*40u;unsigned offset=resource_u32(entry+32),size=resource_u32(entry+36);
        if(!entry[0]||!memchr(entry,0,32)||offset<payload_begin||(offset&3u)||offset>(unsigned)bytes||size>(unsigned)bytes-offset){free(data);return 0;}
        for(unsigned j=0;j<i;++j)if(resource_name_equal((const char*)entry,(const char*)(data+16u+j*40u))){free(data);return 0;}
    }
    resource_pack=data;resource_pack_size=(size_t)bytes;resource_count=count;return 1;
}
static FILE *resource_open(const char *path){
    const char *name=resource_basename(path);
    for(unsigned i=0;i<resource_count;++i){
        const unsigned char *entry=resource_pack+16u+i*40u;
        if(resource_name_equal(name,(const char*)entry)){
            FILE *f=calloc(1,sizeof(*f));if(!f)return 0;
            f->cache=resource_pack+resource_u32(entry+32);f->cache_size=resource_u32(entry+36);f->cache_borrowed=1;return f;
        }
    }
    return 0;
}
FILE *fopen(const char*path,const char*mode){
    char normalized[512];size_t n=strlen(path);int handle;FILE*f;
    if(n>=sizeof(normalized))return 0;
    if(resource_pack&&mode[0]=='r'&&!strchr(mode,'+')){f=resource_open(path);if(f)return f;}
    for(size_t i=0;i<=n;++i)normalized[i]=path[i]=='/'?'\\':path[i];
    handle=bda_fs_fopen_raw(normalized,mode);if(!bda_fs_file_is_valid(handle))return 0;
    f=calloc(1,sizeof(*f));if(!f){bda_fs_close_raw(handle);return 0;}f->handle=handle;
    /* DAT parsing performs thousands of two/four-byte reads. On the firmware
     * file API those are expensive NAND operations. Read bounded immutable
     * resources once; the parser then uses the original stdio interface. */
    if(mode[0]=='r'&&!strchr(mode,'+')){
        int bytes=bda_fs_seek_raw(handle,0,SEEK_END);
        int rewound=bda_fs_seek_raw(handle,0,SEEK_SET);
        if(bytes>=0&&bytes<=4*1024*1024&&rewound==0){
            unsigned char*data=malloc(bytes?(size_t)bytes:1u);
            if(data){
                int read=bytes?bda_fs_read_raw(handle,data,(size_t)bytes):0;
                if(read==bytes){
                    bda_fs_close_raw(handle);f->handle=0;f->cache=data;f->cache_size=(size_t)bytes;
                }else{
                    free(data);
                    if(bda_fs_seek_raw(handle,0,SEEK_SET)<0){bda_fs_close_raw(handle);free(f);return 0;}
                }
            }
        }else if(rewound<0){bda_fs_close_raw(handle);free(f);return 0;}
    }
    return f;
}
int fclose(FILE*f){int result=0;if(!f||f==&console)return -1;if(f->cache){if(!f->cache_borrowed)free(f->cache);}else result=bda_fs_close_raw(f->handle);free(f);return result;}
size_t fread(void*p,size_t size,size_t n,FILE*f){
    int r;if(!f||!size||!n||n>0xffffffffu/size)return 0;
    if(f->cache){
        size_t requested=size*n,available=f->position<f->cache_size?f->cache_size-f->position:0;
        size_t bytes=requested<available?requested:available;
        if(bytes)memcpy(p,f->cache+f->position,bytes);f->position+=bytes;
        if(bytes<requested)f->eof=1;
        return bytes/size;
    }
    r=bda_fs_fread_raw(p,size,n,f->handle);if(r<0){f->error=1;return 0;}if((size_t)r<n)f->eof=1;return(size_t)r;
}
size_t fwrite(const void*p,size_t size,size_t n,FILE*f){int r;if(!f||!size||!n||n>0xffffffffu/size)return 0;if(f==&console){log_append(p,size*n);return n;}if(f->cache){f->error=1;return 0;}r=bda_fs_fwrite_raw(p,size,n,f->handle);if(r<0){f->error=1;return 0;}return(size_t)r;}
int fgetc(FILE*f){unsigned char c;return fread(&c,1,1,f)==1?c:EOF;}
int fseek(FILE*f,long offset,int where){
    if(!f)return -1;
    if(f->cache){
        int64_t base=where==SEEK_SET?0:where==SEEK_CUR?(int64_t)f->position:where==SEEK_END?(int64_t)f->cache_size:-1;
        if(base<0)return -1;int64_t position=base+(int64_t)offset;
        if(position<0||position>0x7fffffffu)return -1;f->position=(size_t)position;
    }else if(bda_fs_seek_raw(f->handle,offset,where)<0)return -1;
    f->eof=0;return 0;
}
long ftell(FILE*f){return f?(f->cache?(long)f->position:bda_fs_tell_raw(f->handle)):-1;}
int ferror(FILE*f){return f?f->error:1;}int feof(FILE*f){return f?f->eof:0;}
int fflush(FILE*f){(void)f;bda_fs_flush_all();return 0;}

typedef struct{char*p;size_t capacity,used;} Formatter;
static void emit(Formatter*f,char c){if(f->used+1<f->capacity)f->p[f->used]=c;++f->used;}
int vsnprintf(char*p,size_t capacity,const char*fmt,va_list args){
    Formatter f={p,capacity,0};
    while(*fmt){
        if(*fmt!='%'){emit(&f,*fmt++);continue;}++fmt;
        int left=0,zero=0,width=0,precision=-1,length=0;
        while(*fmt=='-'||*fmt=='0'||*fmt=='+'||*fmt==' '||*fmt=='#'){if(*fmt=='-')left=1;if(*fmt=='0')zero=1;++fmt;}
        if(*fmt=='*'){width=va_arg(args,int);++fmt;}else while(*fmt>='0'&&*fmt<='9')width=width*10+*fmt++-'0';
        if(*fmt=='.'){++fmt;precision=0;if(*fmt=='*'){precision=va_arg(args,int);++fmt;}else while(*fmt>='0'&&*fmt<='9')precision=precision*10+*fmt++-'0';}
        if(*fmt=='z'){length=1;++fmt;}else if(*fmt=='l'){length=1;++fmt;if(*fmt=='l'){length=2;++fmt;}}else if(*fmt=='h'){++fmt;if(*fmt=='h')++fmt;}
        char kind=*fmt++,digits[70],sign=0;const char*s=digits;int n=0;
        if(kind=='%'){emit(&f,'%');continue;}
        if(kind=='s'){s=va_arg(args,const char*);if(!s)s="(null)";n=(int)strlen(s);if(precision>=0&&n>precision)n=precision;}
        else if(kind=='c'){digits[0]=(char)va_arg(args,int);n=1;}
        else if(strchr("diuxXp",kind)){
            uint64_t value;unsigned base=kind=='x'||kind=='X'||kind=='p'?16u:10u;
            if(kind=='d'||kind=='i'){int64_t v=length==2?va_arg(args,long long):length==1?va_arg(args,long):va_arg(args,int);if(v<0){sign='-';value=0-(uint64_t)v;}else value=(uint64_t)v;}
            else value=kind=='p'?(uintptr_t)va_arg(args,void*):length==2?va_arg(args,unsigned long long):length==1?va_arg(args,unsigned long):va_arg(args,unsigned);
            do{unsigned d=(unsigned)(value%base);digits[n++]=(char)(d<10?'0'+d:(kind=='X'?'A':'a')+d-10);value/=base;}while(value);
            for(int i=0;i<n/2;++i){char c=digits[i];digits[i]=digits[n-1-i];digits[n-1-i]=c;}
        }else if(kind=='f'||kind=='g'){
            double value=va_arg(args,double);int decimals=precision>=0?precision:6;if(decimals>7)decimals=7;
            if(value<0){sign='-';value=-value;}unsigned whole=(unsigned)value,scale=1;for(int i=0;i<decimals;++i)scale*=10;
            unsigned fraction=(unsigned)((value-whole)*scale+0.5);if(fraction>=scale){++whole;fraction=0;}
            char temp[16];int count=0;do{temp[count++]=(char)('0'+whole%10);whole/=10;}while(whole);while(count)digits[n++]=temp[--count];
            if(decimals){digits[n++]='.';for(unsigned div=scale/10;div;div/=10)digits[n++]=(char)('0'+fraction/div%10);}
        }else{emit(&f,'?');continue;}
        int padding=width-n-(sign?1:0);if(padding<0)padding=0;
        if(!left&&!zero)while(padding-->0)emit(&f,' ');if(sign)emit(&f,sign);
        if(!left&&zero)while(padding-->0)emit(&f,'0');for(int i=0;i<n;++i)emit(&f,s[i]);if(left)while(padding-->0)emit(&f,' ');
    }
    if(capacity)p[f.used<capacity?f.used:capacity-1]=0;return(int)f.used;
}
int snprintf(char*p,size_t n,const char*fmt,...){va_list a;va_start(a,fmt);int r=vsnprintf(p,n,fmt,a);va_end(a);return r;}
int sprintf(char*p,const char*fmt,...){va_list a;va_start(a,fmt);int r=vsnprintf(p,0x7fffffffu,fmt,a);va_end(a);return r;}
int fprintf(FILE*f,const char*fmt,...){char p[768];va_list a;va_start(a,fmt);int r=vsnprintf(p,sizeof(p),fmt,a);va_end(a);fwrite(p,1,strlen(p),f);return r;}
int printf(const char*fmt,...){char p[768];va_list a;va_start(a,fmt);int r=vsnprintf(p,sizeof(p),fmt,a);va_end(a);log_append(p,strlen(p));return r;}
int putchar(int c){char b=(char)c;log_append(&b,1);return c;}int puts(const char*s){log_append(s,strlen(s));putchar('\n');return 0;}
int sscanf(const char*s,const char*fmt,...){
    int count=0;va_list a;va_start(a,fmt);
    while(*fmt){if(*fmt==' '||*fmt=='\t'){while(*s==' '||*s=='\t')++s;++fmt;continue;}if(*fmt!='%'){if(*s++!=*fmt++)break;continue;}++fmt;
        if(*fmt=='d'||*fmt=='u'){char*end;unsigned long v=strtoul(s,&end,10);if(end==s)break;*va_arg(a,int*)=(int)v;s=end;++count;++fmt;continue;}break;}
    va_end(a);return count;
}

/* Float inputs in the engine are at table scale. IEEE-based square-root seed
 * followed by Newton steps keeps error below float rounding in this domain. */
float fabsf(float x){return x<0?-x:x;}double fabs(double x){return x<0?-x:x;}
float sqrtf(float x){if(x==0)return 0;if(x<0)return __builtin_nanf("");union{float f;uint32_t u;}v={x};v.u=(v.u>>1)+0x1fc00000u;float y=v.f;for(int i=0;i<6;++i)y=0.5f*(y+x/y);return y;}
double sqrt(double x){return(double)sqrtf((float)x);}
float floorf(float x){if(x>=2147483520.0f||x<=-2147483648.0f)return x;int i=(int)x;return x<(float)i?(float)(i-1):(float)i;}
double floor(double x){if(x>=2147483647.0||x<=-2147483648.0)return x;int i=(int)x;return x<i?(double)(i-1):(double)i;}
float ceilf(float x){return-floorf(-x);}double ceil(double x){return-floor(-x);}
float fmodf(float x,float y){if(y==0)return __builtin_nanf("");int n=(int)(x/y);return x-n*y;}
double fmod(double x,double y){return(double)fmodf((float)x,(float)y);}
static float angle_reduce(float x){const float pi=3.14159265358979323846f,tau=6.2831853071795864769f;x=fmodf(x,tau);if(x>pi)x-=tau;if(x<-pi)x+=tau;return x;}
float sinf(float x){x=angle_reduce(x);if(x>1.5707963267948966f)x=3.1415926535897932f-x;if(x<-1.5707963267948966f)x=-3.1415926535897932f-x;float z=x*x;return x*(1.0f+z*(-0.16666666666667f+z*(0.008333333333333f+z*(-0.0001984126984127f+z*(0.0000027557319224f+z*(-0.0000000250521084f+z*0.0000000001605904f))))));}
float cosf(float x){return sinf(x+1.5707963267948966f);}double sin(double x){return(double)sinf((float)x);}double cos(double x){return(double)cosf((float)x);}
static float atan_small(float x){float z=x*x,term=x,sum=x;for(int n=3;n<=23;n+=2){term=-term*z;sum+=term/n;}return sum;}
static float atan_positive(float x){if(x>1.0f)return 1.5707963267948966f-atan_positive(1.0f/x);if(x>0.41421356237f)return 0.7853981633974483f+atan_small((x-1.0f)/(x+1.0f));return atan_small(x);}
float acosf(float x){if(x<-1.0f||x>1.0f)return __builtin_nanf("");if(x<=-1.0f)return 3.1415926535897932f;if(x>=1.0f)return 0;if(x<0)return 3.1415926535897932f-acosf(-x);return 2.0f*atan_positive(sqrtf((1.0f-x)/(1.0f+x)));}
double acos(double x){return(double)acosf((float)x);}
float atan2f(float y,float x){if(x==0)return y>0?1.5707963267948966f:y<0?-1.5707963267948966f:0;float a=atan_positive(fabsf(y/x));if(x<0)a=3.1415926535897932f-a;return y<0?-a:a;}
double atan2(double y,double x){return(double)atan2f((float)y,(float)x);}

__attribute__((weak,noreturn)) void scp_runtime_panic(const char*message){
    FILE*f=fopen("A:\\PINBALL.LOG","wb");if(f){fwrite(scp_runtime_log,1,strlen(scp_runtime_log),f);fprintf(f,"\nFATAL: %s\n",message);fclose(f);}
    bda_msgbox("Space Cadet",message);for(;;)bda_sys_delay(1);
}
void abort(void){scp_runtime_panic("Engine allocation or bounds failure");}
void exit(int result){(void)result;scp_runtime_panic("Engine requested exit");}
