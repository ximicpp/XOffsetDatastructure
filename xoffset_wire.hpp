#ifndef X_OFFSET_WIRE_HPP
#define X_OFFSET_WIRE_HPP

// The wire: its header and extent table, the emission forms, the save and
// delta options, and validation of a received wire against its type. Part
// of xoffsetdatastructure.hpp; include that header. Format: docs/WIRE_FORMAT.md;
// what validation guarantees: docs/V2_THREAT_MODEL.md.

#include "xoffset_containers.hpp"

namespace XOffsetDatastructure {

    namespace detail {
        template <typename T>
        inline constexpr bool is_fixed_string_v =
            std::is_base_of_v<XString, std::remove_cv_t<T>>;

        template <typename T, typename = void>
        struct is_fixed_vector : std::false_type {};
        template <typename T>
        struct is_fixed_vector<T, std::void_t<typename std::remove_cv_t<T>::value_type>>
            : std::bool_constant<
                std::is_base_of_v<
                    XVectorCore<typename std::remove_cv_t<T>::value_type>,
                    std::remove_cv_t<T>>> {};
        template <typename T>
        inline constexpr bool is_fixed_vector_v = is_fixed_vector<T>::value;

        template <typename T, typename = void>
        struct is_fixed_flat_set : std::false_type {};
        template <typename T>
        struct is_fixed_flat_set<T, std::void_t<typename std::remove_cv_t<T>::value_type>>
            : std::bool_constant<
                std::is_base_of_v<
                    XSet<typename std::remove_cv_t<T>::value_type>,
                    std::remove_cv_t<T>>> {};
        template <typename T>
        inline constexpr bool is_fixed_flat_set_v = is_fixed_flat_set<T>::value;

        template <typename T, typename = void>
        struct is_fixed_flat_map : std::false_type {};
        template <typename T>
        struct is_fixed_flat_map<T, std::void_t<typename std::remove_cv_t<T>::key_type,
                                                typename std::remove_cv_t<T>::mapped_type>>
            : std::bool_constant<
                std::is_base_of_v<
                    XMap<typename std::remove_cv_t<T>::key_type,
                                  typename std::remove_cv_t<T>::mapped_type>,
                    std::remove_cv_t<T>>> {};
        template <typename T>
        inline constexpr bool is_fixed_flat_map_v = is_fixed_flat_map<T>::value;

        template <typename T>
        struct is_wire_admitted_impl;

        template <typename T, std::size_t N>
        consteval bool reflected_member_wire_admitted() {
            using member = schema::member_t<T, N>;
            using ReflectedMemberType = typename member::type;
            using MemberType = std::remove_cv_t<ReflectedMemberType>;
            return is_wire_admitted_impl<MemberType>::value;
        }

        template <typename T, std::size_t N = 0>
        consteval bool reflected_members_wire_admitted_impl() {
            if constexpr (N >= schema::member_count_v<T>) {
                return true;
            } else {
                return reflected_member_wire_admitted<T, N>() &&
                    reflected_members_wire_admitted_impl<T, N + 1>();
            }
        }

        template <typename T>
        consteval bool reflected_members_wire_admitted() {
            return reflected_members_wire_admitted_impl<T>();
        }

        // A function template rather than a lambda in the static member's
        // initializer: MSVC instantiates the discarded if-constexpr branches of
        // a non-generic lambda (C2825 on CleanT::key_type for CleanT = char).
        template <typename CleanT>
        consteval bool compute_wire_admitted() {
            if constexpr (!is_byte_copy_safe_v<CleanT>) {
                return false;
            } else if constexpr (std::is_reference_v<CleanT> ||
                                 std::is_pointer_v<CleanT> ||
                                 std::is_member_pointer_v<CleanT> ||
                                 std::is_union_v<CleanT>) {
                return false;
            } else if constexpr (std::is_array_v<CleanT>) {
                return is_wire_admitted_impl<std::remove_extent_t<CleanT>>::value;
            } else if constexpr (fixed_size_scalar<CleanT>) {
                return true;
            } else if constexpr (std::is_enum_v<CleanT>) {
                return fixed_size_scalar<std::underlying_type_t<CleanT>>;
            } else if constexpr (is_fixed_string_v<CleanT>) {
                return std::is_same_v<CleanT, XString>;
            } else if constexpr (is_fixed_flat_map_v<CleanT>) {
                return std::is_same_v<CleanT, XMap<typename CleanT::key_type, typename CleanT::mapped_type>> &&
                    is_wire_admitted_impl<typename CleanT::key_type>::value &&
                    is_wire_admitted_impl<typename CleanT::mapped_type>::value;
            } else if constexpr (is_fixed_flat_set_v<CleanT>) {
                return std::is_same_v<CleanT, XSet<typename CleanT::value_type>> &&
                    is_wire_admitted_impl<typename CleanT::value_type>::value;
            } else if constexpr (is_fixed_vector_v<CleanT>) {
                return std::is_same_v<CleanT, XVector<typename CleanT::value_type>> &&
                    is_wire_admitted_impl<typename CleanT::value_type>::value;
            } else if constexpr (std::is_class_v<CleanT>) {
                return alignof(CleanT) <= 8 &&
                    std::is_standard_layout_v<CleanT> &&
                    !std::is_polymorphic_v<CleanT> &&
                    schema::base_count_v<CleanT> == 0 &&
                    reflected_members_wire_admitted<CleanT>();
            } else {
                return false;
            }
        }

        template <typename T>
        struct is_wire_admitted_impl {
            static constexpr bool value = compute_wire_admitted<std::remove_cv_t<T>>();
        };

        template <typename T>
        consteval bool wire_admitted() {
            return is_wire_admitted_impl<std::remove_cvref_t<T>>::value;
        }

        // Validation errors leave the hot path through one out-of-line call.
        XOFFSET_VNORETURN XOFFSET_DETAIL_COLD inline vstatus wire_reject(const char* what) {
            XOFFSET_VFAIL(XException, XErrc::invalid_payload, what);
        }

        // Retained validation words of one long-lived reload buffer: the
        // bitmaps of payloads above WireGranuleIndex::inline_extent reuse
        // them message after message instead of a fresh host block. They grow
        // to the largest payload validated and are released with the buffer
        // that owns them (or by XBuffer::release_reload_scratch()). The
        // validator clears every word it reads, exactly as on fresh memory,
        // and the words never reach the payload or the wire.
        class WireValidationScratch {
        public:
            WireValidationScratch() = default;
            WireValidationScratch(const WireValidationScratch&) = delete;
            WireValidationScratch& operator=(const WireValidationScratch&) = delete;
            ~WireValidationScratch() { release(); }
            std::size_t capacity() const noexcept { return capacity_; }
            std::uint64_t* words(std::size_t count) {
                if (count > capacity_) {
                    auto* fresh = new std::uint64_t[count];
                    delete[] words_; // the tables may be in use (direct reload)
                    words_ = fresh;
                    capacity_ = count;
                }
                return words_;
            }
            // The extent table of a direct reload.
            std::byte* tables(std::size_t bytes) {
                if (bytes > tables_.size()) tables_.resize(bytes);
                return tables_.data();
            }
            void release(bool tables = true) noexcept {
                delete[] words_;
                words_ = nullptr;
                capacity_ = 0;
                if (tables) std::vector<std::byte>().swap(tables_);
            }
        private:
            std::uint64_t* words_ = nullptr;
            std::size_t capacity_ = 0;
            std::vector<std::byte> tables_;
        };

        // Validation index: one bit per 8-byte granule of the payload, one
        // bitmap for live blocks and one for holes. Every range the validator
        // registers is granule aligned: a block is 8-byte aligned with its
        // length rounded up to 8 (register_allocation), and the arena header
        // and every hole are multiples of 8 at multiples of 8. A granule bit
        // therefore records exactly the byte membership that a byte index
        // would, and every overlap, coverage and gap answer is the same.
        //
        // Payloads up to inline_extent use words inside the index (no host
        // memory). Larger ones use a reload buffer's retained words or one
        // block from the upstream resource, returned when the index dies.
        // The size follows the payload extent, which the caller has already
        // bounded by its maximum capacity, never a wire count. Both bitmaps
        // are cleared once, on the first registration.
        class WireGranuleIndex {
        public:
            static constexpr std::size_t inline_words = 64;              // per bitmap
            static constexpr std::size_t inline_extent = inline_words * 64 * 8; // 32 KiB
            WireGranuleIndex() noexcept {} // inline words stay uninitialized until prepare()
            WireGranuleIndex(const WireGranuleIndex&) = delete;
            WireGranuleIndex& operator=(const WireGranuleIndex&) = delete;
            WireGranuleIndex(WireGranuleIndex&&) = delete;
            WireGranuleIndex& operator=(WireGranuleIndex&&) = delete;
            ~WireGranuleIndex() {
                if (block_) block_upstream_->deallocate(block_, block_words_ * sizeof(std::uint64_t),
                                                        alignof(std::uint64_t));
            }

            bool prepared() const noexcept { return owned_ != nullptr; }
            // Granules of the payload; a trailing partial granule (extent not
            // a multiple of 8) is never registered and never free.
            std::size_t granules() const noexcept { return granules_; }
            // Host words this index took from outside itself (0: inline or retained).
            std::size_t upstream_words() const noexcept { return block_words_; }

            void prepare(std::size_t extent, XMemoryResource* upstream,
                         WireValidationScratch* retained) {
                const auto granules = extent / 8u;
                const auto words = (granules + 63u) / 64u;
                std::uint64_t* storage;
                if (words <= inline_words) {
                    storage = inline_;
                } else if (retained) {
                    storage = retained->words(2 * words);
                } else {
                    if (!upstream) XOFFSET_THROW(std::invalid_argument("validation scratch: null upstream"));
                    storage = static_cast<std::uint64_t*>(
                        upstream->allocate(2 * words * sizeof(std::uint64_t), alignof(std::uint64_t)));
                    block_ = storage;
                    block_words_ = 2 * words;
                    block_upstream_ = upstream;
                }
                if (words) std::memset(storage, 0, 2 * words * sizeof(std::uint64_t));
                owned_ = storage;
                free_ = storage + words;
                granules_ = granules;
            }

            // [g0, g1) nonempty, g1 <= granules(). 0 none, 1 live, 2 free. A
            // live hit anywhere in the range wins over a free hit (a free
            // region's check). One word inline, more out of line (the callers
            // stay small).
            XOFFSET_DETAIL_INLINE unsigned overlap(std::size_t g0, std::size_t g1) const noexcept {
                const auto w0 = g0 / 64u, w1 = (g1 - 1u) / 64u;
                if (w0 == w1) [[likely]] {
                    const auto m = (~std::uint64_t(0) << (g0 % 64u)) & (~std::uint64_t(0) >> (63u - (g1 - 1u) % 64u));
                    if (owned_[w0] & m) return 1;
                    return (free_[w0] & m) ? 2 : 0;
                }
                return overlap_words(g0, g1);
            }
            // True when a granule of [g0, g1) (nonempty, g1 <= granules()) is
            // live. A live block is checked against the live blocks only: the
            // free regions are registered after every live block, each checked
            // against both (verify_structural_wire_graph_with).
            XOFFSET_DETAIL_INLINE bool live_hit(std::size_t g0, std::size_t g1) const noexcept {
                const auto w0 = g0 / 64u, w1 = (g1 - 1u) / 64u;
                if (w0 == w1) [[likely]]
                    return (owned_[w0] & (~std::uint64_t(0) << (g0 % 64u)) &
                            (~std::uint64_t(0) >> (63u - (g1 - 1u) % 64u))) != 0;
                return live_hit_words(g0, g1);
            }
            XOFFSET_DETAIL_NOINLINE bool live_hit_words(std::size_t g0, std::size_t g1) const noexcept {
                const auto w0 = g0 / 64u, w1 = (g1 - 1u) / 64u;
                if (owned_[w0] & (~std::uint64_t(0) << (g0 % 64u))) return true;
                for (auto w = w0 + 1u; w < w1; ++w)
                    if (owned_[w]) return true;
                return (owned_[w1] & (~std::uint64_t(0) >> (63u - (g1 - 1u) % 64u))) != 0;
            }
            XOFFSET_DETAIL_NOINLINE unsigned overlap_words(std::size_t g0, std::size_t g1) const noexcept {
                const auto w0 = g0 / 64u, w1 = (g1 - 1u) / 64u;
                const auto first = ~std::uint64_t(0) << (g0 % 64u);
                const auto last = ~std::uint64_t(0) >> (63u - (g1 - 1u) % 64u);
                if (owned_[w0] & first) return 1;
                std::uint64_t dead = free_[w0] & first;
                for (auto w = w0 + 1u; w < w1; ++w) {
                    if (owned_[w]) return 1;
                    dead |= free_[w];
                }
                if (owned_[w1] & last) return 1;
                dead |= free_[w1] & last;
                return dead ? 2 : 0;
            }

