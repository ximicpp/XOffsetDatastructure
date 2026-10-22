#pragma once
// Experiment-local SHA-256. No production dependency or authentication claim.
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sc {
using Digest256 = std::array<std::uint8_t, 32>;
using Digest = Digest256;
inline Digest256 sha256(std::span<const std::uint8_t> input) {
    static constexpr std::uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    if (input.size() > std::numeric_limits<std::uint64_t>::max()/8)
        throw std::length_error("sha256 input too long");
    std::array<std::uint32_t,8> h = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    auto block = [&](const std::uint8_t* p) {
        std::uint32_t w[64];
        for (unsigned i=0;i<16;++i) w[i]=(std::uint32_t(p[4*i])<<24)|(std::uint32_t(p[4*i+1])<<16)|(std::uint32_t(p[4*i+2])<<8)|p[4*i+3];
        for (unsigned i=16;i<64;++i) {
            auto a=std::rotr(w[i-15],7)^std::rotr(w[i-15],18)^(w[i-15]>>3);
            auto b=std::rotr(w[i-2],17)^std::rotr(w[i-2],19)^(w[i-2]>>10);
            w[i]=w[i-16]+a+w[i-7]+b;
        }
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
        for (unsigned i=0;i<64;++i) {
            auto t1=z+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^((~e)&g))+k[i]+w[i];
            auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));
            z=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
    };
    std::size_t pos=0;
    while (input.size()-pos>=64) {block(input.data()+pos);pos+=64;}
    std::array<std::uint8_t,128> tail{};
    auto left=input.size()-pos;
    for(std::size_t i=0;i<left;++i) tail[i]=input[pos+i];
    tail[left]=0x80;
    auto n=left<56?64u:128u;
    auto bits=std::uint64_t(input.size())*8;
    for(unsigned i=0;i<8;++i) tail[n-1-i]=std::uint8_t(bits>>(8*i));
    block(tail.data());if(n==128) block(tail.data()+64);
    Digest256 out{};
    for(unsigned i=0;i<8;++i) for(unsigned j=0;j<4;++j) out[4*i+j]=std::uint8_t(h[i]>>(24-8*j));
    return out;
}
inline std::string hex_digest(const Digest256& digest) {
    static constexpr char digits[]="0123456789abcdef";
    std::string out(64,'0');
    for(std::size_t i=0;i<32;++i){out[2*i]=digits[digest[i]>>4];out[2*i+1]=digits[digest[i]&15];}
    return out;
}
inline std::string sha256_hex(std::span<const std::uint8_t> input) {return hex_digest(sha256(input));}
inline Digest256 sha256(std::string_view input) {return sha256(std::span(reinterpret_cast<const std::uint8_t*>(input.data()),input.size()));}
inline std::string sha256_hex(std::string_view input) {return hex_digest(sha256(input));}
} // namespace sc
