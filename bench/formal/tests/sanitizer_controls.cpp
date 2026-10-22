// Deliberate faults verify that the separate diagnostic profile can detect them.
// These are never linked into an adapter or a timing executable.
#include <climits>
#include <cstdlib>
#include <cstring>

__attribute__((noinline)) void leak_control(){
    void* p=std::malloc(123);
    if(!p)std::abort();
    std::memset(p,42,123);
    asm volatile("" : : "r"(p) : "memory");
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    if(std::strcmp(argv[1],"uaf")==0){
        volatile int* p=new int[1];p[0]=42;delete[] p;return p[0];
    }
    if(std::strcmp(argv[1],"overflow")==0){
        volatile int x=INT_MAX;volatile int y=1;return x+y;
    }
    if(std::strcmp(argv[1],"leak")==0){leak_control();return 0;}
    return 2;
}
