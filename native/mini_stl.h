/* Small freestanding C++11 containers used by the original pinball engine.
 * Only the engine's actual API surface is supplied; no desktop C++ runtime. */
#ifndef SCP_MINI_STL_H
#define SCP_MINI_STL_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

void *operator new(size_t); void *operator new[](size_t);
void operator delete(void *) noexcept; void operator delete[](void *) noexcept;
inline void *operator new(size_t,void *p) noexcept{return p;}
inline void operator delete(void *,void *) noexcept{}

namespace std {
using ::size_t; using ::int8_t; using ::uint8_t; using ::int16_t; using ::uint16_t;
using ::int32_t; using ::uint32_t; using ::int64_t; using ::uint64_t;
using ::memcpy;using ::memmove;using ::memset;using ::memcmp;using ::strlen;
using ::strcmp;using ::strncmp;using ::strcpy;using ::strncpy;
using ::rand;using ::srand;using ::tolower;using ::toupper;
using ::sqrt;using ::floor;using ::ceil;using ::fabs;
inline float abs(float v){return v<0?-v:v;} inline double abs(double v){return v<0?-v:v;}
inline int abs(int v){return v<0?-v:v;}
template<class T> struct remove_reference{typedef T type;};
template<class T> struct remove_reference<T&>{typedef T type;};
template<class T> struct remove_reference<T&&>{typedef T type;};
template<class T> typename remove_reference<T>::type&& move(T&& t){return static_cast<typename remove_reference<T>::type&&>(t);}
template<class T> void swap(T& a,T& b){T tmp=std::move(a);a=std::move(b);b=std::move(tmp);}
template<class T> const T& min(const T& a,const T& b){return a<b?a:b;}
template<class T> const T& max(const T& a,const T& b){return a>b?a:b;}
template<class I,class T> I find(I first,I last,const T& value){while(first!=last&& !(*first==value))++first;return first;}
template<class I,class O,class F> O transform(I first,I last,O out,F f){while(first!=last)*out++=f(*first++);return out;}
template<class I,class F> void sort(I first,I last,F less){
    if(first==last)return;
    for(I i=first+1;i<last;++i){auto value=std::move(*i);I j=i;while(j>first&&less(value,*(j-1))){*j=std::move(*(j-1));--j;}*j=std::move(value);}
}
template<class I> void sort(I first,I last){
    if(first==last)return;
    for(I i=first+1;i<last;++i){auto value=std::move(*i);I j=i;while(j>first&&value<*(j-1)){*j=std::move(*(j-1));--j;}*j=std::move(value);}
}
template<class B,class D> struct is_base_of{static const bool value=__is_base_of(B,D);};
template<class T> class reference_wrapper {T* p;public:reference_wrapper(T& v):p(&v){} operator T&()const{return *p;}T& get()const{return *p;}};

template<class T> class initializer_list {
    const T *p_; size_t n_;
    constexpr initializer_list(const T *p,size_t n):p_(p),n_(n){}
public:
    constexpr initializer_list():p_(nullptr),n_(0){}
    constexpr const T* begin()const{return p_;}constexpr const T* end()const{return p_+n_;}
    constexpr size_t size()const{return n_;}
};

template<class T> class vector {
    T* p_;size_t size_,capacity_;
public:
    typedef T* iterator;typedef const T* const_iterator;
    class reverse_iterator{T*p;public:explicit reverse_iterator(T*v):p(v){}T&operator*()const{return *(p-1);}T*operator->()const{return p-1;}reverse_iterator&operator++(){--p;return *this;}bool operator!=(const reverse_iterator&other)const{return p!=other.p;}bool operator==(const reverse_iterator&other)const{return p==other.p;}};
    vector():p_(nullptr),size_(0),capacity_(0){}
    explicit vector(size_t n):vector(){resize(n);}
    vector(initializer_list<T> values):vector(){reserve(values.size());for(const T& v:values)push_back(v);}
    vector(const vector& v):vector(){reserve(v.size_);for(const T& t:v)push_back(t);}
    vector(vector&& v):p_(v.p_),size_(v.size_),capacity_(v.capacity_){v.p_=nullptr;v.size_=v.capacity_=0;}
    ~vector(){clear();::operator delete(p_);}
    vector& operator=(const vector& v){if(this!=&v){clear();reserve(v.size_);for(const T& t:v)push_back(t);}return *this;}
    vector& operator=(vector&& v){if(this!=&v){clear();::operator delete(p_);p_=v.p_;size_=v.size_;capacity_=v.capacity_;v.p_=nullptr;v.size_=v.capacity_=0;}return *this;}
    iterator begin(){return p_;}iterator end(){return p_+size_;}
    const_iterator begin()const{return p_;}const_iterator end()const{return p_+size_;}
    reverse_iterator rbegin(){return reverse_iterator(p_+size_);}reverse_iterator rend(){return reverse_iterator(p_);}
    size_t size()const{return size_;}bool empty()const{return size_==0;}
    T* data(){return p_;}const T* data()const{return p_;}
    T& operator[](size_t i){return p_[i];}const T& operator[](size_t i)const{return p_[i];}
    T& at(size_t i){if(i>=size_)abort();return p_[i];}const T& at(size_t i)const{if(i>=size_)abort();return p_[i];}
    T& back(){return p_[size_-1];}const T& back()const{return p_[size_-1];}
    T& front(){return p_[0];}const T& front()const{return p_[0];}
    void reserve(size_t n){
        if(n<=capacity_)return;if(n>((size_t)-1)/sizeof(T))abort();
        T* q=static_cast<T*>(::operator new(n*sizeof(T)));
        for(size_t i=0;i<size_;++i){new(q+i)T(std::move(p_[i]));p_[i].~T();}
        ::operator delete(p_);p_=q;capacity_=n;
    }
    void clear(){while(size_)p_[--size_].~T();}
    void shrink_to_fit(){}
    void resize(size_t n){if(n<size_){while(size_>n)p_[--size_].~T();}else{reserve(n);while(size_<n){new(p_+size_)T();++size_;}}}
    void push_back(const T& t){if(size_==capacity_){T saved(t);reserve(capacity_?capacity_*2:4);new(p_+size_)T(std::move(saved));}else new(p_+size_)T(t);++size_;}
    void push_back(T&& t){if(size_==capacity_){T saved(std::move(t));reserve(capacity_?capacity_*2:4);new(p_+size_)T(std::move(saved));}else new(p_+size_)T(std::move(t));++size_;}
    void pop_back(){if(size_)p_[--size_].~T();}
    iterator erase(iterator at){size_t index=at-p_;for(size_t i=index;i+1<size_;++i)p_[i]=std::move(p_[i+1]);pop_back();return p_+index;}
};

class string {
    char *p_;size_t size_,capacity_;
    void ensure(size_t n){if(n+1<=capacity_)return;size_t cap=n+1;char*q=static_cast<char*>(::operator new(cap));if(size_)memcpy(q,p_,size_);q[size_]=0;::operator delete(p_);p_=q;capacity_=cap;}
public:
    static const size_t npos=(size_t)-1;
    string():p_(nullptr),size_(0),capacity_(0){}
    string(const char*s):string(){assign(s?s:"");}
    string(const string&s):string(){assign(s.c_str());}
    string(string&&s):p_(s.p_),size_(s.size_),capacity_(s.capacity_){s.p_=nullptr;s.size_=s.capacity_=0;}
    ~string(){::operator delete(p_);}
    string& operator=(const string&s){if(this!=&s)assign(s.c_str());return *this;}
    string& operator=(string&&s){if(this!=&s){::operator delete(p_);p_=s.p_;size_=s.size_;capacity_=s.capacity_;s.p_=nullptr;s.size_=s.capacity_=0;}return *this;}
    string& operator=(const char*s){assign(s?s:"");return *this;}
    void assign(const char*s){size_t n=strlen(s),offset=0;bool own=p_&&(uintptr_t)s>=(uintptr_t)p_&&(uintptr_t)s<=(uintptr_t)(p_+size_);if(own)offset=s-p_;ensure(n);if(own)s=p_+offset;memmove(p_,s,n+1);size_=n;}
    const char*c_str()const{return p_?p_:"";}size_t size()const{return size_;}size_t length()const{return size_;}bool empty()const{return size_==0;}
    const char*begin()const{return c_str();}const char*end()const{return c_str()+size_;}
    char*begin(){ensure(size_);return p_;}char*end(){ensure(size_);return p_+size_;}
    char& operator[](size_t i){return p_[i];}const char& operator[](size_t i)const{return c_str()[i];}
    void clear(){size_=0;if(p_)p_[0]=0;}
    string& insert(size_t at,size_t n,char c){if(at>size_)at=size_;ensure(size_+n);memmove(p_+at+n,p_+at,size_-at+1);for(size_t i=0;i<n;++i)p_[at+i]=c;size_+=n;return *this;}
    string& insert(size_t at,const char*s){if(p_&&(uintptr_t)s>=(uintptr_t)p_&&(uintptr_t)s<=(uintptr_t)(p_+size_)){string copy(s);return insert(at,copy.c_str());}if(at>size_)at=size_;size_t n=strlen(s);ensure(size_+n);memmove(p_+at+n,p_+at,size_-at+1);memcpy(p_+at,s,n);size_+=n;return *this;}
    size_t find(char c,size_t start=0)const{for(size_t i=start;i<size_;++i)if(c_str()[i]==c)return i;return npos;}
    size_t rfind(char c)const{for(size_t i=size_;i;--i)if(c_str()[i-1]==c)return i-1;return npos;}
    string substr(size_t at,size_t n=npos)const{string s;if(at>size_)at=size_;if(n>size_-at)n=size_-at;s.ensure(n);memcpy(s.p_,c_str()+at,n);s.p_[n]=0;s.size_=n;return s;}
    string& operator+=(const char*s){size_t n=strlen(s),offset=0;bool own=p_&&(uintptr_t)s>=(uintptr_t)p_&&(uintptr_t)s<=(uintptr_t)(p_+size_);if(own)offset=s-p_;ensure(size_+n);if(own)s=p_+offset;memmove(p_+size_,s,n+1);size_+=n;return *this;}
    string& operator+=(const string&s){return *this+=s.c_str();}
    string& operator+=(char c){ensure(size_+1);p_[size_++]=c;p_[size_]=0;return *this;}
    friend string operator+(const string&a,const string&b){string s(a);s+=b;return s;}
    friend string operator+(const string&a,const char*b){string s(a);s+=b;return s;}
    friend string operator+(const char*a,const string&b){string s(a);s+=b;return s;}
    friend bool operator==(const string&a,const string&b){return strcmp(a.c_str(),b.c_str())==0;}
    friend bool operator!=(const string&a,const string&b){return !(a==b);}
    friend bool operator<(const string&a,const string&b){return strcmp(a.c_str(),b.c_str())<0;}
};
}
#endif
