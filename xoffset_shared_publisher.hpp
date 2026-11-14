#ifndef X_OFFSET_SHARED_PUBLISHER_HPP
#define X_OFFSET_SHARED_PUBLISHER_HPP

// The server side of shared deltas (docs/SHARED_DELTA.md): a publisher, two
// kinds of tables, per-connection state and the message a server builds for
// one connection. Clients do not need this header (they apply messages with
// XReceiver<XLink::from_server>::apply_sync, xoffset_receiver.hpp).
//
//   XSyncPublisher        the tick and this tick's body store
//   XSyncDirtyTable       entities the application marks dirty member by
//                         member; their images live with the application
//                         (records with containers: an XBuffer)
//   XSyncImageTable       plain records compared whole; the image sits in
//                         the table's entry (dense index, or a ring by id)
//   XSyncConn             one connection, hot: the message version and the
//                         client's free slots of each table with enters and
//                         leaves (a fixed stack per table, cache-line aligned)
//   XSyncConnCold         one connection, cold: the stream and the free slots
//                         past the hot stacks
//   XSyncMessage          one message for one connection
//
// Each entity's body is written once per tick into the publisher's store and
// copied into every message that carries it. A table with enters and leaves
// sends each client the slot the server gives it for the entity (the client
// keeps its records by slot and needs no index). Which entity holds which
// slot on a connection is the application's (it keeps the slot next to its
// record of what the client holds); which slots are free is the library's,
// in the connection.
//
// Per tick:
//   pub.begin_tick(tick);
//   units.publish(pub, changed_units, unit_source);     // before the tick's first message
// Per connection (each table at most once per kind, sections in this order):
//   XSyncMessage m(pub, conn, cold, out, root);
//   units.leaves(m, index, slot);  marches.leaves(m, slot);
//   units.changes(m, src, index, slot);  marches.changes(m, src, ref, slot);  tiles.changes(m, src, id);
//   units.enters(m, src, index, [&](std::size_t r, std::uint32_t s) { held[r].slot = s; });
//   const std::size_t n = m.finish();
//   XSyncConn<...>::prefetch(&conn + 1);                 // the connection served next
//
// Contracts (the first is checked in every build, the others in builds with
// XOFFSET_SYNC_CHECKS, on by default without NDEBUG; define it the same in
// every translation unit):
//   1. A tick's dirty bits are published before its first message.
//   2. Every connection that holds an entity gets every body the entity has:
//      skipping a connection is a disconnect (release, then a key message).
//   3. A disconnect, and a key message, first release every unit the
//      connection holds (XSyncDirtyTable::release).
//   4. The slot enters() reports is stored with the application's record and
//      passed back to changes() and leaves().
// A publisher and its tables are used from one thread.

#include "xoffset_shared_delta.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#if !defined(XOFFSET_SYNC_CHECKS)
#  if defined(NDEBUG)
#    define XOFFSET_SYNC_CHECKS 0
#  else
#    define XOFFSET_SYNC_CHECKS 1
#  endif
#endif

// A loop that usually runs one to three times: no vector code (its prologue
// and epilogue cost more than the loop).
#if !defined(XOFFSET_DETAIL_NOVECTOR)
#  if defined(__clang__)
#    define XOFFSET_DETAIL_NOVECTOR _Pragma("clang loop vectorize(disable)")
#  elif defined(__GNUC__) && __GNUC__ >= 14
#    define XOFFSET_DETAIL_NOVECTOR _Pragma("GCC novector")
#  elif defined(_MSC_VER)
#    define XOFFSET_DETAIL_NOVECTOR __pragma(loop(no_vector))
#  else
#    define XOFFSET_DETAIL_NOVECTOR
#  endif
#endif

namespace XOffsetDatastructure::xsync {

    /// A tick no entry has seen.
    inline constexpr std::uint32_t kSyncNever = 0xFFFFFFFFu;

    template <class Conn> class XSyncMessage;
    template <unsigned K, class T, std::size_t ChangeOp, std::size_t EnterOp> class XSyncDirtyTable;
    template <unsigned K, class T, class Index, bool Keyed, std::size_t Align> class XSyncImageTable;

    namespace sd_detail {
        [[noreturn]] XOFFSET_SYNC_COLD inline void sync_fail(const char* what) {
            XOFFSET_THROW(XException(XErrc::invalid_state, what));
        }
        [[noreturn]] XOFFSET_SYNC_COLD inline void sync_too_many() {
            XOFFSET_THROW(std::length_error("xsync: more than 2^24 - 1 ops in one section"));
        }
        // An entity's two bodies of this tick in the publisher's store: a
        // change body (tick, off, len) and an enter body (ktick, koff, klen).
        // A body counts only when its tick is the publisher's.
        struct EntryHead {
            std::uint32_t tick = kSyncNever, ktick = kSyncNever;
            std::uint32_t off = 0, len = 0, koff = 0, klen = 0;
        };
        constexpr std::uint32_t section(std::size_t n, unsigned table, XSyncKind kind) noexcept {
            return std::uint32_t(n) << 8 | std::uint32_t(table << 2 | unsigned(kind));
        }
        // How the slow paths take the application's Src: a light view (two
        // pointers at most, trivially copyable) by value, so the caller's
        // copy stays in registers; anything else by reference.
        template <class S, class B = std::remove_cvref_t<S>>
        using src_arg = std::conditional_t<std::is_trivially_copyable_v<B> && sizeof(B) <= 2 * sizeof(void*), B, std::remove_reference_t<S>&>;
        // A message's bytes up to q kept and n more writable past them:
        // where q and the end of the room are now.
        struct Room { char* q; char* end; };
        XOFFSET_SYNC_COLD inline Room grow_message(XSyncOut& out, char* q, std::size_t n) {
            out.commit(std::size_t(q - out.data()) - out.size());
            char* const r = out.tail(n);
            return {r, out.data() + out.capacity()};
        }
        template <class... F> constexpr std::size_t conn_bytes() noexcept {
            return 4 + 6 * sizeof...(F) + (std::size_t(0) + ... + F::hot);
        }
    } // namespace sd_detail

