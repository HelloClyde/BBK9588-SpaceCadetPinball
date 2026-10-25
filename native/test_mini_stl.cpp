#include "mini_stl.h"
int main(){
    std::vector<std::string> v;
    for(int i=0;i<100;++i){char s[20];snprintf(s,sizeof(s),"entry%d",i);v.push_back(std::string(s));}
    if(v.size()!=100||v[33]!="entry33")return 1;
    auto copied=v;v.erase(v.begin()+3);if(v.size()!=99||v[3]!="entry4"||copied[3]!="entry3")return 2;
    v.resize(120);v[119]="tail";if(v.back()!="tail")return 3;
    std::vector<int> numbers{5,1,4,3,2};std::sort(numbers.begin(),numbers.end());
    for(int i=0;i<5;++i)if(numbers[i]!=i+1)return 4;
    std::sort(numbers.begin(),numbers.end(),[](int a,int b){return a>b;});
    if(numbers[0]!=5||std::find(numbers.begin(),numbers.end(),3)==numbers.end())return 5;
    std::string path="A:";path+="\\PINBALL";path+=std::string("\\PINBALL.DAT");
    auto moved=std::move(path);if(moved!="A:\\PINBALL\\PINBALL.DAT"||!path.empty())return 6;
    int one=1,two=2;std::reference_wrapper<int> refs[]={one,two};refs[1].get()=9;if(two!=9)return 7;
    int sum=0;for(auto it=numbers.rbegin();it!=numbers.rend();++it)sum=sum*10+*it;if(sum!=12345)return 8;
    std::string self="abcd";self+=self;if(self!="abcdabcd")return 9;self.insert(2,self.c_str()+4);if(self!="ababcdcdabcd")return 10;
    std::vector<std::string> alias;for(int i=0;i<4;++i)alias.push_back("ball");alias.push_back(alias[0]);if(alias[4]!="ball")return 11;
    printf("mini_stl: vector growth/copy/erase/resize, string move/path, sort/find/reference wrapper passed\n");
    return 0;
}
