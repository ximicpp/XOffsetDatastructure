// Write/read mutation fixture for the V2 wire format. V1 wire compatibility is
// retired: a V2.1 reader must reject V1 input ("wire format 1 is not
// 1"), so the former cross-reader gate (candidate vs a pinned V1 reader) is now
// --expect-v1-rejection, which feeds this reader a wire relabeled as format 1.
// The resize+copy fallback below remains for readers without assign_range.
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include "xoffsetdatastructure.hpp"
namespace xo = XOffsetDatastructure;
struct CompatAttr { xo::XString key; xo::XVector<std::uint8_t> value; };
struct MutationCompat {
    xo::XVector<std::uint32_t> numbers;
    xo::XVector<std::uint8_t> payload;
    xo::XVector<CompatAttr> attrs;
};
XOFFSET_REGISTER_SCHEMA_NAME(MutationCompat, "test.v4.mutation-compatibility.v1")
namespace {
template<class V, class T> void replace(V& v, std::span<const T> input) {
    if constexpr (requires { v.assign_range(input); }) v.assign_range(input);
    else { v.resize(input.size()); if (!input.empty()) std::memmove(v.data(), input.data(), input.size_bytes()); }
}
auto policy() { return xo::XBuffer::maximum_capacity(16u<<20).with_growth(xo::XGrowPolicy::Automatic); }
void fill(MutationCompat& root) {
    root.numbers.resize(3, 7u); root.numbers.resize(53);
    for(unsigned i=3;i!=53;++i) if(root.numbers[i]!=0) throw std::runtime_error("default suffix");
    std::vector<std::uint32_t> numbers(97); for(unsigned i=0;i!=97;++i) numbers[i]=i*17+3;
    replace(root.numbers,std::span<const std::uint32_t>(numbers));
    replace(root.numbers,std::span<const std::uint32_t>(root.numbers.data()+3,71));
    std::vector<std::uint8_t> bytes(384); for(unsigned i=0;i!=384;++i) bytes[i]=std::uint8_t(i*31);
    replace(root.payload,std::span<const std::uint8_t>(bytes)); root.attrs.resize(3);
    for(unsigned i=0;i!=3;++i) {
        root.attrs[i].key.assign(std::string(19+i,char('a'+i)));
        replace(root.attrs[i].value,std::span<const std::uint8_t>(root.payload.data()+i,32+i));
    }
}
std::string canonical(const MutationCompat& root) {
    std::string out;
    auto add=[&](const void* p,std::size_t n) {auto count=std::uint64_t(n);out.append(reinterpret_cast<const char*>(&count),8);if(n)out.append(static_cast<const char*>(p),n);};
    add(root.numbers.data(),root.numbers.size()*sizeof(std::uint32_t)); add(root.payload.data(),root.payload.size());
    for(const auto& a:root.attrs) {add(a.key.data(),a.key.size());add(a.value.data(),a.value.size());} return out;
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("--write-wire/--read-wire/--expect-v1-rejection PATH");
        auto b=xo::XBuffer::create<MutationCompat>(1u<<20,policy()); fill(b.root<MutationCompat>());
        const auto expected=canonical(b.root<MutationCompat>());
        if(std::string(argv[1])=="--write-wire") {
            const auto wire=b.save_verified<MutationCompat>();std::ofstream out(argv[2],std::ios::binary);
            out.write(wire.data(),std::streamsize(wire.size()));if(!out)throw std::runtime_error("write failed");
        } else if(std::string(argv[1])=="--read-wire") {
            std::ifstream in(argv[2],std::ios::binary);if(!in)throw std::runtime_error("read failed");
            const std::string wire((std::istreambuf_iterator<char>(in)),{});
            auto loaded=xo::XBuffer::load_verified<MutationCompat>(wire,policy());
            if(canonical(loaded.root<MutationCompat>())!=expected)throw std::runtime_error("old/new logical contents");
            fill(loaded.root<MutationCompat>());
            if(canonical(loaded.root<MutationCompat>())!=expected)throw std::runtime_error("loaded mutation");
        } else if(std::string(argv[1])=="--expect-v1-rejection") {
            std::ifstream in(argv[2],std::ios::binary);if(!in)throw std::runtime_error("read failed");
            std::string wire((std::istreambuf_iterator<char>(in)),{});
            if(wire.size()<sizeof(xo::XWireHeaderV2))throw std::runtime_error("wire too small");
            // Relabel the stored wire as format 1 (the V1 magic).
            std::memcpy(wire.data(),"XOFFV1\0\0",8);
            bool rejected=false;
            try{(void)xo::XBuffer::load_verified<MutationCompat>(wire,policy());}
            catch(const xo::XException& e){rejected=std::string(e.what()).find("wire format 1 is not supported")!=std::string::npos;}
            if(!rejected)throw std::runtime_error("V1 wire was not rejected as format version 1");
        } else throw std::runtime_error("unknown command");
        std::cout<<"V2 mutation fixture passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
