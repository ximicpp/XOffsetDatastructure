// Unit 1: builds a buffer and saves it.
#include "shared.hpp"

using namespace XOffsetDatastructure;

std::string config_check_save() {
    auto buffer = TypedXBuffer<ConfigCheckRecord>::create(4096, XBuffer::maximum_capacity(1 << 20));
    auto record = buffer.root();
    record.id() = 7;
    for (std::int32_t i = 0; i < 100; ++i) record.values().push_back(i);
    return buffer.ok() ? buffer.save_verified() : std::string();
}
