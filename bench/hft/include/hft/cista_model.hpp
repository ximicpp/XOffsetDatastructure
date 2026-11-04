// HFT demo: the state on cista (v0.16) offset-mode containers, the same
// shape as hft::State, so apply.hpp runs on it unchanged. The server keeps it
// natively; a snapshot is cista::serialize, and a joining client deserializes
// it in place (mode::CAST trusted, mode::DEEP_CHECK checked) and keeps
// applying updates to it (vectors that grow move to the heap, cista's own
// behaviour).
#pragma once
#include <cstdint>

#include <cista/containers/vector.h>

#include "hft/model.hpp"

namespace hft::cm {
namespace data = cista::offset;

struct Level {
    std::int64_t price;
    std::uint64_t qty;
    std::uint32_t count;
    std::uint32_t pad;
    data::vector<Order> orders;
};
struct Book {
    std::uint32_t instrument;
    std::uint32_t pad;
    std::uint64_t symbol;
    data::vector<Level> bids;
    data::vector<Level> asks;
};
struct State {
    std::uint64_t seq;
    data::vector<Book> books;
    data::vector<Row> rows;
};

}  // namespace hft::cm
