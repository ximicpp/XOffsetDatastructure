#ifndef X_OFFSET_DATA_STRUCTURE_HPP
#define X_OFFSET_DATA_STRUCTURE_HPP

#if defined(__LP64__) || defined(_WIN64) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
    #define XOFFSET_ARCH_64BIT 1
#else
    #define XOFFSET_ARCH_64BIT 0
#endif

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    #define XOFFSET_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#elif defined(_WIN32) || defined(_WIN64) || defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
    #define XOFFSET_LITTLE_ENDIAN 1
#else
    #define XOFFSET_LITTLE_ENDIAN 0
#endif

#ifndef XOFFSET_DISABLE_PLATFORM_CHECKS
    #if !XOFFSET_ARCH_64BIT
        #error "XOffsetDatastructure requires 64-bit architecture"
    #endif
    #if !XOFFSET_LITTLE_ENDIAN
        #error "XOffsetDatastructure requires little-endian architecture"
    #endif
#endif

#include <experimental/meta>
#include <type_traits>
#include <string>
#include <vector>
#include <cstring>

// TypeLayout library — the authoritative type-signature and type-safety engine.
// XOffset delegates ALL type safety and layout portability decisions to TypeLayout.
//   - is_byte_copy_safe_v<T>              — recursive domain admission predicate
//   - is_transfer_safe<T>(remote_sig)     — byte-copy safe + layout signature match
//   - get_layout_signature<T>()           — binary layout signature
#include <boost/typelayout.hpp>
#include <boost/typelayout/tools/sig_types.hpp>  // PlatformInfo
#include <boost/container/scoped_allocator.hpp>

// Platform: 64-bit little-endian only.
// Type safety: delegated to TypeLayout (see boost::typelayout::is_byte_copy_safe_v<T>).
#ifndef XOFFSET_DISABLE_PLATFORM_CHECKS
static_assert(sizeof(void*) == 8,
    "XOffsetDatastructure requires 64-bit platform (sizeof(void*) must be 8)");
static_assert(XOFFSET_LITTLE_ENDIAN,
    "XOffsetDatastructure requires little-endian platform");
#endif // XOFFSET_DISABLE_PLATFORM_CHECKS

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/detail/managed_memory_impl.hpp>
#include <boost/interprocess/indexes/iset_index.hpp>
#include <boost/interprocess/exceptions.hpp>
#include <boost/interprocess/mem_algo/rbtree_best_fit.hpp>
#include <boost/interprocess/sync/mutex_family.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/string.hpp>
#include <boost/container/detail/next_capacity.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace interprocess {

template <class MutexFamily, class VoidPointer = offset_ptr<void>, std::size_t MemAlignment = 0>
class x_best_fit : public rbtree_best_fit<MutexFamily, VoidPointer, MemAlignment>
{
    typedef rbtree_best_fit<MutexFamily, VoidPointer, MemAlignment> supertype;

public:
    typedef typename supertype::size_type size_type;

    x_best_fit(typename supertype::size_type size, typename supertype::size_type extra_hdr_bytes)
        : supertype(size, extra_hdr_bytes)
    {
    }
};

// ============================================================================
// XManagedMemory — Managed memory segment backed by std::vector<char>
//
// Uses vector::reserve() for virtual address pre-allocation (lazy-commit by OS).
// grow() stays on a fast path while size <= capacity; vector relocation triggers
// close_impl/open_impl + epoch bump + re-reserve for address stability.
// ============================================================================
template <
    class CharType,
    class AllocationAlgorithm,
    template <class IndexConfig> class IndexType>
