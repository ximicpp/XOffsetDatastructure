// describe_dump cases beyond bench/slg (checked by the describe_dump_cases
// test, and compiled under every backend by test_schema_describe):
//   - records derived from XOffset heads still get their own line;
//   - a std::string reached through a member gets a comment, not a broken TU;
//   - overlapping [[no_unique_address]] members get a comment, not a line.
#pragma once

#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <string>

namespace sdc {
    struct ExtendedString : XOffsetDatastructure::XString { std::int32_t extra; };
    struct ExtendedMap : XOffsetDatastructure::XMap<std::int32_t, std::int32_t> { std::int64_t version; };
    struct Named { std::string name; };
    struct DerivedNamed : Named { std::int32_t id; };
    struct CaseRoot { ExtendedString text; ExtendedMap table; DerivedNamed named; };

    struct Empty {};
    struct Overlap { std::int32_t a; [[no_unique_address]] Empty e; std::int32_t b; };
} // namespace sdc

XOFFSET_RECORD_DERIVED(sdc::ExtendedString, (XOffsetDatastructure::XString), extra)
XOFFSET_RECORD_DERIVED(sdc::ExtendedMap, (XOffsetDatastructure::XMap<int, int>), version)
XOFFSET_RECORD(sdc::Named, name)
XOFFSET_RECORD_DERIVED(sdc::DerivedNamed, (sdc::Named), id)
XOFFSET_RECORD(sdc::CaseRoot, text, table, named)
