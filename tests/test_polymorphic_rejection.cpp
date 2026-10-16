// This must fail at XBuffer::make<T>'s byte-copy-safety static assertion.
#include "xoffsetdatastructure.hpp"

struct PolymorphicRoot {
    int value = 7;
    virtual ~PolymorphicRoot() = default;
};

int main() {
    XOffsetDatastructure::XBuffer buffer;
    return buffer.make<PolymorphicRoot>()->value == 7 ? 0 : 1;
}