class XManagedMemory
    : public ipcdetail::basic_managed_memory_impl<CharType, AllocationAlgorithm, IndexType>
{
private:
    typedef ipcdetail::basic_managed_memory_impl<CharType, AllocationAlgorithm, IndexType> base_t;
    BOOST_MOVABLE_BUT_NOT_COPYABLE(XManagedMemory)

public:
    typedef typename base_t::size_type size_type;

    // Adaptive reservation: reserve = clamp(size × 16, 64KB, 256MB).
    static constexpr std::size_t GROWTH_HEADROOM = 16;
    static constexpr std::size_t MIN_RESERVE = 64ULL * 1024;         // 64 KB
    static constexpr std::size_t MAX_RESERVE = 256ULL * 1024 * 1024; // 256 MB

    static constexpr std::size_t compute_reservation(std::size_t size) {
        std::size_t r;
        if (size > MAX_RESERVE / GROWTH_HEADROOM) {
            r = MAX_RESERVE;
        } else {
            r = size * GROWTH_HEADROOM;
        }
        if (r < MIN_RESERVE) r = MIN_RESERVE;
        if (r > MAX_RESERVE) r = MAX_RESERVE;
        return r;
    }

    XManagedMemory() noexcept
    {
    }

    ~XManagedMemory()
    {
        this->priv_close();
    }

    // Bumped on base-address change (vector relocation). XHandle uses this for O(1) caching.
    uint64_t epoch() const noexcept { return m_epoch; }

    /// Construct with adaptive reservation.
    XManagedMemory(size_type size)
        : m_buffer(size, char(0))
    {
        m_buffer.reserve(compute_reservation(size));
        void *addr = m_buffer.data();
        if (!base_t::create_impl(addr, size))
        {
            this->priv_close();
            throw interprocess_exception("Could not initialize heap in XManagedMemory constructor");
        }
    }

    /// Construct with explicit max capacity.
    XManagedMemory(size_type size, size_type max_reserved)
        : m_buffer(size, char(0))
    {
        m_buffer.reserve(max_reserved);
        void *addr = m_buffer.data();
        if (!base_t::create_impl(addr, size))
        {
            this->priv_close();
            throw interprocess_exception("Could not initialize heap in XManagedMemory constructor");
        }
    }

    /// Construct from serialized data (load path).
    XManagedMemory(const char* data, size_type size)
        : m_buffer(data, data + size)
    {
        m_buffer.reserve(compute_reservation(size));
        void *addr = m_buffer.data();
        BOOST_ASSERT((0 == (((std::size_t)addr) & (AllocationAlgorithm::Alignment - size_type(1u)))));
        if (!base_t::open_impl(addr, size))
        {
            throw interprocess_exception("Could not initialize m_buffer in constructor");
        }
    }

    /// Construct from existing vector (legacy).
    XManagedMemory(std::vector<char> &externalBuffer)
        : m_buffer(externalBuffer)
    {
        m_buffer.reserve(compute_reservation(m_buffer.size()));
        void *addr = m_buffer.data();
        size_type size = m_buffer.size();
        BOOST_ASSERT((0 == (((std::size_t)addr) & (AllocationAlgorithm::Alignment - size_type(1u)))));
        if (!base_t::open_impl(addr, size))
        {
            throw interprocess_exception("Could not initialize m_buffer in constructor");
        }
    }

    /// Construct by moving existing vector.
    XManagedMemory(std::vector<char> &&externalBuffer)
        : m_buffer(std::move(externalBuffer))
    {
        m_buffer.reserve(compute_reservation(m_buffer.size()));
        void *addr = m_buffer.data();
        size_type size = m_buffer.size();
        BOOST_ASSERT((0 == (((std::size_t)addr) & (AllocationAlgorithm::Alignment - size_type(1u)))));
        if (!base_t::open_impl(addr, size))
        {
            throw interprocess_exception("Could not initialize m_buffer in move constructor");
        }
    }

    XManagedMemory(BOOST_RV_REF(XManagedMemory) moved) noexcept
    {
        this->swap(moved);
    }

    XManagedMemory &operator=(BOOST_RV_REF(XManagedMemory) moved) noexcept
    {
        XManagedMemory tmp(boost::move(moved));
        this->swap(tmp);
        return *this;
    }

    // Grow by extra_bytes. Fast path (within capacity) is address-stable.
    bool grow(size_type extra_bytes)
    {
        size_type old_size = m_buffer.size();
        char* old_addr = m_buffer.data();

        try {
            m_buffer.resize(old_size + extra_bytes, char(0));
        } catch (...) {
            return false;
        }

        if (m_buffer.data() == old_addr) {
            base_t::grow(extra_bytes);  // within reserve — address stable
        } else {
            // Vector relocated — reopen segment at new address.
            base_t::close_impl();
            if (!base_t::open_impl(m_buffer.data(), old_size)) {
                throw interprocess_exception(
                    "XManagedMemory: failed to reopen segment after vector relocation");
            }
            base_t::grow(extra_bytes);
            ++m_epoch;
            m_buffer.reserve(compute_reservation(m_buffer.size()));
        }
        return true;
    }

    void swap(XManagedMemory &other) noexcept
    {
        base_t::swap(other);
        m_buffer.swap(other.m_buffer);
        std::swap(m_epoch, other.m_epoch);
    }

    // Shrinks segment logical size (does NOT shrink the vector — preserves base address).
    void shrink_to_fit()
    {
        base_t::shrink_to_fit();
    }

    std::vector<char> *get_buffer()
    {
        return &m_buffer;
    }

    const void* get_address() const
    {
        return m_buffer.data();
    }

    size_type get_size() const
    {
        return m_buffer.size();
    }

    /// Segment logical size (byte-exact, for serialization).
    size_type segment_size() const
    {
        return base_t::get_size();
    }

private:
    void priv_close()
    {
        base_t::destroy_impl();
        m_buffer.clear();
        m_buffer.shrink_to_fit();
    }

    std::vector<char> m_buffer;
    uint64_t m_epoch = 0;
};

} // namespace interprocess
} // namespace boost

namespace XOffsetDatastructure {
    using namespace boost::interprocess;

    // rbtree_best_fit: O(log n) allocation with automatic free-block coalescing.
    using XBufferCore = XManagedMemory<char, x_best_fit<null_mutex_family>, iset_index>;

    // ========================================================================
    // Container implementation details.
    // ========================================================================
    namespace detail {

        // Container concepts — used only by XCompactor for migration dispatch.
        template<typename T>
        concept SequentialContainer = requires(T t) {
            { t.begin() } -> std::input_or_output_iterator;
            { t.end() } -> std::input_or_output_iterator;
            typename T::value_type;
            { t.emplace_back(std::move(std::declval<typename T::value_type>())) };
        };

        template<typename T>
        concept SetLikeContainer = requires(T t) {
            { t.begin() } -> std::input_or_output_iterator;
            { t.end() } -> std::input_or_output_iterator;
            typename T::value_type;
            typename T::key_type;
            { t.emplace(std::move(std::declval<typename T::value_type>())) };
        } && !requires { typename T::mapped_type; };

        template<typename T>
        concept MapLikeContainer = requires(T t) {
            { t.begin() } -> std::input_or_output_iterator;
            { t.end() } -> std::input_or_output_iterator;
            typename T::key_type;
            typename T::mapped_type;
            { t.emplace(std::move(std::declval<typename T::key_type>()),
                         std::move(std::declval<typename T::mapped_type>())) };
        };
        /// Growth factor: 1.1x (11/10)
        struct growth_factor_custom
            : boost::container::dtl::grow_factor_ratio<0, 11, 10> {};

        using x_vector_options = boost::container::vector_options_t<
            boost::container::growth_factor<growth_factor_custom>>;

        template <typename T>
        using x_base_scoped_alloc = boost::container::scoped_allocator_adaptor<
            boost::interprocess::allocator<T, XBufferCore::segment_manager>>;

        // Forward declarations (resolved at instantiation time).
        template <typename T, typename Alloc>
        void reflect_init_all(void* raw, Alloc alloc);

        template <typename T, typename Src>
        void reflect_transfer_init_all(void* dst, Src&& src,
            XBufferCore::segment_manager* sm);

        // True for pure aggregates needing reflection-based construction.
        template <typename T>
        concept needs_reflect_construct =
            std::is_class_v<T> &&
            !std::is_trivially_copyable_v<T> &&
            !requires { typename T::allocator_type; } &&
            !requires(XBufferCore::segment_manager* sm) { T(sm); };

