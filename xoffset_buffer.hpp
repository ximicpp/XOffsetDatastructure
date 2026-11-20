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
                : (buffer_->root_bound() && !buffer_->template root_is<T>()
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
        // save_compact's admission (xoffset_verified_paths.hpp).
        template <class U> consteval bool compact_direct_supported();

        // Internal friend bridge; no Core/container/wire field is added.
        struct VerifiedPathsAccess {
            using Identity = XBufferCore::OwnerRecordPtr;
            static Identity identity(XBufferCore& source) { return source.owner_identity(); }
            static XBufferCore& owner(const Identity& value) { return XBufferCore::require_record_owner(value); }
            template<class T> static XWireHeader header(XBufferCore&);
            template<class T> static void save_into(XBufferCore&, std::string&, XSaveOptions);
            template<class T, bool Band> static bool save_small_into(XBufferCore&, std::string&, XWireEmission);
            template<class T> static void save_into_general(XBufferCore&, std::string&, XSaveOptions);
            template<class T> static bool save_watched_into(XBufferCore&, std::string&);
            template<class T> static bool auto_compact(XBufferCore&);
            template<class T> static bool save_auto_compact_into(XBufferCore&, std::string&);
            template<class T> static bool save_root_spare_into(XBufferCore&, std::string&);
            template<class T> static void rebuild_free_table(T&, XBufferCore&);
            template<class T> static bool try_save_compact_into(XBufferCore&, std::string&, std::uint32_t);
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
            if (arena()->used_end != root_at) {
                XOFFSET_THROW(XException(XErrc::invalid_state, "make<T>(): root object already exists. "
                    "Call root<T>() to access the existing object."));
            }
            auto* result = detail::construct_root<T>(*this);
            typed_root_access_allowed_ = true;
            this->template bind_root<T>();
            if constexpr (has_schema_name_v<T>) write_fingerprint<T>();
            return result;
        }

    private:
        // An open pin scope of edit, build or read. Bare (the pin flag, see
        // XBufferCore::pin_bare_free): a block made while it is open takes
        // it over and the close exits that block. Counted: entered on the
        // lineage's block (ctl; null when the entry failed and nothing is
        // to exit), which follows the content if this XBuffer is moved
        // meanwhile. edit and read inline f once, after whichever entry;
        // build once more in its retry (build_again).
        struct PinScope {
            XBufferCore* core;
            detail::XCtl* ctl;
            bool bare;
            bool open = true;
            XOFFSET_DETAIL_INLINE PinScope(XBufferCore& owner, detail::XCtl* entered, bool is_bare) noexcept
                : core(&owner), ctl(entered), bare(is_bare) {}
            XOFFSET_DETAIL_INLINE ~PinScope() {
                if (open) (void)close();
            }
            PinScope(const PinScope&) = delete;
            PinScope& operator=(const PinScope&) = delete;
            // Closes the scope. False: a failure was recorded inside it (a
            // counted scope: the buffer's state as the scope ends).
            XOFFSET_DETAIL_INLINE bool close() noexcept {
                open = false;
                if (bare) [[likely]] return core->pin_close_bare();
                return close_counted(core, ctl);
            }
            XOFFSET_DETAIL_NOINLINE static bool close_counted(XBufferCore* core, detail::XCtl* ctl) noexcept {
                const bool ok = core->ok();
                XBufferCore::pin_exit(ctl);
                return ok;
            }
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
        // The root: when the root key is T's (make<T>(), a verified load of
        // T, reset<T>(), a clone of such a buffer) it is at root_at, read
        // with one compare. Anything else takes the checked path.
        template<typename T>
        T& root() {
            if (this->template root_is<T>()) [[likely]] return *detail::image_root<std::remove_cv_t<T>>(*this);
            return root_checked<T>();
        }

    private:
        // root<T>()'s checked path, out of line so the compare above stays
        // a compare and a load where it is inlined: a root read through
        // another type (one of the same fingerprint, or any type when the
        // bound root type has no schema name), or a failure.
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
        XOFFSET_DETAIL_INLINE bool edit(F&& f, size_type expected_growth = 0) {
            using R = std::invoke_result_t<F&, T&>;
            static_assert(std::is_void_v<R>,
                "edit(f): f must return void (a value made inside may point into the payload; "
                "use read(f) to compute a value)");
            // Fast entry: no growth asked, the root key is T's (the root is
            // at root_at) and nothing has to happen first (edit_quiet: no
            // block, no open scope, a payload that stays put). The scope is
            // bare: the pin flag.
            ScopeEntry<T> entry;
            if (expected_growth == 0 && this->template root_is<T>() && this->edit_quiet()) [[likely]] {
                entry = {detail::image_root<std::remove_cv_t<T>>(*this), nullptr};
                this->pin_open_bare();
            } else {
                entry = edit_enter<T>(expected_growth);
                if (!entry.root) return false;
            }
            PinScope scope(*this, entry.ctl, entry.ctl == nullptr);
            record_error(detail::run_fail_soft([&] { f(*entry.root); }));
            return scope.close();
        }

    private:
        // An entered scope of edit or build: the root, and the block the
        // scope counts on (null: a bare scope). A null root: nothing entered.
        template<typename T>
        struct ScopeEntry {
            T* root;
            detail::XCtl* ctl;
        };
        // edit's entry when something has to happen first: a failed buffer
        // refuses, a payload that may move moves (pin_root), a nested scope
        // counts on the lineage's block.
        template<typename T>
        XOFFSET_DETAIL_NOINLINE ScopeEntry<T> edit_enter(size_type expected_growth) {
            if (!ok()) return {nullptr, nullptr};
            T* root_ptr = pin_root<T>(expected_growth);
            if (!root_ptr || !pin_enter()) return {nullptr, nullptr};
            return {root_ptr, control()};
        }

    public:

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
            using R = std::invoke_result_t<F&, T&>;
            static_assert(std::is_void_v<R>,
                "build(f): f must return void (a value made inside may point into the payload; "
                "use read(f) to compute a value)");
            // Each message of a stream: the root key is T's and a bare scope
            // may open (no block, so no end of lineage either).
            ScopeEntry<T> entry;
            if (expected_growth == 0 && this->pin_bare_free() && this->template root_is<T>()) [[likely]] {
                entry = {&reset_same_root<T, false>(), nullptr};
                this->pin_open_bare();
            } else {
                entry = build_enter<T>(expected_growth);
                if (!entry.root) return build_again<T>(f);
            }
            PinScope scope(*this, entry.ctl, entry.ctl == nullptr);
            record_error(detail::run_fail_soft([&] { f(*entry.root); }));
            if (scope.close()) [[likely]] return true;
            return build_again<T>(f);
        }

    private:
        // A reset<T>() for the cold paths below, out of line.
        template<typename T>
        XOFFSET_DETAIL_NOINLINE T& reset_cold() {
            return reset<T>();
        }
        // build's entry after reset<T>(): a bare scope when one may open and
        // no growth is asked, otherwise edit's (a lineage with a block or an
        // open scope counts the scope on it). The fresh root, or null with
        // nothing entered.
        template<typename T>
        XOFFSET_DETAIL_NOINLINE ScopeEntry<T> build_enter(size_type expected_growth) {
            T* fresh = &reset_cold<T>();
            if (expected_growth == 0 && this->pin_bare_free()) {
                this->pin_open_bare();
                return {fresh, nullptr};
            }
            return edit_enter<T>(expected_growth);
        }
        // A failure a larger reservation can cure: the scope could not move
        // the payload, and the buffer can still grow.
        bool build_outgrew() const noexcept {
            const XErrc e = error();
            if (e != XErrc::out_of_storage && e != XErrc::borrowed) return false;
            return !this->pinned() && !this->address_stable() && reservation_limit() < maximum_capacity();
        }
        // build's later attempts (at most 16) while a larger reservation can
        // cure the failure: a reservation of at least twice the size
        // reached, then build's entry and f again (its one other copy).
        template<typename T, typename F>
        XOFFSET_DETAIL_NOINLINE bool build_again(F& f) {
            for (int attempt = 0; attempt < 16 && build_outgrew(); ++attempt) {
                const size_type limit = reservation_limit();
                const size_type reached = arena() ? static_cast<size_type>(arena()->used_end) : 0;
                (void)reset_cold<T>();
                const size_type target = std::min<size_type>(std::max(limit, reached) * 2u, maximum_capacity());
                const size_type used = arena() ? static_cast<size_type>(arena()->used_end) : 0;
                if (target <= used || make_room(target - used) != XErrc::ok || reservation_limit() <= limit) {
                    record_error(XErrc::out_of_storage);
                    return false;
                }
                const ScopeEntry<T> entry = build_enter<T>(0);
                if (!entry.root) continue;
                PinScope scope(*this, entry.ctl, entry.ctl == nullptr);
                record_error(detail::run_fail_soft([&] { f(*entry.root); }));
                if (scope.close()) return true;
            }
            return false;
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
            const bool bare = this->pin_bare_free();
            detail::XCtl* entered = nullptr;
            if (bare) [[likely]] this->pin_open_bare();
            else entered = read_enter();
            PinScope scope(*this, entered, bare);
            if constexpr (std::is_void_v<R>) {
                f(root_ref);
            } else {
                return R(f(root_ref));
            }
        }

    private:
        // read's scope on a lineage with a block, or nested in another
        // scope: counted on the block (null: not entered).
        XOFFSET_DETAIL_NOINLINE detail::XCtl* read_enter() noexcept {
            return pin_enter() ? control() : nullptr;
        }

    public:

        template<typename T>
        bool has_root() {
            if (this->root_named() && !matches_schema<T>()) return false;
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
                if (!buffer.typed_root_access_allowed_ || !buffer.template root_is<T>()) return nullptr;
                if (buffer.root_named() && !buffer.template matches_schema<T>()) return nullptr;
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

        // The binding is T's: a root of a type with a schema name was made or
        // loaded, and the image header holds T's fingerprint.
        template <typename T>
        bool matches_schema() const {
            if (!this->root_named()) return false;
            if constexpr (!has_schema_name_v<T>) {
                return false;
            } else {
                return holds_fingerprint<T>();
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
        // save_verified; the source is not changed. A valid wire: every
        // receiver accepts it.
        template <typename T>
        XOFFSET_DETAIL_INLINE void save_compact_into(std::string& out, XCompactOptions options = {}) {
            if (refuse_failed_emit(out)) return;
            if (detail::VerifiedPathsAccess::try_save_compact_into<T>(*this, out, options.slack_div)) [[likely]]
                return;
            save_compact_into_checked<T>(out, options);
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

        // If a host allocation failure (or an unverified load) left the free
        // state incomplete, recompute it exactly from the live graph.
        template <typename T>
        void ensure_hole_table(const T& root) {
            if (this->free_table_complete()) return;
            std::vector<WireHole> holes;
            XOFFSET_VREQUIRE(detail::verify_structural_wire_graph(root, *this, std::pmr::new_delete_resource(),
                                                                  nullptr, 0, false, &holes));
            this->replace_free_blocks(holes);
        }

        // The wire header of this image as T's: the image's own header with
        // T's fingerprint.
        template <typename T>
        XWireHeader image_wire_header() const {
            // The used extent: used_end, at least the header, at most the
            // logical length.
            const auto* sm = this->arena();
            const auto used = std::min<std::uint64_t>(
                std::max<std::uint64_t>(sm->used_end, sizeof(arena_header)), this->get_size());
            return detail::make_wire_header<T>(used, sm->free_dir, sm->free_bytes);
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
            const auto header = image_wire_header<T>();
            if (options.emission == XWireEmission::automatic) {
                if constexpr (detail::compact_direct_supported<T>())
                    if (detail::VerifiedPathsAccess::auto_compact<T>(*this))
                        return save_compact<T>();
                options.emission = XWireEmission::free_runs;
            }
            detail::WireGaps gaps;
            if (options.emission == XWireEmission::free_runs) gaps = detail::wire_free_runs(*this, header.used_bytes);
            const auto output_size = options.emission == XWireEmission::zero_runs
                ? std::size_t(header.used_bytes)
                : detail::wire_free_run_size(header.used_bytes, gaps);
            std::string out;
            if (output_size > out.max_size())
                XOFFSET_THROW(std::length_error("save_verified<T>(): output size exceeds string limit"));
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
            out.resize_and_overwrite(output_size, [&](char* output, std::size_t) noexcept {
                return detail::write_wire(output, header, base, options.emission, gaps);
            });
#else
            out.resize(output_size);
            out.resize(detail::write_wire(out.data(), header, base, options.emission, gaps));
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
        // save_compact_into past its reused-output path: the full checks,
        // then the general writer (measure, reserve, write).
        template <typename T>
        XOFFSET_DETAIL_NOINLINE void save_compact_into_checked(std::string& out, XCompactOptions options) {
            require_typed_root_access("save_compact<T>(): unverified payload cannot be relabeled as verified");
            require_schema_match<T>("save_compact<T>(): schema mismatch for requested root type");
            if (!detail::find_root<T>(*this))
                XOFFSET_THROW(XException(XErrc::no_root, "save_compact<T>(): no root object of requested type"));
            detail::VerifiedPathsAccess::save_compact_into<T>(*this, out, options);
        }

        // upstream == nullptr: this buffer's own reload scratch. The small
        // image's in-place path is always inline at the caller (a receive
        // loop); any other wire takes reload_into_general.
        template <typename T>
        XOFFSET_DETAIL_INLINE detail::vstatus reload_into(std::span<const std::byte> data,
                                                          std::pmr::memory_resource* upstream) {
            constexpr const char* who = "reload_verified<T>()";
            const std::size_t size = data.size_bytes();
            if (admit_image<T>(data) && size <= small_reload_max && detail::small_reload_fast_path &&
                this->can_reload_image_in_place(size)) [[likely]] {
                // An image <= small_reload_max: the inline bitmaps suffice, so
                // neither upstream nor the retained scratch is touched.
                if (!this->reload_image_in_place(reinterpret_cast<const char*>(data.data()), size)) [[unlikely]]
                    return reload_storage_failed();
                XOFFSET_ROLLBACK_BEGIN
                    XOFFSET_VTRY(finish_verified_load<T>(size, upstream ? upstream : detail::default_wire_upstream(),
                                                         who, nullptr));
                XOFFSET_ROLLBACK_END(reset_arena(0))
                return detail::vok();
            }
            return reload_into_general<T>(data, upstream);
        }
        template <typename T>
        XOFFSET_DETAIL_NOINLINE detail::vstatus reload_into_general(std::span<const std::byte> data,
                                                                    std::pmr::memory_resource* upstream) {
            constexpr const char* who = "reload_verified<T>()";
            XOFFSET_ROLLBACK_BEGIN
                detail::WireValidationScratch* retained = nullptr;
                if (!upstream) {
                    if (!reload_scratch_) reload_scratch_ = std::make_unique<detail::WireValidationScratch>();
                    retained = reload_scratch_.get();
                    upstream = detail::default_wire_upstream();
                }
                XWireHeader header{};
                detail::WireLayout layout;
                XOFFSET_VTRY(admit_wire_header<T>(data, header, who, layout));
                XOFFSET_VTRY(reinit_from_wire({layout.stored, layout.extents, layout.extent_count}, header.used_bytes));
                XOFFSET_VTRY(finish_verified_load<T>(header.used_bytes, upstream, who, retained));
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
            XWireHeader header{};
            detail::WireLayout layout;
            XOFFSET_VTRY(admit_wire_header<T>(data, header, who, layout));
            const auto used = static_cast<std::size_t>(header.used_bytes);
            // M1: without an explicit limit a wire may claim at most the
            // default maximum; a sparse wire of a few hundred bytes could
            // otherwise make the receiver reserve up to MAX_PAYLOAD.
            const auto capacity = cap ? *cap : MaximumCapacity{DEFAULT_MAXIMUM_CAPACITY};
            XOFFSET_VTRY(detail::require_wire_within_capacity(used, capacity.value, cap.has_value(),
                                                              "load_verified<T>()"));
            XOFFSET_VTRY(init_wire_v(WirePayload{layout.stored, layout.extents, layout.extent_count}, used, capacity));
            return finish_verified_load<T>(used, upstream, who);
        }

        detail::WireValidationScratch& retained_reload_scratch() {
            if (!reload_scratch_) reload_scratch_ = std::make_unique<detail::WireValidationScratch>();
            return *reload_scratch_;
        }

        std::unique_ptr<detail::WireValidationScratch> reload_scratch_;
        // direct_reload_open_ is in XBufferCore's padding (see there).
        template <typename> friend class VerifiedReloadToken;
        friend struct detail::DeltaAccess;
        friend struct detail::ReceiverAccess;

    public:
        // ---- Delta sync (xoffset_delta.hpp, docs/DELTA_SYNC.md) ----------
        /// Writes into `out` a delta that turns the state `baseline` records
        /// (what one receiver holds) into this buffer's current state, and
        /// advances `baseline` to it. Only payload bytes that differ are sent
        /// (the free blocks travel as the image bytes they are); an empty
        /// baseline gives a key delta, which a receiver applies to any
        /// buffer. When nothing changed, `out` is left empty and the version
        /// does not advance (send nothing). The buffer is not changed
        /// (beyond an incomplete free table being recomputed, as
        /// save_verified does).
        template <typename T>
        XDeltaStats save_delta_into(std::string& out, XDeltaBaseline& baseline, XDeltaOptions options = {});
        /// The same into the caller's memory `out`: returns the delta's size
        /// (0: nothing changed, send nothing). A delta larger than `out` is
        /// not written: its size is returned and `baseline` keeps it, and
        /// its next save_delta_into (into memory at least that large, or a
        /// string) writes it before anything else.
        template <typename T>
        std::size_t save_delta_into(std::span<char> out, XDeltaBaseline& baseline, XDeltaOptions options = {});
        template <typename T>
        std::string save_delta(XDeltaBaseline& baseline, XDeltaOptions options = {});
    private:
        // save_delta_into(std::string&) past its short path.
        template <typename T>
        XDeltaStats save_delta_into_general(std::string& out, XDeltaBaseline& baseline, XDeltaOptions options);
    public:
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
        /// library copy. The transport writes the wire into the token's
        /// spans: the image (its 40-byte header as on the wire, then the
        /// image bytes, densely or each extent at its offset with gaps
        /// untouched) and, for a sparse wire, the extent table. It calls
        /// complete() with the byte counts it wrote (the image's counts the
        /// gaps), then finish(). `used_bytes` and `extents` are the header's
        /// used_bytes and extent_count. Admission, verification, rejection
        /// and the result are exactly those of reload_verified<T>() on the
        /// same wire; a length that disagrees with the header, an incomplete
        /// transfer, or a token dropped before finish() rejects. From begin
        /// until finish() succeeds the buffer has no typed root access, and
        /// after any failure it is left empty as after a failed reload.
        /// The buffer must not be used, moved or destroyed while the token is
        /// open; one token per buffer at a time. No host allocation per
        /// message once the retained scratch has grown (sparse wires excepted).
        template <typename T>
        VerifiedReloadToken<T> begin_verified_reload(std::size_t used_bytes, std::size_t extents = 0) {
            std::byte* table = nullptr;
            std::size_t clean_from = 0;
            XOFFSET_VREQUIRE(begin_verified_reload_v<T>(used_bytes, extents, table, clean_from));
            return VerifiedReloadToken<T>(*this, table, used_bytes, extents, clean_from);
        }

        /// begin_verified_reload without exceptions: the token, or the code
        /// and message begin_verified_reload would have thrown.
        template <typename T>
        XResult<VerifiedReloadToken<T>> try_begin_verified_reload(std::size_t used_bytes, std::size_t extents = 0) {
            std::byte* table = nullptr;
            std::size_t clean_from = 0;
            XStatus status = detail::try_v([&] {
                return begin_verified_reload_v<T>(used_bytes, extents, table, clean_from);
            });
            if (!status) return std::move(status).error();
            return XResult<VerifiedReloadToken<T>>(
                VerifiedReloadToken<T>(*this, table, used_bytes, extents, clean_from));
        }

    private:
        template <typename T>
        detail::vstatus begin_verified_reload_v(std::size_t used_bytes, std::size_t extents,
                                                std::byte*& table, std::size_t& clean_from) {
            static_assert(is_wire_admitted_v<T>,
                "begin_verified_reload<T>() requires an admitted fixed-schema wire type.");
            if (direct_reload_open_)
                XOFFSET_VFAIL(XException, XErrc::invalid_state,
                              "begin_verified_reload<T>(): a direct reload is already open");
            XOFFSET_ROLLBACK_BEGIN
                // Extents are disjoint, non-empty 8-byte multiples inside the
                // image, so larger counts never verify; they are refused
                // before the table is sized.
                if (extents > used_bytes / 8u)
                    XOFFSET_VFAIL(XException, XErrc::size_mismatch, "reload_verified<T>(): used size mismatch");
                XOFFSET_VTRY(reinit_for_overwrite(used_bytes, clean_from));
                table = retained_reload_scratch().tables(extents * 8u + 8u);
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
            // key and the fingerprint already are what make<T>() would write.
            if (this->template root_is<T>()) [[likely]] return reset_same_root<T, true>();
            return reset_general<T>();
        }

    private:
        // reset<T>() of a buffer whose root key is T's: an empty root in
        // place of the old one (XBufferCore::reset_keeping_root). Without
        // MayHaveBlock the caller knows the lineage has no block.
        template<typename T, bool MayHaveBlock>
        XOFFSET_DETAIL_INLINE T& reset_same_root() {
            using U = std::remove_cv_t<T>;
            using S = detail::root_storage_t<U>;
            char* const raw = this->template reset_keeping_root<MayHaveBlock>(
                arena_header::block_size(static_cast<std::uint32_t>(sizeof(S))));
            if constexpr (std::is_nothrow_constructible_v<S, fixed_detail::allocator_ref>) {
                return *detail::construct_root_at<U>(*this, raw);
            } else {
                U* result = nullptr;
                XOFFSET_TRY {
                    result = detail::construct_root_at<U>(*this, raw);
                } XOFFSET_CATCH(...) {
                    this->forget_root();
                    XOFFSET_RETHROW;
                }
                return *result;
            }
        }
        template<typename T>
        XOFFSET_DETAIL_NOINLINE T& reset_general() {
            reset_arena(this->get_size());
            if (!this->arena()) XOFFSET_THROW(XException(XErrc::invalid_state, "reset<T>(): buffer has no arena"));
            return *this->template make_inline<T>();
        }

    public:

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
        // Small dense reload (reload_verified): an image of at most
        // small_reload_max bytes, which the inline bitmaps validate.
        static constexpr std::size_t small_reload_max = detail::WireGranuleIndex::inline_extent;

        // T's fixed header words: the magic and header size, then the
        // fingerprint.
        template <typename T>
        static constexpr std::array<std::uint64_t, 3> image_words() noexcept {
            return {XBufferCore::image_magic, detail::wire_schema_hash<T>(), detail::wire_root_type_id<T>()};
        }
        // The smallest image of T: the header and the root object.
        template <typename T>
        static constexpr std::size_t root_end = XWIRE_PAYLOAD_BEGIN + sizeof(detail::root_storage_t<T>);

        // A dense image of T: the wire is T's image with nothing else, so it
        // is copied as it is. The header's fixed words are T's, and its
        // used_bytes and extent count (one word: extent_count 0) are the
        // wire's size; the image holds the root and ends on a word. Any other
        // wire, valid or not, takes the general path (admit_wire_header),
        // which reports the first error.
        template <typename T>
        XOFFSET_DETAIL_INLINE static bool admit_image(std::span<const std::byte> data) noexcept {
            static_assert(offsetof(XWireHeader, used_bytes) == 24 && offsetof(XWireHeader, extent_count) == 28);
            static_assert(root_end<T> >= sizeof(XWireHeader));
            const std::uint64_t size = data.size_bytes();
            if (size < root_end<T> || (size & 7u) != 0) return false;
            const std::byte* at = data.data();
            XOFFSET_DETAIL_OPAQUE(at);
            std::uint64_t words[4];
            std::memcpy(words, at, sizeof(words));
            // Compared word by word with T's constants (immediates).
            constexpr auto image = image_words<T>();
            return words[0] == image[0] && words[1] == image[1] && words[2] == image[2] && words[3] == size;
        }

        // Wire admission shared by load_verified and reload_verified.
        template <typename T>
        static detail::vstatus admit_wire_header(std::span<const std::byte> data, XWireHeader& header,
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
            return detail::parse_wire_layout(header, reinterpret_cast<const char*>(data.data()), data.size_bytes(),
                                             who, layout);
        }

        // The root of a loaded image of `used` bytes (T's, or its reflected
        // wrapper's, at offset 40), or null when the image cannot hold it.
        template <typename T>
        T* loaded_root(std::size_t used) noexcept {
            if (this->arena()->used_end != used || used < root_end<T>) return nullptr;
            return detail::find_root<T>(*this);
        }

        // Typed access to the root of T that a load admitted (at root_at).
        template <typename T>
        XOFFSET_DETAIL_INLINE void publish_loaded_root(T*) noexcept {
            typed_root_access_allowed_ = true;
            this->template bind_root<T>();
        }

        // Post-copy verification shared by load_verified and reload_verified:
        // the graph walk, with the image's free blocks (the directory and
        // its stacks, walked and checked as they are registered) tiling the
        // image with the live blocks.
        template <typename T>
        detail::vstatus finish_verified_load(std::size_t used, std::pmr::memory_resource* upstream, const char* who,
                                             detail::WireValidationScratch* retained = nullptr) {
            auto* root_ptr = loaded_root<T>(used);
            if (!root_ptr) {
                XOFFSET_VFAIL(XException, XErrc::root_mismatch, std::string(who) + ": root not found");
            }
            XOFFSET_VTRY(detail::verify_loaded_image(*root_ptr, *this, upstream, retained, who));
            publish_loaded_root<T>(root_ptr);
            return detail::vok();
        }

        // ---- Trust tier admit (xoffset_receiver.hpp, XCheck::admit) ------
        // The wire parse and admission of reload_verified / load_verified
        // (header, fingerprint, extent table, capacity) and the same copy
        // into the arena, without the graph walk: only that the image holds
        // the root is checked. Everything else about the image, the free
        // blocks included, is the sender's promise: only for trusted links.
        template <typename T>
        XOFFSET_DETAIL_INLINE detail::vstatus finish_admitted_load(std::size_t used, const char* who) {
            auto* root_ptr = loaded_root<T>(used);
            if (!root_ptr) [[unlikely]] {
                XOFFSET_VFAIL(XException, XErrc::root_mismatch, std::string(who) + ": root not found");
            }
            publish_loaded_root<T>(root_ptr);
            return detail::vok();
        }

        // A dense image of T in place (each message of a stream): the header
        // compare, one copy and the stores that publish T's root at offset
        // 40, inline in the caller. Everything else takes the general path.
        template <typename T>
        XOFFSET_DETAIL_INLINE detail::vstatus reload_admit_v(std::span<const std::byte> data) {
            const std::size_t size = data.size_bytes();
            if (admit_image<T>(data) && this->can_reload_image_in_place(size)) [[likely]] {
                if (!this->reload_image_in_place(reinterpret_cast<const char*>(data.data()), size)) [[unlikely]]
                    return reload_storage_failed();
                publish_loaded_root<T>(detail::image_root<T>(*this));
                return detail::vok();
            }
            return reload_admit_general<T>(data);
        }

        // An injected storage failure of reload_image_in_place: the buffer
        // is left empty, as after any failed reload.
        XOFFSET_DETAIL_COLD detail::vstatus reload_storage_failed() {
            reset_arena(0);
            XOFFSET_VFAIL(XBadAlloc, XErrc::out_of_storage, detail::arena_failure_message);
        }

        template <typename T>
        XOFFSET_DETAIL_NOINLINE detail::vstatus reload_admit_general(std::span<const std::byte> data) {
            constexpr const char* who = "XReceiver::reload<T, admit>()";
            XOFFSET_ROLLBACK_BEGIN
                XWireHeader header{};
                detail::WireLayout layout;
                XOFFSET_VTRY(admit_wire_header<T>(data, header, who, layout));
                XOFFSET_VTRY(reinit_from_wire({layout.stored, layout.extents, layout.extent_count}, header.used_bytes));
                XOFFSET_VTRY(finish_admitted_load<T>(header.used_bytes, who));
            XOFFSET_ROLLBACK_END(reset_arena(0))
            return detail::vok();
        }

        // load_verified_v with the admit check: into this default-constructed buffer.
        template <typename T>
        detail::vstatus load_admit_v(std::span<const std::byte> data, MaximumCapacity cap) {
            constexpr const char* who = "XReceiver::load<T, admit>()";
            XWireHeader header{};
            detail::WireLayout layout;
            XOFFSET_VTRY(admit_wire_header<T>(data, header, who, layout));
            const auto used = static_cast<std::size_t>(header.used_bytes);
            XOFFSET_VTRY(detail::require_wire_within_capacity(used, cap.value, true, who));
            XOFFSET_VTRY(init_wire_v(WirePayload{layout.stored, layout.extents, layout.extent_count}, used, cap));
            return finish_admitted_load<T>(used, who);
        }

        void require_typed_root_access(const char* message) const {
            if (!typed_root_access_allowed_) {
                XOFFSET_THROW(XException(XErrc::no_root, message));
            }
        }

        // T's fingerprint in the image header (only a type with a schema name
        // has one, and only such a type is sent). make<T>() writes it; a
        // buffer whose root key is such a T holds it (make<T>(), a load or
        // delta of T, a clone), so a save copies the image as it is.
        template <typename T>
        void write_fingerprint() noexcept {
            auto* sm = this->arena();
            sm->schema_hash = detail::wire_schema_hash<T>();
            sm->root_type_id = detail::wire_root_type_id<T>();
        }
        template <typename T>
        bool holds_fingerprint() const noexcept {
            using U = std::remove_cv_t<T>;
            if constexpr (has_schema_name_v<U>) {
                const auto* sm = this->arena();
                return sm && sm->schema_hash == detail::wire_schema_hash<U>() &&
                       sm->root_type_id == detail::wire_root_type_id<U>();
            } else {
                return true;
            }
        }

        template <typename T>
        void require_schema_match(const char* message) const {
            if (this->root_named() && !matches_schema<T>()) {
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
            : buffer_(std::exchange(other.buffer_, nullptr)), table_(other.table_), used_(other.used_),
              extents_(other.extents_), clean_from_(other.clean_from_), received_(other.received_),
              complete_(other.complete_) {}
        VerifiedReloadToken& operator=(VerifiedReloadToken&&) = delete;
        VerifiedReloadToken(const VerifiedReloadToken&) = delete;
        VerifiedReloadToken& operator=(const VerifiedReloadToken&) = delete;
        ~VerifiedReloadToken() { if (buffer_) abort(); }

        explicit operator bool() const noexcept { return buffer_ != nullptr; }
        // The image: [0, used_bytes) of the buffer, its header first.
        std::span<std::byte> image_bytes() const noexcept {
            return buffer_ ? std::span<std::byte>(reinterpret_cast<std::byte*>(buffer_->data()), used_)
                           : std::span<std::byte>{};
        }
        std::span<std::byte> extent_bytes() const noexcept {
            return buffer_ ? std::span<std::byte>(table_, extents_ * 8u) : std::span<std::byte>{};
        }
        // image_bytes counts the image's bytes including sparse gaps
        // (used_bytes).
        void complete(std::size_t image_bytes, std::size_t extent_bytes = 0) noexcept {
            received_ = {image_bytes, extent_bytes};
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
                if (!complete_ || received_[0] != used_ || received_[1] != extents_ * 8u)
                    XOFFSET_VFAIL(XException, XErrc::incomplete_input, "verified reload: incomplete input");
                // admit_wire_header and parse_wire_layout, with the lengths
                // fixed by begin instead of read from one wire.
                auto* base = buffer->data();
                XWireHeader header;
                XOFFSET_VTRY(detail::read_wire_header(base, used_, who, header));
                if (header.root_type_id != detail::wire_root_type_id<T>())
                    XOFFSET_VFAIL(XException, XErrc::root_type_mismatch, std::string(who) + ": root type id mismatch");
                if (header.schema_hash != detail::wire_schema_hash<T>())
                    XOFFSET_VFAIL(XException, XErrc::schema_mismatch, std::string(who) + ": schema hash mismatch");
                XOFFSET_VTRY(detail::check_wire_extent(header, who));
                if (header.used_bytes != used_ || header.extent_count != extents_)
                    XOFFSET_VFAIL(XException, XErrc::size_mismatch, std::string(who) + ": used size mismatch");
                std::vector<XBufferCore::WireHole> gaps;
                std::uint64_t stored;
                XOFFSET_VTRY(detail::parse_extent_table(reinterpret_cast<const char*>(table_), extents_,
                                                        header.used_bytes, who, gaps, stored));
                // Gaps read as zero, as in assign_extents: clear what an
                // earlier content may have left below clean_from_.
                for (const auto& gap : gaps) {
                    const auto end = std::min<std::size_t>(std::size_t(gap.offset) + gap.bytes, clean_from_);
                    if (gap.offset < end) std::memset(base + gap.offset, 0, end - gap.offset);
                }
                buffer->arena()->extent_count = 0;
                buffer->take_loaded_free_state();
                XOFFSET_VTRY(buffer->check_reinit_payload());
                XOFFSET_VTRY(buffer->template finish_verified_load<T>(used_, std::pmr::new_delete_resource(),
                                                                      who, &buffer->retained_reload_scratch()));
            XOFFSET_ROLLBACK_END(buffer->reset_arena(0))
            return detail::vok();
        }

        friend class XBuffer;
        VerifiedReloadToken(XBuffer& buffer, std::byte* table, std::size_t used, std::size_t extents,
                            std::size_t clean_from) noexcept
            : buffer_(&buffer), table_(table), used_(used), extents_(extents), clean_from_(clean_from) {}
        void abort() noexcept {
            auto* buffer = std::exchange(buffer_, nullptr);
            buffer->reset_arena(0);
            buffer->direct_reload_open_ = false;
        }

        XBuffer* buffer_;
        std::byte* table_;
        std::size_t used_, extents_, clean_from_;
        std::array<std::size_t, 2> received_{};
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
        XOFFSET_DETAIL_INLINE XDeltaStats save_delta_into(std::string& out, XDeltaBaseline& baseline,
                                                          XDeltaOptions options = {}) {
            return XBuffer::save_delta_into<T>(out, baseline, options);
        }
        XOFFSET_DETAIL_INLINE std::size_t save_delta_into(std::span<char> out, XDeltaBaseline& baseline,
                                                          XDeltaOptions options = {}) {
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
