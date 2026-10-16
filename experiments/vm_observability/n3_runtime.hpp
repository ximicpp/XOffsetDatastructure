#ifndef XOFFSET_EXPERIMENT_N3_RUNTIME_HPP
#define XOFFSET_EXPERIMENT_N3_RUNTIME_HPP

// N3 diagnostic runtime, separate from production and formal timing builds.
//
// emit() does not allocate or throw. The recorder has 16 lifetime thread slots,
// each with 16,384 events; slots are never reused. Kind and phase labels are
// copied into 64-byte fields (at most 63 bytes plus NUL). Overflow, exhausted
// thread slots, an invalid kind, or an overlong label drops the event and makes
// the entire record invalid. Recorder storage is diagnostic overhead, excluded
// from the tracked host-allocation domains. Do not interpret these events as
// all-process allocation, kernel copy, RSS, or uninstrumented-path accounting.
//
// Thread identifiers are assigned on first emission and are zero-based slot
// indices in [0, thread_count). Global
// sequence numbers order recorder observation points, not simultaneous physical
// work. Scope supplies causal owner identity, not exclusive allocation ownership.
// A null emit owner uses the active Scope; use Scope(nullptr) for an explicitly
// unowned event. Scope/Phase, including failure injection, affect only this thread.
//
// Lock clock probes and sequence atomics perturb contention. Lock wait excludes
// the pre-lock clock call; hold ends before recording and unlocking, so it omits
// the recorder/unlock overhead while the mutex is still held. These values are
// diagnostic observations, never admissible formal timing evidence.
//
// dump() may allocate and must run only after all emitting workers are quiescent
// (for example, after join). There is intentionally no reset/reuse API. Labels
// must be NUL-terminated UTF-8 strings. Event byte counts are requested extents,
// not allocator usable-size or physical memory. Unrepresentable extents make the
// record invalid. Failure injection affects only Allocator<T, Domain>.

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <ostream>
#include <type_traits>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#include <sys/types.h>
#endif

namespace n3 {

inline constexpr std::size_t max_threads = 16;
inline constexpr std::size_t events_per_thread = 16384;
inline constexpr std::size_t label_capacity = 64;

enum AllocationDomain : int {
    core_vector = 1,
    owner_registry = 2,
    pool_slots = 3,
    pool_state = 4,
    validation_ranges = 5
};

namespace detail {

static_assert(sizeof(std::uintptr_t) <= sizeof(std::uint64_t),
              "N3 pointer identities require at most 64 bits");
static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t),
              "N3 byte counts require at most 64 bits");

struct Event {
    std::uint64_t sequence;
    std::uint64_t thread;
    std::uint64_t owner;
    std::uint64_t subject;
    std::uint64_t bytes;
    std::uint64_t value;
    char phase[label_capacity];
    char kind[label_capacity];
};

struct ThreadBuffer {
    std::size_t used = 0;
    std::array<Event, events_per_thread> events{};
};

struct Recorder {
    std::atomic<std::uint64_t> next_sequence{1};
    std::atomic<std::size_t> next_thread{0};
    std::atomic<std::uint64_t> recorded{0};
    std::atomic<std::uint64_t> dropped{0};
    std::atomic<bool> valid{true};
    std::array<ThreadBuffer, max_threads> threads{};
};

inline Recorder recorder{};
inline constexpr std::size_t unregistered_thread = max_threads;
inline constexpr std::size_t exhausted_thread = max_threads + 1;

struct ThreadContext {
    std::size_t index = unregistered_thread;
    const void* owner = nullptr;
    const char* phase = "unscoped";
    // -1 disables a domain; zero fails every following allocation until cleared.
    std::array<int, 6> successful_before_failure{{-1, -1, -1, -1, -1, -1}};
};

inline thread_local ThreadContext context{};