        // Scoped allocator intercepting construct() for pure aggregates via reflection.
        template <typename T>
        class x_reflect_scoped_alloc : public x_base_scoped_alloc<T> {
            using Base = x_base_scoped_alloc<T>;
        public:
            using Base::Base;

            template <typename U>
            struct rebind { using other = x_reflect_scoped_alloc<U>; };

            template <typename U>
            x_reflect_scoped_alloc(const x_reflect_scoped_alloc<U>& other) noexcept
                : Base(other) {}

            template <typename U>
            void construct(U* p) {
                if constexpr (needs_reflect_construct<U>) {
                    reflect_init_all<U>(
                        static_cast<void*>(p),
                        this->outer_allocator().get_segment_manager());
                } else {
                    Base::construct(p);
                }
            }

            template <typename U, typename Arg>
            void construct(U* p, Arg&& arg) {
                if constexpr (needs_reflect_construct<U> &&
                              std::is_same_v<std::decay_t<Arg>, U>) {
                    reflect_transfer_init_all<U>(
                        static_cast<void*>(p),
                        std::forward<Arg>(arg),
                        this->outer_allocator().get_segment_manager());
                } else {
                    Base::construct(p, std::forward<Arg>(arg));
                }
            }

            template <typename U, typename A1, typename A2, typename... Rest>
            void construct(U* p, A1&& a1, A2&& a2, Rest&&... rest) {
                Base::construct(p,
                    std::forward<A1>(a1),
                    std::forward<A2>(a2),
                    std::forward<Rest>(rest)...);
            }
        };

        template <typename T>
        using x_scoped_alloc = x_reflect_scoped_alloc<T>;

        template <typename T>
        using x_vector_impl = boost::container::vector<
            T, x_scoped_alloc<T>, x_vector_options>;

        template <typename K, typename V>
        using x_map_impl = boost::container::flat_map<K, V, std::less<void>,
            x_vector_impl<std::pair<K, V>>>;

        template <typename T>
        using x_set_impl = boost::container::flat_set<T, std::less<void>, x_vector_impl<T>>;

    } // namespace detail

    using XString = boost::container::basic_string<
        char, std::char_traits<char>, allocator<char, XBufferCore::segment_manager>>;

    using XAllocator = boost::interprocess::allocator<char, XBufferCore::segment_manager>;

    // ========================================================================
    // Public container wrappers (overloads for allocator-aware element types).
    // ========================================================================

    template <typename T>
    class XVector : public detail::x_vector_impl<T> {
        using Base = detail::x_vector_impl<T>;
        using SM = XBufferCore::segment_manager;
        auto* sm() { return this->get_stored_allocator().get_segment_manager(); }

    public:
        using Base::Base;           // inherit all constructors
        using Base::push_back;      // keep base push_back(const T&), push_back(T&&)
        using Base::insert;         // keep all base insert overloads
        using Base::resize;
        using Base::assign;

        // Overloads forwarding to emplace for allocator-aware element types.
        // Constraint: T has allocator_type AND Arg is not T itself.
        template<typename Arg>
            requires (requires { typename T::allocator_type; } &&
                      !std::is_same_v<std::decay_t<Arg>, T>)
        void push_back(Arg&& arg) {
            Base::emplace_back(std::forward<Arg>(arg));
        }

        template<typename Arg>
            requires (requires { typename T::allocator_type; } &&
                      !std::is_same_v<std::decay_t<Arg>, T>)
        typename Base::iterator insert(typename Base::const_iterator pos, Arg&& arg) {
            return Base::emplace(pos, std::forward<Arg>(arg));
        }

        template<typename Arg>
            requires (requires { typename T::allocator_type; } &&
                      !std::is_same_v<std::decay_t<Arg>, T>)
        typename Base::iterator insert(typename Base::const_iterator pos,
                                       typename Base::size_type n, Arg&& arg) {
            T tmp(std::forward<Arg>(arg), sm());
            return Base::insert(pos, n, tmp);
        }

        template<typename Arg>
            requires (requires { typename T::allocator_type; } &&
                      !std::is_same_v<std::decay_t<Arg>, T>)
        void resize(typename Base::size_type n, Arg&& arg) {
            T tmp(std::forward<Arg>(arg), sm());
            Base::resize(n, tmp);
        }

        template<typename Arg>
            requires (requires { typename T::allocator_type; } &&
                      !std::is_same_v<std::decay_t<Arg>, T>)
        void assign(typename Base::size_type n, Arg&& arg) {
            T tmp(std::forward<Arg>(arg), sm());
            Base::assign(n, tmp);
        }
    };

    template <typename K, typename V>
    class XMap : public detail::x_map_impl<K, V> {
        using Base = detail::x_map_impl<K, V>;
        using SM = XBufferCore::segment_manager;
        auto* sm() { return this->get_stored_allocator().get_segment_manager(); }

    public:
        using Base::Base;
        using Base::operator[];
        using Base::erase;
        using Base::try_emplace;
        using Base::insert_or_assign;

        // Heterogeneous key overloads — piecewise_construct for allocator injection.
        template<typename KeyArg>
            requires (!std::is_same_v<std::decay_t<KeyArg>, K>)
        V& operator[](const KeyArg& key) {
            auto it = this->find(key);  // transparent comparator
            if (it != this->end()) return it->second;
            auto result = this->emplace(
                std::piecewise_construct,
                std::forward_as_tuple(key),
                std::forward_as_tuple());
            return result.first->second;
        }

        template<typename KeyArg>
            requires (!std::is_same_v<std::decay_t<KeyArg>, K> &&
                      !std::is_convertible_v<const KeyArg&, typename Base::const_iterator>)
        typename Base::size_type erase(const KeyArg& key) {
            auto it = this->find(key);
            if (it == this->end()) return 0;
            Base::erase(it);
            return 1;
        }