    // ---- publisher -----------------------------------------------------------------

    /// The tick (32 bits, strictly increasing; not kSyncNever) and the store of
    /// this tick's bodies. Entries record where their bodies are by offset, so
    /// the store may grow while a tick is being dispatched.
    class XSyncPublisher {
    public:
        /// Starts a tick: the store empties and tables may publish again.
        void begin_tick(std::uint32_t tick) {
            if (tick == kSyncNever || (last_ != kSyncNever && tick <= last_)) [[unlikely]]
                sd_detail::sync_fail("xsync: begin_tick needs a tick past the last one (and not 0xFFFFFFFF)");
            tick_ = last_ = tick;
            bodies_.clear();
            dispatching_ = false;
        }
        std::uint32_t tick() const noexcept { return tick_; }
        /// Whether a message of this tick has been started (no publish after that).
        bool dispatching() const noexcept { return dispatching_; }
        const XSyncOut& bodies() const noexcept { return bodies_; }
        std::size_t bytes() const noexcept { return bodies_.capacity(); }
        void reserve(std::size_t n) { bodies_.reserve(n); }

    private:
        template <class> friend class XSyncMessage;
        template <unsigned, class, std::size_t, std::size_t> friend class XSyncDirtyTable;
        template <unsigned, class, class, bool, std::size_t> friend class XSyncImageTable;
        XSyncOut bodies_;
        std::uint32_t tick_ = kSyncNever, last_ = kSyncNever;
        bool dispatching_ = false;
    };

    // ---- connections ---------------------------------------------------------------

    /// The free slots of table K (with enters and leaves) in a connection's hot
    /// part: a stack of Hot one-byte slots, the last freed taken first. Slots
    /// that do not fit (the stack full, or a slot past 255) go to the cold
    /// part's spill list; taking a slot never fails. Hot is tuning, not a limit.
    template <unsigned K, std::size_t Hot> struct XSyncFreeSlots {
        static_assert(K >= 1 && K < kSyncTables, "xsync: tables 1 to 63");
        static_assert(Hot >= 1 && Hot <= 255, "xsync: a hot stack has 1 to 255 slots");
        static constexpr unsigned table = K;
        static constexpr std::size_t hot = Hot;
    };

    template <class... F> struct XSyncConnCold;

    /// A connection's hot part: the message version and, for each table with
    /// enters and leaves, its free-slot stack. Messages touch it in three
    /// places: a prefetch at construction, enters (slots taken) and finish()
    /// (slots given back, version). Aligned to its size (64 or 128 bytes).
    template <class... F>
    struct alignas(sd_detail::conn_bytes<F...>() <= 64 ? 64 : 128) XSyncConn {
        static constexpr std::size_t keyed = sizeof...(F);
        static_assert(keyed >= 1, "xsync: a connection names its tables with enters and leaves");
        using cold_type = XSyncConnCold<F...>;
        static constexpr std::array<unsigned, keyed> table{F::table...};
        static constexpr std::array<std::size_t, keyed> hot{F::hot...};
        static constexpr std::array<std::size_t, keyed> at = [] {
            std::array<std::size_t, keyed> a{};
            std::size_t s = 0;
            for (std::size_t j = 0; j < keyed; ++j) { a[j] = s; s += hot[j]; }
            return a;
        }();
        /// Which stack table K uses.
        template <unsigned K> static constexpr std::size_t index = [] {
            for (std::size_t j = 0; j < keyed; ++j) if (table[j] == K) return j;
            return keyed;
        }();

        std::uint32_t version = 0;
        std::array<std::uint32_t, keyed> used{};   // slots given out so far
        std::array<std::uint8_t, keyed> n{};       // free slots on each stack
        std::array<std::uint8_t, keyed> spill{};   // the cold spill list is not empty
        std::array<std::uint8_t, (std::size_t(0) + ... + F::hot)> stack{};   // stack j at at[j], hot[j] places

        /// Fetches c's lines (writing). Safe for any address, one past the end
        /// of an array of connections included: call it after finish() for the
        /// connection served next.
        XOFFSET_SYNC_INLINE static void prefetch(const XSyncConn* c) noexcept {
            const char* p = reinterpret_cast<const char*>(c);
            for (std::size_t o = 0; o < sizeof(XSyncConn); o += 64) XOFFSET_DETAIL_PREFETCH(p + o, 1);
        }
    };

    /// A connection's cold part: the stream (key messages only) and the spill
    /// lists; with XOFFSET_SYNC_CHECKS, which entry holds each slot.
    template <class... F> struct XSyncConnCold {
        std::uint64_t stream = 0;
        std::array<std::vector<std::int32_t>, sizeof...(F)> spill;
#if XOFFSET_SYNC_CHECKS
        std::array<std::vector<std::uint32_t>, sizeof...(F)> owner;   // slot -> entry + 1, 0 when free
        std::array<std::vector<std::uint32_t>, sizeof...(F)> mark;    // slot -> the last message naming it
        std::uint32_t serial = 0;
#endif
        std::size_t bytes() const noexcept {
            std::size_t n = 0;
            for (const auto& v : spill) n += v.capacity() * sizeof(std::int32_t);
            return n;
        }
    };

    // ---- message ---------------------------------------------------------------------

    /// One message for one connection, built in out: the head and the root
    /// record at construction, the tables' sections, then finish(). Its
    /// members are forced inline and its address never leaves the caller, so
    /// its cursor stays in registers.
    template <class Conn>
    class XSyncMessage {
    public:
        using conn_type = Conn;
        using cold_type = typename Conn::cold_type;

