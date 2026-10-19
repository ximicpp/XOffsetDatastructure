#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <xoffsetdatastructure.hpp>
namespace bitmap_test {
using Index=XOffsetDatastructure::detail::WireValidationByteIndex;
inline void need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Snapshot {
    std::size_t registrations{},extent{},directory{},pages{};
    std::vector<std::uint64_t> words;
    bool operator==(const Snapshot&)const=default;
};
inline Snapshot snapshot(const Index& index){
    Snapshot value{index.size(),index.extent(),index.directory_entries(),index.allocated_pages(),{}};
    const auto n=index.extent()/64+(index.extent()%64!=0);
    value.words.reserve(n);for(std::size_t i=0;i<n;++i)value.words.push_back(index.word_at(i));
    return value;
}
template<class T>std::size_t begin(const T& x){if constexpr(requires{x.begin;})return x.begin;else return x.first;}
template<class T>std::size_t end(const T& x){if constexpr(requires{x.end;})return x.end;else return x.second;}
// Independent representation: one scalar byte at a time over the oracle's
// unsorted accepted intervals. No candidate mask/page helper is reused.
template<class Ranges>void same(const Index& index,const Ranges& ranges,std::size_t extent){
    need(index.size()==ranges.size(),"bitmap registration cardinality differs from oracle");
    std::vector<std::uint64_t> expected(extent/64+(extent%64!=0));
    for(const auto& range:ranges){
        const auto b=begin(range),e=end(range);need(b<e&&e<=extent,"invalid oracle interval");
        for(auto i=b;i<e;++i)expected[i/64]|=std::uint64_t{1}<<(i%64);
    }
    for(std::size_t i=0;i<expected.size();++i)need(index.word_at(i)==expected[i],"exact bitmap byte coverage differs from oracle");
    need(index.word_at(expected.size())==0,"bitmap escaped oracle backing extent");
}
} // namespace bitmap_test