        template<typename KeyArg, typename... Args>
            requires (!std::is_same_v<std::decay_t<KeyArg>, K>)
        std::pair<typename Base::iterator, bool> try_emplace(KeyArg&& key, Args&&... args) {
            auto it = this->find(key);
            if (it != this->end()) return {it, false};
            return this->emplace(
                std::piecewise_construct,
                std::forward_as_tuple(std::forward<KeyArg>(key)),
                std::forward_as_tuple(std::forward<Args>(args)...));
        }

        template<typename KeyArg, typename M>
            requires (!std::is_same_v<std::decay_t<KeyArg>, K>)
        std::pair<typename Base::iterator, bool> insert_or_assign(KeyArg&& key, M&& obj) {
            auto it = this->find(key);
            if (it != this->end()) {
                it->second = std::forward<M>(obj);
                return {it, false};
            }
            return this->emplace(
                std::piecewise_construct,
                std::forward_as_tuple(std::forward<KeyArg>(key)),
                std::forward_as_tuple(std::forward<M>(obj)));
        }
    };

    template <typename T>
    class XSet : public detail::x_set_impl<T> {
        using Base = detail::x_set_impl<T>;
        using SM = XBufferCore::segment_manager;

    public:
        using Base::Base;
        using Base::insert;
        using Base::erase;

        template<typename Arg>
            requires (!std::is_same_v<std::decay_t<Arg>, T>)
        std::pair<typename Base::iterator, bool> insert(const Arg& val) {
            return this->emplace(val);  // scoped_alloc handles T construction
        }

        template<typename Arg>
            requires (!std::is_same_v<std::decay_t<Arg>, T>)
        typename Base::iterator insert(typename Base::const_iterator pos, const Arg& val) {
            return this->emplace_hint(pos, val);
        }

        template<typename KeyArg>
            requires (!std::is_same_v<std::decay_t<KeyArg>, T> &&
                      !std::is_convertible_v<const KeyArg&, typename Base::const_iterator>)
        typename Base::size_type erase(const KeyArg& key) {
            auto it = this->find(key);
            if (it == this->end()) return 0;
            Base::erase(it);
            return 1;
        }
    };

    static_assert(sizeof(XVector<int>) == sizeof(detail::x_vector_impl<int>),
        "XVector wrapper must be zero-overhead");
    static_assert(sizeof(XMap<int,int>) == sizeof(detail::x_map_impl<int,int>),
        "XMap wrapper must be zero-overhead");
    static_assert(sizeof(XSet<int>) == sizeof(detail::x_set_impl<int>),
        "XSet wrapper must be zero-overhead");

    struct MemoryStats {
        std::size_t total_size;
        std::size_t free_size;
        std::size_t used_size;
        double usage_percent() const { return total_size > 0 ? (used_size * 100.0 / total_size) : 0.0; }
        double free_percent() const  { return total_size > 0 ? (free_size * 100.0 / total_size) : 0.0; }
    };

    inline MemoryStats memory_stats(XBufferCore& xbuf) {
        return { xbuf.get_size(), xbuf.get_free_memory(), xbuf.get_size() - xbuf.get_free_memory() };
    }

    // Internal constant for the single root object name.
    // Users never see this — all public APIs hide the naming layer.
    inline constexpr const char* XBUFFER_ROOT_NAME = "__root__";

    namespace detail {

        // Type Safety — domain admission delegated to TypeLayout.
        using boost::typelayout::is_byte_copy_safe_v;

        template<typename T>
        consteval const char* describe_safety() {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (boost::typelayout::is_byte_copy_safe_v<CleanT>) {
                return "byte-copy safe";
            } else if constexpr (std::is_polymorphic_v<CleanT>) {
                return "rejected: polymorphic type (has vtable pointer)";
            } else if constexpr (std::is_union_v<CleanT>) {
                return "rejected: union type";
            } else {
                return "rejected: contains pointer, reference, or unsafe member";
            }
        }
    } // namespace detail

    /// Public admission gate — alias for TypeLayout's is_byte_copy_safe_v<T>.
    /// Prefer using boost::typelayout::is_byte_copy_safe_v<T> directly in new code.
    template<typename T>
    struct is_xbuffer_safe {
        static constexpr bool value = boost::typelayout::is_byte_copy_safe_v<T>;
        static constexpr const char* reason() { return detail::describe_safety<T>(); }
    };

    template<typename T>
    constexpr void validate_xbuffer_type() {
        static_assert(is_xbuffer_safe<T>::value,
            "XBuffer Type Safety Error: type is not byte-copy safe. "
            "Allowed: primitives, XString, XVector<T>, XMap<K,V>, XSet<T>, "
            "or structs composed of these. "
            "Not allowed: pointers, references, virtual types, std containers.");
    }

    // ========================================================================
    // Reflection-based construction and transfer (C++26 P2996).
    // ========================================================================
    namespace detail {

        template <typename T>
        concept has_allocator_type_member = requires { typename T::allocator_type; };

        template <typename T>
        concept has_segment_manager_ctor = requires(XBufferCore::segment_manager* sm) {
            T(sm);
        };

        template <typename T, typename Alloc>
        void reflect_init_all_impl(void* raw, Alloc alloc);

        template <typename T, std::size_t N, typename Alloc>
        void reflect_init_nth(void* raw, Alloc alloc) {
            using namespace std::meta;
            constexpr auto member =
                nonstatic_data_members_of(^^T, access_context::unchecked())[N];
            using M = [:type_of(member):];
            T& obj = *reinterpret_cast<T*>(raw);
            if constexpr (has_allocator_type_member<M>) {
                std::construct_at(&(obj.[:member:]), alloc);
            } else if constexpr (!std::is_trivially_copyable_v<M> && std::is_class_v<M>) {
                reflect_init_all_impl<M>(reinterpret_cast<void*>(&(obj.[:member:])), alloc);
            } else {
                std::construct_at(&(obj.[:member:]));  // value-init (zero)
            }
        }

        template <typename T, typename Alloc, std::size_t... Is>
        void reflect_init_expand(void* raw, Alloc alloc, std::index_sequence<Is...>) {
            (reflect_init_nth<T, Is>(raw, alloc), ...);
        }