        /// A delta: the connection's next version.
        template <class Root>
        XOFFSET_SYNC_INLINE XSyncMessage(XSyncPublisher& pub, Conn& conn, cold_type& cold, XSyncOut& out, const Root& root)
            : pub_(&pub), conn_(&conn), cold_(&cold), out_(&out) {
            Conn::prefetch(&conn);
            start(root, 0, nullptr);
        }
        /// A key message of stream: the connection starts over (version 1,
        /// every slot free). Release the units it held first (contract 3).
        template <class Root>
        XSyncMessage(XSyncPublisher& pub, Conn& conn, cold_type& cold, XSyncOut& out, const Root& root, std::uint64_t stream)
            : pub_(&pub), conn_(&conn), cold_(&cold), out_(&out) {
            conn = Conn{};
            for (auto& v : cold.spill) v.clear();
#if XOFFSET_SYNC_CHECKS
            for (auto& v : cold.owner) v.clear();
            for (auto& v : cold.mark) v.clear();
#endif
            cold.stream = stream;
            start(root, kFlagKey, &stream);
        }
        XSyncMessage(const XSyncMessage&) = delete;
        XSyncMessage& operator=(const XSyncMessage&) = delete;

        /// Ends the message: the slots that left are free from the next
        /// message on, the version goes up and is written. Returns the length.
        XOFFSET_SYNC_INLINE std::size_t finish() {
            [&]<std::size_t... J>(std::index_sequence<J...>) XOFFSET_SYNC_LAMBDA_INLINE {
                (give<J>(), ...);
            }(std::make_index_sequence<Conn::keyed>{});
            char* const b = out_->data();
            sd_detail::store<std::uint32_t>(b + 4, ++conn_->version);
            out_->commit(std::size_t(q_ - b) - out_->size());
            return out_->size();
        }

    private:
        template <unsigned, class, std::size_t, std::size_t> friend class XSyncDirtyTable;
        template <unsigned, class, class, bool, std::size_t> friend class XSyncImageTable;

        template <class Root>
        XOFFSET_SYNC_INLINE void start(const Root& root, std::uint8_t flags, const std::uint64_t* stream) {
            static_assert(XRecordBody<Root>::container_count == 0, "xsync: the root record (table 0) has no containers");
            pub_->dispatching_ = true;
            out_->clear();
            char* b = out_->tail(kSyncHead + 8 + XRecordBody<Root>::whole_body::fixed + kSyncSlack);
            b[0] = 'X'; b[1] = 'S'; b[2] = char(kFormat); b[3] = char(flags);
            char* d = b + kSyncHead;
            if (stream) { sd_detail::store<std::uint64_t>(d, *stream); d += 8; }
            q_ = XRecordBody<Root>::put_whole(d, root);
            end_ = b + out_->capacity();
#if XOFFSET_SYNC_CHECKS
            tick_ = pub_->tick_;
            serial_ = ++cold_->serial;
#endif
        }
        // Room for n more bytes at q_.
        XOFFSET_SYNC_INLINE void room(std::size_t n) {
            if (std::size_t(end_ - q_) < n) [[unlikely]] {
                const sd_detail::Room r = sd_detail::grow_message(*out_, q_, n);
                q_ = r.q;
                end_ = r.end;
            }
        }
        // The same inside a section: its cursor q and its head sec move with the bytes.
        XOFFSET_SYNC_INLINE void room(char*& q, char*& sec, std::size_t n) {
            if (std::size_t(end_ - q) < n) [[unlikely]] {
                const sd_detail::Room r = sd_detail::grow_message(*out_, q, n);
                sec = r.q - (q - sec);
                q = r.q;
                end_ = r.end;
            }
        }
        XOFFSET_SYNC_INLINE static void count(std::size_t n) {
            if (n > kSyncSectionMax) [[unlikely]] sd_detail::sync_too_many();
        }
        XOFFSET_SYNC_INLINE const char* bodies() const noexcept { return pub_->bodies_.data(); }

        // Taking slots of stack J: the counters in locals while a section
        // writes (its stores may alias the connection), written back by done().
        template <std::size_t J> struct Taker {
            Conn& c;
            cold_type& k;
            std::uint32_t n, used;
            XOFFSET_SYNC_INLINE explicit Taker(XSyncMessage& m) : c(*m.conn_), k(*m.cold_), n(m.conn_->n[J]), used(m.conn_->used[J]) {}
            XOFFSET_SYNC_INLINE std::uint32_t take() {
                if (n) return c.stack[Conn::at[J] + --n];
                if (c.spill[J]) [[unlikely]] return unspill<J>(c, k);
                return used++;
            }
            XOFFSET_SYNC_INLINE void done() noexcept { c.n[J] = std::uint8_t(n); c.used[J] = used; }
        };
        template <std::size_t J> XOFFSET_SYNC_COLD static std::uint32_t unspill(Conn& c, cold_type& k) noexcept {
            auto& sp = k.spill[J];
            const std::int32_t v = sp.back();
            sp.pop_back();
            c.spill[J] = std::uint8_t(!sp.empty());
            return std::uint32_t(v);
        }
        // Gives back the k slots of stack J at p (u32 each, the leave
        // section's ops), one by one: what does not fit the stack spills.
        template <std::size_t J> XOFFSET_SYNC_COLD static void give_slow(Conn& c, cold_type& cold, const char* p, std::size_t k) {
            std::uint32_t n = c.n[J];
            for (std::size_t i = 0; i < k; ++i) {
                const std::uint32_t v = sd_detail::load<std::uint32_t>(p + 4 * i);
                if (n < Conn::hot[J] && v < 256) c.stack[Conn::at[J] + n++] = std::uint8_t(v);
                else { cold.spill[J].push_back(std::int32_t(v)); c.spill[J] = 1; }
            }
            c.n[J] = std::uint8_t(n);
        }
        template <std::size_t J> XOFFSET_SYNC_INLINE void give() {
            const std::uint32_t k = ln_[J];
            if (!k) return;
            const char* const p = out_->data() + loff_[J];
            Conn& c = *conn_;
#if XOFFSET_SYNC_CHECKS
            for (std::uint32_t i = 0; i < k; ++i) cold_->owner[J][sd_detail::load<std::uint32_t>(p + 4 * i)] = 0;
#endif
            const std::uint32_t n = c.n[J];
            if (n + k <= Conn::hot[J] && c.used[J] <= 256) [[likely]] {
                XOFFSET_DETAIL_NOVECTOR
                for (std::uint32_t i = 0; i < k; ++i) c.stack[Conn::at[J] + n + i] = std::uint8_t(sd_detail::load<std::uint32_t>(p + 4 * i));
                c.n[J] = std::uint8_t(n + k);
            } else give_slow<J>(c, *cold_, p, k);
        }

#if XOFFSET_SYNC_CHECKS
        void check_tick() const {
            if (tick_ != pub_->tick_) sd_detail::sync_fail("xsync: a message used across ticks");
        }
        // Slot s of stack J named in this message: not twice, and held by
        // entry + 1 (want 0: any holder).
        template <std::size_t J> void check_slot(std::uint32_t s, std::uint32_t want) {
            auto& own = cold_->owner[J];
            auto& mk = cold_->mark[J];
            if (s >= own.size() || own[s] == 0) sd_detail::sync_fail("xsync: a slot the connection does not hold");
            if (want && own[s] != want) sd_detail::sync_fail("xsync: a slot held by another entry (the slot enters() gave is not the one passed)");
            if (mk[s] == serial_) sd_detail::sync_fail("xsync: a slot twice in one message");
            mk[s] = serial_;
        }
        template <std::size_t J> void check_take(std::uint32_t s, std::uint32_t entry) {
            auto& own = cold_->owner[J];
            auto& mk = cold_->mark[J];
            if (s >= own.size()) { own.resize(std::size_t(s) + 1, 0); mk.resize(std::size_t(s) + 1, 0); }
            if (own[s] != 0) sd_detail::sync_fail("xsync: a slot given out twice (a slot left twice?)");
            if (mk[s] == serial_) sd_detail::sync_fail("xsync: a slot left and entered in one message");
            own[s] = entry + 1;
            mk[s] = serial_;
        }
#endif

