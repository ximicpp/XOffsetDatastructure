// X4 of docs/positioning/MUTABLE_RELOCATABLE.md (section 5): same-host handoff
// of a live XOffset buffer to another process through a sealed memfd.
//
// The sender copies the payload once into a new memfd (save_sealed), seals it
// against writes and resizing, and passes the descriptor over a Unix socket
// (SCM_RIGHTS). The receiver checks the seals and adopts the payload pages
// copy-on-write (load_verified_sealed): full structural validation runs in
// place, nothing is copied, and later writes copy only the pages they touch.
// Because the file is sealed before the receiver looks at it, the bytes it
// validated cannot change afterwards (the exclusivity obligation O7).
//
// Compared, per handoff (parent -> child -> 1-byte ack, round trip):
//   stream   save_verified + write(2) the wire, child read(2) + load_verified
//   fd+copy  the same wire bytes in a sealed memfd, child mmap + load_verified
//   adopt    save_sealed, child load_verified_sealed (no receive-side copy)
//   move     the arena is a shared memfd from the start (create_shared);
//            release_sealed gives the payload up without any copy, child
//            load_verified_sealed. The sender's buffer is empty afterwards.
// Child action after the load: "load" only touches the root, "edit" runs a few
// SLG edits on it (copy-on-write cost for adopt). The child digests the whole
// state on the first handoff of each run (checked, not timed).
//
// Linux only. Cloud VM numbers: ratios and orders of magnitude only.
// Build (GCC 16): see build_handoff.sh next to this file.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "xoffsetdatastructure.hpp"
#include "slg/generic.hpp"
#include "slg/model_std.hpp"
#include "slg/model_xoffset.hpp"
#include "slg/trace.hpp"

using namespace XOffsetDatastructure;
using Clock = std::chrono::steady_clock;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    std::printf("  check %-52s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) ++failures;
}

XBuffer::MaximumCapacity cap() {
    return XBuffer::maximum_capacity(std::size_t(256) << 20).with_growth(XGrowPolicy::Automatic);
}

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// ---- socket helpers ---------------------------------------------------------

void write_all(int s, const void* p, std::size_t n) {
    auto* c = static_cast<const char*>(p);
    while (n) {
        const auto w = ::write(s, c, n);
        if (w <= 0) { std::perror("write"); std::exit(2); }
        c += w; n -= std::size_t(w);
    }
}
void read_all(int s, void* p, std::size_t n) {
    auto* c = static_cast<char*>(p);
    while (n) {
        const auto r = ::read(s, c, n);
        if (r <= 0) { std::perror("read"); std::exit(2); }
        c += r; n -= std::size_t(r);
    }
}
void send_fd(int s, int fd) {
    char byte = 'F';
    iovec iov{&byte, 1};
    alignas(cmsghdr) char ctl[CMSG_SPACE(sizeof(int))]{};
    msghdr msg{};
    msg.msg_iov = &iov; msg.msg_iovlen = 1;
    msg.msg_control = ctl; msg.msg_controllen = sizeof ctl;
    cmsghdr* c = CMSG_FIRSTHDR(&msg);
    c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS; c->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(c), &fd, sizeof(int));
    if (::sendmsg(s, &msg, 0) != 1) { std::perror("sendmsg"); std::exit(2); }
}
int recv_fd(int s) {
    char byte = 0;
    iovec iov{&byte, 1};
    alignas(cmsghdr) char ctl[CMSG_SPACE(sizeof(int))]{};
    msghdr msg{};
    msg.msg_iov = &iov; msg.msg_iovlen = 1;
    msg.msg_control = ctl; msg.msg_controllen = sizeof ctl;
    if (::recvmsg(s, &msg, MSG_CMSG_CLOEXEC) != 1) { std::perror("recvmsg"); std::exit(2); }
    cmsghdr* c = CMSG_FIRSTHDR(&msg);
    if (!c || c->cmsg_type != SCM_RIGHTS) { std::fprintf(stderr, "no fd\n"); std::exit(2); }
    int fd;
    std::memcpy(&fd, CMSG_DATA(c), sizeof(int));
    return fd;
}

