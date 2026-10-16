// Auxiliary N3 recorder controls. Each case requires a fresh process.
// Compile as C++17 or later; stdout contains exactly one n3-events-v1 object.
#include "n3_runtime.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void threads_control() {
    std::mutex subject;
    // Thread slots are lifetime slots. Sequential joins deliberately avoid
    // making this a contention/stress test, including when an OS reuses its tid.
    for (std::size_t index = 0; index < n3::max_threads + 1; ++index) {
        std::thread worker([&subject] {
            n3::emit("registry_lock", nullptr, &subject, 0, 0);
        });
        worker.join();
    }
    require(n3::event_count() == n3::max_threads,
            "thread exhaustion must retain exactly the first 16 events");
    require(n3::dropped_count() > 0,
            "the seventeenth lifetime thread must invalidate the recorder");
}

void labels_control() {
    // Exactly 64 non-NUL label bytes exceed the fixed 63-byte label payload.
    std::array<char, n3::label_capacity + 1> label{};
    label.fill('L');
    label.back() = '\0';
    std::mutex subject;
    {
        n3::Phase phase(label.data());
        n3::emit("registry_lock", nullptr, &subject, 0, 0);
    }
    require(n3::dropped_count() > 0,
            "an overlong phase label must invalidate the recorder");
}

void allocator_control() {
    // A stable diagnostic owner identity, deliberately not a production arena.
    alignas(std::max_align_t) unsigned char owner[64]{};
    n3::Allocator<int, 1> allocator;
    int* first = nullptr;
    int* unexpected = nullptr;
    bool failed_as_expected = false;
    {
        n3::Scope scope(owner);
        n3::fail_after(1, 1);
        try {
            first = allocator.allocate(4);
            first[0] = 17;
            first[3] = 29;
            try {
                unexpected = allocator.allocate(2);
            } catch (const std::bad_alloc&) {
                failed_as_expected = true;
            }
            n3::clear_failure();
            if (unexpected) allocator.deallocate(unexpected, 2);
            require(first[0] == 17 && first[3] == 29,
                    "injected failure must preserve the earlier allocation");
            allocator.deallocate(first, 4);
            first = nullptr;
        } catch (...) {
            n3::clear_failure();
            if (first) allocator.deallocate(first, 4);
            throw;
        }
    }
    require(failed_as_expected,
            "the second tracked allocation must throw std::bad_alloc");
    require(n3::dropped_count() == 0,
            "allocator failure control must leave a valid recorder");
    require(n3::event_count() == 3,
            "allocator control must record allocate, failed allocate, deallocate");
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3 || std::string(argv[1]) != "--case") {
            std::cerr << "usage: runtime_controls --case threads|labels|allocator\n";
            return 2;
        }
        const std::string name = argv[2];
        if (name == "threads") threads_control();
        else if (name == "labels") labels_control();
        else if (name == "allocator") allocator_control();
        else {
            std::cerr << "unknown runtime control case\n";
            return 2;
        }
        // All emitting workers have joined and all tested allocations are freed.
        n3::dump(std::cout);
        return std::cout ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
