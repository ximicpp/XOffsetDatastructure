#ifndef X_OFFSET_BUFFER_HPP
#define X_OFFSET_BUFFER_HPP

// XBuffer (one root object per buffer: make, save, load and reload with
// validation), TypedXBuffer<T>, XHandle<T> and XByteView. Part of
// xoffsetdatastructure.hpp; include that header. API: docs/USER_GUIDE.md.

#include "xoffset_schema_hash.hpp"

namespace XOffsetDatastructure {
    // XHandle<T> -- Epoch-cached handle. O(1) deref when address is stable.
    // Each dereference compares the cached epoch with the buffer's (two loads
    // and a compare, inlined); the re-resolution after a change is out of
    // line. In a loop over many events, resolve once per packet or batch
    // (`T& r = *handle;`) and use the reference: nothing in the loop body may
    // then move or reload the buffer.
    // ========================================================================
    template <typename T>
    class XHandle {
    public:
        XHandle() noexcept : buffer_(nullptr) {}

        using resolver_type = T* (*)(XBufferCore&);

        explicit XHandle(XBufferCore& buf, resolver_type resolver = nullptr) noexcept
            : buffer_(&buf), cached_ptr_(nullptr), cached_epoch_(0), resolver_(resolver)
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
        // A cached pointer was resolved through buffer_, which is then set:
        // buffer_ is null only together with cached_ptr_.
        XOFFSET_DETAIL_INLINE T* resolve() const {
            if (cached_ptr_ && cached_epoch_ == buffer_->epoch()) [[likely]] return cached_ptr_;
            return refresh();
        }
        // First use, or the buffer changed (epoch) since the last resolution.
        XOFFSET_DETAIL_COLD T* refresh() const {
            if (!buffer_) return nullptr;
            const uint64_t current_epoch = buffer_->epoch();
            cached_ptr_ = resolver_ ? resolver_(*buffer_)
                : (buffer_->root_type_known() && !buffer_->template root_type_matches<T>()
                    ? nullptr : detail::find_root<T>(*buffer_));
            cached_epoch_ = current_epoch;
            return cached_ptr_;
        }

        XBufferCore* buffer_;
        mutable T* cached_ptr_ = nullptr;
        mutable uint64_t cached_epoch_ = 0;
        resolver_type resolver_ = nullptr;
    };

    // XBuffer -- Single root object per buffer (make/root/has_root).
    // ========================================================================
    namespace detail {
        // Internal friend bridge; no Core/container/wire field is added.
        struct VerifiedPathsAccess {
            using Identity = XBufferCore::OwnerRecordPtr;
            static Identity identity(XBufferCore& source) noexcept { return source.owner_record_; }
            static XBufferCore& owner(const Identity& value) { return XBufferCore::require_record_owner(value); }
            // The source is its record's owner (what owner(identity(source))
            // resolves to on entry), without copying the record.
            static bool owns_record(const XBufferCore& source) noexcept {
                return XBufferCore::record_owner(source.owner_record_) == &source;
            }
            template<class T> static XWireHeaderV2 header(XBufferCore&);
            // header<T>'s body, inlined into save_into's fast path.
            template<class T> XOFFSET_DETAIL_INLINE static XWireHeaderV2 header_inline(XBufferCore&);
            template<class T> static void save_into(XBufferCore&, std::string&, XSaveOptions);
            // In-band: save_small_into's hole table gathered on its frame.
            static constexpr std::size_t small_wire_holes = 16;
            template<class T> static bool save_small_into(XBufferCore&, std::string&, XWireEmission);
            template<class T> static void save_into_general(XBufferCore&, std::string&, XSaveOptions);
            template<class T> static void rebuild_free_table(T&, XBufferCore&);
            template<class T> static void save_compact_into(XBufferCore&, std::string&, XCompactOptions);
            template<class T, class... M> static void save_composed_into(XBufferCore&, std::string&, XCompactOptions,
                                                                         const XAssign<M>&...);
        };
    }

    template <typename T> class VerifiedReloadToken;
} // namespace XOffsetDatastructure

#include "xoffset_proxy.hpp"

namespace XOffsetDatastructure {
    // The bytes of a wire or delta as the load, reload and apply entries take
    // them: a std::string, a std::string_view or anything that converts to
    // std::span<const std::byte>. A view: the bytes must outlive the call.
    class XByteView {
    public:
        template <typename S>
            requires std::is_convertible_v<const S&, std::string_view>
        XByteView(const S& text) noexcept : bytes_(bytes_of(text)) {}

        template <typename R>
            requires std::is_convertible_v<const R&, std::span<const std::byte>>
        XByteView(const R& bytes) noexcept : bytes_(bytes) {}

        operator std::span<const std::byte>() const noexcept { return bytes_; }

    private:
        static std::span<const std::byte> bytes_of(std::string_view text) noexcept {
            return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
        }

        std::span<const std::byte> bytes_;
    };

    class XBuffer : public XBufferCore {
    public:
        XBuffer() = default;

        explicit XBuffer(std::size_t size)
            : XBufferCore(size) {}

        XBuffer(const char* data, std::size_t size)
            : XBufferCore(data, size)
        {
            typed_root_access_allowed_ = false;
        }

        XBuffer(const char* data, std::size_t size, std::size_t max_reserved)
            : XBufferCore(data, size, max_reserved)
        {
            typed_root_access_allowed_ = false;
        }

        XBuffer(const char* data, std::size_t size, MaximumCapacity cap)
            : XBufferCore(data, size, cap) {
            typed_root_access_allowed_ = false;
        }

        XBuffer(WirePayload wire, std::size_t size, MaximumCapacity cap)
            : XBufferCore(wire, size, cap) {
            typed_root_access_allowed_ = false;
        }

        /// Legacy minimum reservation hint. Use maximum_capacity() for a hard limit.
        struct MaxCapacity { std::size_t value; };
        static MaxCapacity max_capacity(std::size_t bytes) { return {bytes}; }

        XBuffer(std::size_t size, MaxCapacity cap)
            : XBufferCore(size, cap.value) {}

        XBuffer(std::size_t size, MaximumCapacity cap)
            : XBufferCore(size, cap) {}

        void swap(XBuffer& other) noexcept {
            if (this == &other) return;
            XBufferCore::swap(other);
            reload_scratch_.swap(other.reload_scratch_);
            touch_log_.swap(other.touch_log_);
            std::swap(direct_reload_open_, other.direct_reload_open_);
        }

        friend void swap(XBuffer& a, XBuffer& b) noexcept { a.swap(b); }

        template <typename T>
        static XBuffer create(std::size_t reserve_bytes = 4096) {
            XBuffer xbuf(reserve_bytes);
            (void)xbuf.template make<T>();
            return xbuf;
        }

        template <typename T>
        static XBuffer create(std::size_t initial_bytes, MaximumCapacity cap) {
            XBuffer xbuf(initial_bytes, cap);
            (void)xbuf.template make<T>();
            return xbuf;
        }

        /// Create the root object. Growth/shrink/save retain its address;
        /// replacing this buffer's storage or destroying it invalidates the pointer.
        template<typename T>
        T* make() { return make_inline<T>(); }

    private:
        // make<T>()'s body; reset<T>() inlines it after its reset_arena.
        template<typename T>
        XOFFSET_DETAIL_INLINE T* make_inline() {
            static_assert(is_byte_copy_safe_v<T>,
                "Type is not byte-copy safe. "
                "Allowed: primitives, XString, XVector<T>, XMap<K,V>, XSet<T>, "
                "or structs composed of these.");
            if (!arena()) XOFFSET_THROW(XException(XErrc::invalid_state, "make<T>(): buffer has no arena"));
            if (arena()->root_offset != 0) {
                XOFFSET_THROW(XException(XErrc::invalid_state, "make<T>(): root object already exists. "
                    "Call root<T>() to access the existing object."));
            }
            auto* result = detail::construct_root<T>(*this);
            typed_root_access_allowed_ = true;
            this->template bind_root_type<T>();
            bind_schema_if_available<T>();
            this->cache_root(result);
            return result;
        }