        template <typename T, std::size_t N, typename Alloc>
        void reflect_init_base_nth(void* raw, Alloc alloc) {
            using namespace std::meta;
            constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
            using BaseType = [:type_of(base_info):];
            T* obj = reinterpret_cast<T*>(raw);
            BaseType* base_ptr = static_cast<BaseType*>(obj);
            reflect_init_all_impl<BaseType>(static_cast<void*>(base_ptr), alloc);
        }

        template <typename T, typename Alloc, std::size_t... Is>
        void reflect_init_bases_expand(void* raw, Alloc alloc, std::index_sequence<Is...>) {
            (reflect_init_base_nth<T, Is>(raw, alloc), ...);
        }

        template <typename T, typename Alloc>
        void reflect_init_all_impl(void* raw, Alloc alloc) {
            constexpr auto bc = std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size();
            constexpr auto mc = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).size();
            if constexpr (bc > 0) reflect_init_bases_expand<T>(raw, alloc, std::make_index_sequence<bc>{});
            if constexpr (mc > 0) reflect_init_expand<T>(raw, alloc, std::make_index_sequence<mc>{});
        }

        template <typename T, typename Alloc>
        void reflect_init_all(void* raw, Alloc alloc) {
            std::memset(raw, 0, sizeof(T));  // zero ONCE at top level
            reflect_init_all_impl<T>(raw, alloc);
        }

        // ── Reflection Transfer (move/copy with allocator injection) ──

        template <typename T, typename Src>
        void reflect_transfer_init_all_impl(void* dst, Src&& src,
                                            XBufferCore::segment_manager* sm);

        template <typename T, std::size_t N, typename Src>
        void reflect_transfer_init_nth(void* dst, Src&& src,
                                       XBufferCore::segment_manager* sm) {
            using namespace std::meta;
            constexpr auto member =
                nonstatic_data_members_of(^^T, access_context::unchecked())[N];
            using M = [:type_of(member):];
            T& d = *reinterpret_cast<T*>(dst);
            if constexpr (has_allocator_type_member<M>) {
                std::construct_at(&(d.[:member:]),
                    std::forward<Src>(src).[:member:], sm);
            } else if constexpr (!std::is_trivially_copyable_v<M> && std::is_class_v<M>) {
                reflect_transfer_init_all_impl<M>(
                    reinterpret_cast<void*>(&(d.[:member:])),
                    std::forward<Src>(src).[:member:], sm);
            } else {
                std::construct_at(&(d.[:member:]),
                    std::forward<Src>(src).[:member:]);
            }
        }

        template <typename T, typename Src, std::size_t... Is>
        void reflect_transfer_init_expand(void* dst, Src&& src,
                                          XBufferCore::segment_manager* sm,
                                          std::index_sequence<Is...>) {
            (reflect_transfer_init_nth<T, Is>(dst, std::forward<Src>(src), sm), ...);
        }

        template <typename T, std::size_t N, typename Src>
        void reflect_transfer_base_nth(void* dst, Src&& src,
                                       XBufferCore::segment_manager* sm) {
            using namespace std::meta;
            constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
            using BaseType = [:type_of(base_info):];

            T* dst_obj = reinterpret_cast<T*>(dst);
            BaseType* dst_base = static_cast<BaseType*>(dst_obj);

            if constexpr (std::is_lvalue_reference_v<Src&&>) {
                reflect_transfer_init_all_impl<BaseType>(
                    static_cast<void*>(dst_base),
                    static_cast<const BaseType&>(src), sm);
            } else {
                reflect_transfer_init_all_impl<BaseType>(
                    static_cast<void*>(dst_base),
                    static_cast<BaseType&&>(std::move(src)), sm);
            }
        }

        template <typename T, typename Src, std::size_t... Is>
        void reflect_transfer_bases_expand(void* dst, Src&& src,
                                           XBufferCore::segment_manager* sm,
                                           std::index_sequence<Is...>) {
            (reflect_transfer_base_nth<T, Is>(dst, std::forward<Src>(src), sm), ...);
        }