            XOFFSET_DETAIL_INLINE void mark(bool free, std::size_t g0, std::size_t g1) noexcept {
                auto* bits = free ? free_ : owned_;
                const auto w0 = g0 / 64u, w1 = (g1 - 1u) / 64u;
                if (w0 == w1) [[likely]] {
                    bits[w0] |= (~std::uint64_t(0) << (g0 % 64u)) & (~std::uint64_t(0) >> (63u - (g1 - 1u) % 64u));
                    return;
                }
                mark_words(bits, g0, g1);
            }
            XOFFSET_DETAIL_NOINLINE static void mark_words(std::uint64_t* bits, std::size_t g0, std::size_t g1) noexcept {
                const auto w0 = g0 / 64u, w1 = (g1 - 1u) / 64u;
                const auto first = ~std::uint64_t(0) << (g0 % 64u);
                const auto last = ~std::uint64_t(0) >> (63u - (g1 - 1u) % 64u);
                bits[w0] |= first;
                for (auto w = w0 + 1u; w < w1; ++w) bits[w] = ~std::uint64_t(0);
                bits[w1] |= last;
            }

            // Marks [g0, g1) free unless one of its granules is free or live
            // already (false, nothing marked).
            XOFFSET_DETAIL_INLINE bool mark_free_disjoint(std::size_t g0, std::size_t g1) noexcept {
                const auto w0 = g0 / 64u, w1 = (g1 - 1u) / 64u;
                if (w0 == w1) [[likely]] {
                    const auto m = (~std::uint64_t(0) << (g0 % 64u)) & (~std::uint64_t(0) >> (63u - (g1 - 1u) % 64u));
                    if ((free_[w0] | owned_[w0]) & m) [[unlikely]] return false;
                    free_[w0] |= m;
                    return true;
                }
                return mark_free_words(g0, g1);
            }
            // mark_free_disjoint over several words: one pass checks, one marks.
            XOFFSET_DETAIL_NOINLINE bool mark_free_words(std::size_t g0, std::size_t g1) noexcept {
                const auto w0 = g0 / 64u, w1 = (g1 - 1u) / 64u;
                const auto first = ~std::uint64_t(0) << (g0 % 64u);
                const auto last = ~std::uint64_t(0) >> (63u - (g1 - 1u) % 64u);
                if ((free_[w0] | owned_[w0]) & first) return false;
                for (auto w = w0 + 1u; w < w1; ++w)
                    if (free_[w] | owned_[w]) return false;
                if ((free_[w1] | owned_[w1]) & last) return false;
                free_[w0] |= first;
                for (auto w = w0 + 1u; w < w1; ++w) free_[w] = ~std::uint64_t(0);
                free_[w1] |= last;
                return true;
            }

            bool member(bool free, std::size_t granule) const noexcept {
                // The pointer is chosen first: GCC 13 at -O0 with UBSan
                // miscompiles the conditional used directly as the array.
                const std::uint64_t* bits = free ? free_ : owned_;
                return (bits[granule / 64u] >> (granule % 64u)) & 1u;
            }

        private:
            std::uint64_t* owned_ = nullptr;
            std::uint64_t* free_ = nullptr;
            std::size_t granules_ = 0;
            std::uint64_t* block_ = nullptr;
            std::size_t block_words_ = 0;
            XMemoryResource* block_upstream_ = nullptr;
            std::uint64_t inline_[2 * inline_words];
        };

        // Test switch for the cursor path below (differential on/off runs).
        inline constinit bool wire_validation_cursor = true;
        // Test switch for XBuffer's small dense reload path (differential on/off runs).
        inline constinit bool small_reload_fast_path = true;

        // default_memory_resource(), looked up once: verified reloads
        // name it on every read, and the lookup may be an out-of-line call.
        inline XMemoryResource* default_wire_upstream() noexcept {
            static XMemoryResource* const resource = default_memory_resource();
            return resource;
        }

#if defined(XOFFSET_VALIDATION_LAYOUT_STATS)
        // Diagnostic build only: layout classes, per root type, of the wires
        // that pass verified validation, printed to stderr at exit.
        // canonical: the cursor path validated the whole graph. ascending:
        // live blocks in visiting order at rising offsets, with holes between
        // them. out_of_order: some block starts below the end of the previous
        // one (first_offset_sum adds the payload offset of the first such
        // block, out_of_order_blocks counts them all).
        struct WireLayoutStats {
            const char* type = nullptr;
            WireLayoutStats* next = nullptr;
            std::uint64_t canonical = 0, ascending = 0, out_of_order = 0;
            std::uint64_t out_of_order_blocks = 0, first_offset_sum = 0;
        };
        inline WireLayoutStats* wire_layout_stats_list = nullptr;
        struct WireLayoutStatsReport {
            ~WireLayoutStatsReport() {
                for (auto* s = wire_layout_stats_list; s; s = s->next)
                    std::fprintf(stderr,
                        "xoffset layout stats %s: canonical %llu ascending %llu out_of_order %llu"
                        " out_of_order_blocks %llu first_offset_sum %llu\n", s->type,
                        (unsigned long long)s->canonical, (unsigned long long)s->ascending,
                        (unsigned long long)s->out_of_order, (unsigned long long)s->out_of_order_blocks,
                        (unsigned long long)s->first_offset_sum);
            }
        };
        inline WireLayoutStatsReport wire_layout_stats_report;
        template <typename T> inline WireLayoutStats wire_layout_stats_of{};
        template <typename T> WireLayoutStats& wire_layout_stats() {
            auto& s = wire_layout_stats_of<T>;
            if (!s.type) {
                s.type = typeid(T).name();
                s.next = wire_layout_stats_list;
                wire_layout_stats_list = &s;
            }
            return s;
        }
#endif

        struct WireValidationContext {
            static constexpr std::size_t no_cursor = ~std::size_t(0);
            const char* base{};
            const char* end{};
            XArena* segment{};
            // Exact coverage: bytes registered as live and as holes. Ranges
            // never overlap (rejected on insert), so equality with used_end
            // proves the payload is tiled exactly.
            std::uint64_t owned_bytes = 0;
            std::uint64_t free_bytes = 0;
            // Host-only scratch for bitmaps above the inline size: a reload
            // buffer's retained words when set, else one upstream block.
            XMemoryResource* upstream = default_wire_upstream();
            WireValidationScratch* retained = nullptr;
            // Cursor path: while no hole is registered and every live block
            // starts where the previous one ended (canonical preorder), the
            // live set is exactly [0, cursor) and no bitmap is touched. At the
            // first block elsewhere, or the first hole, the live bitmap is set
            // to [0, cursor) (the state the bitmaps would have reached) and
            // validation continues on the bitmaps, so accept/reject and the
            // first error are unchanged. no_cursor: bitmaps only.
            std::size_t cursor = wire_validation_cursor ? 0 : no_cursor;
            WireGranuleIndex granules;
#if defined(XOFFSET_VALIDATION_LAYOUT_STATS)
            std::size_t stats_end = 0, stats_first = no_cursor;
            std::uint64_t stats_out_of_order = 0;
#endif

            std::size_t extent() const noexcept {
                return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(end) -
                                                reinterpret_cast<std::uintptr_t>(base));
            }
            // The bitmaps with the cursor's live range folded in. The only
            // place that prepares them, so prepared() implies cursor ==
            // no_cursor (register_owned_bitmap relies on it).
            WireGranuleIndex& general() {
                auto& bits = granules;
                if (!bits.prepared()) bits.prepare(extent(), upstream, retained);
                if (cursor != no_cursor) {
                    if (cursor) bits.mark(false, 0, cursor / 8u);
                    cursor = no_cursor;
                }
                return bits;
            }
        };

        XOFFSET_DETAIL_INLINE vstatus validate_storage_range(const void* ptr, std::size_t bytes,
                                           std::size_t alignment,
                                           const WireValidationContext& ctx,
                                           const char* what) {
            if (bytes == 0) return vok();
            if (!ptr) {
                return wire_reject(what);
            }
            auto begin = reinterpret_cast<std::uintptr_t>(ptr);
            auto base = reinterpret_cast<std::uintptr_t>(ctx.base);
            auto end = reinterpret_cast<std::uintptr_t>(ctx.end);
            if (begin < base || begin > end || bytes > end - begin) {
                return wire_reject(what);
            }
            if ((reinterpret_cast<std::uintptr_t>(ptr) % alignment) != 0) {
                return wire_reject(what);
            }
            return vok();
        }

        vstatus register_owned_bitmap(std::size_t begin, std::size_t bytes, WireValidationContext& ctx);

        // Registers granules [begin, begin + bytes) of a range that is already
        // known to lie in the payload on 8-byte granules.
        XOFFSET_DETAIL_INLINE vstatus register_owned_granules(std::size_t begin, std::size_t bytes,
                                                                 WireValidationContext& ctx) {
#if defined(XOFFSET_VALIDATION_LAYOUT_STATS)
            if (begin < ctx.stats_end) {
                if (ctx.stats_first == WireValidationContext::no_cursor) ctx.stats_first = begin;
                ++ctx.stats_out_of_order;
            }
            ctx.stats_end = begin + bytes;
#endif
            if (begin == ctx.cursor) [[likely]] {
                ctx.cursor += bytes;
                ctx.owned_bytes += bytes;
                return vok();
            }
            return register_owned_bitmap(begin, bytes, ctx);
        }

        vstatus register_owned_general(std::size_t begin, std::size_t bytes, WireValidationContext& ctx);

        // Bitmap registration, kept out of line so that the validators the
        // cursor path inlines into stay small. A range within one bitmap word
        // that meets no live block is marked here with no further call (prepared
        // bitmaps hold the cursor's range already: see general()); the first
        // registration off the cursor, a longer range and every rejection go
        // through register_owned_general.
        XOFFSET_DETAIL_NOINLINE inline vstatus register_owned_bitmap(std::size_t begin, std::size_t bytes,
                                                               WireValidationContext& ctx) {
            auto& index = ctx.granules;
            const auto g0 = begin / 8u, g1 = (begin + bytes) / 8u;
            if (index.prepared() && g0 / 64u == (g1 - 1u) / 64u && !index.live_hit(g0, g1)) [[likely]] {
                index.mark(false, g0, g1);
                ctx.owned_bytes += bytes;
                return vok();
            }
            return register_owned_general(begin, bytes, ctx);
        }

        XOFFSET_DETAIL_NOINLINE inline vstatus register_owned_general(std::size_t begin, std::size_t bytes,
                                                                WireValidationContext& ctx) {
            auto& index = ctx.general();
            const auto g0 = begin / 8u, g1 = (begin + bytes) / 8u;
            if (index.live_hit(g0, g1)) return wire_reject("wire validation detected overlapping dynamic ranges");
            index.mark(false, g0, g1);
            ctx.owned_bytes += bytes;
            /* XBV3: owned-registered */
            return vok();
        }

        // Offset of a range that passed validate_storage_range; a range that
        // is not on 8-byte granules is rejected with `what` (the validator
        // never registers one: see WireGranuleIndex).
        inline vstatus granule_range_offset(const void* ptr, std::size_t bytes,
                                            const WireValidationContext& ctx,
                                            const char* what, std::size_t& begin) {
            begin = static_cast<std::size_t>(
                reinterpret_cast<std::uintptr_t>(ptr) -
                reinterpret_cast<std::uintptr_t>(ctx.base));
            if (((begin | bytes) & 7u) != 0) return wire_reject(what);
            return vok();
        }

        inline vstatus register_owned_range(const void* ptr, std::size_t bytes,
                                            WireValidationContext& ctx,
                                            const char* what) {
            if (bytes == 0 || !ptr) return vok();
            XOFFSET_VTRY(validate_storage_range(ptr, bytes, 1, ctx, what));
            std::size_t begin;
            XOFFSET_VTRY(granule_range_offset(ptr, bytes, ctx, what, begin));
            return register_owned_granules(begin, bytes, ctx);
        }

        inline vstatus register_free_range(const void* ptr, std::size_t bytes,
                                           WireValidationContext& ctx,
                                           const char* what) {
            if (bytes == 0 || !ptr) return vok();
            XOFFSET_VTRY(validate_storage_range(ptr, bytes, 1, ctx, what));
            std::size_t begin;
            XOFFSET_VTRY(granule_range_offset(ptr, bytes, ctx, what, begin));
            auto& index = ctx.general();
            const auto g0 = begin / 8u, g1 = (begin + bytes) / 8u;
            const auto overlap = index.overlap(g0, g1);
            if (overlap == 1) {
                return wire_reject(
                    "wire validation detected free-list overlap with live ranges");
            }
            if (overlap == 2) {
                return wire_reject(
                    "wire validation detected overlapping free-list ranges");
            }
            index.mark(true, g0, g1);
            ctx.free_bytes += bytes;
            /* XBV3: free-registered */
            return vok();
        }

