#include "xoffsetdatastructure.hpp"
namespace xo = XOffsetDatastructure;
extern "C" __attribute__((noinline)) void m1_resize_u8(xo::XVector<std::uint8_t>& v, std::size_t n) { v.resize(n); }
extern "C" __attribute__((noinline)) void m1_resize_u32(xo::XVector<std::uint32_t>& v, std::size_t n) { v.resize(n); }