inline std::uint64_t identity(const void* pointer) noexcept {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

inline void lose_event() noexcept {
    recorder.valid.store(false, std::memory_order_relaxed);
    recorder.dropped.fetch_add(1, std::memory_order_relaxed);
}

inline bool copy_label(char (&destination)[label_capacity],
                       const char* source) noexcept {
    if (!source) source = "";
    for (std::size_t i = 0; i < label_capacity; ++i) {
        destination[i] = source[i];
        if (source[i] == '\0') return true;
    }
    destination[label_capacity - 1] = '\0';
    return false;
}

inline std::size_t thread_index() noexcept {
    if (context.index == unregistered_thread) {
        const auto index = recorder.next_thread.fetch_add(1, std::memory_order_relaxed);
        context.index = index < max_threads ? index : exhausted_thread;
    }
    return context.index;
}

inline bool injection_fails(int domain) noexcept {
    return domain >= core_vector && domain <= validation_ranges &&
           context.successful_before_failure[static_cast<std::size_t>(domain)] == 0;
}

inline void allocation_succeeded(int domain) noexcept {
    if (domain >= core_vector && domain <= validation_ranges) {
        auto& remaining =
            context.successful_before_failure[static_cast<std::size_t>(domain)];
        if (remaining > 0) --remaining;
    }
}

template<class T>
inline std::uint64_t allocation_bytes(std::size_t count) noexcept {
    if (count > std::numeric_limits<std::uint64_t>::max() / sizeof(T)) {
        // A failure event can still identify the allocator domain, but its byte
        // extent cannot be represented. The record must not claim exactness.
        lose_event();
        return std::numeric_limits<std::uint64_t>::max();
    }
    return static_cast<std::uint64_t>(count) * sizeof(T);
}

inline std::uint64_t nanoseconds(std::chrono::steady_clock::duration value) noexcept {
    const auto count = std::chrono::duration_cast<std::chrono::nanoseconds>(value).count();
    return count > 0 ? static_cast<std::uint64_t>(count) : 0;
}

inline void write_literal(std::ostream& output, const char* text) {
    output.write(text, static_cast<std::streamsize>(std::strlen(text)));
}

inline void write_number(std::ostream& output, std::uint64_t value) {
    char digits[std::numeric_limits<std::uint64_t>::digits10 + 2];
    const auto result = std::to_chars(digits, digits + sizeof(digits), value);
    output.write(digits, static_cast<std::streamsize>(result.ptr - digits));
}

inline void write_string(std::ostream& output, const char* text) {
    static constexpr char hex[] = "0123456789abcdef";
    output.put('"');
    for (const auto* cursor = reinterpret_cast<const unsigned char*>(text);
         *cursor; ++cursor) {
        switch (*cursor) {
        case '"': write_literal(output, "\\\""); break;
        case '\\': write_literal(output, "\\\\"); break;
        case '\b': write_literal(output, "\\b"); break;
        case '\f': write_literal(output, "\\f"); break;
        case '\n': write_literal(output, "\\n"); break;
        case '\r': write_literal(output, "\\r"); break;
        case '\t': write_literal(output, "\\t"); break;
        default:
            if (*cursor < 0x20) {
                const char escaped[]{'\\', 'u', '0', '0',
                                     hex[*cursor >> 4], hex[*cursor & 15]};
                output.write(escaped, sizeof(escaped));
            } else {
                output.put(static_cast<char>(*cursor));
            }
        }
    }
    output.put('"');
}

} // namespace detail

inline void emit(const char* kind, const void* owner = nullptr,
                 const void* subject = nullptr, std::uint64_t bytes = 0,
                 std::uint64_t value = 0) noexcept {
    const auto index = detail::thread_index();
    if (index >= max_threads) {
        detail::lose_event();
        return;
    }
    auto& buffer = detail::recorder.threads[index];
    if (buffer.used == events_per_thread || !kind) {
        detail::lose_event();
        return;
    }
    auto& event = buffer.events[buffer.used];
    if (!detail::copy_label(event.kind, kind) ||
        !detail::copy_label(event.phase, detail::context.phase)) {
        detail::lose_event();
        return;
    }
    event.sequence = detail::recorder.next_sequence.fetch_add(1, std::memory_order_relaxed);
    event.thread = index;
    event.owner = detail::identity(owner ? owner : detail::context.owner);
    event.subject = detail::identity(subject);
    event.bytes = bytes;
    event.value = value;
    ++buffer.used;
    detail::recorder.recorded.fetch_add(1, std::memory_order_relaxed);
}

class Scope {
public:
    explicit Scope(const void* owner) noexcept : previous_(detail::context.owner) {
        detail::context.owner = owner;
    }
    ~Scope() noexcept { detail::context.owner = previous_; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

private:
    const void* previous_;
};

class Phase {
public:
    explicit Phase(const char* phase) noexcept : previous_(detail::context.phase) {
        // Keep our own copy: a caller may pass a temporary string's c_str().
        if (!detail::copy_label(label_, phase ? phase : "unscoped")) {
            detail::lose_event();
            label_[0] = '\0';
        }
        detail::context.phase = label_;
    }
    ~Phase() noexcept { detail::context.phase = previous_; }
    Phase(const Phase&) = delete;
    Phase& operator=(const Phase&) = delete;
    Phase(Phase&&) = delete;
    Phase& operator=(Phase&&) = delete;

private:
    const char* previous_;
    char label_[label_capacity]{};
};

// A domain can fail independently of the other domains. N counts successful
// allocator calls (including allocate(0)), not objects or bytes. Backend failure
// does not consume a success. Negative N disables that domain. Invalid domains
// are ignored. Injection remains active until clear_failure() or replacement.
inline void fail_after(int domain, int successful_before_failure) noexcept {
    if (domain >= core_vector && domain <= validation_ranges) {
        detail::context.successful_before_failure[static_cast<std::size_t>(domain)] =
            successful_before_failure < 0 ? -1 : successful_before_failure;
    }
}

inline void clear_failure() noexcept {
    detail::context.successful_before_failure.fill(-1);
}

inline void clear_failure(int domain) noexcept {
    fail_after(domain, -1);
}

template<class T, int Domain>
class Allocator {
    static_assert(Domain >= core_vector && Domain <= validation_ranges,
                  "N3 allocator requires a declared allocation domain");

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using is_always_equal = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    template<class U>
    struct rebind { using other = Allocator<U, Domain>; };

