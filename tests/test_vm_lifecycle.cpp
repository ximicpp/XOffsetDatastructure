// Each --exit-* invocation is a separate CTest process: failures in static
// destructors must fail the test even when main() already returned success.
#include <cassert>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// Intentionally unregistered. Local root identity must preserve the historical
// const view without changing registered schema/hash rules.
struct LifecycleRoot { std::int64_t value{}; };
struct DifferentLifecycleRoot { std::int64_t value{}; };

namespace {

XBuffer global_first;
XBuffer global_second;

XBuffer make_buffer(std::int64_t value) {
    // VM storage: these checks rely on grow() keeping the address.
    auto buffer = XBuffer::create<LifecycleRoot>(4096,
        XBuffer::maximum_capacity(XBufferCore::DEFAULT_MAXIMUM_CAPACITY).with_storage(XStorage::Pinned));
    buffer.root<LifecycleRoot>().value = value;
    return buffer;
}

struct ShutdownTransfers {
    bool enabled = false;

    ~ShutdownTransfers() {
        if (!enabled) return;
        // Both globals outlive this object. The registry was first used in
        // main(), so an ordinary function-static mutex is already destroyed.
        auto first = global_first.handle<const LifecycleRoot>();
        auto second = global_second.handle<const LifecycleRoot>();
        global_first.swap(global_second);
        assert(first->value == 22 && second->value == 11);
        XBuffer local(std::move(global_first));
        assert(!first);
        static_cast<XBufferCore&>(global_first) =
            std::move(static_cast<XBufferCore&>(global_second));
        assert(first->value == 11 && !second);
        global_second = std::move(local);
        assert(second->value == 22);
        auto alloc = global_second.allocator<std::int64_t>();
        // V2: allocation resolves the owner from the host page before the
        // payload; after the shutdown transfers it names global_second.
        assert(XBufferCore::owner_from_arena_noexcept(alloc.arena()) == &global_second);
        auto* value = alloc.allocate(1);
        *value = 77;
        assert(*value == 77);
        alloc.deallocate(value, 1);
    }
} shutdown_transfers;

XBuffer& function_static_buffer() {
    static XBuffer buffer;
    return buffer;
}

struct ConstHandles {
    XHandle<const LifecycleRoot> checked;
    XHandle<const LifecycleRoot> unsafe;
    XHandle<const LifecycleRoot> direct;

    explicit ConstHandles(XBuffer& buffer)
        : checked(buffer.handle<const LifecycleRoot>()),
          unsafe(buffer.unsafe_handle<const LifecycleRoot>()),
          direct(static_cast<XBufferCore&>(buffer)) {}

    void expect(const LifecycleRoot* root, std::int64_t value) const {
        assert(checked.get() == root && unsafe.get() == root && direct.get() == root);
        assert(checked->value == value && unsafe->value == value && direct->value == value);
    }

