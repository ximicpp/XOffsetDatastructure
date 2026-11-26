#pragma once
// Two translation units that pass one buffer's wire between them: the kind
// of program the configuration check (xoffset_config.hpp, "Configuration
// check") is for. tests/CMakeLists.txt builds it with both units configured
// alike (it must link and run) and with one switch or size set differently
// in each unit (it must fail to link).
#include "../../xoffsetdatastructure.hpp"

#include <cstdint>
#include <string>

struct ConfigCheckRecord {
    std::int32_t id{};
    XOffsetDatastructure::XVector<std::int32_t> values;
};
XOFFSET_RECORD(ConfigCheckRecord, id, values)
XOFFSET_REGISTER_SCHEMA_NAME(ConfigCheckRecord, "test.ConfigCheckRecord")

std::string config_check_save();