    private:
        // An open pin scope (nests); closes on the block, which follows the
        // content if this XBuffer is moved meanwhile.
        struct PinGuard {
            detail::XCtl* ctl = nullptr;
            explicit PinGuard(XBuffer& buffer) noexcept {
                if (buffer.pin_enter()) ctl = buffer.control();
            }
            explicit PinGuard(detail::XCtl* entered) noexcept : ctl(entered) {}
            ~PinGuard() { XBufferCore::pin_exit(ctl); }
            PinGuard(const PinGuard&) = delete;
            PinGuard& operator=(const PinGuard&) = delete;
        };
        // The root for a pin scope, after the entry move (see edit).
        template<typename T>
        T* pin_root(size_type expected_growth) {
            if (!pinned() && !address_stable()) {
                const size_type used = arena() ? static_cast<size_type>(arena()->used_end) : 0;
                const size_type limit = reservation_limit();
                size_type want = expected_growth;
                if (used > limit / 2) want = std::max(want, limit - used + 1u);
                if (want != 0) (void)make_room(want);
            }
            T* root_ptr = nullptr;
            XOFFSET_TRY {
                root_ptr = &root<T>();
            } XOFFSET_CATCH(...) {
                record_error(XErrc::no_root);
            }
            return root_ptr;
        }

    public:
        template<typename T>
        T& root() {
            if (this->template root_cached_as<T>()) [[likely]] return *this->template cached_root_hit<T>();
            return root_checked<T>();
        }

    private:
        // root<T>()'s checked path, out of line so the cache hit above stays
        // a compare and a load where it is inlined.
        template<typename T>
        XOFFSET_DETAIL_NOINLINE T& root_checked() {
            require_typed_root_access(
                "root<T>(): typed root access requires local construction or load_verified<T>(); "
                "use unsafe_root<T>() only for trusted raw payloads");
            require_schema_match<T>("root<T>(): schema mismatch for requested root type");
            T* ptr = detail::find_root<T>(*this);
            if (!ptr) {
                XOFFSET_THROW(XException(XErrc::no_root, "root<T>(): no root object in buffer. "
                    "Call make<T>() first or check with has_root<T>()."));
            }
            // Passed the checks make<T>() and a verified load pass, so the
            // next call takes the fast path (a buffer moved into a container
            // pays this path once, not on every call).
            this->cache_root(ptr);
            return *ptr;
        }

    public:
        // ---- proxies and pin scopes (xoffset_proxy.hpp)

        // A proxy of the root: valid across moves of the payload (and of this
        // XBuffer object); fail-soft. An empty proxy when the buffer has no
        // root of type T.
        template<typename T>
        XRef<T> ref() noexcept {
            T* root_ptr = nullptr;
            XOFFSET_TRY {
                root_ptr = &root<T>();
            } XOFFSET_CATCH(...) {
                root_ptr = nullptr;
            }
            auto* ctl = root_ptr ? control() : nullptr;
            if (!ctl) return XRef<T>();
            return XRef<T>(proxy_detail::Anchor(ctl),
                           proxy_detail::RootPath{static_cast<std::uint32_t>(
                               reinterpret_cast<char*>(root_ptr) - data())});
        }

        // Pin scope: f(T& root) with plain references at plain speed. The
        // payload does not move while it runs (an operation that would need
        // to fails with `borrowed`), and blocks the containers retire are
        // reused only after it. On entry a Relocating buffer whose
        // reservation is more than half used moves first, or makes room for
        // `expected_growth` bytes. False when an operation inside failed
        // (the first failure is error()); the failing operation has no
        // effect, and with exceptions the rest of f is skipped.
        // f may not return a pointer, reference, string_view or span.
        template<typename T, typename F>
        bool edit(F&& f, size_type expected_growth = 0) {
            using R = std::invoke_result_t<F&, T&>;
            static_assert(std::is_void_v<R>,
                "edit(f): f must return void (a value made inside may point into the payload; "
                "use read(f) to compute a value)");
            T* root_ptr = nullptr;
            // Fast entry (no growth asked, the root cached, see pin_quiet).
            if (expected_growth == 0 && (root_ptr = this->template cached_root<T>()) && pin_quiet()) [[likely]] {
            } else {
                if (!ok()) return false;
                root_ptr = pin_root<T>(expected_growth);
                if (!root_ptr) return false;
                if (!pin_enter()) return false;
            }
            PinGuard guard(control());
            record_error(detail::run_fail_soft([&] { f(*root_ptr); }));
            return ok();
        }

        // A message in one step, for senders that fill a new one each time:
        // reset<T>(), then f(T& root) in a pin scope entered with the fresh
        // root (no root lookup and no entry move: the emptied payload is at
        // its smallest). Otherwise as edit<T>(f, expected_growth): the same
        // fail-soft result, and f must return void. A failed buffer starts
        // over (reset replaces its content and clears the failure).
        //
        // A Relocating buffer whose reservation turns out too small (the
        // payload cannot move inside the scope) is built again: reset, a
        // reservation of at least twice the size reached, and f once more,
        // until it fits or the buffer is at its maximum. So f may run more
        // than once: it must only write into the buffer (no side effects
        // outside it that a second run would repeat). The larger reservation
        // stays with the buffer, so a reused buffer pays this once.
        template<typename T, typename F>
        XOFFSET_DETAIL_INLINE bool build(F&& f, size_type expected_growth = 0) {
            if (build_once<T>(f, expected_growth)) [[likely]] return true;
            return build_again<T>(f);
        }

    private:
        // A failure a larger reservation can cure: the scope could not move
        // the payload, and the buffer can still grow.
        bool build_outgrew() const noexcept {
            const XErrc e = error();
            if (e != XErrc::out_of_storage && e != XErrc::borrowed) return false;
            return !this->pinned() && !this->address_stable() && reservation_limit() < maximum_capacity();
        }
        template<typename T, typename F>
        XOFFSET_DETAIL_NOINLINE bool build_again(F& f) {
            for (int attempt = 0; attempt < 16 && build_outgrew(); ++attempt) {
                const size_type limit = reservation_limit();
                const size_type reached = arena() ? static_cast<size_type>(arena()->used_end) : 0;
                (void)reset<T>();
                const size_type target = std::min<size_type>(std::max(limit, reached) * 2u, maximum_capacity());
                const size_type used = arena() ? static_cast<size_type>(arena()->used_end) : 0;
                if (target <= used || make_room(target - used) != XErrc::ok || reservation_limit() <= limit) {
                    record_error(XErrc::out_of_storage);
                    return false;
                }
                if (build_once<T>(f, 0)) return true;
            }
            return false;
        }
        template<typename T, typename F>
        XOFFSET_DETAIL_INLINE bool build_once(F& f, size_type expected_growth) {
            using R = std::invoke_result_t<F&, T&>;
            static_assert(std::is_void_v<R>,
                "build(f): f must return void (a value made inside may point into the payload; "
                "use read(f) to compute a value)");
            T& fresh = reset<T>();
            if (expected_growth != 0 || this->pinned()) [[unlikely]] return edit<T>(f, expected_growth);
            // Lean scope: bare while this lineage has no control block.
            struct Close {
                XBufferCore* core;
                detail::XCtl* ctl;
                ~Close() {
                    if (!ctl) core->pin_close_bare();
                    else XBufferCore::pin_exit(ctl);
                }
            } close{this, nullptr};
            this->pin_open_lean(&close.ctl);
            record_error(detail::run_fail_soft([&] { f(fresh); }));
            // A failure made a block (record_error); the block follows the content.
            return !close.ctl || close.ctl->sticky == XErrc::ok;
        }

    public:

        // Read-only pin scope: returns f(const T& root). The result may not
        // be (or hold) a pointer, reference, string_view or span.
        template<typename T, typename F>
        decltype(auto) read(F&& f) {
            using R = std::invoke_result_t<F&, const T&>;
            static_assert(!proxy_detail::escapes<std::remove_cv_t<R>>::value && !std::is_reference_v<R>,
                "read(f): f may not return a pointer, reference, string_view or span "
                "(it would point into the payload after the scope)");
            const T& root_ref = root<T>();
            PinGuard guard(*this);
            if constexpr (std::is_void_v<R>) {
                f(root_ref);
            } else {
                return R(f(root_ref));
            }
        }

        template<typename T>
        bool has_root() {
            if (schema_bound_ && !matches_schema<T>()) return false;
            return detail::find_root<T>(*this) != nullptr;
        }

