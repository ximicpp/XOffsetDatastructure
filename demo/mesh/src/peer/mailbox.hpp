// Device mesh demo: mailbox file names (INTERFACES.md section 6), shared by
// xo_peer --mailbox and the hub's offline plan and score modes. OWNER:
// network thread.
//
// A mailbox is a directory of files; the file name says what a file is,
// which job it belongs to and which device it is for, so a device (or a
// script that copies files to a phone) picks its own without opening the
// others:
//
//   j000123_to101.xmjob            Job payload (HopMeta, hop 0) for route[0] = device 101
//   j000123_h00_to7.xmhop          Hop payload route[0] sent to route[1] = device 7
//   j000123_p01_from7.xmrep        HopReport wire of route[1] (device 7)
//   j000123_p01_from7.xmerr        route[1] read no meta, so no report: the error text
//   build_7.txt                    device 7's build line
//
// Positions and hops: route[0] answers the Job (position 0) and sends hop 0;
// the device at position k >= 1 answers hop k - 1 and sends hop k.
// Writers write `<name>.tmp` and rename it, so a reader never sees half a
// file; anything else in the directory is ignored.
#pragma once
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace xmesh::mailbox {

enum class FileKind : std::uint8_t { Job, Hop, Report, Err };

struct FileName {
    FileKind kind = FileKind::Job;
    std::uint64_t job = 0;
    std::uint32_t number = 0;   // Hop: the hop; Report, Err: the position; Job: 0
    std::uint32_t device = 0;   // Job, Hop: the receiver; Report, Err: the reporter
};

inline std::string job_file(std::uint64_t job, std::uint32_t to) {
    char b[64];
    std::snprintf(b, sizeof b, "j%06llu_to%u.xmjob", static_cast<unsigned long long>(job), unsigned(to));
    return b;
}
inline std::string hop_file(std::uint64_t job, std::uint32_t hop, std::uint32_t to) {
    char b[64];
    std::snprintf(b, sizeof b, "j%06llu_h%02u_to%u.xmhop", static_cast<unsigned long long>(job), unsigned(hop),
                  unsigned(to));
    return b;
}
inline std::string report_file(std::uint64_t job, std::uint32_t pos, std::uint32_t from) {
    char b[64];
    std::snprintf(b, sizeof b, "j%06llu_p%02u_from%u.xmrep", static_cast<unsigned long long>(job), unsigned(pos),
                  unsigned(from));
    return b;
}
inline std::string error_file(std::uint64_t job, std::uint32_t pos, std::uint32_t from) {
    std::string s = report_file(job, pos, from);
    s.replace(s.size() - 3, 3, "err");
    return s;
}
inline std::string build_file(std::uint32_t device) { return "build_" + std::to_string(device) + ".txt"; }

namespace detail {
// Reads decimal digits at `s` (at least one, at most 19); advances `s`.
inline bool number(std::string_view& s, std::uint64_t& out) {
    std::size_t n = 0;
    std::uint64_t v = 0;
    while (n < s.size() && n < 19 && s[n] >= '0' && s[n] <= '9') v = v * 10 + std::uint64_t(s[n++] - '0');
    if (n == 0) return false;
    s.remove_prefix(n);
    out = v;
    return true;
}
inline bool literal(std::string_view& s, std::string_view lit) {
    if (s.substr(0, lit.size()) != lit) return false;
    s.remove_prefix(lit.size());
    return true;
}
} // namespace detail

// The meaning of a mailbox file name; nullopt for anything else.
inline std::optional<FileName> parse(std::string_view name) {
    FileName f;
    std::uint64_t a = 0, b = 0;
    if (!detail::literal(name, "j") || !detail::number(name, f.job) || !detail::literal(name, "_")) return std::nullopt;
    if (detail::literal(name, "to")) {
        if (!detail::number(name, a) || name != ".xmjob") return std::nullopt;
        f.kind = FileKind::Job;
        f.device = std::uint32_t(a);
    } else if (detail::literal(name, "h")) {
        if (!detail::number(name, b) || !detail::literal(name, "_to") || !detail::number(name, a) || name != ".xmhop")
            return std::nullopt;
        f.kind = FileKind::Hop;
        f.number = std::uint32_t(b);
        f.device = std::uint32_t(a);
    } else if (detail::literal(name, "p")) {
        if (!detail::number(name, b) || !detail::literal(name, "_from") || !detail::number(name, a)) return std::nullopt;
        if (name == ".xmrep") f.kind = FileKind::Report;
        else if (name == ".xmerr") f.kind = FileKind::Err;
        else return std::nullopt;
        f.number = std::uint32_t(b);
        f.device = std::uint32_t(a);
    } else {
        return std::nullopt;
    }
    if (a > 0xffffffffull || b > 0xffffffffull || f.job == 0) return std::nullopt;
    return f;
}

} // namespace xmesh::mailbox
