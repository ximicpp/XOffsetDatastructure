// K7 schema pins: a matching pin compiles; schema_pin<T>() reproduces the pin
// text; the pinned text is the same signature that feeds the wire schema hash.
// The mismatch case is tests/schema_pin/pin_mismatch.cpp, which must fail to
// compile (CTest schema_pin_mismatch).
#include "xoffsetdatastructure.hpp"
#include "schema_pin/pin_schema.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

namespace xo = XOffsetDatastructure;

XOFFSET_PIN_SCHEMA(pin::Inner, "record[s:20,a:4]{@0:u32,@4:bool,@8:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10}}");
XOFFSET_PIN_SCHEMA(pin::Root,
    "record[s:80,a:8]{@0:u64,@8:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10},@20:xvector[s:12,a:4]{data:rel32,size:u32,capacity:u32}<record[s:20,a:4]{@0:u32,@4:bool,@8:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10}}>,@32:xflatset[s:12,a:4]{data:rel32,size:u32,capacity:u32}<i32>,@44:xflatmap[s:12,a:4]{data:rel32,size:u32,capacity:u32}<xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10},u64>,@56:array[3]<enum<u8char>>,@64:f64,@72:i16}");

namespace {
[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_schema_pin FAILED: " << message << "\n";
    std::exit(1);
}
}

int main() {
    // Block scope works too.
    XOFFSET_PIN_SCHEMA(pin::Inner, "record[s:20,a:4]{@0:u32,@4:bool,@8:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10}}");

    const auto line = xo::schema_pin<pin::Root>("pin::Root");
    // wire_abi_signature returns the text by value; keep it alive for the view.
    const auto signature_text = xo::wire_abi_signature<pin::Root>();
    const std::string_view signature(signature_text);
    if (line != "XOFFSET_PIN_SCHEMA(pin::Root, \"" + std::string(signature) + "\");")
        fail("schema_pin<T>() does not reproduce the pin line: " + line);
    static_assert(xo::detail::schema_pin_matches<pin::Root>(
        std::string_view(xo::wire_abi_signature<pin::Root>())));
    static_assert(!xo::detail::schema_pin_matches<pin::Root>("record[s:8,a:8]{@0:u64}"));

    // The pin is exactly the layout that the wire schema hash covers.
    auto buffer = xo::TypedXBuffer<pin::Root>::create(4096);
    const auto wire = buffer.save_verified();
    xo::XWireHeaderV2 header{};
    std::memcpy(&header, wire.data(), sizeof(header));
    if (header.schema_hash != xo::wire_schema_hash_v<pin::Root>())
        fail("wire header does not carry wire_schema_hash_v");
    std::cout << line << "\n" << xo::schema_pin<pin::Inner>("pin::Inner") << "\n";
    std::cout << "schema pins: match compiles, generator reproduces the pin\n";
    return 0;
}