        template <typename T, typename Src>
        void reflect_transfer_init_all_impl(void* dst, Src&& src,
                                            XBufferCore::segment_manager* sm) {
            constexpr auto bc = std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size();
            constexpr auto mc = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).size();
            if constexpr (bc > 0) reflect_transfer_bases_expand<T>(dst, std::forward<Src>(src), sm, std::make_index_sequence<bc>{});
            if constexpr (mc > 0) reflect_transfer_init_expand<T>(dst, std::forward<Src>(src), sm, std::make_index_sequence<mc>{});
        }

        template <typename T, typename Src>
        void reflect_transfer_init_all(void* dst, Src&& src,
                                       XBufferCore::segment_manager* sm) {
            std::memset(dst, 0, sizeof(T));  // zero ONCE at top level
            reflect_transfer_init_all_impl<T>(dst, std::forward<Src>(src), sm);
        }

        // Internal wrapper: constructs T via reflection on raw storage.
        template <typename T>
        struct ReflectRoot {
            alignas(T) unsigned char storage[sizeof(T)];

            template <typename Alloc>
            ReflectRoot(Alloc alloc) {
                reflect_init_all<T>(storage, alloc);
            }

            T& ref() noexcept {
                return *std::launder(reinterpret_cast<T*>(storage));
            }
            const T& ref() const noexcept {
                return *std::launder(reinterpret_cast<const T*>(storage));
            }
        };

        template <typename T>
        T* construct_root(XBufferCore& xbuf) {
            auto* sm = xbuf.get_segment_manager();
            if constexpr (has_segment_manager_ctor<T>) {
                return xbuf.template construct<T>(XBUFFER_ROOT_NAME)(sm);
            } else {
                auto* wrapper = xbuf.template construct<ReflectRoot<T>>(
                    XBUFFER_ROOT_NAME)(sm);
                return &wrapper->ref();
            }
        }

        template <typename T>
        T* find_root(XBufferCore& xbuf) {
            if constexpr (has_segment_manager_ctor<T>) {
                return xbuf.template find<T>(XBUFFER_ROOT_NAME).first;
            } else {
                auto result = xbuf.template find<ReflectRoot<T>>(XBUFFER_ROOT_NAME);
                return result.first ? &result.first->ref() : nullptr;
            }
        }

    } // namespace detail (reflection)

    // XHandle<T> — Epoch-cached handle. O(1) deref when address is stable.
    // ========================================================================
    template <typename T>
    class XHandle {
    public:
        XHandle() noexcept : buffer_(nullptr) {}

        explicit XHandle(XBufferCore& buf) noexcept
            : buffer_(&buf), cached_ptr_(nullptr), cached_epoch_(0)
        {
            resolve();
        }

        T* operator->() const {
            return resolve();
        }

        T& operator*() const {
            return *resolve();
        }

        T* get() const {
            return resolve();
        }

        explicit operator bool() const {
            return resolve() != nullptr;
        }

    private:
        T* resolve() const {
            if (!buffer_) return nullptr;
            uint64_t current_epoch = buffer_->epoch();
            if (cached_ptr_ && cached_epoch_ == current_epoch) return cached_ptr_;
            cached_ptr_ = detail::find_root<T>(*buffer_);
            cached_epoch_ = current_epoch;
            return cached_ptr_;
        }

        XBufferCore* buffer_;
        mutable T* cached_ptr_ = nullptr;
        mutable uint64_t cached_epoch_ = 0;
    };

    // XBuffer — Single root object per buffer (make/root/has_root).
    // ========================================================================
    class XBuffer : public XBufferCore {
    public:
        using XBufferCore::XBufferCore;

        /// Explicit max capacity override for the adaptive reservation.
        struct MaxCapacity { std::size_t value; };
        static MaxCapacity max_capacity(std::size_t bytes) { return {bytes}; }

        XBuffer(std::size_t size, MaxCapacity cap)
            : XBufferCore(size, cap.value) {}

        /// Create the root object. Returned pointer may dangle after grow/compact.
        template<typename T>
        T* make() {
            validate_xbuffer_type<T>();
            if (detail::find_root<T>(*this) != nullptr) {
                throw boost::interprocess::interprocess_exception(
                    "make<T>(): root object already exists. "
                    "Call root<T>() to access the existing object.");
            }
            return detail::construct_root<T>(*this);
        }
        
        template<typename T>
        T& root() {
            T* ptr = detail::find_root<T>(*this);
            if (!ptr) {
                throw boost::interprocess::interprocess_exception(
                    "root<T>(): no root object in buffer. "
                    "Call make<T>() first or check with has_root<T>().");
            }
            return *ptr;
        }

        template<typename T>
        bool has_root() {
            return detail::find_root<T>(*this) != nullptr;
        }

        template<typename T>
        XHandle<T> make_handle() {
            make<T>();
            return XHandle<T>(*this);
        }

        template<typename T>
        XHandle<T> handle() {
            return XHandle<T>(*this);
        }

        template<typename T>
        boost::interprocess::allocator<T, XBufferCore::segment_manager> allocator() {
            validate_xbuffer_type<T>();
            return boost::interprocess::allocator<T, XBufferCore::segment_manager>(this->get_segment_manager());
        }

        std::size_t used_size() {
            return stats().used_size;
        }

        // Serialize to string (shrinks first, byte-exact).
        std::string save() {
            this->shrink_to_fit();
            const char* base = static_cast<const char*>(this->get_address());
            std::size_t exact_size = this->segment_size();
            return std::string(base, exact_size);
        }

        // Serialize without shrinking (faster).
        std::string save_raw() {
            const char* base = static_cast<const char*>(this->get_address());
            std::size_t exact_size = this->segment_size();
            return std::string(base, exact_size);
        }

        std::vector<char> save_bytes() {
            this->shrink_to_fit();
            const char* base = static_cast<const char*>(this->get_address());
            std::size_t exact_size = this->segment_size();
            return std::vector<char>(base, base + exact_size);
        }

        static XBuffer load(const std::string& data) {
            XBuffer xbuf(data.data(), data.size());
            return xbuf;
        }

        static XBuffer load(const std::vector<char>& data) {
            XBuffer xbuf(data.data(), data.size());
            return xbuf;
        }

        MemoryStats stats() {
            return memory_stats(*this);
        }

        // Estimates a suitable buffer size for the given user data payload.
        // Accounts for segment_manager overhead + 20% headroom for container growth.
        static std::size_t estimate_buffer_size(std::size_t user_data_bytes) {
            std::size_t min_overhead = XBufferCore::segment_manager::get_min_size();
            std::size_t estimated = min_overhead + user_data_bytes;
            estimated += estimated / 5;  // +20% headroom
            return std::max(estimated, (std::size_t)512);
        }
    };

    // ========================================================================
    // TypedXBuffer<T> — Type-bound buffer with compile-time root type
    //
    // Wraps XBuffer and binds it to a specific root type T at compile time.
    // Eliminates the need to repeat <T> on every make/root/handle call.
    //
    // Usage:
    //   TypedXBuffer<GameData> buf(4096);
    //   auto* game = buf.make();         // no <GameData> needed
    //   auto& g = buf.root();            // no <GameData> needed
    //   auto bytes = buf.save();          // standard save
    //   auto loaded = TypedXBuffer<GameData>::load(bytes);
    // ========================================================================
    template<typename T>
    class TypedXBuffer : public XBuffer {
    public:
        using XBuffer::XBuffer;

        /// Creates the single root object of type T.
        T* make() { return XBuffer::make<T>(); }

        /// Returns a reference to the root object.
        T& root() { return XBuffer::root<T>(); }

        /// Returns true if a root object exists.
        bool has_root() { return XBuffer::has_root<T>(); }

        /// Creates root and returns an epoch-cached handle.
        XHandle<T> make_handle() { return XBuffer::make_handle<T>(); }

        /// Returns an epoch-cached handle to the existing root.
        XHandle<T> handle() { return XBuffer::handle<T>(); }

        /// Load from serialized data and return a typed buffer.
        static TypedXBuffer load(const std::string& data) {
            return TypedXBuffer(XBuffer::load(data));
        }

        static TypedXBuffer load(const std::vector<char>& data) {
            return TypedXBuffer(XBuffer::load(data));
        }

        /// Construct from an existing XBuffer (e.g., after compaction).
        TypedXBuffer(XBuffer&& other) noexcept
            : XBuffer(std::move(other)) {}
    };

    // ================================================================
    // XCompactor — Memory compaction via C++26 reflection.
    // ================================================================
    class XCompactor {
    public:
        enum class MigrateStrategy {
            Bitwise,          // direct assignment (primitives, enums, POD)
            AllocatorAware,   // reconstruct with new allocator (XString-like)
            Container,        // iterate elements, recurse (XVector/XSet/XMap)
            Composite,        // reflect members, recurse (user structs)
            NotRegistered     // use built-in auto-detection
        };

        template<typename T>
        struct migrate_as { static constexpr MigrateStrategy value = MigrateStrategy::NotRegistered; };

        // Compact root object into a new tightly-packed buffer.
        // Progressive allocation: tries 2x, 2.5x, 3x, 4x multipliers.
        template<typename T>
        static XBuffer compact(XBufferCore& old_xbuf) {
            validate_xbuffer_type<T>();
            auto stats = memory_stats(old_xbuf);
            auto* old_obj = detail::find_root<T>(old_xbuf);

            // Progressive multipliers: try smaller first, fall back to larger
            constexpr double multipliers[] = {2.0, 2.5, 3.0, 4.0};
            for (double mult : multipliers) {
                std::size_t new_size = static_cast<std::size_t>(stats.used_size * mult);
                if (new_size < 4096) new_size = 4096;

                try {
                    XBuffer new_xbuf(new_size);
                    if (!old_obj) {
                        return new_xbuf;
                    }
                    auto* new_obj = detail::construct_root<T>(new_xbuf);
                    migrate_members(*old_obj, *new_obj, old_xbuf, new_xbuf);
                    new_xbuf.shrink_to_fit();
                    return new_xbuf;
                } catch (const boost::interprocess::bad_alloc&) {
                    // Not enough headroom — try next multiplier
                    continue;
                }
            }
            // All multipliers exhausted — should not happen in practice
            throw boost::interprocess::interprocess_exception(
                "XCompactor::compact failed: insufficient memory even at 4x multiplier");
        }

    private:

        // Resolve strategy: user-registered > trivial > allocator gate > composite.
        template<typename T>
        static consteval MigrateStrategy resolve_strategy() {
            using CleanT = std::remove_cv_t<T>;
            static_assert(boost::typelayout::is_byte_copy_safe_v<CleanT>,
                "XCompactor: type is not byte-copy safe and cannot be migrated.");
            if constexpr (migrate_as<CleanT>::value != MigrateStrategy::NotRegistered) {
                return migrate_as<CleanT>::value;
            } else if constexpr (std::is_trivially_copyable_v<CleanT>) {
                return MigrateStrategy::Bitwise;
            } else if constexpr (requires { typename CleanT::allocator_type; }) {
                // Unregistered container trap: if type has allocator_type + iterator
                // but was not registered via XOFFSET_REGISTER_*, this static_assert
                // fires at compile time. (The assert condition is always false here
                // because registered types already returned in the first branch above.)
                if constexpr (requires(const CleanT& c) {
                    typename CleanT::iterator;
                    { c.begin() };
                    { c.end()   };
                }) {
                    static_assert(
                        migrate_as<CleanT>::value != MigrateStrategy::NotRegistered,
                        "XCompactor: container type has allocator_type but no migrate_as "
                        "registration. Register it via "
                        "XOFFSET_REGISTER_TYPE(YourType, AllocatorAware) or "
                        "XOFFSET_REGISTER_TYPE(YourType, Container). See docs.");
                    return MigrateStrategy::Composite;  // unreachable; satisfies return requirement
                } else {
                    return MigrateStrategy::Composite;
                }
            } else {
                return MigrateStrategy::Composite;
            }
        }

        template<typename ElementType>
        static auto migrate_element(const ElementType& old_elem, XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            constexpr auto strategy = resolve_strategy<ElementType>();
            if constexpr (strategy == MigrateStrategy::Bitwise) {
                return old_elem;
            } else if constexpr (strategy == MigrateStrategy::AllocatorAware) {
                return ElementType(old_elem, new_xbuf.get_segment_manager());
            } else if constexpr (detail::has_segment_manager_ctor<ElementType>) {
                ElementType new_elem(new_xbuf.get_segment_manager());
                migrate_members(old_elem, new_elem, old_xbuf, new_xbuf);
                return std::move(new_elem);
            } else {
                alignas(ElementType) unsigned char buf[sizeof(ElementType)];
                detail::reflect_init_all<ElementType>(buf, new_xbuf.get_segment_manager());
                ElementType& new_elem = *std::launder(reinterpret_cast<ElementType*>(buf));
                migrate_members(old_elem, new_elem, old_xbuf, new_xbuf);
                return std::move(new_elem);
            }
        }
        
        template<typename ContainerType>
        static void migrate_container(const ContainerType& old_container, 
                                      ContainerType& new_container,
                                      XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using ElementType = typename ContainerType::value_type;

            if constexpr (std::is_trivially_copyable_v<ElementType>) {
                new_container = old_container;
                return;
            }
            
            if constexpr (detail::MapLikeContainer<ContainerType>) {
                for (const auto& [key, value] : old_container) {
                    auto new_key = migrate_element(key, old_xbuf, new_xbuf);
                    auto new_value = migrate_element(value, old_xbuf, new_xbuf);
                    new_container.emplace(std::move(new_key), std::move(new_value));
                }
            } else {
                for (const auto& elem : old_container) {
                    auto migrated_elem = migrate_element(elem, old_xbuf, new_xbuf);
                    if constexpr (detail::SetLikeContainer<ContainerType>) {
                        new_container.emplace(std::move(migrated_elem));
                    } else {
                        new_container.emplace_back(std::move(migrated_elem));
                    }
                }
            }
        }
        
        template<typename MemberType>
        static void migrate_member(const MemberType& old_member, MemberType& new_member, 
                                  XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            constexpr auto strategy = resolve_strategy<MemberType>();
            if constexpr (strategy == MigrateStrategy::Bitwise) {
                new_member = old_member;
            } else if constexpr (strategy == MigrateStrategy::AllocatorAware) {
                new_member = MemberType(old_member, new_xbuf.get_segment_manager());
            } else if constexpr (strategy == MigrateStrategy::Container) {
                migrate_container(old_member, new_member, old_xbuf, new_xbuf);
            } else {
                migrate_members(old_member, new_member, old_xbuf, new_xbuf);
            }
        }
        
        template<typename T, std::size_t Index>
        static consteval auto get_member_at() {
            using namespace std::meta;
            auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
            return members[Index];
        }
        
        template<typename T, std::size_t Index>
        static void migrate_member_at(const T& old_obj, T& new_obj,
                                      XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using namespace std::meta;
            constexpr auto member = get_member_at<T, Index>();
            using MemberType = [:type_of(member):];
            const auto& old_member = old_obj.[:member:];
            auto& new_member = new_obj.[:member:];
            migrate_member<MemberType>(old_member, new_member, old_xbuf, new_xbuf);
        }
        
        template<typename T, std::size_t... Is>
        static void migrate_members_impl(const T& old_obj, T& new_obj,
                                         XBufferCore& old_xbuf, XBufferCore& new_xbuf,
                                         std::index_sequence<Is...>) {
            (migrate_member_at<T, Is>(old_obj, new_obj, old_xbuf, new_xbuf), ...);
        }

        template<typename T, std::size_t N>
        static void migrate_base_at(const T& old_obj, T& new_obj,
                                    XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using namespace std::meta;
            constexpr auto base_info = bases_of(^^T, access_context::unchecked())[N];
            using BaseType = [:type_of(base_info):];
            const BaseType& old_base = static_cast<const BaseType&>(old_obj);
            BaseType& new_base = static_cast<BaseType&>(new_obj);
            migrate_members(old_base, new_base, old_xbuf, new_xbuf);
        }

        template<typename T, std::size_t... Is>
        static void migrate_bases_impl(const T& old_obj, T& new_obj,
                                       XBufferCore& old_xbuf, XBufferCore& new_xbuf,
                                       std::index_sequence<Is...>) {
            (migrate_base_at<T, Is>(old_obj, new_obj, old_xbuf, new_xbuf), ...);
        }
        
        template<typename T>
        static void migrate_members(const T& old_obj, T& new_obj, 
                                   XBufferCore& old_xbuf, XBufferCore& new_xbuf) {
            using namespace std::meta;
            constexpr auto bc = bases_of(^^T, access_context::unchecked()).size();
            constexpr auto mc = nonstatic_data_members_of(^^T, access_context::unchecked()).size();
            if constexpr (bc > 0) migrate_bases_impl(old_obj, new_obj, old_xbuf, new_xbuf, std::make_index_sequence<bc>{});
            if constexpr (mc > 0) migrate_members_impl(old_obj, new_obj, old_xbuf, new_xbuf, std::make_index_sequence<mc>{});
        }
    };
}