        XSyncPublisher* pub_;
        Conn* conn_;
        cold_type* cold_;
        XSyncOut* out_;
        char* q_ = nullptr;
        char* end_ = nullptr;
        std::array<std::uint32_t, Conn::keyed> loff_{};   // each stack's leave ops, from the message's start
        std::array<std::uint32_t, Conn::keyed> ln_{};     // and how many
#if XOFFSET_SYNC_CHECKS
        std::uint32_t tick_ = kSyncNever, serial_ = 0;
#endif
    };

    // ---- dirty table -----------------------------------------------------------------

    /// Entities with enters and leaves whose changes the application marks
    /// member by member (a dirty bit per member of T, declaration order). The
    /// images (what every holder has) are the application's: Src gives them.
    /// An entry per index (dense): two bodies, the dirty bits not yet used,
    /// and the number of connections holding the entity.
    ///
    /// Src:
    ///   std::uint64_t dirty(std::uint32_t i);              this tick's dirty bits of entity i
    ///   T& image(std::uint32_t i);                         its image
    ///   void load(std::uint32_t i, T& image, std::uint64_t mask);   copies the members in mask in
    ///   (optional) T* images();                            the images as one array (publish prefetches them)
    ///
    /// ChangeOp / EnterOp: op bytes (id and body) a section reserves per op;
    /// a longer body takes a slow path.
    ///
    /// State per entity and tick: the first use (publish, for entities held;
    /// else the tick's first changes() or enters()) takes the dirty bits.
    /// Bits of earlier ticks that no first use took mean nobody held the
    /// entity then: the image catches up and no body is written. Otherwise
    /// the members in this tick's bits are loaded and their change body is
    /// written once for every holder.
    template <unsigned K, class T, std::size_t ChangeOp = 256, std::size_t EnterOp = 1024>
    class XSyncDirtyTable {
        static_assert(K >= 1 && K < kSyncTables, "xsync: tables 1 to 63");
        static_assert(XRecordBody<T>::members::N <= 64, "xsync: a dirty bit per member, at most 64");

    public:
        struct Entry : sd_detail::EntryHead {
            std::uint64_t now = 0, old = 0;          // bits of tick ptick; bits of earlier ticks no first use took
            std::uint32_t ptick = kSyncNever, rtick = kSyncNever;   // the tick of now; of the last first use
            std::int32_t hold = 0;                   // connections holding the entity
#if XOFFSET_SYNC_CHECKS
            std::uint32_t h0 = 0, left = 0, sent = 0;   // at the change body's tick: holders then, leaves, change ops
#endif
        };
        static constexpr std::size_t entry_size = sizeof(Entry);
        static constexpr unsigned table = K;

        /// n entries, all new (nobody holds anything).
        void resize(std::size_t n) { e_.assign(n, Entry{}); }
        std::size_t size() const noexcept { return e_.size(); }
        std::size_t bytes() const noexcept { return e_.capacity() * sizeof(Entry); }
        std::int32_t holders(std::uint32_t i) const noexcept { return e_[i].hold; }

        /// Once per tick, before its first message (contract 1): the dirty
        /// bits of every index in changed are taken, and every changed entity
        /// some connection holds is loaded and encoded (once, for all of them).
        /// Three passes, so the misses of different entities overlap.
        template <class Range, class Src> void publish(XSyncPublisher& pub, const Range& changed, Src&& src) {
            if (pub.dispatching_) [[unlikely]] sd_detail::sync_fail("xsync: publish after the tick's first message");
            const std::uint32_t tick = pub.tick_;
#if XOFFSET_SYNC_CHECKS
            seen_tick_ = tick;
#endif
            Entry* const es = e_.data();
            for (const auto i : changed) XOFFSET_DETAIL_PREFETCH(&es[std::size_t(i)], 0);
            const T* images = nullptr;
            if constexpr (requires { src.images(); }) images = src.images();
            const auto image_at = [&](std::uint32_t i) -> const T* {
                if constexpr (requires { src.images(); }) return images + i;
                else return &src.image(i);
            };
            for (const auto i : changed) {
                Entry& e = es[std::size_t(i)];
                if (e.ptick != tick) { e.old |= e.now; e.now = 0; e.ptick = tick; }
                e.now |= src.dirty(std::uint32_t(i));
                if (e.hold > 0) {
                    const char* r = reinterpret_cast<const char*>(image_at(std::uint32_t(i)));
                    for (std::size_t o = 0; o < sizeof(T); o += 64) XOFFSET_DETAIL_PREFETCH(r + o, 0);
                }
            }
            for (const auto i : changed) {
                Entry& e = es[std::size_t(i)];
                if (e.hold > 0 && e.rtick != tick) fill<sd_detail::src_arg<Src>>(e, std::uint32_t(i), pub, src);
            }
        }