        template<typename T>
        T& unsafe_root() {
            T* ptr = detail::find_root<T>(*this);
            if (!ptr) {
                XOFFSET_THROW(XException(XErrc::no_root, "unsafe_root<T>(): no root object in buffer"));
            }
            return *ptr;
        }

        template<typename T>
        bool unsafe_has_root() {
            return detail::find_root<T>(*this) != nullptr;
        }

        template<typename T>
        XHandle<T> make_handle() {
            make<T>();
            return handle<T>();
        }

        template<typename T>
        XHandle<T> handle() {
            require_typed_root_access(
                "handle<T>(): typed handle access requires local construction or load_verified<T>(); "
                "use unsafe_handle<T>() only for trusted raw payloads");
            require_schema_match<T>("handle<T>(): schema mismatch for requested root type");
            return XHandle<T>(*this, [](XBufferCore& core) -> T* {
                auto& buffer = static_cast<XBuffer&>(core);
                if (!buffer.typed_root_access_allowed_ || !buffer.template root_type_matches<T>())
                    return nullptr;
                if (buffer.schema_bound_ && !buffer.template matches_schema<T>()) return nullptr;
                return detail::find_root<T>(buffer);
            });
        }

        template<typename T>
        XHandle<T> unsafe_handle() {
            return XHandle<T>(*this);
        }

        template<typename T>
        XArenaAllocator<T> allocator() {
            static_assert(is_byte_copy_safe_v<T>,
                "Type is not byte-copy safe.");
            return XArenaAllocator<T>(this->arena());
        }

        template <typename T>
        bool matches_schema() const {
            if (!schema_bound_) return false;
            if constexpr (!has_schema_name_v<T>) {
                return false;
            } else {
                return bound_root_type_id_ == detail::wire_root_type_id<T>() &&
                    bound_schema_hash_ == detail::wire_schema_hash<T>();
            }
        }

        std::span<const std::byte> bytes() const noexcept {
            auto* base = reinterpret_cast<const std::byte*>(this->get_address());
            return std::span<const std::byte>(base, this->get_size());
        }

        // Serialize with normalized transport semantics: preserve the live object graph,
        // but compact away spare tail capacity before emitting bytes.
        std::string save() {
            this->shrink_to_fit();
            const char* base = static_cast<const char*>(this->get_address());
            std::size_t exact_size = this->get_size();
            return exact_size == 0 ? std::string{} : std::string(base, exact_size);
        }

        template <typename T>
        void save_verified_into(std::string& out, XSaveOptions options = {}) {
            if (refuse_failed_emit(out)) return;
            detail::VerifiedPathsAccess::save_into<T>(*this, out, options);
        }

        // Compacted wire written by direct traversal: preorder blocks, strings
        // of at most 10 bytes inline, no holes, and capacity == size except
        // for the bounded slack of XCompactOptions (slack_div = 0: the
        // canonical wire of XCompactor, the smallest one). Same checks as
        // save_verified; the source is not changed. A valid V2 wire: every
        // receiver accepts it.
        template <typename T>
        void save_compact_into(std::string& out, XCompactOptions options = {}) {
            if (refuse_failed_emit(out)) return;
            require_typed_root_access("save_compact<T>(): unverified payload cannot be relabeled as verified");
            require_schema_match<T>("save_compact<T>(): schema mismatch for requested root type");
            if (!detail::find_root<T>(*this))
                XOFFSET_THROW(XException(XErrc::no_root, "save_compact<T>(): no root object of requested type"));
            detail::VerifiedPathsAccess::save_compact_into<T>(*this, out, options);
        }

        // Composes a wire from parts: writes the wire save_compact_into would
        // write after `*a.target = *a.source` for every assignment (all
        // sources read as they were), without changing this buffer or any
        // source. Targets are distinct, non-overlapping member subobjects of
        // this buffer's root (not inside a container); their implicit
        // padding is written as zero. Throws XException on a bad target or
        // past the wire limit; out is then empty.
        template <typename T, typename A0, typename... A>
        void save_compact_into(std::string& out, XCompactOptions options, const XAssign<A0>& first,
                               const XAssign<A>&... rest) {
            if (refuse_failed_emit(out)) return;
            XOFFSET_TRY {
                require_typed_root_access("save_compact<T>(): unverified payload cannot be relabeled as verified");
                require_schema_match<T>("save_compact<T>(): schema mismatch for requested root type");
                if (!detail::find_root<T>(*this))
                    XOFFSET_THROW(XException(XErrc::no_root, "save_compact<T>(): no root object of requested type"));
            } XOFFSET_CATCH(...) {
                out.clear();
                XOFFSET_RETHROW;
            }
            detail::VerifiedPathsAccess::save_composed_into<T>(*this, out, options, first, rest...);
        }

        template <typename T>
        std::string save_compact(XCompactOptions options = {}) {
            std::string out;
            save_compact_into<T>(out, options);
            return out;
        }

        // V2: if a host allocation failure (or an unverified load) left the
        // free table incomplete, recompute it exactly from the live graph.
        template <typename T>
        void ensure_hole_table(const T& root) {
            if (this->free_table_complete()) return;
            std::vector<WireHole> holes;
            XOFFSET_VREQUIRE(detail::verify_structural_wire_graph(root, *this, std::pmr::new_delete_resource(),
                                                                  nullptr, 0, false, &holes));
            this->replace_free_blocks(holes);
        }

        template <typename T>
        XWireHeaderV2 wire_header_v2(const T& root) const {
            // The used extent (what the logical length was after the
            // shrink_to_fit that saves no longer do): used_end, at least the
            // header, at most the logical length.
            const auto used = std::min<std::uint64_t>(
                std::max<std::uint64_t>(this->arena()->used_end, sizeof(arena_header)), this->get_size());
            return detail::make_wire_header<T>(used,
                reinterpret_cast<std::uintptr_t>(&root) - reinterpret_cast<std::uintptr_t>(this->get_address()),
                this->wire_hole_count());
        }

        template <typename T>
        std::string save_verified(XSaveOptions options = {}) {
            static_assert(is_wire_admitted_v<T>,
                "save_verified<T>() requires an admitted fixed-schema wire type.");
#ifdef XOFFSET_REJECT_IMPLICIT_PADDING
            static_assert(!has_implicit_padding_v<T>,
                "XOFFSET_REJECT_IMPLICIT_PADDING: the wire type has implicit padding (threat model T5/P3)");
#endif
            {
                std::string refused;
                if (refuse_failed_emit(refused)) return refused;
            }
            require_typed_root_access("save_verified<T>(): unverified payload cannot be relabeled as verified");
            require_schema_match<T>("save_verified<T>(): schema mismatch for requested root type");
            auto* root_ptr = detail::find_root<T>(*this);
            if (!root_ptr) {
                XOFFSET_THROW(XException(XErrc::no_root, "save_verified<T>(): no root object of requested type"));
            }
            ensure_hole_table(*root_ptr);
            const char* base = static_cast<const char*>(this->get_address());
            const auto header = wire_header_v2(*root_ptr);
            detail::WireGaps gaps;
            if (options.emission == XWireEmission::free_runs) gaps = detail::wire_free_runs(*this, header.used_bytes);
            const auto output_size = options.emission == XWireEmission::zero_runs
                ? detail::wire_dense_size(header, "save_verified<T>()")
                : detail::wire_free_run_size(header, gaps, "save_verified<T>()");
            std::string out;
            if (output_size > out.max_size())
                XOFFSET_THROW(std::length_error("save_verified<T>(): output size exceeds string limit"));
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
            out.resize_and_overwrite(output_size, [&](char* output, std::size_t) noexcept {
                return detail::write_wire(output, header, base, *this, options.emission, gaps);
            });
#else
            out.resize(output_size);
            out.resize(detail::write_wire(out.data(), header, base, *this, options.emission, gaps));
#endif
            return out;
        }

        static XBuffer load_unverified(XByteView bytes,
                                       std::optional<MaximumCapacity> cap = std::nullopt) {
            const std::span<const std::byte> data = bytes;
            XBuffer xbuf = cap
                ? XBuffer(reinterpret_cast<const char*>(data.data()), data.size_bytes(), *cap)
                : XBuffer(reinterpret_cast<const char*>(data.data()), data.size_bytes());
            xbuf.typed_root_access_allowed_ = false;
            xbuf.schema_bound_ = false;
            xbuf.bound_root_type_id_ = 0;
            xbuf.bound_schema_hash_ = 0;
            return xbuf;
        }