// The wire bytes of save_verified in a sealed memfd (the fd+copy path).
int sealed_copy_of(const std::string& wire) {
    const int fd = ::memfd_create("xoffset-wire-copy", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0 || ::ftruncate(fd, off_t(wire.size())) != 0 ||
        ::pwrite(fd, wire.data(), wire.size(), 0) != ssize_t(wire.size()) ||
        ::fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_SEAL) != 0) {
        std::perror("sealed_copy_of"); std::exit(2);
    }
    return fd;
}

enum class Path : int { Stream = 0, FdCopy = 1, Adopt = 2, Move = 3, AckOnly = 4 };
const char* path_name(Path p) {
    switch (p) {
    case Path::Stream: return "stream (save_verified+write / read+load_verified)";
    case Path::FdCopy: return "fd+copy (wire in sealed memfd / mmap+load_verified)";
    case Path::Adopt: return "adopt (save_sealed / load_verified_sealed)";
    case Path::Move: return "move (release_sealed / load_verified_sealed)";
    case Path::AckOnly: return "ack only (1-byte round trip)";
    }
    return "?";
}

// ---- one state, all paths ---------------------------------------------------

template <class XT, class Dig, class Edit>
void handoff_family(const char* label, XBuffer& src, int reps, Dig&& dig, Edit&& edit) {
    const std::size_t used = std::size_t(src.arena()->used_end);
    std::printf("\n-- %s: payload %zu B, %zu free blocks --\n", label, used, src.free_block_count());
    slg::Digest want;
    dig(want, src.root<XT>());

    for (int edit_mode = 0; edit_mode < 2; ++edit_mode) {
        std::printf("   child action: %s\n", edit_mode ? "edit (SLG edits after load)" : "load (touch the root only)");
        double ack = 0;
        for (Path path : {Path::AckOnly, Path::Stream, Path::FdCopy, Path::Adopt, Path::Move}) {
            int sv[2];
            if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { std::perror("socketpair"); std::exit(2); }
            const pid_t pid = ::fork();
            if (pid == 0) {
                ::close(sv[0]);
                const int s = sv[1];
                std::string rx;
                std::uint64_t bad = 0;
                for (int i = 0; i < reps; ++i) {
                    std::uint64_t h = 0;
                    if (path == Path::AckOnly) {
                        char c; read_all(s, &c, 1);
                    } else {
                        std::optional<XBuffer> b;
                        if (path == Path::Stream) {
                            std::uint64_t n = 0;
                            read_all(s, &n, sizeof n);
                            rx.resize(n);
                            read_all(s, rx.data(), n);
                            b.emplace(XBuffer::load_verified<XT>(rx, cap()));
                        } else if (path == Path::FdCopy) {
                            const int fd = recv_fd(s);
                            struct stat st{};
                            ::fstat(fd, &st);
                            void* m = ::mmap(nullptr, std::size_t(st.st_size), PROT_READ, MAP_SHARED, fd, 0);
                            b.emplace(XBuffer::load_verified<XT>(
                                std::span<const std::byte>(static_cast<const std::byte*>(m), std::size_t(st.st_size)), cap()));
                            ::munmap(m, std::size_t(st.st_size));
                            ::close(fd);
                        } else {  // Adopt, Move
                            const int fd = recv_fd(s);
                            b.emplace(XBuffer::load_verified_sealed<XT>(fd, cap()));
                            ::close(fd);
                        }
                        auto& root = b->root<XT>();
                        if (i == 0) {
                            slg::Digest g;
                            dig(g, root);
                            h = g.h;
                            if (h != want.h) ++bad;
                        }
                        if (edit_mode) edit(root, std::uint64_t(i));
                        asm volatile("" : : "r"(&root) : "memory");
                    }
                    char c = 'A';
                    write_all(s, &c, 1);
                    (void)h;
                }
                write_all(s, &bad, sizeof bad);
                ::_exit(0);
            }
            ::close(sv[1]);
            const int s = sv[0];
            std::vector<double> t;
            t.reserve(std::size_t(reps));
            std::optional<XBuffer> shared;
            for (int i = 0; i < reps; ++i) {
                if (path == Path::Move) {  // untimed: a fresh shared-file copy of the source
                    shared.emplace(XBuffer::create_shared<XT>(used, cap()));
                    src.clone_into(*shared);
                }
                const auto t0 = Clock::now();
                if (path == Path::AckOnly) {
                    char c = 'P'; write_all(s, &c, 1);
                } else if (path == Path::Stream) {
                    const std::string wire = src.template save_verified<XT>();
                    const std::uint64_t n = wire.size();
                    write_all(s, &n, sizeof n);
                    write_all(s, wire.data(), wire.size());
                } else if (path == Path::FdCopy) {
                    const int fd = sealed_copy_of(src.template save_verified<XT>());
                    send_fd(s, fd);
                    ::close(fd);
                } else if (path == Path::Adopt) {
                    const int fd = src.template save_sealed<XT>();
                    send_fd(s, fd);
                    ::close(fd);
                } else {
                    const int fd = shared->template release_sealed<XT>();
                    send_fd(s, fd);
                    ::close(fd);
                }
                char c; read_all(s, &c, 1);
                if (i > 0)  // the first handoff also digests in the child
                    t.push_back(std::chrono::duration<double, std::nano>(Clock::now() - t0).count());
            }
            std::uint64_t bad = 0;
            read_all(s, &bad, sizeof bad);
            ::close(s);
            int status = 0;
            ::waitpid(pid, &status, 0);
            const double med = median(t);
            if (path == Path::AckOnly) ack = med;
            std::printf("   %-56s %10.0f ns", path_name(path), med);
            if (path != Path::AckOnly) std::printf("   minus ack %10.0f ns", med - ack);
            std::printf("\n");
            if (path != Path::AckOnly)
                check(bad == 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0, "child saw the same digest every time");
        }
    }
}

