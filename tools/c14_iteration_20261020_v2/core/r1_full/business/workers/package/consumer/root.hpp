#pragma once
#include "xoffsetdatastructure.hpp"
#include "xoffset_verified_paths.hpp"
#include "xoffset_validation_scratch.hpp"
#include "validation_scratch.hpp"
#include "xoffset_snapshot_index.hpp"
#include "xoffset_snapshot_index_budget.hpp"
#include "receive_raw_fwd.hpp"
#include "receive_raw.hpp"
#include "receive_future.hpp"
#include "xoffset_core_lifetime.hpp"
#include "xoffset_verified_receive.hpp"
#include <cstdint>
namespace xp=XOffsetDatastructure;
struct PackageReceiveRoot { std::uint32_t tag{};xp::XString text;xp::XVector<std::uint32_t> values; };
namespace XOffsetDatastructure {
template<>struct wire_record_layout<PackageReceiveRoot>{static constexpr bool ordinary_members=true;};
}
XOFFSET_REGISTER_SCHEMA_NAME(PackageReceiveRoot,"package.CompleteReceive.v1")
static_assert(xp::detail::receive_raw::future_supported_v<PackageReceiveRoot>);
int raw_first();int core_first();int future_first();
