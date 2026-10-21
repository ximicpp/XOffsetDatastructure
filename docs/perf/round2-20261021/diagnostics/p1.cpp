// P1 probe (diagnostic, not a candidate): fresh std::string + copy 17.22 MB in a new process,
// with in-timer page-preparation variants. usage: p1 <variant> ; prints "<ns> <minflt>"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <vector>
static long minflt(){rusage r{};getrusage(RUSAGE_SELF,&r);return r.ru_minflt;}
int main(int argc,char**argv){
  const size_t n=17215264; const std::string v=argv[1];
  std::vector<char> src(n); for(size_t i=0;i<n;i+=64) src[i]=char(i*31+7);
  const size_t pg=(size_t)sysconf(_SC_PAGESIZE);
  std::string hot; if(v=="resident"){ hot.resize(n); memset(hot.data(),1,n); }
  long f0=minflt(); auto a=std::chrono::steady_clock::now();
  if(v=="resident"){ memcpy(hot.data(),src.data(),n); }
  else {
    std::string s;
    s.resize_and_overwrite(n,[&](char*p,size_t m){
      auto adv=[&](char* b,size_t len,int how){ uintptr_t lo=(uintptr_t)b & ~(pg-1); uintptr_t hi=((uintptr_t)b+len+pg-1)&~(pg-1); madvise((void*)lo,hi-lo,how); };
      if(v=="base"){ memcpy(p,src.data(),m); }
      else if(v=="whole"){ adv(p,m,MADV_WILLNEED); memcpy(p,src.data(),m); }
      else if(v=="mlock"){ mlock(p,m); memcpy(p,src.data(),m); munlock(p,m); }
      else { size_t ch = v=="c4m"?(4u<<20): v=="c1m"?(1u<<20):(256u<<10);
        for(size_t off=0;off<m;off+=ch){ size_t len=std::min(ch,m-off); adv(p+off,len,MADV_WILLNEED); memcpy(p+off,src.data()+off,len);} }
      return m; });
    hot.swap(s);
  }
  auto b=std::chrono::steady_clock::now(); long f1=minflt();
  volatile char sink=hot[n/2]; (void)sink;
  printf("%lld %ld\n",(long long)std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count(),f1-f0);
}