        /// A leave section: the slots the client keeps the entities at index
        /// in (one per index); each entity has one holder fewer. The slots are
        /// free from the next message on.
        template <class Msg>
        XOFFSET_SYNC_INLINE void leaves(Msg& m, std::span<const std::int32_t> index, std::span<const std::int32_t> slot) {
            constexpr std::size_t J = Msg::conn_type::template index<K>;
            static_assert(J < Msg::conn_type::keyed, "xsync: the connection has no free slots for this table");
            const std::size_t n = slot.size();
            if (!n) return;
            Msg::count(n);
#if XOFFSET_SYNC_CHECKS
            m.check_tick();
            if (index.size() != n) sd_detail::sync_fail("xsync: leaves() takes an index per slot");
            if (m.ln_[J]) sd_detail::sync_fail("xsync: two leave sections of one table in a message");
            for (std::size_t i = 0; i < n; ++i) {
                Entry& e = e_[std::size_t(index[i])];
                m.template check_slot<J>(std::uint32_t(slot[i]), std::uint32_t(index[i]) + 1);
                if (e.hold <= 0) sd_detail::sync_fail("xsync: leave of an entity nobody holds");
                if (e.tick == m.pub_->tick_) ++e.left;
            }
#endif
            m.room(4 + 4 * n);
            char* const q = m.q_;
            sd_detail::store<std::uint32_t>(q, sd_detail::section(n, K, XSyncKind::leave));
            const std::int32_t* const s = slot.data();
            XOFFSET_DETAIL_NOVECTOR
            for (std::size_t i = 0; i < n; ++i) sd_detail::store<std::uint32_t>(q + 4 + 4 * i, std::uint32_t(s[i]));
            m.loff_[J] = std::uint32_t(q + 4 - m.out_->data());
            m.ln_[J] = std::uint32_t(n);
            m.q_ = q + 4 + 4 * n;
            const std::int32_t* const ix = index.data();
            Entry* const es = e_.data();
            XOFFSET_DETAIL_NOVECTOR
            for (std::size_t i = 0; i < n; ++i) --es[std::size_t(ix[i])].hold;
        }

        /// A change section: for each index the client holds (at slot), its
        /// change body of this tick, if it has one.
        template <class Msg, class Src>
        XOFFSET_SYNC_INLINE void changes(Msg& m, Src&& src, std::span<const std::int32_t> index, std::span<const std::int32_t> slot) {
            const std::size_t n = index.size();
            if (!n) return;
            Msg::count(n);
            const std::int32_t* const ix = index.data();
            const std::int32_t* const sl = slot.data();
            Entry* const es = e_.data();
            // The whole list is in hand: its entries are fetched first, so their misses overlap.
            XOFFSET_DETAIL_NOVECTOR
            for (std::size_t r = 0; r < n; ++r) XOFFSET_DETAIL_PREFETCH(&es[std::size_t(ix[r])], 0);
#if XOFFSET_SYNC_CHECKS
            m.check_tick();
            if (slot.size() != n) sd_detail::sync_fail("xsync: changes() takes a slot per index");
            for (std::size_t r = 0; r < n; ++r)
                m.template check_slot<Msg::conn_type::template index<K>>(std::uint32_t(sl[r]), std::uint32_t(ix[r]) + 1);
#endif
            m.room(4 + n * kChangeRoom);
            char* q = m.q_;
            char* sec = q;
            q += 4;
            const std::uint32_t tick = m.pub_->tick_;
            std::uint32_t k = 0;
            for (std::size_t r = 0; r < n; ++r) {
                Entry& e = es[std::size_t(ix[r])];
                if (e.rtick != tick) fill<sd_detail::src_arg<Src>>(e, std::uint32_t(ix[r]), *m.pub_, src);
                if (e.tick == tick) {
                    if (e.len > ChangeOp) [[unlikely]] m.room(q, sec, (n - r) * kChangeRoom + e.len);
                    sd_detail::copy_body(q, m.bodies() + e.off, e.len);
                    sd_detail::store<std::uint32_t>(q, std::uint32_t(sl[r]));
                    q += e.len;
                    ++k;
#if XOFFSET_SYNC_CHECKS
                    ++e.sent;
#endif
                }
            }
            sd_detail::store<std::uint32_t>(sec, sd_detail::section(k, K, XSyncKind::change));
            m.q_ = k ? q : sec;
        }

        /// An enter section: each entity at index enters the client's view
        /// with its whole body (written once per tick, on first use), at a
        /// free slot; out(r, slot) gets the slot the r-th one takes (store it
        /// with the application's record, contract 4). Each entity has one
        /// holder more.
        template <class Msg, class Src, class Out>
        XOFFSET_SYNC_INLINE void enters(Msg& m, Src&& src, std::span<const std::int32_t> index, Out&& out) {
            constexpr std::size_t J = Msg::conn_type::template index<K>;
            static_assert(J < Msg::conn_type::keyed, "xsync: the connection has no free slots for this table");
            const std::size_t n = index.size();
            if (!n) return;
            Msg::count(n);
            const std::int32_t* const ix = index.data();
            Entry* const es = e_.data();
            XOFFSET_DETAIL_NOVECTOR
            for (std::size_t r = 0; r < n; ++r) XOFFSET_DETAIL_PREFETCH(&es[std::size_t(ix[r])], 1);
#if XOFFSET_SYNC_CHECKS
            m.check_tick();
#endif
            m.room(4 + n * kEnterRoom);
            typename Msg::template Taker<J> take(m);
            char* q = m.q_;
            char* sec = q;
            q += 4;
            const std::uint32_t tick = m.pub_->tick_;
            for (std::size_t r = 0; r < n; ++r) {
                const std::uint32_t i = std::uint32_t(ix[r]);
                Entry& e = es[i];
                if (e.ktick != tick) key<sd_detail::src_arg<Src>>(e, i, *m.pub_, src);
                ++e.hold;
                if (e.klen > EnterOp) [[unlikely]] m.room(q, sec, (n - r) * kEnterRoom + e.klen);
                const std::uint32_t s = take.take();
#if XOFFSET_SYNC_CHECKS
                m.template check_take<J>(s, i);
#endif
                out(r, s);
                sd_detail::copy_body(q, m.bodies() + e.koff, e.klen);
                sd_detail::store<std::uint32_t>(q, s);
                q += e.klen;
            }
            take.done();
            sd_detail::store<std::uint32_t>(sec, sd_detail::section(n, K, XSyncKind::enter));
            m.q_ = q;
        }