// ---- in-process pieces and correctness ---------------------------------------

template <class XT, class Dig, class Edit>
void pieces_and_checks(const char* label, XBuffer& src, int reps, Dig&& dig, Edit&& edit) {
    std::printf("\n-- %s: in-process pieces (median ns) --\n", label);
    auto timed = [&](auto&& f) {
        std::vector<double> t;
        for (int i = 0; i < reps; ++i) {
            const auto t0 = Clock::now();
            f();
            t.push_back(std::chrono::duration<double, std::nano>(Clock::now() - t0).count());
        }
        return median(t);
    };
    const std::string wire = src.template save_verified<XT>();
    const int fd = src.template save_sealed<XT>();
    std::printf("   %-44s %10.0f\n", "sender save_verified", timed([&] { auto w = src.template save_verified<XT>(); (void)w; }));
    std::printf("   %-44s %10.0f\n", "sender save_sealed (+close)", timed([&] { ::close(src.template save_sealed<XT>()); }));
    std::printf("   %-44s %10.0f\n", "receiver load_verified (from memory)", timed([&] { auto b = XBuffer::load_verified<XT>(wire, cap()); (void)b; }));
    std::printf("   %-44s %10.0f\n", "receiver load_verified_sealed (same fd)", timed([&] { auto b = XBuffer::load_verified_sealed<XT>(fd, cap()); (void)b; }));
    {
        const std::size_t used = std::size_t(src.arena()->used_end);
        std::vector<double> t;
        for (int i = 0; i < reps; ++i) {
            XBuffer sh = XBuffer::create_shared<XT>(used, cap());
            src.clone_into(sh);
            const auto t0 = Clock::now();
            const int f = sh.template release_sealed<XT>();
            t.push_back(std::chrono::duration<double, std::nano>(Clock::now() - t0).count());
            ::close(f);
        }
        std::printf("   %-44s %10.0f\n", "sender release_sealed (shared arena, +close)", median(t));
    }

    slg::Digest want;
    dig(want, src.root<XT>());
    {   // Same content, same free table, same wire.
        XBuffer b = XBuffer::load_verified_sealed<XT>(fd, cap());
        slg::Digest g;
        dig(g, b.root<XT>());
        check(g.h == want.h, "adopted digest");
        check(b.free_block_count() == src.free_block_count() &&
              b.free_block_bytes() == src.free_block_bytes(), "adopted free table");
        check(b.template save_verified<XT>() == wire, "adopted buffer saves the same wire");
        // Copy-on-write: edits stay private, the sealed file is unchanged.
        for (int i = 0; i < 8; ++i) edit(b.root<XT>(), std::uint64_t(100 + i));
        XBuffer again = XBuffer::load_verified_sealed<XT>(fd, cap());
        slg::Digest g2, g3;
        dig(g2, again.root<XT>());
        dig(g3, b.root<XT>());
        check(g2.h == want.h && g3.h != want.h, "edits after adoption stay private");
        // Growth past the file pages, then a verified round trip.
        XBuffer moved = XBuffer::load_verified_sealed<XT>(fd, cap());
        const auto before = moved.get_size();
        for (int i = 0; i < 64; ++i) edit(moved.root<XT>(), std::uint64_t(1000 + i));
        const std::string w2 = moved.template save_verified<XT>();
        XBuffer back = XBuffer::load_verified<XT>(w2, cap());
        slg::Digest g4, g5;
        dig(g4, moved.root<XT>());
        dig(g5, back.root<XT>());
        check(g4.h == g5.h, "adopted buffer keeps mutating and round-trips");
        std::printf("   (payload %zu -> %zu B after 64 edit rounds)\n", before, moved.get_size());
    }
    {   // Zero-copy move: same content, sender left empty, file sealed.
        XBuffer sh = XBuffer::create_shared<XT>(std::size_t(src.arena()->used_end), cap());
        src.clone_into(sh);
        for (int i = 0; i < 4; ++i) edit(sh.root<XT>(), std::uint64_t(500 + i));  // mutate on the shared arena
        slg::Digest gs;
        dig(gs, sh.root<XT>());
        const std::string ws = sh.template save_verified<XT>();
        const int f = sh.template release_sealed<XT>();
        check(sh.arena() == nullptr, "sender buffer empty after release_sealed");
        const int seals = ::fcntl(f, F_GET_SEALS);
        check((seals & (F_SEAL_WRITE | F_SEAL_SHRINK)) == (F_SEAL_WRITE | F_SEAL_SHRINK), "released file is sealed");
        XBuffer b = XBuffer::load_verified_sealed<XT>(f, cap());
        slg::Digest gb;
        dig(gb, b.root<XT>());
        check(gb.h == gs.h, "moved digest");
        check(b.template save_verified<XT>() == ws, "moved buffer saves the same wire");
        ::close(f);
    }
    {   // Rejections.
        auto rejects = [&](int f) {
            try { auto b = XBuffer::load_verified_sealed<XT>(f, cap()); (void)b; return false; }
            catch (const XException&) { return true; }
        };
        const int unsealed = ::memfd_create("unsealed", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        std::vector<char> file(std::size_t(::lseek(fd, 0, SEEK_END)));
        ::pread(fd, file.data(), file.size(), 0);
        ::ftruncate(unsealed, off_t(file.size()));
        ::pwrite(unsealed, file.data(), file.size(), 0);
        check(rejects(unsealed), "unsealed memfd rejected");
        ::fcntl(unsealed, F_ADD_SEALS, F_SEAL_SHRINK);
        check(rejects(unsealed), "memfd sealed only against shrinking rejected");
        ::close(unsealed);

        auto sealed_variant = [&](auto&& mutate) {
            std::vector<char> v = file;
            mutate(v);
            const int f = ::memfd_create("variant", MFD_CLOEXEC | MFD_ALLOW_SEALING);
            ::ftruncate(f, off_t(v.size()));
            ::pwrite(f, v.data(), v.size(), 0);
            ::fcntl(f, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE);
            return f;
        };
        const long page = ::sysconf(_SC_PAGESIZE);
        int f = sealed_variant([](std::vector<char>& v) { v[offsetof(XWireHeaderV2, schema_hash_lo)] ^= 1; });
        check(rejects(f), "schema hash mismatch rejected");
        ::close(f);
        f = sealed_variant([](std::vector<char>& v) { v.pop_back(); });
        check(rejects(f), "file size mismatch rejected");
        ::close(f);
        // Corrupt every 8-byte word of the root object, one at a time, and
        // compare with load_verified on the same bytes (differential).
        XWireHeaderV2 h{};
        std::memcpy(&h, file.data(), sizeof h);
        int same = 0, total = 0, rejected = 0;
        for (std::size_t off = std::size_t(h.root_offset); off < std::size_t(h.root_offset) + 64 && off + 8 <= h.used_bytes; off += 8) {
            std::uint64_t garbage = 0x7fff'fff0'7fff'fff0ull;
            auto corrupt = [&](std::vector<char>& v) { std::memcpy(v.data() + page + off, &garbage, 8); };
            f = sealed_variant(corrupt);
            const bool r_sealed = rejects(f);
            ::close(f);
            std::string w = wire;
            std::memcpy(w.data() + sizeof(XWireHeaderV2) + off, &garbage, 8);
            bool r_wire = false;
            try { auto b = XBuffer::load_verified<XT>(w, cap()); (void)b; } catch (const XException&) { r_wire = true; }
            same += r_sealed == r_wire;
            rejected += r_sealed;
            ++total;
        }
        std::printf("   corrupted root words: %d of %d rejected; sealed and wire paths agree on %d of %d\n",
                    rejected, total, same, total);
        check(same == total, "sealed path rejects exactly what load_verified rejects");
    }
    ::close(fd);
}

} // namespace