        XOFFSET_DETAIL_INLINE vstatus checked_relative(const std::int32_t& delta,
                                                          const WireValidationContext& ctx,
                                                          const char*& ptr) {
            auto address = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(&delta)) + delta;
            ptr = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address));
            // Flat (validated range).
            return validate_storage_range(ptr, 1, 1, ctx, "wire validation: relative pointer out of bounds");
        }

        // One check for a head's out-of-line block: data_delta resolves to an
        // 8-byte aligned address with [data, data + block) inside the payload
        // (block >= 1 is the head's 8-rounded extent). When it holds, every
        // check of the full sequence (checked_relative, the payload range,
        // register_allocation's block range) passes, so the block is
        // registered directly; otherwise `data` is nullptr and the caller runs
        // that sequence, which rejects with its own first error.
        XOFFSET_DETAIL_INLINE vstatus checked_block(const std::int32_t& delta, std::size_t block,
                                                       WireValidationContext& ctx, const char*& data) {
            const auto address = static_cast<std::uintptr_t>(
                static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(&delta)) + delta);
            const auto base = reinterpret_cast<std::uintptr_t>(ctx.base);
            const auto end = reinterpret_cast<std::uintptr_t>(ctx.end);
            if (address < base || address > end || block > end - address || (address & 7u) != 0 ||
                ((address - base) & 7u) != 0) [[unlikely]] {
                data = nullptr;
                return vok();
            }
            XOFFSET_VTRY(register_owned_granules(static_cast<std::size_t>(address - base), block, ctx));
            data = reinterpret_cast<const char*>(address);
            return vok();
        }

        // A block has no header. Its extent is derived from its owner:
        // 8-byte aligned, length rounded up to a multiple of 8.
        inline vstatus register_allocation(const void* ptr, std::size_t bytes,
                                           std::size_t alignment, WireValidationContext& ctx,
                                           bool free = false) {
            const auto block = (bytes + 7u) & ~std::size_t(7u);
            XOFFSET_VTRY(validate_storage_range(ptr, block, std::max<std::size_t>(alignment, 8u), ctx,
                                                "wire validation: allocation out of bounds"));
            if (free) return register_free_range(ptr, block, ctx, "wire validation: invalid free block");
            else return register_owned_range(ptr, block, ctx, "wire validation: invalid live block");
        }

        // One free region whose range is known valid (a nonempty run of
        // 8-byte granules inside the payload) into the bitmaps of
        // ctx.general().
        XOFFSET_DETAIL_INLINE vstatus register_wire_hole(XBufferCore::WireHole hole, WireGranuleIndex& index,
                                                         WireValidationContext& ctx) {
            const auto g0 = std::size_t(hole.offset) / 8u, g1 = (std::size_t(hole.offset) + hole.bytes) / 8u;
            const auto overlap = index.overlap(g0, g1);
            if (overlap == 1) [[unlikely]]
                return wire_reject("wire validation detected free-list overlap with live ranges");
            if (overlap == 2) [[unlikely]]
                return wire_reject("wire validation detected overlapping free-list ranges");
            index.mark(true, g0, g1);
            ctx.free_bytes += hole.bytes;
            return vok();
        }

        // Free regions (a delta's hole table): each entry is a free range of
        // the image.
        inline vstatus register_wire_holes(const void* holes, std::size_t count,
                                           WireValidationContext& ctx) {
            // A valid entry is a nonempty range on 8-byte granules inside the
            // payload, so register_free_range's own checks would pass: each
            // hole goes to the bitmaps directly, with its errors in its order.
            // The bitmaps are prepared after the first entry's check (an
            // invalid first entry is rejected with no host words taken).
            const auto used = static_cast<std::uint64_t>(ctx.end - ctx.base);
            const auto entry = [&](std::size_t i, XBufferCore::WireHole& hole) {
                std::memcpy(&hole, static_cast<const char*>(holes) + i * sizeof(hole), sizeof(hole));
                return hole.bytes != 0 && (hole.bytes & 7u) == 0 && (hole.offset & 7u) == 0 &&
                       hole.offset >= sizeof(XArena) && hole.offset <= used && hole.bytes <= used - hole.offset;
            };
            XBufferCore::WireHole hole;
            if (count == 0) return vok();
            if (!entry(0, hole)) return wire_reject("wire validation failed: invalid hole table entry");
            auto& index = ctx.general();
            for (std::size_t i = 0; i < count; ++i) {
                if (!entry(i, hole)) [[unlikely]] return wire_reject("wire validation failed: invalid hole table entry");
                XOFFSET_VTRY(register_wire_hole(hole, index, ctx));
            }
            return vok();
        }

        XOFFSET_VNORETURN XOFFSET_DETAIL_COLD inline vstatus reject_free_blocks(const char* who) {
            XOFFSET_VFAIL(XException, XErrc::invalid_payload, std::string(who) + ": invalid free blocks");
        }

        // The full check of a loaded image's free state, after the graph
        // walk registered the live blocks, each region registered in ctx's
        // bitmaps as the walk reaches it: the directory and every stack hold
        // what the allocator relies on (blocks inside the image, aligned,
        // each stack of one size, the sizes ascending, the unused entries
        // unused, a second word of zero in every free block, the free bytes
        // the header names), and no region overlaps a live block or another
        // region (a stack that loops comes back to a marked block, so every
        // walk ends). The coverage check then requires that they tile the
        // image with the live blocks.
        inline vstatus register_inband_free_regions(const XBufferCore& xbuf, WireValidationContext& ctx,
                                                    const char* who) {
            const auto* const sm = xbuf.arena();
            const std::uint32_t dir = sm->free_dir;
            if (dir == 0) {
                if (sm->free_bytes != 0) [[unlikely]] return reject_free_blocks(who);
                return vok();
            }
            const char* const base = reinterpret_cast<const char*>(sm);
            const auto word = [&](std::uint64_t at) noexcept { return XBufferCore::inband_word(base + at); };
            const std::uint64_t used = sm->used_end;
            if ((dir & 7u) != 0 || dir < XBufferCore::root_at || std::uint64_t(dir) + 8u > used) [[unlikely]]
                return reject_free_blocks(who);
            const std::uint32_t n = word(dir), cap = word(dir + 4u);
            const std::uint64_t dir_bytes = 8u + 8u * std::uint64_t(cap);  // not inband_dir_bytes: no wrap
            if (n >= cap || dir + dir_bytes > used) [[unlikely]] return reject_free_blocks(who);
            auto& index = ctx.general();
            if (!index.mark_free_disjoint(dir / 8u, (dir + dir_bytes) / 8u)) [[unlikely]] return reject_free_blocks(who);
            std::uint64_t total = 0;
            std::uint32_t previous = 0;
            for (std::uint32_t i = 0; i < cap; ++i) {
                const std::uint32_t bytes = word(dir + 8u + 8u * i);
                const std::uint32_t head = word(dir + 12u + 8u * i);
                if (i >= n) {
                    if (bytes != XBufferCore::inband_end || head != 0) [[unlikely]] return reject_free_blocks(who);
                    continue;
                }
                if (bytes <= previous || (bytes & 7u) != 0 || bytes == XBufferCore::inband_end) [[unlikely]]
                    return reject_free_blocks(who);
                previous = bytes;
                // The highest offset a block of this size may have (none: 0,
                // below every block; an empty stack of any size is valid).
                const std::uint64_t last = bytes <= used ? used - bytes : 0u;
                const std::size_t granules = bytes / 8u;
                for (std::uint64_t at = head; at != 0; at = word(at)) {
                    if ((at & 7u) != 0 || at < XBufferCore::root_at || at > last || word(at + 4u) != 0 ||
                        !index.mark_free_disjoint(at / 8u, at / 8u + granules)) [[unlikely]]
                        return reject_free_blocks(who);
                    total += bytes;
                }
            }
            if (total != sm->free_bytes) [[unlikely]] return reject_free_blocks(who);
            ctx.free_bytes += dir_bytes + total;
            return vok();
        }

        inline vstatus require_exact_coverage(const WireValidationContext& ctx) {
            if (ctx.owned_bytes + ctx.free_bytes != static_cast<std::uint64_t>(ctx.end - ctx.base)) {
                return wire_reject(
                    "wire validation failed: payload is not exactly covered by live blocks and holes");
            }
            return vok();
        }

        // Calls fn(begin, end) for each maximal run of payload bytes in no
        // live block, in ascending order (the bytes of a trailing partial
        // granule are in no block).
        template <typename Fn>
        void for_each_unowned_run(WireValidationContext& ctx, Fn&& fn) {
            const auto used = ctx.extent();
            const auto& index = ctx.general();
            std::size_t g = 0;
            const auto count = index.granules();
            while (g < count) {
                if (index.member(false, g)) { ++g; continue; }
                const auto start = g;
                while (g < count && !index.member(false, g)) ++g;
                if (g == count) { fn(start * 8u, used); return; }
                fn(start * 8u, g * 8u);
            }
            if (count * 8u < used) fn(count * 8u, used);
        }

        template <typename T>
        vstatus validate_wire_value(const T& value, WireValidationContext& ctx);
        // True means that visiting a value can reject wire bytes or validate a
        // binding/range. False is permitted only for an admitted representation
        // whose old recursive validator has no observable checks at all.
        template <typename T> struct wire_value_needs_validation;

        template <typename T, std::size_t N = 0>
        consteval bool reflected_member_needs_validation() {
            if constexpr (N >= schema::member_count_v<T>) {
                return false;
            } else {
                using member = schema::member_t<T, N>;
                using Member = typename member::type;
                return wire_value_needs_validation<Member>::value ||
                    reflected_member_needs_validation<T, N + 1>();
            }
        }
        // See compute_wire_admitted.
        template <typename CleanT>
        consteval bool compute_wire_value_needs_validation() {
            if constexpr (!wire_admitted<CleanT>()) {
                return true; // conservative for non-admitted/detail callers
            } else if constexpr (std::is_same_v<CleanT, bool>) {
                return true; // representation bytes must still be checked
            } else if constexpr (std::is_array_v<CleanT>) {
                return wire_value_needs_validation<std::remove_extent_t<CleanT>>::value;
            } else if constexpr (fixed_size_scalar<CleanT> || std::is_enum_v<CleanT>) {
                return false; // enum values were not range-checked by V1
            } else if constexpr (is_fixed_string_v<CleanT> || is_fixed_vector_v<CleanT> ||
                                 is_fixed_flat_set_v<CleanT> || is_fixed_flat_map_v<CleanT>) {
                return true; // also covers sorted-key checks and empty bindings
            } else if constexpr (std::is_class_v<CleanT>) {
                // Admitted classes have no bases. Non-admitted classes took
                // the conservative branch above, retaining their old walk.
                return reflected_member_needs_validation<CleanT>();
            } else {
                return true;
            }
        }
        template <typename T>
        struct wire_value_needs_validation {
            static constexpr bool value = compute_wire_value_needs_validation<std::remove_cv_t<T>>();
        };
        template <typename T>
        inline constexpr bool wire_value_needs_validation_v = wire_value_needs_validation<T>::value;



        // Padding bytes [n, 11) of an inline string of length n, as masks of
        // bytes 0..7 and 8..10 (little endian). A table keeps the check free
        // of length-dependent branches.
        struct InlinePadMasks {
            std::uint64_t low[XString::inline_max + 1];
            std::uint32_t high[XString::inline_max + 1];
        };
        inline constexpr InlinePadMasks inline_pad_masks = [] {
            InlinePadMasks masks{};
            for (unsigned n = 0; n <= XString::inline_max; ++n)
                for (unsigned i = n; i < 11u; ++i) {
                    if (i < 8u) masks.low[n] |= std::uint64_t(0xff) << (8u * i);
                    else masks.high[n] |= std::uint32_t(0xff) << (8u * (i - 8u));
                }
            return masks;
        }();

        // Head bytes 0..11 of an inline string (byte 11 = 0x80 | length).
        // Forced inline only without exceptions (see validate_wire_element).
        XOFFSET_DETAIL_INLINE_NO_EXCEPTIONS vstatus validate_inline_string_bytes(const char* head) {
            std::uint64_t low;   // head bytes 0..7
            std::uint32_t high;  // head bytes 8..11
            std::memcpy(&low, head, sizeof(low));
            std::memcpy(&high, head + 8, sizeof(high));
            const unsigned length = (high >> 24) & 0x7fu;
            if (length > XString::inline_max)
                return wire_reject("wire validation failed: inline XString length exceeds 10");
            // String bytes [length, 11) must be zero.
            if (((low & inline_pad_masks.low[length]) | (high & inline_pad_masks.high[length])) != 0)
                return wire_reject("wire validation failed: inline XString has nonzero padding");
            return vok();
        }

        inline vstatus validate_wire_value(const XString& value,
                                           WireValidationContext& ctx) {
            if (value.is_inline()) {
                return validate_inline_string_bytes(reinterpret_cast<const char*>(&value));
            }
            if (value.size_ > value.capacity_ || value.capacity_ >= XBufferCore::MAX_PAYLOAD) {
                return wire_reject(
                    "wire validation failed: XString size exceeds capacity");
            }
            if (value.capacity_ == 0) {
                if (value.size_ != 0 || value.data_delta != 0) {
                    return wire_reject(
                        "wire validation failed: empty XString has invalid payload");
                }
                return vok();
            }

            const auto block = fixed_detail::round_up_to_alignment(value.capacity_ + 1, 8);
            const char* data;
            XOFFSET_VTRY(checked_block(value.data_delta, block, ctx, data));
            if (!data) [[unlikely]] {
                XOFFSET_VTRY(checked_relative(value.data_delta, ctx, data));
                XOFFSET_VTRY(validate_storage_range(data, static_cast<std::size_t>(value.capacity_) + 1,
                                                    alignof(char), ctx,
                                                    "wire validation failed: XString payload out of bounds"));
                XOFFSET_VTRY(register_allocation(data, block, 8, ctx));
            }
            if (data[value.size_] != '\0') {
                return wire_reject(
                    "wire validation failed: XString missing terminator");
            }
            return vok();
        }

        // A member, array element or vector element. Without exceptions the
        // reject paths return a status instead of ending in a noreturn call,
        // and the compilers then stopped inlining the string checks into the
        // per-element loops (GCC: the demo's views spent 1.7x as long), so
        // there an inline string is checked in place and only a string with
        // a payload block calls the overload above.
        template <typename T>
        XOFFSET_DETAIL_INLINE vstatus validate_wire_element(const T& value, WireValidationContext& ctx) {
#if XOFFSET_NO_EXCEPTIONS
            if constexpr (is_fixed_string_v<std::remove_cv_t<T>>) {
                const XString& string = value;
                if (string.is_inline())
                    return validate_inline_string_bytes(reinterpret_cast<const char*>(&string));
                return validate_wire_value(string, ctx);
            } else
#endif
                return validate_wire_value(value, ctx);
        }


        template <typename T>
        vstatus validate_wire_value(const XVectorCore<T>& value,
                                    WireValidationContext& ctx) {
            if (value.size_ > value.capacity_ ||
                value.capacity_ > (XBufferCore::MAX_PAYLOAD - 8u) / sizeof(T)) {
                return wire_reject(
                    "wire validation failed: XVector size exceeds capacity");
            }
            if (value.capacity_ == 0) {
                if (value.size_ != 0 || value.data_delta != 0) {
                    return wire_reject(
                        "wire validation failed: empty XVector has invalid payload");
                }
                return vok();
            }

            const auto bytes = static_cast<std::size_t>(value.capacity_) * sizeof(T);
            const auto block = (bytes + 7u) & ~std::size_t(7u);
            const char* raw = nullptr;
            if constexpr (alignof(T) <= 8) XOFFSET_VTRY(checked_block(value.data_delta, block, ctx, raw));
            if (!raw) [[unlikely]] {
                XOFFSET_VTRY(checked_relative(value.data_delta, ctx, raw));
                XOFFSET_VTRY(validate_storage_range(raw, bytes, alignof(T), ctx,
                                                    "wire validation failed: XVector payload out of bounds"));
                XOFFSET_VTRY(register_allocation(raw, block, std::max<std::size_t>(alignof(T), 8), ctx));
            }
            auto* data = reinterpret_cast<const T*>(raw);
            if constexpr (wire_value_needs_validation_v<T>) {
                for (std::uint32_t i = 0; i < value.size_; ++i) {
                    /* XV4: checked-element-visited */
                    XOFFSET_VTRY(validate_wire_element(data[i], ctx));
                }
            } else {
                /* XV4: no-op-elements-elided */
                (void)data;
            }
            return vok();
        }

        template <typename T, std::size_t N>
        vstatus validate_base_at(const T& value, WireValidationContext& ctx) {
            using BaseType = schema::base_t<T, N>;
            return validate_wire_value(static_cast<const BaseType&>(value), ctx);
        }

        template <typename T, std::size_t N>
        vstatus validate_member_at(const T& value, WireValidationContext& ctx) {
            using member = schema::member_t<T, N>;
            using Member = typename member::type;
            if constexpr (wire_value_needs_validation_v<Member>)
                return validate_wire_element(member::get(value), ctx);
            else
                return vok();
        }

        template <typename T>
        vstatus validate_wire_value(const T& value, WireValidationContext& ctx) {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (std::is_same_v<CleanT, bool>) {
                unsigned char bits;
                std::memcpy(&bits, &value, 1);
                if (bits > 1) return wire_reject("wire validation: invalid bool representation");
            } else if constexpr (std::is_array_v<CleanT>) {
                if constexpr (wire_value_needs_validation_v<std::remove_extent_t<CleanT>>)
                    for (const auto& elem : value) XOFFSET_VTRY(validate_wire_element(elem, ctx));
            } else if constexpr (is_fixed_string_v<CleanT>) {
                return validate_wire_value(static_cast<const XString&>(value), ctx);
            } else if constexpr (is_fixed_flat_set_v<CleanT>) {
                XOFFSET_VTRY(validate_wire_value(
                    static_cast<const XVectorCore<typename CleanT::value_type>&>(value), ctx));
                // No early exit: one error, and the loop vectorizes.
                bool unordered = false;
                for (std::uint32_t i = 1; i < value.size(); ++i)
                    unordered |= !fixed_detail::ordered_less(value.data()[i - 1], value.data()[i]);
                if (unordered) return wire_reject("wire validation: set keys not strictly ordered");
            } else if constexpr (is_fixed_flat_map_v<CleanT>) {
                XOFFSET_VTRY(validate_wire_value(
                    static_cast<const XVectorCore<typename CleanT::value_type>&>(value), ctx));
                // No early exit: one error, and the loop vectorizes.
                bool unordered = false;
                for (std::uint32_t i = 1; i < value.size(); ++i)
                    unordered |= !fixed_detail::ordered_less(value.data()[i - 1].first, value.data()[i].first);
                if (unordered) return wire_reject("wire validation: map keys not strictly ordered");
            } else if constexpr (is_fixed_vector_v<CleanT>) {
                return validate_wire_value(
                    static_cast<const XVectorCore<typename CleanT::value_type>&>(value), ctx);
            } else if constexpr (std::is_class_v<CleanT> && wire_value_needs_validation_v<CleanT>) {
                return for_each_reflected_subobject_v<CleanT>(
                    [&](auto index) {
                        return validate_base_at<CleanT, decltype(index)::value>(value, ctx);
                    },
                    [&](auto index) {
                        return validate_member_at<CleanT, decltype(index)::value>(value, ctx);
                });
            }
            return vok();
        }

        // holes == nullptr && check_coverage == false walks the graph without
        // free regions (used to recompute them from the live set).
        template <typename T, typename RegisterFree>
        vstatus verify_structural_wire_graph_with(const T& root, XBufferCore& xbuf,
                                                  XMemoryResource* upstream,
                                                  RegisterFree&& register_free,
                                                  bool check_coverage,
                                                  std::vector<XBufferCore::WireHole>* recomputed,
                                                  WireValidationScratch* retained);

        template <typename T>
        vstatus verify_structural_wire_graph(const T& root, XBufferCore& xbuf,
                                             XMemoryResource* upstream,
                                             const void* holes,
                                             std::size_t hole_count,
                                             bool check_coverage = true,
                                             std::vector<XBufferCore::WireHole>* recomputed = nullptr,
                                             WireValidationScratch* retained = nullptr) {
            return verify_structural_wire_graph_with(root, xbuf, upstream,
                [&](WireValidationContext& ctx) -> vstatus {
                    if (holes && hole_count != 0) return register_wire_holes(holes, hole_count, ctx);
                    return vok();
                }, check_coverage, recomputed, retained);
        }

        // The graph walk of an image loaded with its in-band free state: the
        // directory and its stacks are checked and tile the image with the
        // live blocks (load_verified, reload_verified, a direct reload).
        template <typename T>
        vstatus verify_loaded_image(const T& root, XBufferCore& xbuf, XMemoryResource* upstream,
                                    WireValidationScratch* retained, const char* who) {
            return verify_structural_wire_graph_with(root, xbuf, upstream,
                [&](WireValidationContext& ctx) -> vstatus { return register_inband_free_regions(xbuf, ctx, who); },
                true, nullptr, retained);
        }

        // register_free(ctx) registers the free regions (after the live
        // blocks, each region checked against them as it is registered).
        template <typename T, typename RegisterFree>
        vstatus verify_structural_wire_graph_with(const T& root, XBufferCore& xbuf,
                                                  XMemoryResource* upstream,
                                                  RegisterFree&& register_free,
                                                  bool check_coverage,
                                                  std::vector<XBufferCore::WireHole>* recomputed,
                                                  WireValidationScratch* retained) {
            if (!upstream)
                XOFFSET_VFAIL_STD(std::invalid_argument, XErrc::invalid_argument, "validation scratch: null upstream");
            const auto used = xbuf.arena()->used_end;
            // used_end <= size is an allocator/load invariant: this only turns
            // a corrupted extent into an error instead of an out-of-bounds
            // validation context.
            if (used > xbuf.get_size())
                return wire_reject("wire validation: payload extent out of bounds");
            const char* validated_end = static_cast<const char*>(xbuf.get_address()) + used;
            // Default-initialized, then assigned: the granule index's inline
            // words are written by prepare() before any read. GCC 16 zeroes
            // the whole 1 KiB context for an aggregate initializer, with or
            // without [[indeterminate]], and for a default-initialized one
            // without the attribute.
#if defined(__APPLE__) && defined(__clang__)
            // Initialize the supplied resource directly rather than looking up
            // the default and immediately replacing it.
            WireValidationContext ctx{
                .base = static_cast<const char*>(xbuf.get_address()),
                .end = validated_end,
                .segment = xbuf.arena(),
                .upstream = upstream,
                .retained = retained,
            };
#else
            XOFFSET_INDETERMINATE WireValidationContext ctx;
            ctx.base = static_cast<const char*>(xbuf.get_address());
            ctx.end = validated_end;
            ctx.segment = xbuf.arena();
            ctx.upstream = upstream;
            ctx.retained = retained;
#endif
            // An image is dense: a loaded wire's extent count is cleared.
            if (ctx.segment->extent_count != 0) {
                return wire_reject("wire validation: unsupported arena header flags");
            }
            XOFFSET_VTRY(register_owned_range(ctx.base, sizeof(XArena), ctx, "wire validation: invalid arena header"));
            XOFFSET_VTRY(register_allocation(&root, sizeof(T), alignof(T), ctx));
            XOFFSET_VTRY(validate_wire_value(root, ctx));
            // The free regions after the live blocks (the graph walk's
            // cursor then covers the live blocks up to the first gap), each
            // checked against them and against each other.
            XOFFSET_VTRY(register_free(ctx));
            if (check_coverage) XOFFSET_VTRY(require_exact_coverage(ctx));
#if defined(XOFFSET_VALIDATION_LAYOUT_STATS)
            auto& stats = wire_layout_stats<T>();
            if (ctx.cursor != WireValidationContext::no_cursor) {
                ++stats.canonical;
            } else if (ctx.stats_out_of_order == 0) {
                ++stats.ascending;
            } else {
                ++stats.out_of_order;
                stats.out_of_order_blocks += ctx.stats_out_of_order;
                stats.first_offset_sum += ctx.stats_first;
            }
#endif
            if (recomputed) {
                // Every byte not covered by a live block is free.
                recomputed->clear();
                for_each_unowned_run(ctx,
                    [&](std::size_t begin, std::size_t end) {
                        while (begin < end) {
                            const auto chunk = std::min<std::size_t>(end - begin, 0x40000000u);
                            recomputed->push_back({static_cast<std::uint32_t>(begin),
                                                   static_cast<std::uint32_t>(chunk)});
                            begin += chunk;
                        }
                    });
            }
            return vok();
        }

        template <typename T>
        vstatus verify_structural_wire_graph(const T& root, XBufferCore& xbuf,
                                             XMemoryResource* upstream = default_memory_resource()) {
            // A live buffer's own free regions, the directory block too: they
            // tile the payload with the live blocks.
            std::vector<XBufferCore::WireHole> holes;
            const bool complete = xbuf.free_table_complete();
            if (complete) {
                holes.reserve(xbuf.free_region_count());
                xbuf.for_each_free_region([&](XBufferCore::WireHole hole) { holes.push_back(hole); });
            }
            return verify_structural_wire_graph(root, xbuf, upstream,
                complete ? holes.data() : nullptr, holes.size(), complete);
        }
    } // namespace detail

    namespace fixed_detail {
        template <typename T> struct is_owning_head : std::false_type {};
        template <> struct is_owning_head<XString> : std::true_type {};
        template <typename T> struct is_owning_head<XVectorCore<T>> : std::true_type {};
        template <typename T> struct is_owning_head<XVector<T>> : std::true_type {};
        template <typename T> struct is_owning_head<XSet<T>> : std::true_type {};
        template <typename K, typename V> struct is_owning_head<XMap<K, V>> : std::true_type {};

        // Whether default_construct<T> passes its allocator to anything that
        // reads it: a custom lifetime policy, or a constructor taking the
        // allocator other than a head's (which ignores it). Aggregates built
        // member by member read it when a base or member does.
        template <typename T, std::size_t... B, std::size_t... M>
        consteval bool default_construct_reads_binding_parts(std::index_sequence<B...>, std::index_sequence<M...>) {
            return (false || ... || default_construct_reads_binding<schema::base_t<T, B>>()) ||
                   (false || ... || default_construct_reads_binding<typename schema::member_t<T, M>::type>());
        }
        template <typename T>
        consteval bool default_construct_reads_binding() {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_array_v<U>) {
                return default_construct_reads_binding<std::remove_extent_t<U>>();
            } else if constexpr (is_owning_head<U>::value) {
                return false;
            } else if constexpr (requires(const allocator_ref& value) { U(value); }) {
                return true;
            } else if constexpr (!std::is_trivially_copyable_v<U> && std::is_class_v<U>) {
                return default_construct_reads_binding_parts<U>(
                    std::make_index_sequence<schema::base_count_v<U>>{},
                    std::make_index_sequence<schema::member_count_v<U>>{});
            } else {
                return false;
            }
        }

        // MV: for a string head, or a vector head whose elements are trivially
        // copyable, the generic function evaluates exactly one containment
        // test (no recursion); this is that test, inline. Every other type
        // calls the generic function, so results are identical for all T.
        template <typename T>
        XOFFSET_DETAIL_INLINE bool move_assignment_owns_address_fast(
                const T& value, const void* address) noexcept {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (std::is_trivially_copyable_v<CleanT> || std::is_array_v<CleanT> ||
                          !is_owning_head<CleanT>::value) {
                return move_assignment_owns_address(value, address);
            } else if constexpr (detail::is_fixed_string_v<CleanT>) {
                return move_assignment_contains(value.data(), value.owned_extent(), address);
            } else if constexpr (std::is_trivially_copyable_v<typename CleanT::value_type>) {
                return move_assignment_contains(value.data(),
                    std::size_t(value.capacity_) * sizeof(typename CleanT::value_type), address);
            } else {
                return move_assignment_owns_address(value, address);
            }
        }

        template <typename T, std::size_t I>
        consteval bool member_can_relocate() {
            using member = schema::member_t<T, I>;
            if constexpr (member::is_bit_field) return false;
            else {
                using Member = typename member::type;
                return can_relocate_without_allocation<Member>();
            }
        }

        template <typename T, std::size_t I>
        consteval bool base_can_relocate() {
            using Base = schema::base_t<T, I>;
            return can_relocate_without_allocation<Base>();
        }

        template <typename T>
        consteval bool can_relocate_without_allocation() {
            if constexpr (std::is_const_v<T> || !std::is_nothrow_destructible_v<T>) return false;
            else if constexpr (std::is_array_v<T>) return can_relocate_without_allocation<std::remove_extent_t<T>>();
            else if constexpr (std::is_trivially_copyable_v<T> || is_owning_head<T>::value) return true;
            else if constexpr (std::is_class_v<T> && is_byte_copy_safe_v<T>) {
                constexpr auto bases = schema::base_count_v<T>;
                constexpr auto members = schema::member_count_v<T>;
                return []<std::size_t... I>(std::index_sequence<I...>) {
                    return (base_can_relocate<T, I>() && ...);
                }(std::make_index_sequence<bases>{}) && []<std::size_t... I>(std::index_sequence<I...>) {
                    return (member_can_relocate<T, I>() && ...);
                }(std::make_index_sequence<members>{});
            } else return false;
        }

        // Detect a head embedded anywhere in the other's owned tree.
        // No allocation, object relocation, or source mutation occurs here.
        template <typename T>
        bool move_assignment_owns_address(const T& value, const void* address) noexcept {
            using CleanT = std::remove_cv_t<T>;
            if constexpr (std::is_trivially_copyable_v<CleanT>) {
                return false;
            } else if constexpr (std::is_array_v<CleanT>) {
                for (const auto& child : value) {
                    if (move_assignment_owns_address(child, address)) return true;
                }
                return false;
            } else if constexpr (is_owning_head<CleanT>::value) {
                if constexpr (detail::is_fixed_string_v<CleanT>) {
                    return move_assignment_contains(value.data(), value.owned_extent(), address);
                } else {
                    const auto* elements = value.data();
                    if (move_assignment_contains(elements,
                        std::size_t(value.capacity_) * sizeof(typename CleanT::value_type), address))
                        return true;
                    if constexpr (!std::is_trivially_copyable_v<typename CleanT::value_type>) {
                        const auto count = value.size();
                        const auto* last = elements ? elements + count : nullptr;
                        for (auto* item = elements; item != last; ++item) {
                            const auto& child = *item;
                            if (move_assignment_owns_address(child, address)) return true;
                        }
                    }
                    return false;
                }
            } else if constexpr (std::is_class_v<CleanT> &&
                                 is_byte_copy_safe_v<CleanT>) {
                bool found = false;
                detail::for_each_reflected_subobject<CleanT>([&](auto index) {
                    using Base = schema::base_t<CleanT, decltype(index)::value>;
                    if (!found) found = move_assignment_owns_address(
                        static_cast<const Base&>(value), address);
                }, [&](auto index) {
                    using member = schema::member_t<CleanT, decltype(index)::value>;
                    if constexpr (!member::is_bit_field) {
                        if (!found) found = move_assignment_owns_address(member::get(value), address);
                    }
                });
                return found;
            } else {
                return false;
            }
        }

        // Both slots must be in the same bounded arena. Only inline ownership
        // heads move; out-of-line payloads keep their addresses and ownership.
        template <typename T>
        void relocate_construct(T* dst, T& src) noexcept {
            static_assert(can_relocate_without_allocation<T>());
            if constexpr (std::is_array_v<T>) {
                for (std::size_t i = 0; i < std::extent_v<T>; ++i) relocate_construct(&(*dst)[i], src[i]);
            } else if constexpr (std::is_trivially_copyable_v<T>) {
                std::memcpy(dst, &src, sizeof(T));
            } else if constexpr (detail::is_fixed_string_v<T>) {
                std::construct_at(dst);
                src.relocate_payload_to(*dst);
            } else if constexpr (is_owning_head<T>::value) {
                std::construct_at(dst);
                set_rel32(dst->data_delta, &dst->data_delta, src.data());
                dst->size_ = src.size_;
                dst->capacity_ = src.capacity_;
                src.data_delta = 0;
                src.size_ = 0;
                src.capacity_ = 0;
            } else {
                dst = detail::begin_relocated_ordinary_outer<T>(dst, src);
                detail::for_each_reflected_subobject<T>([&](auto index) {
                    using Base = schema::base_t<T, decltype(index)::value>;
                    relocate_construct(static_cast<Base*>(dst), static_cast<Base&>(src));
                }, [&](auto index) {
                    using member = schema::member_t<T, decltype(index)::value>;
                    relocate_construct(&(member::get(*dst)), member::get(src));
                });
                detail::scrub_implicit_padding(dst);
            }
        }
    } // namespace fixed_detail

    // Customization is opt-in only. It must explicitly begin the COMPLETE T
    // lifetime at a dead destination, transfer/rebase all inline ownership
    // heads without allocating, and empty source ownership before destruction.
    // Both slots belong to the same bounded arena. Nothrow syntax is checked;
    // the semantic lifetime/ownership obligations remain the author's contract.
    template<class T> struct StableEraseRelocator { static constexpr bool enabled = false; };
    // Explicit SOURCE contract, not a reflection guess: T has only ordinary
    // non-overlapping members, no [[no_unique_address]], union or bases. The
    // mechanical gate also requires aggregate/implicit/standard layout, no
    // bases/bit-fields and recursive support. Do not specialize for a type
    // whose source layout has not been audited against these obligations.
    template<class T> struct StableEraseOrdinaryMembers : std::false_type {};

    // Mechanical part of XOFFSET_ORDINARY_RECORDS. Physical disjointness
    // catches a [[no_unique_address]] member that shares storage, but not one
    // that merely could; the declaration stays a source contract.
    namespace detail {
        template <typename T>
        consteval bool ordinary_record_has_no_bases() {
            return schema::base_count_v<T> == 0;
        }
        template <typename T>
        consteval bool ordinary_record_has_no_bit_fields() {
            return []<std::size_t... I>(std::index_sequence<I...>) {
                return (!schema::member_t<T, I>::is_bit_field && ...);
            }(std::make_index_sequence<schema::member_count_v<T>>{});
        }
        // Every pair of [offset, offset + sizeof) member ranges is disjoint.
        template <typename T>
        consteval bool ordinary_record_members_disjoint() {
            return []<std::size_t... I>(std::index_sequence<I...>) {
                constexpr std::size_t count = sizeof...(I);
                const std::size_t begin[] = {schema::member_t<T, I>::offset..., 0};
                const std::size_t end[] = {
                    (schema::member_t<T, I>::offset + sizeof(schema::member_type_t<T, I>))..., 0};
                for (std::size_t i = 0; i < count; ++i)
                    for (std::size_t j = i + 1; j < count; ++j)
                        if (begin[i] < end[j] && begin[j] < end[i]) return false;
                return true;
            }(std::make_index_sequence<schema::member_count_v<T>>{});
        }
    } // namespace detail

    namespace fixed_detail {
        // [class.prop]: an aggregate whose destructor is not user-provided is
        // implicit-lifetime, even when that destructor is nontrivial because a
        // member owns storage. This independent standard branch does not
        // specialize std traits or alter V1/K0 admission. The fixed
        // SDK's std trait is not used as a necessary condition for this branch.
        // Without std::is_implicit_lifetime (C++23) or the compiler's builtin
        // (C++20 on GCC 13, Clang 18): scalars, arrays, and classes with a
        // trivial destructor and a trivial default, copy or move constructor.
        // Callers handle aggregates with an implicit nontrivial destructor.
        template<class T> consteval bool cxx20_implicit_lifetime() {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_scalar_v<U> || std::is_array_v<U>) return true;
            else if constexpr (std::is_class_v<U>)
                return std::is_trivially_destructible_v<U> &&
                       (std::is_trivially_default_constructible_v<U> ||
                        std::is_trivially_copy_constructible_v<U> ||
                        std::is_trivially_move_constructible_v<U>);
            else return false;
        }
        template<class T> consteval bool std_implicit_lifetime() {
#if defined(__cpp_lib_is_implicit_lifetime)
            return std::is_implicit_lifetime_v<T>;
#elif defined(__clang__)
#if __has_builtin(__builtin_is_implicit_lifetime)
            return __builtin_is_implicit_lifetime(T);
#elif __has_builtin(__is_implicit_lifetime)
            return __is_implicit_lifetime(T);
#else
            return cxx20_implicit_lifetime<T>();
#endif
#else
            return cxx20_implicit_lifetime<T>();
#endif
        }
        template<class T> consteval bool stable_erase_implicit_lifetime() {
            if constexpr (std::is_const_v<T> || std::is_volatile_v<T>) return false;
            else if constexpr (std_implicit_lifetime<T>()) return true;
            else if constexpr (std::is_class_v<T> && std::is_aggregate_v<T> &&
                               StableEraseOrdinaryMembers<T>::value) {
                return schema::implicit_lifetime_aggregate<T>();
            } else return false;
        }
        template<class T> consteval bool stable_erase_supported();
        template<class T, std::size_t I> consteval bool stable_erase_member_supported() {
            using member = schema::member_t<T, I>;
            if constexpr (member::is_bit_field) return false;
            else {
                using Member = typename member::type;
                return stable_erase_supported<Member>();
            }
        }
        template<class T> consteval bool stable_erase_supported() {
            if constexpr (std::is_const_v<T> || std::is_volatile_v<T> ||
                          !can_relocate_without_allocation<T>()) return false;
            else if constexpr (StableEraseRelocator<T>::enabled) {
                return requires(T* destination, T& source) {
                    { StableEraseRelocator<T>::relocate_construct(destination, source) } noexcept -> std::same_as<void>;
                };
            } else if constexpr (is_owning_head<T>::value) return true;
            else if constexpr (!stable_erase_implicit_lifetime<T>()) return false;
            else if constexpr (std::is_array_v<T>) return stable_erase_supported<std::remove_extent_t<T>>();
            else if constexpr (std::is_scalar_v<T>) return true;
            else if constexpr (std::is_class_v<T> &&
                               StableEraseOrdinaryMembers<T>::value &&
                               std::is_aggregate_v<T> && std::is_standard_layout_v<T> &&
                               (std::is_trivially_copyable_v<T> || is_byte_copy_safe_v<T>)) {
                constexpr auto bases = schema::base_count_v<T>;
                constexpr auto members = schema::member_count_v<T>;
                // A placement into an owning base is not by itself a proof of
                // a base-subobject lifetime in a newly begun enclosing T.
                if constexpr (bases != 0) return false;
                else return []<std::size_t... I>(std::index_sequence<I...>) {
                    return (stable_erase_member_supported<T, I>() && ...);
                }(std::make_index_sequence<members>{});
            } else return false;
        }

        template<class T> void stable_erase_relocate(T* destination, T& source) noexcept;
        // The outer memcpy already began every eligible implicit subobject's
        // lifetime. Only nontrivial ownership/custom subobjects need rebuilding.
        template<class T> void stable_erase_rebase_subobjects(T* destination, T& source) noexcept {
            if constexpr (StableEraseRelocator<T>::enabled || is_owning_head<T>::value) {
                stable_erase_relocate(destination, source);
            } else if constexpr (std::is_array_v<T>) {
                for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                    stable_erase_rebase_subobjects(&(*destination)[i], source[i]);
            } else if constexpr (!std::is_trivially_copyable_v<T> && std::is_class_v<T>) {
                static_assert(schema::base_count_v<T> == 0);
                detail::for_each_reflected_subobject<T>([&](auto) {}, [&](auto index) {
                    using member = schema::member_t<T, decltype(index)::value>;
                    stable_erase_rebase_subobjects(&(member::get(*destination)), member::get(source));
                });
            }
        }
        template<class T> void stable_erase_relocate(T* destination, T& source) noexcept {
            static_assert(stable_erase_supported<T>());
            if constexpr (StableEraseRelocator<T>::enabled) {
                StableEraseRelocator<T>::relocate_construct(destination, source);
            } else if constexpr (detail::is_fixed_string_v<T>) {
                std::construct_at(destination);
                source.relocate_payload_to(*destination);
            } else if constexpr (is_owning_head<T>::value) {
                // These explicit constructors begin owning-head lifetimes even
                // when they were not created by an enclosing implicit copy.
                std::construct_at(destination);
                set_rel32(destination->data_delta, &destination->data_delta, source.data());
                destination->size_ = source.size_;
                destination->capacity_ = source.capacity_;
                source.data_delta = 0; source.size_ = 0; source.capacity_ = 0;
            } else {
                static_assert(stable_erase_implicit_lifetime<T>());
                // This copies only sizeof(T) INLINE bytes, never its dynamic
                // payload. memcpy explicitly supplies implicit object creation;
                // member-only construction would not begin a nonimplicit T.
                std::memcpy(destination, &source, sizeof(T));
                stable_erase_rebase_subobjects(destination, source);
            }
        }
    } // namespace fixed_detail

    struct MemoryStats {
        std::size_t total_size;
        std::size_t free_size;
        std::size_t used_size;
        double usage_percent() const { return total_size > 0 ? (used_size * 100.0 / total_size) : 0.0; }
    };

    inline MemoryStats memory_stats(XBufferCore& xbuf) {
        return { xbuf.get_size(), xbuf.get_free_memory(), xbuf.get_size() - xbuf.get_free_memory() };
    }

    // Wire 3.0 (docs/WIRE_FORMAT.md). A buffer's image is its payload
    // bytes [0, used_bytes): this 40-byte header, the root object at offset
    // 40 (XWIRE_PAYLOAD_BEGIN), then the other blocks. Blocks have no
    // headers; the allocator's state is in the image too: free blocks of one
    // size are linked through their first word, and a directory block holds
    // one list head per size (free_dir). Live blocks, free blocks and the
    // directory tile [40, used_bytes) exactly. A dense wire is the image
    // itself (extent_count 0). A sparse wire is the header with extent_count
    // > 0, the extent table (extent_count x {u32 offset, u32 bytes}:
    // ascending, disjoint, 8-byte multiples, the first at offset 40, inside
    // the image), then the extents' bytes; the gaps between extents load as
    // zero bytes, and the loaded image's extent_count is 0. Little-endian.
    struct alignas(8) XWireHeader {
        std::uint32_t magic;         // 'X' 'O' major minor (XWIRE_MAGIC)
        std::uint32_t header_bytes;  // 40
        std::uint64_t schema_hash;   // fingerprint: type name and ABI signature
        std::uint64_t root_type_id;  // fingerprint: type name
        std::uint32_t used_bytes;    // bytes of the image, this header included
        std::uint32_t extent_count;  // 0: dense; else the extent table's entries
        std::uint32_t free_dir;      // offset of the free directory (0: no free block)
        std::uint32_t free_bytes;    // bytes of the free blocks, the directory not counted
    };
    static_assert(sizeof(XWireHeader) == 40 && alignof(XWireHeader) == 8);
    static_assert(sizeof(XWireHeader) == sizeof(XBufferCore::arena_header) &&
                  offsetof(XWireHeader, schema_hash) == offsetof(XBufferCore::arena_header, schema_hash) &&
                  offsetof(XWireHeader, root_type_id) == offsetof(XBufferCore::arena_header, root_type_id) &&
                  offsetof(XWireHeader, used_bytes) == offsetof(XBufferCore::arena_header, used_end) &&
                  offsetof(XWireHeader, extent_count) == offsetof(XBufferCore::arena_header, extent_count) &&
                  offsetof(XWireHeader, free_dir) == offsetof(XBufferCore::arena_header, free_dir) &&
                  offsetof(XWireHeader, free_bytes) == offsetof(XBufferCore::arena_header, free_bytes));

    // Emission forms of save_verified / save_verified_into. Receivers
    // accept every form. Every form but `automatic` loads as the sender's
    // image, byte for byte.
    enum class XWireEmission : std::uint8_t {
        // Default. An image whose dead bytes make up at least an eighth of
        // it (and at least 512 bytes) goes as the compact wire of
        // save_compact (default slack): the receiver gets the same contents
        // in fewer bytes, laid out afresh. Dead bytes are the free blocks
        // (from the header: no walk), the spare capacity of the containers
        // the spare watch holds (up to 8 that shrank) and, from 16 KiB of
        // image, the spare capacity of the root object's own containers
        // beyond the slack the compact wire keeps (detail::wire_spare_scan_min).
        // Any other image goes as with free_runs.
        automatic,
        // Skips each run of at least 4 KiB of adjacent free blocks of at
        // least 1 KiB (one large deletion, for example); everything else is
        // copied (the link word of each skipped block too). Skipped bytes are
        // zero free-block bodies, so every receiver accepts it.
        free_runs,
        // One copy of the whole payload.
        dense,
        // Skips every zero run of at least 64 bytes (capacity slack, holes,
        // zero fields), for cross-machine sends. Costs a scan of the payload.
        // Payloads below 4 KiB, or where it would save less than max(256 B,
        // 1/16 of the payload), stay dense.
        zero_runs,
    };
    struct XSaveOptions {
        XWireEmission emission = XWireEmission::automatic;
    };

    // Options of save_compact / save_compact_into.
    struct XCompactOptions {
        // Vectors of at least 8 elements (not strings) keep size / slack_div
        // + 1 zeroed spare elements, so a receiver that edits them does not
        // relocate on the first insert. 0: capacity == size, byte-identical
        // to XCompactor::compact followed by save_verified (for stored or
        // read-only receivers).
        std::uint32_t slack_div = 8;
    };

    // Delta sync (xoffset_delta.hpp, docs/DELTA_SYNC.md): a sender keeps an
    // XDeltaBaseline per receiver (the state that receiver holds) and sends
    // only the payload bytes that changed since; the receiver patches its
    // copy in place, verifies it as load_verified would and keeps an
    // XDeltaCursor (which state it holds).
    class XDeltaBaseline;
    class XDeltaCursor;
    // Options of save_delta / save_delta_into.
    struct XDeltaOptions {
        // Carry the digest of the whole resulting payload; the receiver
        // rejects (and rolls back) a delta whose result differs, which also
        // catches a replica changed locally since the last delta. Costs one
        // hash pass over the payload on each side.
        bool digest = false;
        // Compare only what changed by this buffer's touch() marks: the
        // 64-byte blocks touched since the baseline's last delta from this
        // buffer (the allocator's own writes are touches too), plus the
        // bytes past the baseline's used size, instead of the whole image.
        // The sender touches everything it changes (a change it does not
        // touch is not sent). A baseline's first such delta from a buffer
        // (unless a key delta from a buffer already keeping touches came
        // before), and any after the buffer's content was replaced (reload,
        // reset, swap, a move of its storage), compares everything.
        bool touched_only = false;
        // Send each changed 64-byte block whole instead of its changed bytes
        // (a block that ends past used_end still goes by its mask): a larger
        // delta that the receiver applies with one copy per block. For key
        // deltas to receivers where apply time counts more than bytes.
        bool whole_blocks = false;
    };
    // What one save_delta_into wrote.
    struct XDeltaStats {
        bool key = false;                 // a key delta: the receiver starts from an empty payload
        bool empty = false;               // nothing changed: no delta written, version not advanced
        std::uint32_t blocks = 0;         // 64-byte payload blocks sent (those with a changed byte), or a run frame's runs
        std::uint64_t changed_bytes = 0;  // payload bytes carried in them
        std::uint64_t compared_bytes = 0; // payload bytes compared against the baseline
        std::size_t delta_bytes = 0;      // size of the delta written (0 when empty)
    };

    // One assignment of save_compact_into(out, options, xassign(target,
    // source)...): the wire is written as if `*target = *source` had been
    // done first, without doing it. target: a member subobject (any depth,
    // scalars and array elements included) of the saved buffer's root, not
    // inside a container; source: a valid value of the same type (one with
    // container heads lives in an XOffset buffer).
    template <class M>
    struct XAssign {
        using value_type = M;
        M* target;
        const M* source;
    };
    template <class M>
    XAssign<M> xassign(M& target, const M& source) noexcept { return {&target, &source}; }

    inline constexpr std::uint8_t XWIRE_FORMAT_MAJOR = XBufferCore::image_format_major;
    inline constexpr std::uint8_t XWIRE_FORMAT_MINOR = XBufferCore::image_format_minor;
    inline constexpr std::uint32_t XWIRE_MAGIC = 0x4F58u | (std::uint32_t(XWIRE_FORMAT_MAJOR) << 16) |
                                                 (std::uint32_t(XWIRE_FORMAT_MINOR) << 24);
    static_assert(XWIRE_MAGIC == 0x00034F58u &&
                  XWIRE_MAGIC == static_cast<std::uint32_t>(XBufferCore::image_magic & 0xFFFFFFFFu));
    // Payload offset of the root object, the first block: the header's size.
    inline constexpr std::size_t XWIRE_PAYLOAD_BEGIN = 40;
    static_assert(sizeof(XBufferCore::arena_header) == XWIRE_PAYLOAD_BEGIN &&
                  XBufferCore::root_at == XWIRE_PAYLOAD_BEGIN);

    namespace detail {
        XOFFSET_VNORETURN inline vstatus wire_reject(XErrc code, const char* who, const char* what) {
            XOFFSET_VFAIL(XException, code, std::string(who) + what);
        }

        // Reads the header at the front of a wire of `size` bytes and checks
        // its version. The version is told apart first, so a short or older
        // wire gets the message that names it. V1 and V2.0 wires begin
        // "XOFFV1" / "XOFFV2"; 'F' is no version byte.
        inline vstatus read_wire_header(const void* data, std::size_t size, const char* who,
                                        XWireHeader& header) {
            const auto* b = static_cast<const unsigned char*>(data);
            if (size < 4 || b[0] != 'X' || b[1] != 'O')
                return wire_reject(XErrc::bad_magic, who, ": not an XOffset wire (bad magic)");
            if (b[2] != XWIRE_FORMAT_MAJOR || b[3] != XWIRE_FORMAT_MINOR) {
                if (size >= 6 && std::memcmp(b, "XOFFV1", 6) == 0)
                    return wire_reject(XErrc::unsupported_version, who,
                                       ": wire format 1 is not supported; this build reads 3.0");
                if (size >= 6 && std::memcmp(b, "XOFFV2", 6) == 0)
                    return wire_reject(XErrc::unsupported_version, who,
                                       ": wire format 2.0 is not supported; this build reads 3.0 "
                                       "(sender and receiver must upgrade together)");
                XOFFSET_VFAIL(XException, XErrc::unsupported_version, std::string(who) + ": wire format " +
                              std::to_string(b[2]) + "." + std::to_string(b[3]) +
                              " is not supported; this build reads 3.0 (sender and receiver must upgrade together)");
            }
            if (size < sizeof(XWireHeader)) return wire_reject(XErrc::truncated, who, ": truncated wire header");
            std::memcpy(&header, data, sizeof(header));
            if (header.header_bytes != sizeof(XWireHeader)) {
                std::string hex = "0x";
                for (int shift = 28; shift >= 0; shift -= 4)
                    hex += "0123456789abcdef"[(header.header_bytes >> shift) & 15u];
                XOFFSET_VFAIL(XException, XErrc::unsupported_flags, std::string(who) + ": unsupported header size " + hex);
            }
            return vok();
        }

        // used_bytes: the image holds its header and a root block, and ends
        // on a word.
        inline vstatus check_wire_extent(const XWireHeader& header, const char* who) {
            if (header.used_bytes < XWIRE_PAYLOAD_BEGIN || header.used_bytes > XBufferCore::MAX_PAYLOAD ||
                header.used_bytes % 8u != 0)
                return wire_reject(XErrc::size_mismatch, who, ": used size mismatch");
            if (header.used_bytes == XWIRE_PAYLOAD_BEGIN)
                return wire_reject(XErrc::root_offset_out_of_bounds, who, ": root offset out of bounds");
            return vok();
        }

        // ---- Wire layout (dense or sparse) --------------------------------

        struct WireLayout {
            const char* stored = nullptr;   // the image (dense), or the header (sparse)
            const char* extents = nullptr;  // extent table (sparse; its bytes follow it) or nullptr
            std::size_t extent_count = 0;
            std::vector<XBufferCore::WireHole> gaps; // skipped image ranges
        };

        // Validates an extent table (count entries at `table`, unaligned) for
        // an image of `used` bytes; appends the gaps and sets `stored` to the
        // extents' bytes on the wire.
        inline vstatus parse_extent_table(const char* table, std::size_t count,
                                          std::uint64_t used, const char* who,
                                          std::vector<XBufferCore::WireHole>& gaps, std::uint64_t& stored) {
            stored = 0;
            if (count == 0) {
                stored = used - XWIRE_PAYLOAD_BEGIN;
                return vok();
            }
            std::uint64_t end = 0;
            for (std::size_t i = 0; i < count; ++i) {
                std::uint32_t e[2];
                std::memcpy(e, table + i * 8u, sizeof(e));
                if (e[1] == 0 || (e[0] & 7u) != 0 || (e[1] & 7u) != 0 ||
                    (i == 0 ? e[0] != XWIRE_PAYLOAD_BEGIN : e[0] <= end) ||
                    std::uint64_t(e[0]) + e[1] > used)
                    return wire_reject(XErrc::invalid_extent_table, who, ": invalid extent table");
                if (i != 0) gaps.push_back({static_cast<std::uint32_t>(end),
                                            static_cast<std::uint32_t>(e[0] - end)});
                end = std::uint64_t(e[0]) + e[1];
                stored += e[1];
            }
            if (end < used) gaps.push_back({static_cast<std::uint32_t>(end),
                                            static_cast<std::uint32_t>(used - end)});
            if (gaps.empty()) return wire_reject(XErrc::invalid_extent_table, who, ": invalid extent table");
            return vok();
        }

        // M2: names the limit a wire exceeds, before any reservation.
        inline vstatus require_wire_within_capacity(std::size_t used, std::size_t limit,
                                                    bool explicit_limit, const char* who) {
            if (used <= limit) return vok();
            XOFFSET_VFAIL(XBadAlloc, XErrc::capacity_exceeded,
                          std::string(who) + ": wire used_bytes " + std::to_string(used) +
                          " exceeds " + (explicit_limit ? "maximum_capacity " :
                                         "the default maximum_capacity ") +
                          std::to_string(limit) +
                          (explicit_limit ? "" : "; pass XBuffer::maximum_capacity() for larger wires"));
        }

        // Validates used_bytes, the extent table and the length of a wire of
        // `size` bytes at `data` whose header passed read_wire_header and the
        // fingerprint check. Does not read the image past the header.
        inline vstatus parse_wire_layout(const XWireHeader& header, const char* data, std::size_t size,
                                         const char* who, WireLayout& layout) {
            XOFFSET_VTRY(check_wire_extent(header, who));
            layout.stored = data;
            if (header.extent_count == 0) {
                if (size != header.used_bytes) return wire_reject(XErrc::size_mismatch, who, ": used size mismatch");
                return vok();
            }
            const std::size_t body = size - sizeof(XWireHeader);
            if (header.extent_count > body / 8u) return wire_reject(XErrc::size_mismatch, who, ": used size mismatch");
            const auto table = std::size_t(header.extent_count) * 8u;
            layout.extent_count = std::size_t(header.extent_count);
            layout.extents = data + sizeof(XWireHeader);
            std::uint64_t stored;
            XOFFSET_VTRY(parse_extent_table(layout.extents, layout.extent_count, header.used_bytes, who, layout.gaps,
                                            stored));
            if (stored != body - table) return wire_reject(XErrc::size_mismatch, who, ": used size mismatch");
            return vok();
        }

        // On Darwin, outputs of at least 1 MiB are copied in 256 KiB
        // chunks, each preceded by madvise(MADV_WILLNEED) on its pages.
#if defined(__linux__)
        // E-2: on Linux, in an output of at least 1 MiB, each copy of at least
        // 256 KiB whose next-to-last page is not resident (a fresh mapping, not
        // a reused string) goes in 256 KiB chunks, each preceded by
        // madvise(MADV_POPULATE_WRITE), so the kernel faults the chunk's pages
        // in one call instead of one trap per page.
        // A kernel without it (before 5.14) answers EINVAL once; the output
        // is then copied as before for the rest of the process.
#ifndef MADV_POPULATE_WRITE
#define MADV_POPULATE_WRITE 23
#endif
#ifndef XOFFSET_WIRE_MADVISE
#define XOFFSET_WIRE_MADVISE ::madvise
#endif
        inline std::atomic<bool> wire_populate_unsupported{false};
#endif
        struct WireCopier {
            bool prefault = false;
            std::size_t page = 0;
            void copy(char* dst, const char* src, std::size_t n) const noexcept {
#if defined(__APPLE__)
                if (prefault) {
                    constexpr std::size_t chunk = std::size_t(256) << 10;
                    for (std::size_t off = 0; off < n; off += chunk) {
                        const auto len = std::min(chunk, n - off);
                        const auto lo = reinterpret_cast<std::uintptr_t>(dst + off) & ~(page - 1u);
                        const auto hi = (reinterpret_cast<std::uintptr_t>(dst + off) + len + page - 1u) & ~(page - 1u);
                        (void)::madvise(reinterpret_cast<void*>(lo), hi - lo, MADV_WILLNEED);
                        std::memcpy(dst + off, src + off, len);
                    }
                    return;
                }
#elif defined(__linux__)
                constexpr std::size_t chunk = std::size_t(256) << 10;
                unsigned char probe_page = 1;
                // Not the last page: libc++'s resize_and_overwrite writes the
                // terminator before the output is filled, which makes that page
                // resident in a fresh mapping too. n >= 256 KiB, so the page
                // before it is still inside the copy.
                if (prefault && n >= chunk && !wire_populate_unsupported.load(std::memory_order_relaxed) &&
                    ::mincore(reinterpret_cast<void*>((reinterpret_cast<std::uintptr_t>(dst + n - 1u) & ~(page - 1u)) - page),
                              page, &probe_page) == 0 && (probe_page & 1u) == 0) {
                    bool populate = true;
                    for (std::size_t off = 0; off < n; off += chunk) {
                        const auto len = std::min(chunk, n - off);
                        if (populate) {
                            const auto lo = reinterpret_cast<std::uintptr_t>(dst + off) & ~(page - 1u);
                            const auto hi = (reinterpret_cast<std::uintptr_t>(dst + off) + len + page - 1u) & ~(page - 1u);
                            if (XOFFSET_WIRE_MADVISE(reinterpret_cast<void*>(lo), hi - lo, MADV_POPULATE_WRITE) != 0 &&
                                errno == EINVAL) {
                                wire_populate_unsupported.store(true, std::memory_order_relaxed);
                                populate = false;
                            }
                        }
                        std::memcpy(dst + off, src + off, len);
                    }
                    return;
                }
#endif
                std::memcpy(dst, src, n);
            }
        };
        inline WireCopier wire_copier(std::size_t output_bytes) noexcept {
            WireCopier copier;
#if defined(__APPLE__)
            if (output_bytes >= (std::size_t(1) << 20)) {
                copier.prefault = true;
                copier.page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
                if (copier.page == 0 || (copier.page & (copier.page - 1u)) != 0) copier.prefault = false;
            }
#elif defined(__linux__)
            if (output_bytes >= (std::size_t(1) << 20) && !wire_populate_unsupported.load(std::memory_order_relaxed)) {
                copier.page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
                copier.prefault = copier.page != 0 && (copier.page & (copier.page - 1u)) == 0;
            }
#else
            (void)output_bytes;
#endif
            return copier;
        }

        // ---- Free-run emission (XWireEmission::free_runs) -------------------
        // Candidates are free blocks of at least wire_free_run_min_block
        // bytes; sorted by offset, adjacent candidates form runs, and in every
        // run of at least wire_free_run_min_gap bytes each block's body (all
        // of it but its first word, which links it to the next free block of
        // its size) is skipped: the gaps load as zero, which is what a free
        // block's body holds. A save looks for runs only when the image's
        // free bytes reach one run (the header's free_bytes: no walk). Up to
        // wire_free_run_local candidates (sorted by insertion) and
        // wire_free_run_local_runs gaps stay in local storage, so such a save
        // allocates nothing for them; more go to the heap, and the
        // candidates are then sorted with one counting pass.
        // A skipped body costs an extent (its table entry, a copy split in
        // two on both sides); below 1 KiB that costs more than the copy it
        // saves, and a free block keeps its link word, so smaller blocks in
        // a run would each add one.
        inline constexpr std::uint32_t wire_free_run_min_block = 1024;
        inline constexpr std::uint32_t wire_free_run_min_gap = 4096;

        // ---- Automatic emission (XWireEmission::automatic, the default) ----
        // The compact walk costs about its blocks plus the live bytes, the
        // image copy the used bytes, on both ends. Premises sweep (Character,
        // images of 1 KB to 400 KB, GCC 16), churned senders: the compact
        // wire took 0.10 to 0.75 of the image's send and checked receive time
        // at every size, and 0.08 to 0.77 of send and trusted receive from
        // 12 KB up; below that the trusted pair took 0.85 to 1.17 (a copy that
        // stays in L1 costs about as much as the walk). Senders whose
        // containers are filled to capacity (no dead bytes): 1.3 to 2.1. The
        // header's free bytes tell the two apart without a walk. Below 512
        // free bytes the walk's fixed cost is not worth it.
        inline constexpr std::uint32_t wire_auto_compact_min_free = 512;
        inline constexpr std::uint32_t wire_auto_compact_share = 8; // free >= used / 8
        constexpr bool wire_prefers_compact(std::uint64_t used, std::uint64_t free_bytes) noexcept {
            return free_bytes >= wire_auto_compact_min_free && free_bytes * wire_auto_compact_share >= used;
        }
        // Spare capacity is dead bytes too, but no free block: push_back
        // doubles a vector in place, so a freshly built message can carry up
        // to half of each block as spare. From wire_spare_scan_min used bytes
        // the choice also counts the spare of the root object's own container
        // heads (its members, records held by value, fixed arrays; not the
        // heads inside container elements): each adds (capacity - size -
        // size / 8 - 1) elements, signed, the bytes the compact wire would
        // not send (it keeps size / 8 + 1 spare elements). A root type with
        // more than wire_spare_scan_max_heads such heads is not read.
        // Character built by push_back, every count times k (cloud x86,
        // 48 KiB L1d, GCC 16, compact / image time of send and trusted
        // receive): k = 2.5 and 3 (11.5 KB, compact 0.74 and 0.87 of the
        // bytes) 1.36 and 1.79, so below 16 KiB the image copy, which stays
        // in L1 on both ends, wins; k = 5, 6, 10, 12, 17, 24, 40, 48, 96
        // (23 to 373 KB, 0.61 to 0.87 of the bytes) 0.37 to 1.02; k = 7, 8,
        // 14, 28, 64 (0.98 to 1.14 of the bytes) 0.99 to 1.28. From 16 KiB
        // the time ratio is about 1.13 times the byte ratio: compact wins
        // from about 1/8 dead, the share above. 120 random Character shapes
        // (1 to 340 KB), geometric mean against always the image: 0.931
        // with this floor and share (0.927 picking the faster wire each
        // time; floors 0, 12 KiB and 24 KiB 0.984, 0.946, 0.951; shares 1/6
        // and 1/10 0.935, 0.934).
        inline constexpr std::uint64_t wire_spare_scan_min = 16384;
        inline constexpr std::size_t wire_spare_scan_max_heads = 64;
        inline constexpr std::size_t wire_free_run_local = 64;
        inline constexpr std::size_t wire_free_run_local_runs = 8;

        // The skipped runs, ascending and disjoint: the first
        // wire_free_run_local_runs inline, all of them on the heap once there
        // are more.
        class WireRuns {
        public:
            void push_back(XBufferCore::WireHole run) {
                if (count_ < local_.size() && heap_.empty()) {
                    local_[count_] = run;
                } else {
                    if (heap_.empty()) heap_.assign(local_.begin(), local_.end());
                    heap_.push_back(run);
                }
                ++count_;
            }
            const XBufferCore::WireHole* begin() const noexcept { return heap_.empty() ? local_.data() : heap_.data(); }
            const XBufferCore::WireHole* end() const noexcept { return begin() + count_; }
            std::size_t size() const noexcept { return count_; }
            bool empty() const noexcept { return count_ == 0; }
            const XBufferCore::WireHole& back() const noexcept { return begin()[count_ - 1u]; }

        private:
            std::array<XBufferCore::WireHole, wire_free_run_local_runs> local_{};
            std::vector<XBufferCore::WireHole> heap_;
            std::size_t count_ = 0;
        };

        struct WireGaps {
            WireRuns runs; // ascending, disjoint
            std::uint64_t skipped = 0;
            std::size_t extent_count = 0;
        };

        // A candidate free block. Unlike XBufferCore::WireHole it has no
        // member initializers, so the local array and the heap buffers below
        // are not zeroed before they are written.
        struct WireCandidate {
            std::uint32_t offset;
            std::uint32_t bytes;
        };

        // Sorts holes[first, last) by offset (distinct) by insertion.
        inline void wire_insertion_sort(WireCandidate* holes, std::size_t first, std::size_t last) noexcept {
            for (std::size_t i = first + 1u; i < last; ++i) {
                const auto hole = holes[i];
                std::size_t j = i;
                for (; j > first && holes[j - 1u].offset > hole.offset; --j) holes[j] = holes[j - 1u];
                holes[j] = hole;
            }
        }

        // Sorts in[0, count) by offset into a new buffer with one counting
        // pass. Candidates are disjoint and at least 256 bytes, so a bucket
        // 2^shift >= 256 bytes wide holds at most 2^shift / 256 of them:
        // about four buckets per candidate over [low, high] usually leaves
        // one per bucket (a bucket that gets more is sorted by insertion, or
        // std::sort over 32). Offsets are distinct.
        inline std::unique_ptr<WireCandidate[]> wire_bucket_sort(const WireCandidate* in, std::size_t count,
                                                                 std::uint32_t low, std::uint32_t high) {
            std::size_t limit = 4;
            while (limit < 4u * count && limit < (std::size_t(1) << 18)) limit <<= 1;
            unsigned shift = 8;
            while (((high - low) >> shift) >= limit) ++shift;
            std::vector<std::uint32_t> bucket_end(std::size_t((high - low) >> shift) + 2u, 0u);
            bool crowded = false;
            for (std::size_t i = 0; i < count; ++i)
                crowded |= ++bucket_end[((in[i].offset - low) >> shift) + 1u] > 1u;
            for (std::size_t i = 1; i < bucket_end.size(); ++i) bucket_end[i] += bucket_end[i - 1u];
            std::unique_ptr<WireCandidate[]> sorted(new WireCandidate[count]);
            for (std::size_t i = 0; i < count; ++i) sorted[bucket_end[(in[i].offset - low) >> shift]++] = in[i];
            // bucket_end[b] is now where bucket b ends (and bucket b + 1 starts).
            if (crowded) {
                std::uint32_t first = 0;
                for (std::size_t b = 0; b + 1u < bucket_end.size(); ++b) {
                    const std::uint32_t last = bucket_end[b];
                    if (last - first > 32u) {
                        std::sort(sorted.get() + first, sorted.get() + last,
                                  [](const auto& x, const auto& y) { return x.offset < y.offset; });
                    } else {
                        wire_insertion_sort(sorted.get(), first, last);
                    }
                    first = last;
                }
            }
            return sorted;
        }

        inline WireGaps wire_free_runs(const XBufferCore& core, std::uint64_t used) {
            WireGaps gaps;
            if (core.free_block_bytes() < wire_free_run_min_gap || used == 0) return gaps;
            // The candidates fill the local array first and move to a heap
            // buffer (doubled as needed) only past wire_free_run_local of
            // them: one capacity check per candidate, as a vector push_back.
            std::array<WireCandidate, wire_free_run_local> local;
            std::unique_ptr<WireCandidate[]> heap;
            WireCandidate* store = local.data();
            std::size_t count = 0, capacity = local.size();
            std::uint64_t total = 0;
            std::uint32_t low = ~std::uint32_t(0), high = 0;
            core.for_each_free_block_at_least(wire_free_run_min_block, [&](XBufferCore::WireHole hole) {
                if (count == capacity) [[unlikely]] {
                    std::unique_ptr<WireCandidate[]> bigger(new WireCandidate[2u * capacity]);
                    std::copy(store, store + count, bigger.get());
                    heap = std::move(bigger);
                    store = heap.get();
                    capacity *= 2u;
                }
                store[count++] = {hole.offset, hole.bytes};
                total += hole.bytes;
                low = (std::min)(low, hole.offset);
                high = (std::max)(high, hole.offset);
            });
            if (total < wire_free_run_min_gap) return gaps;
            if (count <= local.size()) wire_insertion_sort(store, 0, count);
            else heap = wire_bucket_sort(store, count, low, high);
            const WireCandidate* const candidates = count <= local.size() ? store : heap.get();
            // A run is candidates [first, i) with no space between them.
            std::size_t first = 0;
            std::uint64_t start = 0, end = 0;
            auto close = [&](std::size_t last) {
                if (end - start < wire_free_run_min_gap) return;
                for (std::size_t k = first; k < last; ++k) {
                    const auto block = candidates[k];
                    gaps.runs.push_back({block.offset + 8u, block.bytes - 8u});
                    gaps.skipped += block.bytes - 8u;
                }
            };
            for (std::size_t i = 0; i < count; ++i) {
                const auto hole = candidates[i];
                if (hole.offset == end && end != 0) { end += hole.bytes; continue; }
                close(i);
                first = i;
                start = hole.offset;
                end = std::uint64_t(hole.offset) + hole.bytes;
            }
            close(count);
            if (!gaps.runs.empty()) {
                // Every gap starts past a free block's first word, so past 40:
                // one extent precedes each gap.
                const auto& last = gaps.runs.back();
                gaps.extent_count = gaps.runs.size() + (std::uint64_t(last.offset) + last.bytes < used ? 1u : 0u);
            }
            return gaps;
        }

        // Exact wire size of a free-run emission (dense when `gaps` is empty).
        inline std::size_t wire_free_run_size(std::uint64_t used, const WireGaps& gaps) noexcept {
            return static_cast<std::size_t>(used - gaps.skipped) + gaps.extent_count * 8u;
        }

        // ---- Zero-run elision (XWireEmission::zero_runs) ---------------
        // Payloads below wire_zero_scan_min_payload are not scanned. A run is
        // found from a 64-byte aligned all-zero block and extended word by
        // word on both sides; every skipped run is at least
        // wire_zero_run_min bytes. The sparse form is used only when it saves
        // at least max(wire_zero_min_saving, used / 16) bytes.
        inline constexpr std::size_t wire_zero_run_min = 64;
        inline constexpr std::size_t wire_zero_scan_min_payload = 4096;
        inline constexpr std::size_t wire_zero_min_saving = 256;

        // Writes the extent table of `image` (8-aligned, `used` a multiple
        // of 8) at `table` and returns the extent count, or 0 when no run is
        // skipped. The table never exceeds `used` - 40 bytes: every extent
        // but the first holds at least one nonzero word, and every skipped
        // run is at least 64 bytes. The first extent starts at 40 and holds
        // at least the word there. Block 0 holds the header (whose magic is
        // never zero), so no run starts in it.
        inline std::size_t wire_zero_run_extents(const char* image, std::size_t used,
                                                 char* table, std::uint64_t& skipped) noexcept {
            auto word = [&](std::size_t at) noexcept {
                std::uint64_t w;
                std::memcpy(&w, image + at, sizeof(w));
                return w;
            };
            auto block_zero = [&](std::size_t block) noexcept {
                std::uint64_t w[8];
                std::memcpy(w, image + block * 64u, sizeof(w));
                return ((w[0] | w[1]) | (w[2] | w[3]) | (w[4] | w[5]) | (w[6] | w[7])) == 0;
            };
            std::size_t count = 0, cursor = XWIRE_PAYLOAD_BEGIN;
            auto emit = [&](std::size_t begin, std::size_t end) noexcept {
                const std::uint32_t entry[2] = {static_cast<std::uint32_t>(begin),
                                                static_cast<std::uint32_t>(end - begin)};
                std::memcpy(table + count * 8u, entry, sizeof(entry));
                ++count;
            };
            skipped = 0;
            const std::size_t blocks = used / 64u;
            for (std::size_t b = 0; b < blocks;) {
                if (!block_zero(b)) { ++b; continue; }
                std::size_t start = b * 64u;
                while (start > cursor && word(start - 8u) == 0) start -= 8u;
                std::size_t e = b + 1u;
                while (e < blocks && block_zero(e)) ++e;
                std::size_t stop = e * 64u;
                while (stop < used && word(stop) == 0) stop += 8u;
                if (start < XWIRE_PAYLOAD_BEGIN + 8u) start = XWIRE_PAYLOAD_BEGIN + 8u;
                if (stop - start >= wire_zero_run_min) {
                    emit(cursor, start);
                    skipped += stop - start;
                    cursor = stop;
                }
                // The block holding `stop` has a nonzero word at `stop`.
                b = stop / 64u + 1u;
            }
            if (count == 0) return 0;
            if (cursor < used) emit(cursor, used);
            return count;
        }

        // Writes the wire of the image at `image` (`header`: its header with
        // the sender's fingerprint; extent_count is set here) into `out`,
        // which holds at least wire_free_run_size(used, gaps) bytes, or
        // used bytes for zero_runs, and returns the bytes written. `gaps`
        // comes from wire_free_runs (free_runs) or is empty. No allocation
        // and no callback.
        inline std::size_t write_wire(char* out, XWireHeader header, const char* image,
                                      XWireEmission emission, const WireGaps& gaps) noexcept {
            const auto used = static_cast<std::size_t>(header.used_bytes);
            char* at = out + sizeof(XWireHeader);
            std::size_t extents = 0;
            if (emission == XWireEmission::free_runs && gaps.extent_count) {
                extents = gaps.extent_count;
                std::uint32_t begin = XWIRE_PAYLOAD_BEGIN;
                for (const auto& gap : gaps.runs) {
                    const std::uint32_t entry[2] = {begin, gap.offset - begin};
                    std::memcpy(at, entry, sizeof(entry));
                    at += sizeof(entry);
                    begin = gap.offset + gap.bytes;
                }
                if (begin < used) {
                    const std::uint32_t entry[2] = {begin, static_cast<std::uint32_t>(used - begin)};
                    std::memcpy(at, entry, sizeof(entry));
                }
                at = out + sizeof(XWireHeader);
            } else if (emission == XWireEmission::zero_runs && used >= wire_zero_scan_min_payload) {
                std::uint64_t skipped = 0;
                extents = wire_zero_run_extents(image, used, at, skipped);
                if (extents && skipped - extents * 8u < (std::max)(std::uint64_t(wire_zero_min_saving),
                                                                    std::uint64_t(used / 16u)))
                    extents = 0;
            }
            header.extent_count = static_cast<std::uint32_t>(extents);
            std::memcpy(out, &header, sizeof(header));
            const auto copier = wire_copier(used);
            if (extents == 0) {
                copier.copy(at, image + XWIRE_PAYLOAD_BEGIN, used - XWIRE_PAYLOAD_BEGIN);
                return used;
            }
            const char* table = at;
            at += extents * 8u;
            for (std::size_t i = 0; i < extents; ++i) {
                std::uint32_t entry[2];
                std::memcpy(entry, table + i * 8u, sizeof(entry));
                copier.copy(at, image + entry[0], entry[1]);
                at += entry[1];
            }
            return static_cast<std::size_t>(at - out);
        }
    }

    // Type admission: is_byte_copy_safe_v (xoffset_admission.hpp); XOffset's
    // frozen containers are specialized in xoffset_registration.hpp.

    template <typename T>
    inline constexpr bool is_wire_admitted_v = detail::wire_admitted<T>();
} // namespace XOffsetDatastructure

#endif // X_OFFSET_WIRE_HPP