        /// One holder fewer, no message: a connection that closes, or that
        /// gets a key message, releases every entity it held (contract 3).
        void release(std::uint32_t i) {
#if XOFFSET_SYNC_CHECKS
            if (e_[i].hold <= 0) sd_detail::sync_fail("xsync: release of an entity nobody holds");
            if (e_[i].tick == last_tick_seen()) ++e_[i].left;
#endif
            --e_[i].hold;
        }

#if XOFFSET_SYNC_CHECKS
        /// Checks contract 2 over every entry whose change body is of tick:
        /// each holder got it (call at the end of a tick, after its messages).
        void verify(const XSyncPublisher& pub) const {
            for (const Entry& e : e_) if (e.tick == pub.tick_) check_sent(e);
        }
#endif

    private:
        static constexpr std::size_t kChangeRoom = ChangeOp + kSyncSlack;
        static constexpr std::size_t kEnterRoom = EnterOp + kSyncSlack;

#if XOFFSET_SYNC_CHECKS
        static void check_sent(const Entry& e) {
            if (e.sent + e.left != e.h0)
                sd_detail::sync_fail("xsync: a connection holding an entity missed its change body (contract 2)");
        }
        std::uint32_t last_tick_seen() const noexcept { return seen_tick_; }
        std::uint32_t seen_tick_ = kSyncNever;
#endif

        // The tick's first use of entity i.
        template <class Src> XOFFSET_DETAIL_NOINLINE void fill(Entry& e, std::uint32_t i, XSyncPublisher& pub, Src src) {
            const std::uint32_t tick = pub.tick_;
#if XOFFSET_SYNC_CHECKS
            seen_tick_ = tick;
            if (e.tick != kSyncNever && e.tick != tick) check_sent(e);
#endif
            e.rtick = tick;
            const std::uint64_t now = e.ptick == tick ? e.now : 0;
            const std::uint64_t old = e.old | (e.ptick == tick ? 0 : e.now);
            const std::uint64_t m = now | old;
            e.now = e.old = 0;
            if (!m) return;
            T& img = src.image(i);
            if (old) { src.load(i, img, m); return; }   // nobody held it: the image catches up, nothing is sent
            src.load(i, img, now);
            e.off = std::uint32_t(pub.bodies_.size());
            e.len = std::uint32_t(put_change_op(pub.bodies_, i, img, now));
            e.tick = tick;
#if XOFFSET_SYNC_CHECKS
            e.h0 = std::uint32_t(e.hold); e.left = 0; e.sent = 0;
#endif
        }
        // The first use if not yet, then the enter body of this tick.
        template <class Src> XOFFSET_DETAIL_NOINLINE void key(Entry& e, std::uint32_t i, XSyncPublisher& pub, Src src) {
            if (e.rtick != pub.tick_) fill<Src>(e, i, pub, src);
            e.koff = std::uint32_t(pub.bodies_.size());
            e.klen = std::uint32_t(put_whole_op(pub.bodies_, i, src.image(i)));
            e.ktick = pub.tick_;
        }

        std::vector<Entry> e_;
    };

    // ---- image tables ------------------------------------------------------------------

    /// Entries by id, the id the index (0 to size - 1).
    struct XSyncDense {};
    /// Entries in a ring of a power-of-two size, at id & (size - 1), for ids
    /// that only grow (entities with similar lives): the image's member Id
    /// says which id an entry holds. When a live entity holds the place an id
    /// wants, the ring doubles; a holder whose id is below Src::min_live()
    /// has left the world and its place is reused.
    template <auto Id> struct XSyncRing {};

    namespace sd_detail {
        template <class Index> struct ring_id { static constexpr bool ring = false; };
        template <auto Id> struct ring_id<XSyncRing<Id>> {
            static constexpr bool ring = true;
            template <class T> static auto get(const T& r) noexcept { return r.*Id; }
        };
        constexpr std::size_t round16(std::size_t n) noexcept { return (n + 15) & ~std::size_t(15); }
    } // namespace sd_detail

    /// Plain records (no containers) compared whole: the image every holder
    /// has sits in the entry, and the tick's first use of an entity compares
    /// it with the record Src loads and writes the members that differ. An
    /// entity nobody uses in a tick keeps its old image; the next use catches
    /// up in one comparison (so every holder must get every body: contract 2).
    ///
    /// Keyed (enters and leaves, slots from the connection) or not (a fixed
    /// table: changes, and enters in key messages; ops carry the id).
    /// Index: XSyncDense (ids 0 to size - 1, the ref is the id) or
    /// XSyncRing<&T::id> (the ref is the application's handle, Src::id(ref)
    /// gives the id). Align: the entry's alignment (64: one line each).
    ///
    /// Src:
    ///   void load(std::uint32_t ref, T& record);   the record now
    ///   std::uint32_t id(std::uint32_t ref);       ring only
    ///   auto min_live();                           ring only: the lowest id in the world (asked at most once a tick)
    template <unsigned K, class T, class Index, bool Keyed, std::size_t Align = alignof(T)>
    class XSyncImageTable {
        static_assert(K >= 1 && K < kSyncTables, "xsync: tables 1 to 63");
        static_assert(XRecordBody<T>::plain, "xsync: an image table holds records without containers");
        using RI = sd_detail::ring_id<Index>;
        static constexpr bool kRing = RI::ring;
        static_assert(kRing || std::is_same_v<Index, XSyncDense>, "xsync: Index is XSyncDense or XSyncRing<&T::id>");