int main(int argc, char** argv) {
    const int reps = argc > 1 ? std::atoi(argv[1]) : 101;
    std::printf("handoff: reps=%d, page %ld B (median of round trips; child is a forked process)\n",
                reps, ::sysconf(_SC_PAGESIZE));
    constexpr std::uint64_t seed = 20261022;
    auto dig_p = [](slg::Digest& g, const auto& p) { slg::dig_player(g, p); };
    auto edit_p = [](auto& p, std::uint64_t i) { slg::season_edits(p, slg::mix_seed(seed, i), 16); };
    auto dig_s = [](slg::Digest& g, const auto& s) { slg::dig_shard(g, s); };
    auto edit_s = [](auto& s, std::uint64_t i) { slg::shard_ticks(s, slg::mix_seed(seed, i), 1); };
    {
        slg::stdm::PlayerState sp = slg::make_player(seed, 3);
        XBuffer xb = XBuffer::create<slg::xm::PlayerState>(64 * 1024, cap());
        slg::copy_player(xb.root<slg::xm::PlayerState>(), sp);
        slg::season_edits(xb.root<slg::xm::PlayerState>(), 5, 256);
        pieces_and_checks<slg::xm::PlayerState>("player", xb, reps, dig_p, edit_p);
        handoff_family<slg::xm::PlayerState>("player", xb, reps, dig_p, edit_p);
    }
    for (const char* scale : {"M", "L"}) {
        const slg::Scale& sc = *slg::find_scale(scale);
        slg::stdm::ShardState ss = slg::make_shard(seed, sc);
        XBuffer xb = XBuffer::create<slg::xm::ShardState>(4 * 1024 * 1024, cap());
        slg::copy_shard(xb.root<slg::xm::ShardState>(), ss);
        slg::shard_ticks(xb.root<slg::xm::ShardState>(), 9, 30);
        const int r = std::max(11, reps / (scale[0] == 'M' ? 4 : 10));
        char label[32];
        std::snprintf(label, sizeof label, "shard %s", scale);
        pieces_and_checks<slg::xm::ShardState>(label, xb, r, dig_s, edit_s);
        handoff_family<slg::xm::ShardState>(label, xb, r, dig_s, edit_s);
    }
    std::printf("\n%s (%d failed checks)\n", failures ? "FAILED" : "all checks passed", failures);
    return failures ? 1 : 0;
}
