#include "xoffsetdatastructure.hpp"
#include <condition_variable>
#include <exception>
#include <iostream>
#include <meta>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

namespace xo = XOffsetDatastructure;
struct Root { std::uint64_t marker{}; xo::XString label; xo::XVector<std::uint32_t> values; };
XOFFSET_REGISTER_SCHEMA_NAME(Root, "iteration.r1.thread-handoff.v1")
namespace XOffsetDatastructure {
template<> struct wire_record_layout<Root> { static constexpr bool ordinary_members = true; };
}
using Buffer = xo::TypedXBuffer<Root>;
consteval std::meta::info ledger_member() {
    for (auto m : std::meta::members_of(^^xo::XBufferCore, std::meta::access_context::unchecked()))
        if (std::meta::has_identifier(m) && std::meta::identifier_of(m) == "canonical_active_count_") return m;
    throw "canonical ledger missing";
}
constexpr auto ledger_info = ledger_member();
auto count() { return ([:ledger_info:]).load(std::memory_order_acquire); }
void need(bool b, const char* why) { if (!b) throw std::runtime_error(why); }
auto policy() { return xo::XBuffer::maximum_capacity(131072).with_growth(xo::XGrowPolicy::Automatic); }
Buffer canonical(const std::string& wire) {
    auto token = xo::begin_verified_receive<Root>(wire.size() - sizeof(xo::XWireHeaderV1), policy());
    auto op = token.acquire_operation();
    auto h = op.header_bytes(), p = op.payload_bytes();
    std::memcpy(h.data(), wire.data(), h.size());
    std::memcpy(p.data(), wire.data() + h.size(), p.size());
    op.complete(h.size(), p.size());
    return xo::finish_verified(token);
}
void check(Buffer& b, bool is_canonical) {
    // Repetition exercises a cold thread's first lookup and its warmed TLS.
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
        auto& r = b.root();
        need(r.marker == 61 && r.label == "synchronized owner" && r.values.size() == 5 && r.values[4] == 105,
             "mixed-owner contents");
        auto at = reinterpret_cast<std::uintptr_t>(r.values.data());
        auto* found = xo::XBufferCore::canonical_bytes_at(&r.values, at, 5 * sizeof(std::uint32_t));
        need(is_canonical ? found && reinterpret_cast<std::uintptr_t>(found) == at : !found,
             "ordinary/canonical domain classification");
    }
}
struct Sync {
    std::mutex mutex;
    std::condition_variable cv;
    unsigned phase = 0;
    bool cancel = false;
    std::exception_ptr error;
    std::optional<Buffer> transfer;
};
int main() { try {
    auto ordinary = Buffer::create(32768, policy());
    auto& r = ordinary.root(); r.marker = 61; r.label = "synchronized owner";
    r.values.resize(5); for (unsigned i = 0; i < 5; ++i) r.values[i] = 101 + i;
    const auto wire = ordinary.save_verified();
    auto copied = Buffer::load_verified(wire, policy());
    need(count() == 0, "initial ordinary owners affected canonical count"); check(copied, false);
    Sync s;
    std::thread worker([&] { try {
        for (unsigned round = 0; round < 2; ++round) {
            const auto live = 4 * round + 1;
            auto c = canonical(wire); check(c, true);
            need(count() == 1, "worker real C registration missing");
            {
                std::unique_lock lock(s.mutex);
                s.transfer.emplace(std::move(c)); s.phase = live; s.cv.notify_all();
                s.cv.wait(lock, [&] { return s.cancel || s.phase == live + 1; });
                if (s.cancel) return;
                c = std::move(*s.transfer); s.transfer.reset();
            }
            // Ownership has been handed back; the main thread no longer reads C.
            check(c, true); c = Buffer{};
            need(count() == 0, "worker C retirement did not restore zero");
            {
                std::unique_lock lock(s.mutex);
                s.phase = live + 2; s.cv.notify_all();
                s.cv.wait(lock, [&] { return s.cancel || s.phase == live + 3; });
                if (s.cancel) return;
            }
        }
    } catch (...) {
        std::lock_guard lock(s.mutex); s.error = std::current_exception(); s.cancel = true; s.cv.notify_all();
    }});
    try {
        for (unsigned round = 0; round < 2; ++round) {
            const auto live = 4 * round + 1;
            Buffer c;
            {
                std::unique_lock lock(s.mutex);
                s.cv.wait(lock, [&] { return s.cancel || s.phase == live; });
                if (s.error) std::rethrow_exception(s.error);
                c = std::move(*s.transfer); s.transfer.reset();
            }
            // Worker is blocked: this payload is exclusively owned on main.
            need(count() == 1, "handoff lost canonical count");
            check(copied, false); check(c, true);
            auto moved = std::move(c); std::swap(c, moved); check(c, true);
            auto alloc = c.root().values.binding();
            auto* first = xo::fixed_detail::allocate_dynamic_bytes(alloc, 24, 8);
            xo::fixed_detail::retire_dynamic_bytes(alloc, first, 24);
            auto* reused = xo::fixed_detail::allocate_dynamic_bytes(alloc, 24, 8);
            need(reused == first, "live C allocation reuse not exercised");
            xo::fixed_detail::retire_dynamic_bytes(alloc, reused, 24);
            const auto retired = reinterpret_cast<std::uintptr_t>(c.get_address());
            {
                std::unique_lock lock(s.mutex);
                s.transfer.emplace(std::move(c)); s.phase = live + 1; s.cv.notify_all();
                s.cv.wait(lock, [&] { return s.cancel || s.phase == live + 2; });
                if (s.error) std::rethrow_exception(s.error);
            }
            need(count() == 0, "main did not observe synchronized zero transition");
            need(!xo::XBufferCore::canonical_bytes_at(reinterpret_cast<const void*>(retired), retired, 1),
                 "retired numeric anchor revived before any new mapping");
            check(copied, false); copied.root().values[0] += 1; copied.root().values[0] -= 1;
            { std::lock_guard lock(s.mutex); s.phase = live + 3; s.cv.notify_all(); }
        }
    } catch (...) {
        { std::lock_guard lock(s.mutex); s.cancel = true; s.cv.notify_all(); }
        worker.join(); throw;
    }
    worker.join(); if (s.error) std::rethrow_exception(s.error);
    need(count() == 0, "thread exit left active canonical registration"); check(copied, false);
    std::cout << "synchronized mixed-owner zero/active/zero, cold TLS, handoff, move, retire and reuse passed\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