    public:
        struct alignas(Align) Entry : sd_detail::EntryHead {
            T image{};
        };
        static constexpr std::size_t entry_size = sizeof(Entry);
        static constexpr unsigned table = K;

        /// Dense: n entries. Ring: at least n places (a power of two).
        void resize(std::size_t n) {
            if constexpr (kRing) n = std::bit_ceil(std::max<std::size_t>(n, 1));
            e_.assign(n, Entry{});
            ml_tick_ = kSyncNever;
        }
        std::size_t capacity() const noexcept { return e_.size(); }
        std::size_t bytes() const noexcept { return e_.capacity() * sizeof(Entry); }
        /// The entry of id fetched now (an entity the tick will use).
        void prefetch(std::uint32_t id) const noexcept {
            if constexpr (kRing) XOFFSET_DETAIL_PREFETCH(&e_[id & (e_.size() - 1)], 1);
            else XOFFSET_DETAIL_PREFETCH(&e_[id], 1);
        }

        /// A leave section (keyed tables): the slots that leave.
        template <class Msg> XOFFSET_SYNC_INLINE void leaves(Msg& m, std::span<const std::int32_t> slot) requires Keyed {
            constexpr std::size_t J = Msg::conn_type::template index<K>;
            static_assert(J < Msg::conn_type::keyed, "xsync: the connection has no free slots for this table");
            const std::size_t n = slot.size();
            if (!n) return;
            Msg::count(n);
#if XOFFSET_SYNC_CHECKS
            m.check_tick();
            if (m.ln_[J]) sd_detail::sync_fail("xsync: two leave sections of one table in a message");
            for (std::size_t i = 0; i < n; ++i) m.template check_slot<J>(std::uint32_t(slot[i]), 0);
#endif
            m.room(4 + 4 * n);
            char* const q = m.q_;
            sd_detail::store<std::uint32_t>(q, sd_detail::section(n, K, XSyncKind::leave));
            const std::int32_t* const s = slot.data();
            XOFFSET_DETAIL_NOVECTOR
            for (std::size_t i = 0; i < n; ++i) sd_detail::store<std::uint32_t>(q + 4 + 4 * i, std::uint32_t(s[i]));
            m.loff_[J] = std::uint32_t(q + 4 - m.out_->data());
            m.ln_[J] = std::uint32_t(n);
            m.q_ = q + 4 + 4 * n;
        }

        /// A change section (keyed): for each ref the client holds (at slot),
        /// the members that changed since its image, if any.
        template <class Msg, class Src>
        XOFFSET_SYNC_INLINE void changes(Msg& m, Src&& src, std::span<const std::int32_t> ref, std::span<const std::int32_t> slot) requires Keyed {
            const std::size_t n = ref.size();
            if (!n) return;
            Msg::count(n);
#if XOFFSET_SYNC_CHECKS
            m.check_tick();
            if (slot.size() != n) sd_detail::sync_fail("xsync: changes() takes a slot per ref");
            for (std::size_t r = 0; r < n; ++r)
                m.template check_slot<Msg::conn_type::template index<K>>(std::uint32_t(slot[r]), id_of(src, std::uint32_t(ref[r])) + 1);
#endif
            const std::int32_t* const rf = ref.data();
            const std::int32_t* const sl = slot.data();
            m.room(4 + n * kChangeRoom);
            char* q = m.q_;
            char* const sec = q;
            q += 4;
            const std::uint32_t tick = m.pub_->tick_;
            Entry* es = e_.data();
            std::size_t mk = e_.size() - 1;
            std::uint32_t k = 0;
            for (std::size_t r = 0; r < n; ++r) {
                const std::uint32_t rr = std::uint32_t(rf[r]);
                Entry* e = find(es, mk, rr, tick, *m.pub_, src);
                std::memcpy(q, m.bodies() + e->off, kChangeOp);
                sd_detail::store<std::uint32_t>(q, std::uint32_t(sl[r]));
                q += e->len;
                k += e->len != 0;
            }
            sd_detail::store<std::uint32_t>(sec, sd_detail::section(k, K, XSyncKind::change));
            m.q_ = k ? q : sec;
        }
        /// A change section (fixed tables): for each id, the members that
        /// changed since its image, if any.
        template <class Msg, class Src>
        XOFFSET_SYNC_INLINE void changes(Msg& m, Src&& src, std::span<const std::int32_t> id) requires (!Keyed) {
            const std::size_t n = id.size();
            if (!n) return;
            Msg::count(n);
#if XOFFSET_SYNC_CHECKS
            m.check_tick();
#endif
            const std::int32_t* const ids = id.data();
            m.room(4 + n * kChangeRoom);
            char* q = m.q_;
            char* const sec = q;
            q += 4;
            const std::uint32_t tick = m.pub_->tick_;
            Entry* es = e_.data();
            std::size_t mk = e_.size() - 1;
            std::uint32_t k = 0;
            for (std::size_t r = 0; r < n; ++r) {
                const Entry* e = find(es, mk, std::uint32_t(ids[r]), tick, *m.pub_, src);
                if (e->len) { sd_detail::copy_body(q, m.bodies() + e->off, e->len); q += e->len; ++k; }
            }
            sd_detail::store<std::uint32_t>(sec, sd_detail::section(k, K, XSyncKind::change));
            m.q_ = k ? q : sec;
        }