        static XBuffer load(XByteView data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return load_unverified(data, cap);
        }

        template <typename T>
        static XBuffer load(XByteView data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return load_verified<T>(data, cap);
        }

        // `upstream` gives the validation bitmaps of a payload above 32 KiB
        // (one block per call; a fresh context and bitmaps every call). It
        // must be non-null and outlive this synchronous call.
        template <typename T>
        static XBuffer load_verified(XByteView data,
                                     std::optional<MaximumCapacity> cap = std::nullopt,
                                     std::pmr::memory_resource* upstream = std::pmr::new_delete_resource()) {
            XBuffer xbuf;
            XOFFSET_VREQUIRE(xbuf.load_verified_v<T>(data, cap, upstream));
            return xbuf;
        }

        /// load_verified without exceptions: the buffer, or the code and
        /// message load_verified would have thrown. Admission and
        /// verification are exactly those of load_verified. Also available
        /// in builds without exceptions (-fno-exceptions, /EHs-c-).
        template <typename T>
        static XResult<XBuffer> try_load_verified(XByteView data,
                                                  std::optional<MaximumCapacity> cap = std::nullopt,
                                                  std::pmr::memory_resource* upstream = std::pmr::new_delete_resource()) {
            XBuffer xbuf;
            return detail::try_v_into(xbuf, [&] { return xbuf.load_verified_v<T>(data, cap, upstream); });
        }

        /// Verified load INTO this buffer, reusing its mapping, maximum
        /// capacity and owner registration (no new reservation, pool round
        /// trip or registry entry). Admission and verification are exactly
        /// those of load_verified<T>(wire, this buffer's MaximumCapacity).
        /// Every root reference, element pointer and borrowed span into the
        /// previous content is invalidated; XHandles re-resolve. On any
        /// failure the exception propagates and the buffer is left empty and
        /// without typed root access (root<T>() throws) until a later
        /// successful reload_verified or reset<T>(). Committed pages are kept
        /// (see reclaim_tail_pages()).
        /// Validation scratch for payloads above 32 KiB is retained by this
        /// buffer (released with it or by release_reload_scratch()), so a
        /// reload loop does not allocate host memory per message; smaller
        /// payloads need none. A non-null `upstream` gives that scratch
        /// instead, one block per call.
        template <typename T>
        void reload_verified(XByteView data, std::pmr::memory_resource* upstream = nullptr) {
            XOFFSET_VREQUIRE(reload_into<T>(data, upstream));
        }

        /// reload_verified without exceptions: ok, or the code and message
        /// reload_verified would have thrown. The buffer is left exactly as
        /// reload_verified leaves it (empty, without typed root access, after
        /// a failure).
        template <typename T>
        XStatus try_reload_verified(XByteView data, std::pmr::memory_resource* upstream = nullptr) {
            return detail::try_v([&] { return reload_into<T>(data, upstream); });
        }

        /// Returns the retained reload scratch words to the host allocator.
        void release_reload_scratch() noexcept {
            // An open direct reload keeps its tables until it finishes.
            if (reload_scratch_) reload_scratch_->release(!direct_reload_open_);
        }

    private:
        // upstream == nullptr: this buffer's own reload scratch.
        template <typename T>
        detail::vstatus reload_into(std::span<const std::byte> data, std::pmr::memory_resource* upstream) {
            constexpr const char* who = "reload_verified<T>()";
            XWireHeaderV2 header;
            if (admit_small_wire<T>(data, header)) [[likely]] {
                // Payload <= small_reload_max: the inline bitmaps suffice, so
                // neither upstream nor the retained scratch is touched.
                XOFFSET_ROLLBACK_BEGIN
                    const char* stored = reinterpret_cast<const char*>(data.data()) + sizeof(XWireHeaderV2);
                    const auto used = static_cast<std::size_t>(header.used_bytes);
                    XOFFSET_VTRY(reinit_from_dense_wire_in_place(stored, header.root_offset, used));
                    XOFFSET_VTRY(finish_verified_load<T>(header, stored + (used - XWIRE_PAYLOAD_BEGIN),
                                                         static_cast<std::size_t>(header.hole_count),
                                                         upstream ? upstream : detail::default_wire_upstream(),
                                                         who, nullptr));
                XOFFSET_ROLLBACK_END(reset_arena(0))
                return detail::vok();
            }
            XOFFSET_ROLLBACK_BEGIN
                detail::WireValidationScratch* retained = nullptr;
                if (!upstream) {
                    if (!reload_scratch_) reload_scratch_ = std::make_unique<detail::WireValidationScratch>();
                    retained = reload_scratch_.get();
                    upstream = detail::default_wire_upstream();
                }
                header = XWireHeaderV2{};
                detail::WireLayout layout;
                XOFFSET_VTRY(admit_wire_header<T>(data, header, who, layout));
                XOFFSET_VTRY(reinit_from_wire({header.root_offset, layout.stored, layout.extents, layout.extent_count},
                                              header.used_bytes));
                XOFFSET_VTRY(finish_verified_load<T>(header, layout, upstream, who, retained));
            XOFFSET_ROLLBACK_END(reset_arena(0))
            return detail::vok();
        }

        // load_verified into this default-constructed buffer.
        template <typename T>
        detail::vstatus load_verified_v(std::span<const std::byte> data, std::optional<MaximumCapacity> cap,
                                        std::pmr::memory_resource* upstream) {
            constexpr const char* who = "load_verified<T>()";
            if (!upstream)
                XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument, "validation scratch: null upstream");
            XWireHeaderV2 header{};
            detail::WireLayout layout;
            XOFFSET_VTRY(admit_wire_header<T>(data, header, who, layout));
            const auto used = static_cast<std::size_t>(header.used_bytes);
            // M1: without an explicit limit a wire may claim at most the
            // default maximum; a sparse wire of a few hundred bytes could
            // otherwise make the receiver reserve up to MAX_PAYLOAD.
            const auto capacity = cap ? *cap : MaximumCapacity{DEFAULT_MAXIMUM_CAPACITY};
            XOFFSET_VTRY(detail::require_wire_within_capacity(used, capacity.value, cap.has_value(),
                                                              "load_verified<T>()"));
            XOFFSET_VTRY(init_wire_v(WirePayload{header.root_offset, layout.stored, layout.extents,
                                                 layout.extent_count},
                                     used, capacity));
            return finish_verified_load<T>(header, layout, upstream, who);
        }

        detail::WireValidationScratch& retained_reload_scratch() {
            if (!reload_scratch_) reload_scratch_ = std::make_unique<detail::WireValidationScratch>();
            return *reload_scratch_;
        }

        std::unique_ptr<detail::WireValidationScratch> reload_scratch_;
        std::unique_ptr<detail::DeltaTouchLog> touch_log_;  // touch() marks (delta senders)
        // direct_reload_open_ is in XBufferCore's padding (see there).
        template <typename> friend class VerifiedReloadToken;
        friend struct detail::DeltaAccess;
        friend struct detail::ReceiverAccess;

    public:
        // ---- Delta sync (xoffset_delta.hpp, docs/DELTA_SYNC.md) ----------
        /// Writes into `out` a delta that turns the state `baseline` records
        /// (what one receiver holds) into this buffer's current state, and
        /// advances `baseline` to it. Only payload bytes that differ are sent
        /// (plus the hole table when it changed); an empty baseline gives a
        /// key delta, which a receiver applies to any buffer. When nothing
        /// changed, `out` is left empty and the version does not advance
        /// (send nothing). The buffer is not changed (beyond an incomplete
        /// free table being recomputed, as save_verified does).
        template <typename T>
        XDeltaStats save_delta_into(std::string& out, XDeltaBaseline& baseline, XDeltaOptions options = {});
        template <typename T>
        std::string save_delta(XDeltaBaseline& baseline, XDeltaOptions options = {});
        /// For deltas with XDeltaOptions::touched_only: records that `value`
        /// (an object in this buffer's payload) may have changed, with
        /// everything it reaches (the blocks of its strings and containers,
        /// nested). Call it after the change: a container that moved to a new
        /// block is marked where it is now (the block it left becomes a
        /// hole, which the delta finds by itself).
        template <typename T>
        void touch(const T& value) noexcept;
        /// The same for the payload bytes [p, p + bytes) alone.
        void touch(const void* p, std::size_t bytes) noexcept;
        /// Assigns `value` to `field` (an object in this buffer's payload)
        /// only when they differ (field == value is false), then touches the
        /// field (a scalar: only its bytes that changed); returns whether it
        /// assigned. Touches then name exactly what changed, which is what a
        /// delta from an XDeltaRecord::touches baseline sends.
        template <typename T, typename V>
        bool touch_set(T& field, const V& value);

