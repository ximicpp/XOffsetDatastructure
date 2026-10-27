// Must NOT compile: the pin describes `level` as i32 at offset 88 while the
// schema declares std::int16_t. CTest schema_pin_mismatch builds this
// target and requires the K7 diagnostic.
#include "pin_schema.hpp"

XOFFSET_PIN_SCHEMA(pin::Root,
    "record[s:96,a:8]{@0:u64,@8:xstring[s:16,a:8]{arena:rel32,data:rel32,size:u32,capacity:u32}{inline:10},@24:xvector[s:16,a:8]{arena:rel32,data:rel32,size:u32,capacity:u32}<record[s:24,a:8]{@0:u32,@4:bool,@8:xstring[s:16,a:8]{arena:rel32,data:rel32,size:u32,capacity:u32}{inline:10}}>,@40:xflatset[s:16,a:8]{arena:rel32,data:rel32,size:u32,capacity:u32}<i32>,@56:xflatmap[s:16,a:8]{arena:rel32,data:rel32,size:u32,capacity:u32}<xstring[s:16,a:8]{arena:rel32,data:rel32,size:u32,capacity:u32}{inline:10},u64>,@72:array[3]<enum<u8char>>,@80:f64,@88:i32}");

int main() { return 0; }
