#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#ifdef XOFFSET_CONSUMER_FULL
#include <xoffsetdatastructure.hpp>
using namespace XOffsetDatastructure;
struct ConsumerRoot { std::uint64_t marker; XVector<std::uint32_t> values; XString text; };
XOFFSET_REGISTER_SCHEMA_NAME(ConsumerRoot, "package.consumer.v1")
using Owner = TypedXBuffer<ConsumerRoot>;
using Pool = XBufferPool;
#else
#include <xoffset_virtual_memory.hpp>
using Owner = XOffsetDatastructure::detail::VirtualMemoryStorage;
using Pool = XOffsetDatastructure::detail::VmStoragePool;
#endif
constexpr std::size_t capacity = 1024 * 1024;
Owner make_owner(const Pool& pool);
void mutate_owner(Owner& owner);
void validate_owner(Owner& owner);
void hold_until_exit(Owner owner);