    Allocator() noexcept = default;
    template<class U>
    Allocator(const Allocator<U, Domain>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t count) {
        const auto bytes = detail::allocation_bytes<T>(count);
        try {
            if (detail::injection_fails(Domain)) throw std::bad_alloc();
            auto* result = std::allocator<T>{}.allocate(count);
            detail::allocation_succeeded(Domain);
            emit("host_allocate", nullptr, result, bytes, Domain);
            return result;
        } catch (...) {
            emit("host_allocate_failed", nullptr, nullptr, bytes, Domain);
            throw;
        }
    }

    void deallocate(T* pointer, std::size_t count) noexcept {
        const auto bytes = detail::allocation_bytes<T>(count);
        // Publish the release before returning storage to the backend: another
        // thread can immediately reuse the same address after deallocation.
        // The allocator's deallocate operation cannot fail or throw.
        emit("host_deallocate", nullptr, pointer, bytes, Domain);
        std::allocator<T>{}.deallocate(pointer, count);
    }

    template<class U>
    bool operator==(const Allocator<U, Domain>&) const noexcept { return true; }
    template<class U>
    bool operator!=(const Allocator<U, Domain>&) const noexcept { return false; }
};

class Lock {
public:
    explicit Lock(std::mutex& mutex, const char* domain,
                  const void* owner = nullptr)
        : mutex_(mutex), owner_(owner ? owner : detail::context.owner) {
        domain_valid_ = domain && detail::copy_label(domain_, domain);
        const auto start = std::chrono::steady_clock::now();
        mutex_.lock();
        acquired_ = std::chrono::steady_clock::now();
        wait_ns_ = detail::nanoseconds(acquired_ - start);
    }