    void expect_null() const {
        assert(!checked && !unsafe && !direct);
    }
};

static_assert(std::is_same_v<decltype(std::declval<ConstHandles>().checked.get()),
                             const LifecycleRoot*>);
static_assert(std::is_same_v<decltype(*std::declval<ConstHandles>().checked),
                             const LifecycleRoot&>);

void const_handles_grow_and_save() {
    auto buffer = make_buffer(123);
    ConstHandles handles(buffer);
    auto* root = &buffer.root<LifecycleRoot>();
    auto* base = buffer.get_address();
    const auto epoch = buffer.epoch();
    handles.expect(root, 123);
    assert(&buffer.root<const LifecycleRoot>() == root);
    assert(buffer.grow(buffer.accessible_bytes() + 1));
    assert(buffer.get_address() == base && buffer.epoch() == epoch);
    handles.expect(root, 123);
    const auto saved = buffer.save();
    assert(!saved.empty());
    assert(buffer.get_address() == base && buffer.epoch() == epoch);
    handles.expect(root, 123);
    assert(buffer.grow(65536));
    handles.expect(root, 123);

    XBufferCore core(4096);
    auto* core_root = core.construct_root_storage<LifecycleRoot>();
    core_root->value = 789;
    XHandle<const LifecycleRoot> core_handle(core);
    assert(core_handle.get() == core_root && core_handle->value == 789);
}

void const_handles_follow_replacements(bool through_core) {
    auto first = make_buffer(11);
    auto second = make_buffer(22);
    ConstHandles first_handles(first);
    ConstHandles second_handles(second);
    auto swap_buffers = [&] {
        if (through_core) static_cast<XBufferCore&>(first).swap(second);
        else first.swap(second);
    };
    auto assign_first = [&](XBuffer&& replacement) {
        if (through_core)
            static_cast<XBufferCore&>(first) =
                std::move(static_cast<XBufferCore&>(replacement));
        else first = std::move(replacement);
    };

    // Handles follow the owning C++ object across same-type storage changes.
    swap_buffers();
    first_handles.expect(&first.root<LifecycleRoot>(), 22);
    second_handles.expect(&second.root<LifecycleRoot>(), 11);
    assign_first(make_buffer(33));
    first_handles.expect(&first.root<LifecycleRoot>(), 33);

    second = XBuffer::create<DifferentLifecycleRoot>(4096);
    second.root<DifferentLifecycleRoot>().value = 44;
    auto other_checked = second.handle<const DifferentLifecycleRoot>();
    auto other_unsafe = second.unsafe_handle<const DifferentLifecycleRoot>();
    XHandle<const DifferentLifecycleRoot> other_direct(second);
    second_handles.expect_null();
    swap_buffers();
    first_handles.expect_null();
    second_handles.expect(&second.root<LifecycleRoot>(), 33);
    // Reverse rejection is equally important: equal size/layout is not identity.
    assert(!other_checked && !other_unsafe && !other_direct);
    swap_buffers();
    first_handles.expect(&first.root<LifecycleRoot>(), 33);
    second_handles.expect_null();
    assert(other_checked->value == 44 && other_unsafe->value == 44 && other_direct->value == 44);
    assign_first(std::move(second));
    first_handles.expect_null();
    assert(!other_checked && !other_unsafe && !other_direct);

    assign_first(make_buffer(55));
    first_handles.expect(&first.root<LifecycleRoot>(), 55);
    const auto raw_bytes = first.save();
    assign_first(XBuffer::load_unverified(raw_bytes));
    assert(!first_handles.checked);
    // Trusted entry points intentionally remain available for raw payloads.
    assert(first_handles.unsafe->value == 55 && first_handles.direct->value == 55);
    bool rejected = false;
    try { (void)first.handle<const LifecycleRoot>(); }
    catch (const XException&) { rejected = true; }
    assert(rejected);
    auto raw_unsafe = first.unsafe_handle<const LifecycleRoot>();
    XHandle<const LifecycleRoot> raw_direct(first);
    auto different = XBuffer::create<DifferentLifecycleRoot>(4096);
    assign_first(std::move(different));
    first_handles.expect_null();
    assert(!raw_unsafe && !raw_direct);

    assign_first(make_buffer(66));
    first_handles.expect(&first.root<LifecycleRoot>(), 66);
    XBuffer moved(std::move(first));
    first_handles.expect_null();
    ConstHandles moved_handles(moved);
    moved_handles.expect(&moved.root<LifecycleRoot>(), 66);
    assign_first(std::move(moved));
    moved_handles.expect_null();
    first_handles.expect(&first.root<LifecycleRoot>(), 66);
    assign_first(XBuffer{});
    first_handles.expect_null();
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2) {
        const std::string_view mode(argv[1]);
        if (mode == "--exit-empty-globals") return 0;
        if (mode == "--exit-global-assigned") {
            global_first = make_buffer(11);
            return 0;
        }
        if (mode == "--exit-static-before-registry") {
            auto& buffer = function_static_buffer();
            buffer = make_buffer(11);
            return 0;
        }
        if (mode == "--exit-static-after-registry") {
            global_first = make_buffer(11);
            function_static_buffer() = make_buffer(22);
            return 0;
        }
        if (mode == "--exit-shutdown-transfers") {
            global_first = make_buffer(11);
            global_second = make_buffer(22);
            shutdown_transfers.enabled = true;
            return 0;
        }
        return 2;
    }
    assert(argc == 1);
    const_handles_grow_and_save();
    const_handles_follow_replacements(false);
    const_handles_follow_replacements(true);
}