        /// Applies a delta of save_delta_into to this buffer, which holds the
        /// state `cursor` records (any buffer for a key delta). The patched
        /// payload is verified exactly as load_verified<T> verifies a wire;
        /// on success this buffer holds the sender's payload byte for byte
        /// and `cursor` advances. A rejected non-key delta leaves the buffer
        /// and `cursor` exactly as they were; a key delta rejected after its
        /// header checks leaves the buffer empty (as a failed
        /// reload_verified) and `cursor` cleared.
        /// Either way the receiver asks the sender for a key delta
        /// (XDeltaBaseline::reset). Element pointers into the previous
        /// content are invalidated (the root keeps its address while the
        /// root offset and the storage stay; XHandles re-resolve).
        template <typename T>
        void apply_delta_verified(XByteView delta, XDeltaCursor& cursor);
        /// apply_delta_verified without exceptions: ok, or the code and
        /// message it would have thrown (same buffer and cursor state).
        template <typename T>
        XStatus try_apply_delta_verified(XByteView delta, XDeltaCursor& cursor);

    private:

    public:
        /// Direct receive into this buffer: reload_verified<T>() without the
        /// library copy. The transport writes the wire's sections into the
        /// token's spans (header; extent table; the payload bytes, densely or
        /// each extent at its payload offset - XWIRE_PAYLOAD_BEGIN with gaps
        /// untouched; hole table), calls complete() with the byte counts it
        /// wrote, then finish().
        /// `payload_bytes`, `holes` and `extents` are the header's used_bytes,
        /// hole_count and extent_count. Admission, verification, rejection
        /// and the result are exactly those of reload_verified<T>() on the
        /// same wire; a length that disagrees with the header, an incomplete
        /// transfer, or a token dropped before finish() rejects. From begin
        /// until finish() succeeds the buffer has no typed root access, and
        /// after any failure it is left empty as after a failed reload.
        /// The buffer must not be used, moved or destroyed while the token is
        /// open; one token per buffer at a time. No host allocation per
        /// message once the retained scratch has grown (sparse wires excepted).
        template <typename T>
        VerifiedReloadToken<T> begin_verified_reload(std::size_t payload_bytes,
                                                     std::size_t holes = 0,
                                                     std::size_t extents = 0) {
            std::byte* tables = nullptr;
            std::size_t clean_from = 0;
            XOFFSET_VREQUIRE(begin_verified_reload_v<T>(payload_bytes, holes, extents, tables, clean_from));
            return VerifiedReloadToken<T>(*this, tables, payload_bytes, holes, extents, clean_from);
        }

        /// begin_verified_reload without exceptions: the token, or the code
        /// and message begin_verified_reload would have thrown.
        template <typename T>
        XResult<VerifiedReloadToken<T>> try_begin_verified_reload(std::size_t payload_bytes,
                                                                  std::size_t holes = 0,
                                                                  std::size_t extents = 0) {
            std::byte* tables = nullptr;
            std::size_t clean_from = 0;
            XStatus status = detail::try_v([&] {
                return begin_verified_reload_v<T>(payload_bytes, holes, extents, tables, clean_from);
            });
            if (!status) return std::move(status).error();
            return XResult<VerifiedReloadToken<T>>(
                VerifiedReloadToken<T>(*this, tables, payload_bytes, holes, extents, clean_from));
        }

    private:
        template <typename T>
        detail::vstatus begin_verified_reload_v(std::size_t payload_bytes, std::size_t holes, std::size_t extents,
                                                std::byte*& tables, std::size_t& clean_from) {
            static_assert(is_wire_admitted_v<T>,
                "begin_verified_reload<T>() requires an admitted fixed-schema wire type.");
            if (direct_reload_open_)
                XOFFSET_VFAIL(XException, XErrc::invalid_state,
                              "begin_verified_reload<T>(): a direct reload is already open");
            XOFFSET_ROLLBACK_BEGIN
                // Holes and extents are disjoint, non-empty 8-byte multiples
                // inside the payload, so larger counts never verify; they
                // are refused before the tables are sized.
                if (holes > payload_bytes / 8u || extents > payload_bytes / 8u)
                    XOFFSET_VFAIL(XException, XErrc::size_mismatch, "reload_verified<T>(): used size mismatch");
                XOFFSET_VTRY(reinit_for_overwrite(payload_bytes, clean_from));
                tables = retained_reload_scratch().tables(
                    sizeof(XWireHeaderV2) + extents * 8u + holes * sizeof(WireHole));
                direct_reload_open_ = true;
            XOFFSET_ROLLBACK_END(reset_arena(0))
            return detail::vok();
        }

    public:

        /// Empties this buffer and constructs a fresh root of type T in it,
        /// reusing the mapping and owner registration; equivalent to
        /// `*this = create<T>(get_size(), this buffer's MaximumCapacity)`.
        /// Invalidates references into the previous content like
        /// reload_verified.
        template <typename T>
        XOFFSET_DETAIL_INLINE T& reset() {
            // The same root type again (each message of a stream): the root
            // type and schema binding already are what make<T>() would bind.
            if (this->template cached_root<T>() && schema_bound_ == has_schema_name_v<T>) [[likely]] {
                this->reset_arena_keeping_binding();
                T* result = detail::construct_root<T, false>(*this);
                typed_root_access_allowed_ = true;
                this->cache_root(result);
                return *result;
            }
            reset_arena(this->get_size());
            if (!this->arena()) XOFFSET_THROW(XException(XErrc::invalid_state, "reset<T>(): buffer has no arena"));
            return *this->template make_inline<T>();
        }

        /// An independent copy of this buffer in the same process: one copy of
        /// the used payload into a new owner with this buffer's size, maximum
        /// capacity, growth policy and pool. The exact free table, typed root
        /// access and the root/schema binding come along, so the copy behaves
        /// like this buffer would (same allocations, same save_verified bytes);
        /// nothing goes through a wire and nothing is verified. Throws
        /// XException when this buffer has no arena and XBadAlloc when the new
        /// owner cannot be created; this buffer is never modified.
        XBuffer clone() const {
            const auto* sm = this->arena();
            if (!sm) XOFFSET_THROW(XException(XErrc::invalid_state, "clone(): buffer has no arena"));
            XBuffer out(reinterpret_cast<const char*>(sm), static_cast<std::size_t>(sm->used_end),
                        this->clone_capacity());
            out.copy_clone_state(*this);
            return out;
        }

        /// The same copy into an existing buffer, reusing its mapping, address,
        /// owner registration, maximum capacity and growth policy (like
        /// reload_verified); dst's size becomes this buffer's size, at most
        /// dst's maximum capacity. Every reference into dst's previous content is
        /// invalidated and dst.epoch() advances. Throws XException when dst is
        /// this buffer (nothing is changed) and XBadAlloc when this buffer's
        /// used bytes exceed dst's maximum capacity; on any failure other than
        /// the self copy dst is left an empty arena without a root, as after a
        /// failed reload_verified.
        void clone_into(XBuffer& dst) const {
            dst.clone_state_from(*this);
        }

        MemoryStats stats() {
            return memory_stats(*this);
        }

    private:
        // Small dense reload (reload_verified): a payload of at most
        // small_reload_max bytes and no extent table. One pass over the
        // header checks exactly what admit_wire_header, parse_wire_layout and
        // the reinit admission check; any other wire, valid or not, takes the
        // general path, which reports the same first error.
        static constexpr std::size_t small_reload_max = detail::WireGranuleIndex::inline_extent;
        static constexpr std::size_t wire_header_words = sizeof(XWireHeaderV2) / 8u;