    ~Lock() noexcept {
        const auto end = std::chrono::steady_clock::now();
        // Explicit owner resolution happened at acquisition. A nested scope must
        // not accidentally reattribute an unowned lock at release.
        {
            Scope owner_scope(owner_);
            emit(domain_valid_ ? domain_ : nullptr, owner_, &mutex_, wait_ns_,
                 detail::nanoseconds(end - acquired_));
        }
        mutex_.unlock();
    }

    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
    Lock(Lock&&) = delete;
    Lock& operator=(Lock&&) = delete;

private:
    std::mutex& mutex_;
    const void* owner_;
    std::chrono::steady_clock::time_point acquired_{};
    std::uint64_t wait_ns_ = 0;
    char domain_[label_capacity]{};
    bool domain_valid_ = false;
};

// POSIX wrappers preserve the real call's errno even on success. Success value
// is zero; failure value is errno at return. Failed mmap subject is its requested
// address; successful mmap subject is the returned address. Other subjects are
// the supplied base. A syscall failure remains a valid recorded observation.
#if defined(__unix__) || defined(__APPLE__)
inline void* mmap_counted(void* address, std::size_t length, int protection,
                          int flags, int descriptor, off_t offset) noexcept {
    void* result = ::mmap(address, length, protection, flags, descriptor, offset);
    const int saved_errno = errno;
    const bool failed = result == MAP_FAILED;
    const bool fixed = (flags & MAP_FIXED) != 0;
    const char* kind = fixed ? (failed ? "os_replace_failed" : "os_replace")
                             : (failed ? "os_reserve_failed" : "os_reserve");
    emit(kind, nullptr, failed ? address : result, length,
         failed ? static_cast<std::uint64_t>(saved_errno) : 0);
    errno = saved_errno;
    return result;
}

inline int mprotect_counted(void* address, std::size_t length,
                            int protection) noexcept {
    const int result = ::mprotect(address, length, protection);
    const int saved_errno = errno;
    const bool failed = result != 0;
    const bool enabling = protection != PROT_NONE;
    const char* kind = enabling ? (failed ? "os_enable_failed" : "os_enable")
                                : (failed ? "os_protect_failed" : "os_protect");
    emit(kind, nullptr, address, length,
         failed ? static_cast<std::uint64_t>(saved_errno) : 0);
    errno = saved_errno;
    return result;
}

inline int munmap_counted(void* address, std::size_t length) noexcept {
    const int result = ::munmap(address, length);
    const int saved_errno = errno;
    emit(result != 0 ? "os_unmap_failed" : "os_unmap", nullptr, address, length,
         result != 0 ? static_cast<std::uint64_t>(saved_errno) : 0);
    errno = saved_errno;
    return result;
}

inline int madvise_counted(void* address, std::size_t length, int advice) noexcept {
    const int result = ::madvise(address, length, advice);
    const int saved_errno = errno;
    const bool reclaim = advice == MADV_DONTNEED;
    const char* kind = reclaim ? (result != 0 ? "os_reclaim_failed" : "os_reclaim")
                               : (result != 0 ? "os_advise_failed" : "os_advise");
    emit(kind, nullptr, address, length,
         result != 0 ? static_cast<std::uint64_t>(saved_errno) : 0);
    errno = saved_errno;
    return result;
}
#endif

// Byte-operation events are emitted only after the actual operation completes.
// Subject is destination; value is source identity for copy/move and zero for
// zero. Requested byte count is exact for this call; lower-level work is not.
inline void* copy(const char* kind, const void* owner, void* destination,
                  const void* source, std::size_t bytes) noexcept {
    void* result = std::memcpy(destination, source, bytes);
    emit(kind, owner, destination, bytes, detail::identity(source));
    return result;
}

inline void* move(const char* kind, const void* owner, void* destination,
                  const void* source, std::size_t bytes) noexcept {
    void* result = std::memmove(destination, source, bytes);
    emit(kind, owner, destination, bytes, detail::identity(source));
    return result;
}

inline void* zero(const char* kind, const void* owner, void* destination,
                  std::size_t bytes) noexcept {
    void* result = std::memset(destination, 0, bytes);
    emit(kind, owner, destination, bytes, 0);
    return result;
}

inline std::uint64_t event_count() noexcept {
    return detail::recorder.recorded.load(std::memory_order_relaxed);
}

inline std::uint64_t dropped_count() noexcept {
    return detail::recorder.dropped.load(std::memory_order_relaxed);
}

inline void dump(std::ostream& output) {
    std::vector<const detail::Event*> events;
    events.reserve(static_cast<std::size_t>(event_count()));
    const auto thread_count = std::min(
        detail::recorder.next_thread.load(std::memory_order_relaxed), max_threads);
    for (std::size_t thread = 0; thread < thread_count; ++thread) {
        const auto& buffer = detail::recorder.threads[thread];
        for (std::size_t index = 0; index < buffer.used; ++index)
            events.push_back(&buffer.events[index]);
    }
    std::sort(events.begin(), events.end(), [](const auto* left, const auto* right) {
        return left->sequence < right->sequence;
    });

    detail::write_literal(output,
        "{\"schema\":\"n3-events-v1\",\"diagnostic\":true,"
        "\"timing_evidence\":false,\"valid\":");
    detail::write_literal(output,
        detail::recorder.valid.load(std::memory_order_relaxed) ? "true" : "false");
    detail::write_literal(output, ",\"dropped\":");
    detail::write_number(output, dropped_count());
    detail::write_literal(output, ",\"thread_count\":");
    detail::write_number(output, thread_count);
    detail::write_literal(output, ",\"events\":[");
    bool first = true;
    for (const auto* event : events) {
        if (!first) output.put(',');
        first = false;
        detail::write_literal(output, "{\"sequence\":");
        detail::write_number(output, event->sequence);
        detail::write_literal(output, ",\"thread\":");
        detail::write_number(output, event->thread);
        detail::write_literal(output, ",\"phase\":");
        detail::write_string(output, event->phase);
        detail::write_literal(output, ",\"kind\":");
        detail::write_string(output, event->kind);
        detail::write_literal(output, ",\"owner\":");
        detail::write_number(output, event->owner);
        detail::write_literal(output, ",\"subject\":");
        detail::write_number(output, event->subject);
        detail::write_literal(output, ",\"bytes\":");
        detail::write_number(output, event->bytes);
        detail::write_literal(output, ",\"value\":");
        detail::write_number(output, event->value);
        output.put('}');
    }
    detail::write_literal(output, "]}\n");
}

} // namespace n3

#endif // XOFFSET_EXPERIMENT_N3_RUNTIME_HPP
