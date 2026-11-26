// Unit 2: loads the wire unit 1 saved and changes it in place.
#include "shared.hpp"

#include <cstdio>

using namespace XOffsetDatastructure;

int main() {
    const std::string wire = config_check_save();
    if (wire.empty()) return 1;
    auto buffer = TypedXBuffer<ConfigCheckRecord>::load(wire, XBuffer::maximum_capacity(1 << 20));
    buffer.root().values().push_back(100);
    const bool same = buffer.ok() && buffer.read([](const ConfigCheckRecord& r) {
        return r.id == 7 && r.values.size() == 101 && r.values[100] == 100;
    });
    std::puts(same ? "OK" : "MISMATCH");
    return same ? 0 : 1;
}
