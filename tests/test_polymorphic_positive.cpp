// Configure-time positive control for the negative test beside this file.
// Keep its dependency surface and public API call aligned with that test.
#include "xoffsetdatastructure.hpp"

struct NonPolymorphicRoot {
    int value = 7;
};

int main() {
    XOffsetDatastructure::XBuffer buffer;
    return buffer.make<NonPolymorphicRoot>()->value == 7 ? 0 : 1;
}
