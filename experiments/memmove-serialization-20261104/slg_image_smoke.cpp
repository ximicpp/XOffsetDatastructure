#include "xoffset_image_builder.hpp"
#include "slg/model_xoffset.hpp"
#include <cassert>
#include <iostream>
namespace xo=XOffsetDatastructure;namespace im=xo::image;
static_assert(im::build_supported_v<slg::xm::Report>);
static_assert(im::build_supported_v<slg::xm::BattleMsg>);
static_assert(!im::build_supported_v<slg::xm::PlayerState>); // map is not in this prototype
static_assert(!im::build_supported_v<slg::xm::ShardState>);
int main(){
 im::ImageBuilder<slg::xm::Report> builder(1024*1024);auto r=builder.root();
 r.get<0>().set(77);r.get<1>().assign("battle complete");auto events=r.get<2>().resize(10000);
 std::int64_t expected=77;
 for(std::size_t i=0;i<events.size();++i){auto e=events[i];e.get<0>().set(i/100);e.get<1>().set(i%100);e.get<2>().set(3);e.get<3>().set(4);e.get<4>().set(i+100);expected+=i+100;}
 auto wire=std::move(builder).seal();im::BorrowedImageBuffer<slg::xm::Report> destination(wire.bytes().size());destination.reload(wire);
 auto root=destination.root();assert(root.get<0>()==77 && root.get<1>()=="battle complete");
 auto list=root.get<2>();std::int64_t sum=root.get<0>();for(std::size_t i=0;i<list.size();++i)sum+=list[i].get<4>();assert(sum==expected);
 auto native=xo::TypedXBuffer<slg::xm::Report>::load_verified(std::string(reinterpret_cast<const char*>(wire.bytes().data()),wire.bytes().size()));
 assert(native.root().events.size()==10000);std::int64_t native_sum=native.root().battle_id;for(auto& e:native.root().events)native_sum+=e.damage;assert(native_sum==expected);
 std::cout<<"SLG Report: 10000 events, "<<wire.bytes().size()<<" wire bytes, native verified/proxy checksum "<<sum<<"; A/D schema supported, B/C map schemas explicitly rejected\n";
}
