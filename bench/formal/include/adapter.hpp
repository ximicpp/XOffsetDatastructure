#pragma once
#include "model.hpp"
#include <memory>
#include <span>
#include <string_view>

// Adapter contract (all calls statically bound in each independent executable):
// using State = movable RAII owner (normally std::unique_ptr<NativeState>);
// library_id, commit, trust_mode: constexpr string_view;
// static State build(const sc::Snapshot&);
// static State load(std::span<const uint8_t>); // detached, caller bytes not adopted
// static OwningWire emit(const State&);      // native independent byte owner
// static sc::Snapshot export_state(const State&); // audit only, outside timing
// static void query(const State&,span<const Query>,span<QueryResult>);
// static void mutate(State&,span<const Mutation>); // ordered success/status checks
// mutate may batch a stage, but must preserve its exact sequential semantics.
// A stage's operations may not be replaced by its oracle's expected state.
// State = {} must release all borrowed backing/private heap/arena/builders.
namespace sc {
inline constexpr std::uint64_t wire_limit = 512ULL << 20;
inline constexpr std::uint64_t native_arena_maximum = 512ULL << 20;
inline constexpr std::uint64_t native_arena_initial = 4096;
// Preserve each API's natural owning result. Requiring vector<uint8_t> would
// add an artificial string/DetachedBuffer/word-array copy to some libraries.
template<class Wire> std::span<const std::uint8_t> wire_view(const Wire& wire){
    if constexpr(requires{wire.asBytes();}){
        auto bytes=wire.asBytes();
        return {reinterpret_cast<const std::uint8_t*>(bytes.begin()),bytes.size()};
    }else{
        static_assert(sizeof(*wire.data())==1,"owning wire must contain bytes");
        return {reinterpret_cast<const std::uint8_t*>(wire.data()),wire.size()};
    }
}
}
