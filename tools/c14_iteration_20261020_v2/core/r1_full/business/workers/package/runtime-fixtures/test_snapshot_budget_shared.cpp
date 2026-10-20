#include "xoffset_snapshot_index_budget.hpp"
#include <iostream>
#include <stdexcept>
#if defined(_WIN32)
#define PEER_IMPORT __declspec(dllimport)
#else
#define PEER_IMPORT
#endif
extern "C" {
PEER_IMPORT void* budget_one_registry();
PEER_IMPORT bool budget_one_reserve(const void*, std::size_t);
PEER_IMPORT void budget_one_release(const void*, std::size_t);
PEER_IMPORT std::size_t budget_one_bytes();
PEER_IMPORT void* budget_two_registry();
PEER_IMPORT bool budget_two_reserve(const void*, std::size_t);
PEER_IMPORT void budget_two_release(const void*, std::size_t);
PEER_IMPORT std::size_t budget_two_bytes();
}
int main() { try {
    namespace si = XOffsetDatastructure::snapshot_index_detail;
    auto need=[](bool b,const char* m){if(!b)throw std::runtime_error(m);};
    const auto base=si::process_requested_bytes();
    need(si::registry() && budget_one_registry()==si::registry() && budget_two_registry()==si::registry(),
         "hidden DSOs do not share the one process budget accessor");
    int key{};
    need(budget_one_reserve(&key,si::owner_limit),"first hidden DSO could not reserve owner boundary");
    need(!budget_two_reserve(&key,1),"second hidden DSO bypassed the owner cap");
    need(budget_one_bytes()==base+si::owner_limit && budget_two_bytes()==budget_one_bytes(),
         "hidden DSOs have different process accounting");
    budget_two_release(&key,si::owner_limit);
    need(budget_one_bytes()==base && si::process_requested_bytes()==base,"cross-DSO release retained budget");
    std::cout<<"two hidden DSO/TU budget sharing passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
