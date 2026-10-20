// B3 (diagnostic): M4 floor of "fresh std::string + copy N bytes" in a new process.
// usage: floor <bytes> <mode: fresh|hot> -> prints ns of the timed section
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
int main(int argc,char**argv){
  const size_t n=strtoull(argv[1],0,10); const bool hot=argv[2][0]=='h';
  char hdr[88]={1}; std::vector<char> src(n); for(size_t i=0;i<n;i+=64) src[i]=char(i*31+7);  // resident source (like the arena)
  std::string dst; if(hot){ dst.resize(n); memset(dst.data(),1,n); }        // hot: target pages already touched
  auto a=std::chrono::steady_clock::now();
  if(hot){ memcpy(dst.data(),src.data(),n); }
  else { std::string s; s.resize_and_overwrite(n,[&](char*p,size_t m){ memcpy(p,hdr,88); memcpy(p+88,src.data(),m-88); return m; }); dst.swap(s); }
  auto b=std::chrono::steady_clock::now();
  volatile char sink=dst[n/2]; (void)sink;
  printf("%lld\n",(long long)std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count());
}