// ============================================================================
// Registration macros: TypeLayout opaque signature + XCompactor migration strategy.
// Must be placed at global namespace scope.
// ============================================================================

// Namespace sentinel — detects accidental use inside a namespace block.
struct _XOffset_NS_Sentinel {};

#define XOFFSET_CHECK_GLOBAL_NAMESPACE_(macro_name)                            \
    static_assert(                                                             \
        ::std::is_same_v<::_XOffset_NS_Sentinel, _XOffset_NS_Sentinel>,       \
        macro_name " must be used at global namespace scope, "                 \
        "not inside any namespace { } block. "                                 \
        "Move the macro call outside all namespace declarations.");

#define XOFFSET_REGISTER_TYPE(Type, name, strategy)                            \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_REGISTER_TYPE")                   \
    namespace boost { namespace typelayout {                                    \
        TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(XOffsetDatastructure::Type, name)    \
    }}                                                                         \
    template<> struct XOffsetDatastructure::XCompactor::migrate_as<            \
        XOffsetDatastructure::Type> {                                          \
        static constexpr XOffsetDatastructure::XCompactor::MigrateStrategy     \
            value = XOffsetDatastructure::XCompactor::MigrateStrategy::strategy; \
    };

#define XOFFSET_REGISTER_CONTAINER(Template, name, strategy)                   \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_REGISTER_CONTAINER")              \
    namespace boost { namespace typelayout {                                    \
        TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(XOffsetDatastructure::Template, name) \
    }}                                                                         \
    template<typename T_> struct XOffsetDatastructure::XCompactor::migrate_as< \
        XOffsetDatastructure::Template<T_>> {                                  \
        static constexpr XOffsetDatastructure::XCompactor::MigrateStrategy     \
            value = XOffsetDatastructure::XCompactor::MigrateStrategy::strategy; \
    };

#define XOFFSET_REGISTER_MAP(Template, name, strategy)                         \
    XOFFSET_CHECK_GLOBAL_NAMESPACE_("XOFFSET_REGISTER_MAP")                    \
    namespace boost { namespace typelayout {                                    \
        TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(XOffsetDatastructure::Template, name) \
    }}                                                                         \
    template<typename K_, typename V_>                                          \
    struct XOffsetDatastructure::XCompactor::migrate_as<                        \
        XOffsetDatastructure::Template<K_, V_>> {                              \
        static constexpr XOffsetDatastructure::XCompactor::MigrateStrategy     \
            value = XOffsetDatastructure::XCompactor::MigrateStrategy::strategy; \
    };

// Built-in registrations
XOFFSET_REGISTER_TYPE(XString, "string", AllocatorAware)
XOFFSET_REGISTER_CONTAINER(XVector, "vector", Container)
XOFFSET_REGISTER_CONTAINER(XSet, "set", Container)
XOFFSET_REGISTER_MAP(XMap, "map", Container)

#endif