        /// An enter section (keyed): each ref enters with its whole record at a
        /// free slot; out(r, slot) gets the slot of the r-th (contract 4).
        template <class Msg, class Src, class Out>
        XOFFSET_SYNC_INLINE void enters(Msg& m, Src&& src, std::span<const std::int32_t> ref, Out&& out) requires Keyed {
            constexpr std::size_t J = Msg::conn_type::template index<K>;
            static_assert(J < Msg::conn_type::keyed, "xsync: the connection has no free slots for this table");
            const std::size_t n = ref.size();
            if (!n) return;
            Msg::count(n);
#if XOFFSET_SYNC_CHECKS
            m.check_tick();
#endif
            const std::int32_t* const rf = ref.data();
            m.room(4 + n * kEnterRoom);
            typename Msg::template Taker<J> take(m);
            char* q = m.q_;
            sd_detail::store<std::uint32_t>(q, sd_detail::section(n, K, XSyncKind::enter));
            q += 4;
            const std::uint32_t tick = m.pub_->tick_;
            Entry* es = e_.data();
            std::size_t mk = e_.size() - 1;
            for (std::size_t r = 0; r < n; ++r) {
                const std::uint32_t rr = std::uint32_t(rf[r]);
                const Entry* e = find(es, mk, rr, tick, *m.pub_, src);
                const std::uint32_t s = take.take();
#if XOFFSET_SYNC_CHECKS
                m.template check_take<J>(s, id_of(src, rr));
#endif
                out(r, s);
                sd_detail::store<std::uint32_t>(q, s);
                q = XRecordBody<T>::put_whole(q + 4, e->image);
            }
            take.done();
            m.q_ = q;
        }
        /// An enter section (fixed tables, key messages): each id with its
        /// whole record.
        template <class Msg, class Src>
        XOFFSET_SYNC_INLINE void enters(Msg& m, Src&& src, std::span<const std::int32_t> id) requires (!Keyed) {
            const std::size_t n = id.size();
            if (!n) return;
            Msg::count(n);
#if XOFFSET_SYNC_CHECKS
            m.check_tick();
#endif
            const std::int32_t* const ids = id.data();
            m.room(4 + n * kEnterRoom);
            char* q = m.q_;
            sd_detail::store<std::uint32_t>(q, sd_detail::section(n, K, XSyncKind::enter));
            q += 4;
            const std::uint32_t tick = m.pub_->tick_;
            Entry* es = e_.data();
            std::size_t mk = e_.size() - 1;
            for (std::size_t r = 0; r < n; ++r) {
                const std::uint32_t i = std::uint32_t(ids[r]);
                const Entry* e = find(es, mk, i, tick, *m.pub_, src);
                sd_detail::store<std::uint32_t>(q, i);
                q = XRecordBody<T>::put_whole(q + 4, e->image);
            }
            m.q_ = q;
        }

    private:
        // An op's bytes at most (id, mask, every member), copied whole.
        static constexpr std::size_t kChangeOp = sd_detail::round16(4 + XRecordBody<T>::diff_room);
        static constexpr std::size_t kChangeRoom = kChangeOp + kSyncSlack;
        static constexpr std::size_t kEnterRoom = 4 + XRecordBody<T>::whole_body::fixed + kSyncSlack;

        template <class Src> static std::uint32_t id_of(Src& src, std::uint32_t ref) {
            if constexpr (kRing) return std::uint32_t(src.id(ref));
            else return ref;
        }
        // The entry of ref, used this tick: the hot test inline, the rest not.
        template <class Src>
        XOFFSET_SYNC_INLINE Entry* find(Entry*& es, std::size_t& mk, std::uint32_t ref, std::uint32_t tick, XSyncPublisher& pub, Src& src) {
            if constexpr (kRing) {
                const std::uint32_t id = std::uint32_t(src.id(ref));
                Entry* e = &es[id & mk];
                if (std::uint32_t(RI::get(e->image)) != id || e->tick != tick) [[unlikely]] {
                    e = &fill<sd_detail::src_arg<Src&>>(ref, id, pub, src);
                    es = e_.data();
                    mk = e_.size() - 1;
                }
                return e;
            } else {
                Entry* e = &es[ref];
                if (e->tick != tick) [[unlikely]] fill<sd_detail::src_arg<Src&>>(ref, ref, pub, src);
                return e;
            }
        }
        // The tick's first use of id (ref, for Src): a new entry takes the
        // record as its image (no body); a known one writes what differs.
        template <class Src> XOFFSET_DETAIL_NOINLINE Entry& fill(std::uint32_t ref, std::uint32_t id, XSyncPublisher& pub, Src src) {
            Entry* e;
            if constexpr (kRing) {
                e = &e_[id & (e_.size() - 1)];
                if (e->tick != kSyncNever && std::uint32_t(RI::get(e->image)) != id) {
                    if (RI::get(e->image) >= min_live(pub, src)) e = &grow(id, pub, src);
                    else *e = Entry{};
                }
            } else e = &e_[id];
            if (e->tick != pub.tick_) {
                XOFFSET_INDETERMINATE T img;
                src.load(ref, img);
                XSyncOut& b = pub.bodies_;
                e->off = std::uint32_t(b.size());
                if (e->tick == kSyncNever) {
                    e->image = img;
                    e->len = 0;
                    (void)b.tail(kChangeOp + kSyncSlack);   // changes() reads kChangeOp bytes at off
                } else {
                    e->len = std::uint32_t(put_diff_op(b, id, img, e->image));
                }
                e->tick = pub.tick_;
            }
            return *e;
        }
        template <class Src> auto min_live(XSyncPublisher& pub, Src& src) {
            if (ml_tick_ != pub.tick_) { ml_tick_ = pub.tick_; ml_ = decltype(ml_)(src.min_live()); }
            return ml_;
        }
        // A live entity holds id's place: the ring doubles until every live
        // entry has a place of its own. Returns id's (new) entry.
        template <class Src> Entry& grow(std::uint32_t id, XSyncPublisher& pub, Src& src) requires kRing {
            for (std::size_t n = e_.size() * 2;; n *= 2) {
                std::vector<Entry> r(n);
                bool ok = true;
                for (const Entry& e : e_) {
                    if (e.tick == kSyncNever || RI::get(e.image) < min_live(pub, src)) continue;
                    Entry& to = r[std::size_t(std::uint32_t(RI::get(e.image))) & (n - 1)];
                    if (to.tick != kSyncNever) { ok = false; break; }
                    to = e;
                }
                if (!ok || r[id & (n - 1)].tick != kSyncNever) continue;
                e_.swap(r);
                return e_[id & (n - 1)];
            }
        }

        std::vector<Entry> e_;
        std::uint32_t ml_tick_ = kSyncNever;
        std::conditional_t<kRing, std::int64_t, std::uint32_t> ml_ = 0;
    };

} // namespace XOffsetDatastructure::xsync

#endif
