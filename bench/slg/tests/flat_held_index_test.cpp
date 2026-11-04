#include "flat_held_index.hpp"
#include <unordered_map>
#include <cassert>
#include <limits>
#include <iostream>
int main(){slg::FlatHeldIndexMap a;std::unordered_map<std::int32_t,std::int32_t>b;std::uint64_t seed=736271;
auto rng=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
for(int i=0;i<200000;++i){auto r=rng();std::int32_t key=std::int32_t(r%1024)-512;if(i%31==0)key=INT32_MIN;if(i%37==0)key=INT32_MAX;auto val=std::int32_t(r>>32);
switch(r%5){case 0:{auto x=a.emplace(key,val);auto y=b.emplace(key,val);assert(x.second==y.second&&x.first->second==y.first->second);break;}case 1:{auto x=a.find(key);auto y=b.find(key);assert((x==a.end())==(y==b.end()));if(x)a.erase(x);b.erase(key);break;}case 2:a[key]=val;b[key]=val;break;case 3:a.reserve((r>>20)%2048);break;default:{auto x=a.find(key);auto y=b.find(key);assert((x==a.end())==(y==b.end()));if(x)assert(x->second==y->second);}}
assert(a.size()==b.size());if(i%257==0){for(auto kv:b){auto*p=a.find(kv.first);assert(p&&p->second==kv.second);}auto c=a;for(auto kv:b)assert(c.find(kv.first)->second==kv.second);}if(i%10007==0){a.clear();b.clear();}}
std::cout<<"flat index random differential: 200000 operations passed\n";}
