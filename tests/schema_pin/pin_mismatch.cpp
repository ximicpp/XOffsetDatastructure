// Must NOT compile: the pin describes `level` as i32 at offset 72 while the
// schema declares std::int16_t. CTest schema_pin_mismatch builds this
// target and requires the K7 diagnostic.
#include "pin_schema.hpp"

XOFFSET_PIN_SCHEMA(pin::Root,
    "record[s:80,a:8]{@0:u64,@8:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10},@20:xvector[s:12,a:4]{data:rel32,size:u32,capacity:u32}<record[s:20,a:4]{@0:u32,@4:bool,@8:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10}}>,@32:xflatset[s:12,a:4]{data:rel32,size:u32,capacity:u32}<i32>,@44:xflatmap[s:12,a:4]{data:rel32,size:u32,capacity:u32}<xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10},u64>,@56:array[3]<enum<u8char>>,@64:f64,@72:i32}");

int main() { return 0; }
