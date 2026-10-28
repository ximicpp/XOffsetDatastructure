#pragma once
// Demo fixture only; applications should supply their own catalog header.
#include "tests/schema_pin/pin_schema.hpp"
#define XSG_SCHEMA_ROOTS(X) \
    X(pin::Root, "record[s:80,a:8]{@0:u64,@8:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10},@20:xvector[s:12,a:4]{data:rel32,size:u32,capacity:u32}<record[s:20,a:4]{@0:u32,@4:bool,@8:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10}}>,@32:xflatset[s:12,a:4]{data:rel32,size:u32,capacity:u32}<i32>,@44:xflatmap[s:12,a:4]{data:rel32,size:u32,capacity:u32}<xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10},u64>,@56:array[3]<enum<u8char>>,@64:f64,@72:i16}", "[64-le]record[s:80,a:8]{@0:u64[s:8,a:8],@8:i32[s:4,a:4],@12:u32[s:4,a:4],@16:u32[s:4,a:4],@20:i32[s:4,a:4],@24:char32[s:4,a:4],@28:char32[s:4,a:4],@32:i32[s:4,a:4],@36:char32[s:4,a:4],@40:char32[s:4,a:4],@44:i32[s:4,a:4],@48:char32[s:4,a:4],@52:char32[s:4,a:4],@56:array[s:3,a:1]<enum[s:1,a:1]<u8[s:1,a:1]>,3>,@64:f64[s:8,a:8],@72:i16[s:2,a:2]}")

// Every reachable record with business-significant member names belongs here.
#define XSG_RECORD_FIELDS(X) \
    X(pin::Root, "epoch|title|items|keys|attrs|kinds|weight|level") \
    X(pin::Inner, "id|active|name")
static_assert(static_cast<unsigned>(pin::Kind::a) == 0);
static_assert(static_cast<unsigned>(pin::Kind::b) == 1);
static_assert(static_cast<unsigned>(pin::Kind::c) == 2);
