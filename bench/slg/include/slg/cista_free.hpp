// SLG prototype: free what a deserialized cista object moved to the heap.
//
// A cista object deserialized in place owns nothing; containers that grow past their
// deserialized size move to self-allocated heap memory. A vector that is not
// self-allocated does not destroy its elements, so destroying the root does not
// reach those allocations. cista master (10abc43, 2026-04, after v0.16) added
// cista::free_self_allocated for this; v0.16 does not have it. This is the same
// post-order walk (children first, then the container itself), written against
// v0.16's recurse() overloads. cista is MIT licensed (Felix Gündling).
#pragma once
#include <cista/mode.h>
#include <cista/serialization.h>

namespace slg::cista_free {

struct Ctx {
    static constexpr auto const MODE = cista::mode::NONE;
    template <class T> void add_checked(T&&) {}
};

template <class T> void release(T*) {}
template <class T, template <class> class Ptr, bool Indexed, class S>
void release(cista::basic_vector<T, Ptr, Indexed, S>* el) { el->deallocate(); }
template <class Ptr> void release(cista::generic_string<Ptr>* el) { el->reset(); }
template <class Ptr> void release(cista::basic_string<Ptr>* el) { release(static_cast<cista::generic_string<Ptr>*>(el)); }

template <class T> void walk(Ctx& c, T* el) {
    using cista::recurse;
    recurse(c, el, [&](auto* entry) { walk(c, entry); });
    release(el);
}

template <class T> void free_self_allocated(T* el) {
    Ctx c;
    walk(c, el);
}

} // namespace slg::cista_free