        // The header fields that do not depend on the payload, as words;
        // used_bytes, root_offset and hole_count are 0, extent_count is 0 (dense).
        template <typename T>
        static constexpr std::array<std::uint64_t, wire_header_words> small_wire_image() noexcept {
            XWireHeaderV2 h{};
            h.magic = XWIRE_MAGIC;
            h.flags = XWIRE_FLAGS;
            h.schema_hash = detail::wire_schema_hash<T>();
            h.root_type_id = detail::wire_root_type_id<T>();
            return std::bit_cast<std::array<std::uint64_t, wire_header_words>>(h);
        }

        template <typename T>
        XOFFSET_DETAIL_INLINE static bool admit_small_wire(std::span<const std::byte> data,
                                                                XWireHeaderV2& header) noexcept {
            static_assert(sizeof(XWireHeaderV2) == 40 && offsetof(XWireHeaderV2, used_bytes) == 8 &&
                          offsetof(XWireHeaderV2, hole_count) == 16 && offsetof(XWireHeaderV2, schema_hash) == 24);
            if (!detail::small_reload_fast_path || data.size_bytes() < sizeof(XWireHeaderV2)) return false;
            std::memcpy(&header, data.data(), sizeof(header));
            constexpr auto image = small_wire_image<T>();
            const auto words = std::bit_cast<std::array<std::uint64_t, wire_header_words>>(header);
            // Word 1: used_bytes, root_offset; word 2: hole_count, extent_count (0).
            const std::uint64_t diff = (words[0] ^ image[0]) | (words[2] >> 32) | (words[3] ^ image[3]) |
                                       (words[4] ^ image[4]);
            const std::uint64_t used = header.used_bytes;
            const std::uint64_t body = data.size_bytes() - sizeof(XWireHeaderV2);
            const std::uint64_t stored = used - XWIRE_PAYLOAD_BEGIN;
            return diff == 0 && used >= XWIRE_PAYLOAD_BEGIN && used <= small_reload_max && (used & 7u) == 0 &&
                header.root_offset >= XWIRE_PAYLOAD_BEGIN && header.root_offset < used && stored <= body &&
                body - stored == std::uint64_t(header.hole_count) * 8u;
        }

        // Wire admission shared by load_verified and reload_verified.
        template <typename T>
        static detail::vstatus admit_wire_header(std::span<const std::byte> data, XWireHeaderV2& header,
                                                 const char* who, detail::WireLayout& layout) {
            static_assert(is_wire_admitted_v<T>,
                "load_verified<T>() requires an admitted fixed-schema wire type.");
            XOFFSET_VTRY(detail::read_wire_header(data.data(), data.size_bytes(), who, header));
            if (header.root_type_id != detail::wire_root_type_id<T>()) {
                XOFFSET_VFAIL(XException, XErrc::root_type_mismatch, std::string(who) + ": root type id mismatch");
            }
            if (header.schema_hash != detail::wire_schema_hash<T>()) {
                XOFFSET_VFAIL(XException, XErrc::schema_mismatch, std::string(who) + ": schema hash mismatch");
            }
            return detail::parse_wire_layout(
                header, reinterpret_cast<const char*>(data.data()) + sizeof(XWireHeaderV2),
                data.size_bytes() - sizeof(XWireHeaderV2), who, layout);
        }

        // Post-copy verification shared by load_verified and reload_verified.
        template <typename T>
        detail::vstatus finish_verified_load(const XWireHeaderV2& header, const detail::WireLayout& layout,
                                             std::pmr::memory_resource* upstream, const char* who,
                                             detail::WireValidationScratch* retained = nullptr) {
            return finish_verified_load<T>(header, layout.holes, layout.hole_count, upstream, who, retained);
        }

