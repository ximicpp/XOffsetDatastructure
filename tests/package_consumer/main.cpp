#include "consumer.hpp"
#include <string>
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string mode = argv[1];
    Pool pool({2, capacity * 2, capacity * 2});
    auto owner = make_owner(pool);
    if (mode == "lifetime") pool = Pool{};
    auto transferred = std::move(owner);
    mutate_owner(transferred);
    validate_owner(transferred);
    if (mode == "static") {
        hold_until_exit(std::move(transferred));
    } else if (mode == "transfer") {
        { auto returned = std::move(transferred); }
        assert(pool.stats().cached_blocks == 1);
        auto reused = make_owner(pool);
        assert(pool.stats().hits == 1);
        mutate_owner(reused);
        validate_owner(reused);
        // clear must leave a live owner and its cross-TU registry entry valid.
        pool.clear();
        validate_owner(reused);
    } else {
        assert(mode == "lifetime");
    }
}
