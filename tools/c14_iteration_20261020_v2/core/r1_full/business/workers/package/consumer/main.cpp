#include "xoffsetdatastructure.hpp"
#include "root.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
static void need(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static auto receive(const std::string& wire){
    const auto n=wire.size()-sizeof(xp::XWireHeaderV1),first=n/2;
    auto token=xp::begin_verified_receive<PackageReceiveRoot>(first,
        xp::XBuffer::maximum_capacity(n+16384).with_growth(xp::XGrowPolicy::Automatic));
    const auto* base=token.payload_address();
    {auto operation=token.acquire_operation();auto h=operation.header_bytes(),b=operation.payload_bytes();
     for(std::size_t i=0;i<h.size();++i)h[i]=std::byte(static_cast<unsigned char>(wire[i]));
     for(std::size_t i=0;i<b.size();++i)b[i]=std::byte(static_cast<unsigned char>(wire[h.size()+i]));
     operation.cancel();bool busy=false;try{(void)xp::finish_verified(token);}catch(const xp::XException&){busy=true;}
     need(busy,"operation cancellation was mistaken for completed receive");operation.complete(h.size(),b.size());}
    need(token.grow(n-first)&&token.payload_address()==base,"token growth changed arena base");
    {auto operation=token.acquire_operation();auto b=operation.payload_bytes();
     for(auto i=first;i<n;++i)b[i]=std::byte(static_cast<unsigned char>(wire[sizeof(xp::XWireHeaderV1)+i]));
     operation.complete(sizeof(xp::XWireHeaderV1),n);}
    auto output=xp::finish_verified(token);need(!token&&output.get_address()==base,"finish copied reservation");
    return output;
}
int main(){try{
    need(raw_first()+core_first()+future_first()==3,"public include order missing");
    auto source=xp::TypedXBuffer<PackageReceiveRoot>::create(4096);
    source.root().tag=7;source.root().text="prefix consumer";source.root().values.resize(3);
    source.root().values[0]=9;source.root().values[2]=11;
    auto output=receive(source.save_verified());auto& r=output.root();
    need(r.tag==7&&r.text=="prefix consumer"&&r.values[0]==9&&r.values[2]==11,"received graph truth differs");
    r.text="mutated standalone";r.values.reserve(32);r.values.resize(40,r.values[0]);r.values.erase(r.values.begin());
    need(r.values.size()==39&&r.values.back()==9,"received mutation failed");
    const auto wire=output.save_verified();auto next=receive(wire);
    need(next.root().text=="mutated standalone"&&next.root().values.size()==39,"fresh raw V1 reload failed");
    auto copied=xp::TypedXBuffer<PackageReceiveRoot>::load_verified(wire);
    need(copied.root().tag==7&&copied.root().text=="mutated standalone"&&copied.root().values.back()==9,"existing V1 reader differs");
    std::cout<<"standalone receive/grow/finish/mutate/save/reload passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
