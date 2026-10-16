#include "consumer.hpp"
namespace { std::unique_ptr<Owner> static_owner; }
Owner make_owner(const Pool& pool) {
#ifdef XOFFSET_CONSUMER_FULL
    auto owner = Owner::create(4096, XBuffer::maximum_capacity(capacity)
        .with_growth(XGrowPolicy::Automatic).with_pool(pool));
    owner.root().marker = 0x12345678;
#else
    Owner owner(4096, capacity, pool);
    owner.data()[0] = 42;
#endif
    return owner;
}
void hold_until_exit(Owner owner) { static_owner = std::make_unique<Owner>(std::move(owner)); }
