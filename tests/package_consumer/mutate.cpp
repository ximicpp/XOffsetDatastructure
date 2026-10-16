#include "consumer.hpp"
void mutate_owner(Owner& owner) {
#ifdef XOFFSET_CONSUMER_FULL
    for (std::uint32_t i = 0; i != 20000; ++i) owner.root().values.push_back(i);
    owner.root().text = "allocated in another translation unit";
#else
    assert(owner.grow_to(100000));
    for (std::size_t i = 1; i != 100000; ++i) {
        assert(owner.data()[i] == 0);
        owner.data()[i] = static_cast<char>(i % 113);
    }
#endif
}
void validate_owner(Owner& owner) {
#ifdef XOFFSET_CONSUMER_FULL
    assert(owner.root().marker == 0x12345678);
    assert(owner.root().values.size() == 20000);
    for (std::uint32_t i = 0; i != 20000; ++i) assert(owner.root().values[i] == i);
    assert(owner.root().text.size() == 37);
#else
    assert(owner.data()[0] == 42);
    for (std::size_t i = 1; i != 100000; ++i) assert(owner.data()[i] == static_cast<char>(i % 113));
#endif
}