        // WireOrder: false for a key delta's table (see load_free_blocks).
        template <typename T, bool WireOrder = true>
        detail::vstatus finish_verified_load(const XWireHeaderV2& header, const char* holes, std::size_t hole_count,
                                             std::pmr::memory_resource* upstream, const char* who,
                                             detail::WireValidationScratch* retained) {
            if (this->arena()->used_end != header.used_bytes) {
                XOFFSET_VFAIL(XException, XErrc::root_mismatch, std::string(who) + ": non-normalized payload extent");
            }
            // Every caller wrote the payload header from `header`: its root
            // offset is header.root_offset.
            auto* root_ptr = detail::find_root_at<T>(*this, header.root_offset);
            if (!root_ptr) {
                XOFFSET_VFAIL(XException, XErrc::root_mismatch, std::string(who) + ": root not found");
            }
            auto actual_offset = static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(root_ptr) -
                reinterpret_cast<std::uintptr_t>(this->get_address()));
            if (actual_offset != header.root_offset) {
                XOFFSET_VFAIL(XException, XErrc::root_mismatch, std::string(who) + ": root offset mismatch");
            }
            XOFFSET_VTRY(detail::verify_structural_wire_graph(*root_ptr, *this, upstream, holes, hole_count,
                                                              true, nullptr, retained));
            XOFFSET_VTRY(this->template load_free_blocks<WireOrder>(holes, hole_count));
            typed_root_access_allowed_ = true;
            bind_schema<T>();
            this->cache_root(root_ptr);
            return detail::vok();
        }

        // ---- Trust tier admit (xoffset_receiver.hpp, XCheck::admit) ------
        // The wire parse and admission of reload_verified / load_verified
        // (header, fingerprint, extent and hole tables, capacity) and the same
        // copy into the arena, without the structural graph walk. Besides the
        // header, only what the free table and the root binding rely on is
        // checked: the root object lies inside the used payload, and every
        // hole is an aligned, non-empty 8-byte multiple inside the payload
        // (allocations write into holes). Everything else about the payload is
        // the sender's promise: only for trusted links.
        template <typename T, bool WireOrder = true>
        detail::vstatus finish_admitted_load(const XWireHeaderV2& header, const char* holes, std::size_t hole_count,
                                             const char* who) {
            const std::uint64_t used = header.used_bytes;
            if (this->arena()->used_end != used) {
                XOFFSET_VFAIL(XException, XErrc::root_mismatch, std::string(who) + ": non-normalized payload extent");
            }
            using Storage = std::conditional_t<detail::has_wire_allocator_ctor<T>, T, detail::ReflectRoot<T>>;
            auto* root_ptr = detail::find_root_at<T>(*this, header.root_offset);
            if (!root_ptr || header.root_offset < XWIRE_PAYLOAD_BEGIN || header.root_offset >= used ||
                sizeof(Storage) > used - header.root_offset) {
                XOFFSET_VFAIL(XException, XErrc::root_mismatch, std::string(who) + ": root not found");
            }
            // One pass over the hole table with no early exit and no branch
            // per hole; the verdict is taken once at the end. A hole is valid
            // when offset >= 48, bytes >= 1 and offset + bytes <= used. With
            // o = offset - 48 and b = bytes - 1 in 32-bit arithmetic (an
            // offset below 48 or a size of 0 wraps to near 2^32) that is
            // o + b < used - 48 in 64-bit arithmetic, as used (this arena's
            // used_end) is at most MAX_PAYLOAD < 2^31: so only the largest
            // o + b is compared. Alignment: no low bit set in the OR of all
            // offsets and sizes.
            std::uint32_t low_bits = 0;
            std::uint64_t reach = 0;
            for (std::size_t i = 0; i < hole_count; ++i) {
                WireHole hole;
                std::memcpy(&hole, holes + i * sizeof(hole), sizeof(hole));
                low_bits |= hole.offset | hole.bytes;
                const std::uint64_t r = std::uint64_t(std::uint32_t(hole.offset - std::uint32_t(XWIRE_PAYLOAD_BEGIN))) +
                                        std::uint32_t(hole.bytes - 1u);
                reach = r > reach ? r : reach;
            }
            if ((low_bits & 7u) != 0 || reach >= used - XWIRE_PAYLOAD_BEGIN) [[unlikely]] {
                XOFFSET_VFAIL(XException, XErrc::invalid_payload, std::string(who) + ": hole out of range");
            }
            if constexpr (detail::inband_allocator) XOFFSET_VTRY(this->inband_adopt(holes, hole_count, false, WireOrder));
            else XOFFSET_VTRY(this->load_free_blocks(holes, hole_count));
            typed_root_access_allowed_ = true;
            bind_schema<T>();
            this->cache_root(root_ptr);
            return detail::vok();
        }

        template <typename T>
        detail::vstatus reload_admit_v(std::span<const std::byte> data) {
            constexpr const char* who = "XReceiver::reload<T, admit>()";
            XWireHeaderV2 header;
            if (admit_small_wire<T>(data, header)) [[likely]] {
                XOFFSET_ROLLBACK_BEGIN
                    const char* stored = reinterpret_cast<const char*>(data.data()) + sizeof(XWireHeaderV2);
                    const auto used = static_cast<std::size_t>(header.used_bytes);
                    XOFFSET_VTRY(reinit_from_dense_wire_in_place(stored, header.root_offset, used));
                    XOFFSET_VTRY(finish_admitted_load<T>(header, stored + (used - XWIRE_PAYLOAD_BEGIN),
                                                         static_cast<std::size_t>(header.hole_count), who));
                XOFFSET_ROLLBACK_END(reset_arena(0))
                return detail::vok();
            }
            XOFFSET_ROLLBACK_BEGIN
                header = XWireHeaderV2{};
                detail::WireLayout layout;
                XOFFSET_VTRY(admit_wire_header<T>(data, header, who, layout));
                XOFFSET_VTRY(reinit_from_wire({header.root_offset, layout.stored, layout.extents, layout.extent_count},
                                              header.used_bytes));
                XOFFSET_VTRY(finish_admitted_load<T>(header, layout.holes, layout.hole_count, who));
            XOFFSET_ROLLBACK_END(reset_arena(0))
            return detail::vok();
        }

        // load_verified_v with the admit check: into this default-constructed buffer.
        template <typename T>
        detail::vstatus load_admit_v(std::span<const std::byte> data, MaximumCapacity cap) {
            constexpr const char* who = "XReceiver::load<T, admit>()";
            XWireHeaderV2 header{};
            detail::WireLayout layout;
            XOFFSET_VTRY(admit_wire_header<T>(data, header, who, layout));
            const auto used = static_cast<std::size_t>(header.used_bytes);
            XOFFSET_VTRY(detail::require_wire_within_capacity(used, cap.value, true, who));
            XOFFSET_VTRY(init_wire_v(WirePayload{header.root_offset, layout.stored, layout.extents,
                                                 layout.extent_count},
                                     used, cap));
            return finish_admitted_load<T>(header, layout.holes, layout.hole_count, who);
        }

        void require_typed_root_access(const char* message) const {
            if (!typed_root_access_allowed_) {
                XOFFSET_THROW(XException(XErrc::no_root, message));
            }
        }

        template <typename T>
        void bind_schema_if_available() noexcept {
            if constexpr (has_schema_name_v<T>) {
                bind_schema<T>();
            }
        }

        template <typename T>
        void bind_schema() noexcept {
            this->template bind_root_type<T>();
            if constexpr (has_schema_name_v<T>) {
                schema_bound_ = true;
                bound_root_type_id_ = detail::wire_root_type_id<T>();
                bound_schema_hash_ = detail::wire_schema_hash<T>();
            }
        }

        template <typename T>
        void require_schema_match(const char* message) const {
            if (schema_bound_ && !matches_schema<T>()) {
                XOFFSET_THROW(XException(XErrc::schema_mismatch, message));
            }
        }

    };

    // XBuffer adds its two pointers to XBufferCore and nothing else: a new
    // flag goes into XBufferCore's padding, so that vectors of XBuffer and
    // std::optional<XBuffer> members keep their size. The pointers fit in the
    // tail padding of the 64-byte aligned XBufferCore where an ABI reuses it
    // (Itanium); elsewhere (MSVC) they take one more 64-byte unit.
    static_assert(alignof(XBufferCore) == 64 && sizeof(XBufferCore) == 256, "XBufferCore: four 64-byte lines");
    static_assert(sizeof(XBuffer) == sizeof(XBufferCore) || sizeof(XBuffer) == sizeof(XBufferCore) + 64,
                  "XBuffer grew: put the new field into XBufferCore's padding");

    // Open direct reload of one XBuffer (XBuffer::begin_verified_reload).
    template <typename T>
    class VerifiedReloadToken {
    public:
        VerifiedReloadToken(VerifiedReloadToken&& other) noexcept
            : buffer_(std::exchange(other.buffer_, nullptr)), tables_(other.tables_),
              payload_(other.payload_), holes_(other.holes_), extents_(other.extents_),
              clean_from_(other.clean_from_), received_(other.received_), complete_(other.complete_) {}
        VerifiedReloadToken& operator=(VerifiedReloadToken&&) = delete;
        VerifiedReloadToken(const VerifiedReloadToken&) = delete;
        VerifiedReloadToken& operator=(const VerifiedReloadToken&) = delete;
        ~VerifiedReloadToken() { if (buffer_) abort(); }

        explicit operator bool() const noexcept { return buffer_ != nullptr; }
        std::span<std::byte> header_bytes() const noexcept {
            return buffer_ ? std::span<std::byte>(tables_, sizeof(XWireHeaderV2)) : std::span<std::byte>{};
        }
        std::span<std::byte> extent_bytes() const noexcept {
            return buffer_ ? std::span<std::byte>(tables_ + sizeof(XWireHeaderV2), extents_ * 8u)
                           : std::span<std::byte>{};
        }
        // The payload bytes of the wire: payload [48, used_bytes).
        std::span<std::byte> payload_bytes() const noexcept {
            return buffer_ ? std::span<std::byte>(reinterpret_cast<std::byte*>(buffer_->data()) + XWIRE_PAYLOAD_BEGIN,
                                                  payload_ - XWIRE_PAYLOAD_BEGIN)
                           : std::span<std::byte>{};
        }
        std::span<std::byte> hole_bytes() const noexcept {
            return buffer_ ? std::span<std::byte>(tables_ + sizeof(XWireHeaderV2) + extents_ * 8u,
                                                  holes_ * sizeof(XBufferCore::WireHole))
                           : std::span<std::byte>{};
        }
        // payload_bytes counts the payload bytes including sparse gaps
        // (used_bytes - 48).
        void complete(std::size_t header_bytes, std::size_t payload_bytes,
                      std::size_t hole_bytes = 0, std::size_t extent_bytes = 0) noexcept {
            received_ = {header_bytes, payload_bytes, hole_bytes, extent_bytes};
            complete_ = true;
        }

        /// Verifies and publishes the received wire; consumes the token.
        void finish() {
            XOFFSET_VREQUIRE(finish_v());
        }

        /// finish() without exceptions: ok, or the code and message finish()
        /// would have thrown. Consumes the token either way.
        XStatus try_finish() {
            return detail::try_v([&] { return finish_v(); });
        }

    private:
        detail::vstatus finish_v() {
            constexpr const char* who = "reload_verified<T>()";
            auto* buffer = std::exchange(buffer_, nullptr);
            if (!buffer) XOFFSET_VFAIL(XException, XErrc::invalid_state, "verified reload: token consumed");
            struct Close { XBuffer& b; ~Close() { b.direct_reload_open_ = false; } } close{*buffer};
            XOFFSET_ROLLBACK_BEGIN
                if (!complete_ || received_[0] != sizeof(XWireHeaderV2) ||
                    received_[1] != payload_ - XWIRE_PAYLOAD_BEGIN ||
                    received_[2] != holes_ * sizeof(XBufferCore::WireHole) || received_[3] != extents_ * 8u)
                    XOFFSET_VFAIL(XException, XErrc::incomplete_input, "verified reload: incomplete input");
                // admit_wire_header and parse_wire_layout, with the section
                // lengths fixed by begin instead of read from one wire.
                XWireHeaderV2 header;
                XOFFSET_VTRY(detail::read_wire_header(tables_, sizeof(XWireHeaderV2), who, header));
                if (header.root_type_id != detail::wire_root_type_id<T>())
                    XOFFSET_VFAIL(XException, XErrc::root_type_mismatch, std::string(who) + ": root type id mismatch");
                if (header.schema_hash != detail::wire_schema_hash<T>())
                    XOFFSET_VFAIL(XException, XErrc::schema_mismatch, std::string(who) + ": schema hash mismatch");
                XOFFSET_VTRY(detail::check_wire_extent(header, who));
                if (header.used_bytes != payload_ || header.extent_count != extents_ ||
                    header.hole_count != holes_)
                    XOFFSET_VFAIL(XException, XErrc::size_mismatch, std::string(who) + ": used size mismatch");
                detail::WireLayout layout;
                layout.extent_count = extents_;
                if (extents_) layout.extents = reinterpret_cast<const char*>(tables_ + sizeof(XWireHeaderV2));
                std::uint64_t stored;
                XOFFSET_VTRY(detail::parse_extent_table(layout.extents, extents_, header.used_bytes, who, layout.gaps,
                                                        stored));
                layout.holes = reinterpret_cast<const char*>(tables_ + sizeof(XWireHeaderV2) + extents_ * 8u);
                layout.hole_count = holes_;
                // Gaps read as zero, as in assign_extents: clear what an
                // earlier content may have left below clean_from_.
                auto* base = buffer->data();
                for (const auto& gap : layout.gaps) {
                    const auto end = std::min<std::size_t>(std::size_t(gap.offset) + gap.bytes, clean_from_);
                    if (gap.offset < end) std::memset(base + gap.offset, 0, end - gap.offset);
                }
                XBufferCore::write_payload_header(base, header.root_offset, header.used_bytes);
                XOFFSET_VTRY(buffer->check_reinit_payload());
                XOFFSET_VTRY(buffer->template finish_verified_load<T>(header, layout, std::pmr::new_delete_resource(),
                                                                      who, &buffer->retained_reload_scratch()));
            XOFFSET_ROLLBACK_END(buffer->reset_arena(0))
            return detail::vok();
        }

        friend class XBuffer;
        VerifiedReloadToken(XBuffer& buffer, std::byte* tables, std::size_t payload, std::size_t holes,
                            std::size_t extents, std::size_t clean_from) noexcept
            : buffer_(&buffer), tables_(tables), payload_(payload), holes_(holes), extents_(extents),
              clean_from_(clean_from) {}
        void abort() noexcept {
            auto* buffer = std::exchange(buffer_, nullptr);
            buffer->reset_arena(0);
            buffer->direct_reload_open_ = false;
        }

        XBuffer* buffer_;
        std::byte* tables_;
        std::size_t payload_, holes_, extents_, clean_from_;
        std::array<std::size_t, 4> received_{};
        bool complete_ = false;
    };

    // TypedXBuffer<T>: an XBuffer bound to root type T at compile time, so
    // make/root/edit/save/load take no <T> (docs/USER_GUIDE.md).
    template<typename T>
    class TypedXBuffer : public XBuffer {
    public:
        using XBuffer::XBuffer;

        static TypedXBuffer create(std::size_t reserve_bytes = 4096) {
            TypedXBuffer xbuf(reserve_bytes);
            (void)xbuf.make();
            return xbuf;
        }

        static TypedXBuffer create(std::size_t initial_bytes, MaximumCapacity cap) {
            TypedXBuffer xbuf(initial_bytes, cap);
            (void)xbuf.make();
            return xbuf;
        }

        /// Creates the single root object of type T.
        T* make() { return XBuffer::make<T>(); }

        /// A proxy of the root: valid across moves of the payload and of this
        /// buffer, fail-soft (see XBuffer::ref and xoffset_proxy.hpp). With a
        /// field list (XOFFSET_RECORD) its fields have named accessors:
        /// buf.root().units().emplace_back().hp() = 5.
        XRef<T> root() noexcept { return XBuffer::ref<T>(); }
        /// A plain reference to the root (what root() returned before the
        /// growth policy), with no protection: it dangles when the payload
        /// moves (a Relocating buffer growing past its reservation, grow() of
        /// a movable buffer). Prefer root(), or a pin scope (edit / read),
        /// whose plain references cannot dangle.
        T& plain_root() { return XBuffer::root<T>(); }
        /// Pin scopes (see XBuffer::edit / read).
        template <typename F>
        bool edit(F&& f, size_type expected_growth = 0) { return XBuffer::edit<T>(std::forward<F>(f), expected_growth); }
        template <typename F>
        decltype(auto) read(F&& f) { return XBuffer::read<T>(std::forward<F>(f)); }

        /// Returns true if a root object exists.
        bool has_root() { return XBuffer::has_root<T>(); }

        bool matches_schema() const { return XBuffer::matches_schema<T>(); }

        /// Creates root and returns an epoch-cached handle.
        XHandle<T> make_handle() { return XBuffer::make_handle<T>(); }

        /// Returns an epoch-cached handle to the existing root.
        XHandle<T> handle() { return XBuffer::handle<T>(); }

        /// Load from serialized data and return a typed buffer.
        static TypedXBuffer load(XByteView data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return TypedXBuffer(XBuffer::load_verified<T>(data, cap));
        }

        static TypedXBuffer load_unverified(XByteView data, std::optional<MaximumCapacity> cap = std::nullopt) {
            return TypedXBuffer(XBuffer::load_unverified(data, cap));
        }

        std::string save_verified(XSaveOptions options = {}) { return XBuffer::save_verified<T>(options); }
        void reload_verified(XByteView data, std::pmr::memory_resource* upstream = nullptr) {
            XBuffer::reload_verified<T>(data, upstream);
        }
        /// Empties the buffer and makes a new root; a proxy of it (proxies
        /// of the old content become stale).
        XRef<T> reset() {
            (void)XBuffer::reset<T>();
            return XBuffer::ref<T>();
        }

        TypedXBuffer clone() const { return TypedXBuffer(XBuffer::clone()); }
        void clone_into(TypedXBuffer& dst) const { XBuffer::clone_into(dst); }

        void save_verified_into(std::string& out, XSaveOptions options = {}) {
            XBuffer::save_verified_into<T>(out, options);
        }
        std::string save_compact(XCompactOptions options = {}) { return XBuffer::save_compact<T>(options); }
        void save_compact_into(std::string& out, XCompactOptions options = {}) { XBuffer::save_compact_into<T>(out, options); }
        template <typename A0, typename... A>
        void save_compact_into(std::string& out, XCompactOptions options, const XAssign<A0>& first,
                               const XAssign<A>&... rest) {
            XBuffer::save_compact_into<T>(out, options, first, rest...);
        }

        static TypedXBuffer load_verified(XByteView data, std::optional<MaximumCapacity> cap = std::nullopt,
                                          std::pmr::memory_resource* upstream = std::pmr::new_delete_resource()) {
            return TypedXBuffer(XBuffer::load_verified<T>(data, cap, upstream));
        }

        /// load_verified / reload_verified without exceptions (see XBuffer).
        static XResult<TypedXBuffer> try_load_verified(XByteView data,
                                                       std::optional<MaximumCapacity> cap = std::nullopt,
                                                       std::pmr::memory_resource* upstream = std::pmr::new_delete_resource()) {
            auto loaded = XBuffer::try_load_verified<T>(data, cap, upstream);
            if (!loaded) return loaded.error();
            return XResult<TypedXBuffer>(TypedXBuffer(std::move(*loaded)));
        }

        XStatus try_reload_verified(XByteView data, std::pmr::memory_resource* upstream = nullptr) {
            return XBuffer::try_reload_verified<T>(data, upstream);
        }

        /// Delta sync (see XBuffer::save_delta_into / apply_delta_verified).
        XDeltaStats save_delta_into(std::string& out, XDeltaBaseline& baseline, XDeltaOptions options = {}) {
            return XBuffer::save_delta_into<T>(out, baseline, options);
        }
        std::string save_delta(XDeltaBaseline& baseline, XDeltaOptions options = {}) {
            return XBuffer::save_delta<T>(baseline, options);
        }
        void apply_delta_verified(XByteView delta, XDeltaCursor& cursor) {
            XBuffer::apply_delta_verified<T>(delta, cursor);
        }
        XStatus try_apply_delta_verified(XByteView delta, XDeltaCursor& cursor) {
            return XBuffer::try_apply_delta_verified<T>(delta, cursor);
        }

        /// Construct from an existing XBuffer (e.g., after compaction).
        TypedXBuffer(XBuffer&& other) noexcept
            : XBuffer(std::move(other)) {}
    };
} // namespace XOffsetDatastructure

#endif // X_OFFSET_BUFFER_HPP
