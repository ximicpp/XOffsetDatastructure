// Map state demo: client modes that need no server. OWNER: client thread.
//
// --self-check: the snapshot path's rejection table on this platform. A
// snapshot built here is saved dense, compact and as a zstd frame; each
// must verify and read back the same content (positive controls), and each
// malformed variant must be refused by the codec (before allocation) or by
// load_verified. Nothing malformed may be accepted.
//
// --replay DIR: verifies every snapshot of a corpus that mapdemo_a
// --corpus-dir wrote on another machine and checks that the verified wire
// and the content read from it have the digests A logged. Targets that
// cannot reach the Linux servers (simulators) verify the servers' bytes
// this way.
#include "client_core.hpp"

#include <cinttypes>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <utility>

namespace mapdemo::client {
namespace {

XBuffer build_sample(std::uint32_t units) {
    XBuffer b = XBuffer::create<ViewSnapshot>(4096 + std::size_t(units) * 96, cap_snapshot());
    ViewSnapshot& s = b.root<ViewSnapshot>();
    s.request_id = 77;
    s.tick = 1234;
    s.scene_id = kSceneId;
    s.rect = Rect{{0.0f, 0.0f}, {kViewWidth, kViewHeight}};
    s.units.reserve(units);
    for (std::uint32_t i = 0; i < units; ++i) {
        VisibleUnit& v = s.units.emplace_back();
        v.id = i * 3;
        v.hp = std::int32_t(50 + i % 51);
        v.pos = Point{float(i % 1000) + 0.5f, float(i % 700) + 0.25f};
        v.behavior = static_cast<Behavior>(i % 3);
        // Empty, inline-sized and heap-sized names.
        const std::string name = i % 4 == 0 ? std::string()
                               : i % 4 == 1 ? "u" + std::to_string(i)
                                            : "unit-" + std::to_string(i) + "-with-a-name-longer-than-sso";
        v.name = std::string_view(name);
    }
    return b;
}

std::string request_wire() {
    XBuffer req = XBuffer::create<ViewRequest>(256, cap_view_request());
    ViewRequest& r = req.root<ViewRequest>();
    r.request_id = 77;
    r.scene_id = kSceneId;
    r.rect = Rect{{0.0f, 0.0f}, {kViewWidth, kViewHeight}};
    return req.save_verified<ViewRequest>();
}

// Decodes (zstd) and verifies `payload`; returns "" on acceptance, else how
// it was refused. *digest gets the content digest of an accepted snapshot.
std::string try_accept(codec::Decompressor& unzstd, const std::string& payload, bool zstd,
                       std::uint64_t* digest = nullptr) {
    try {
        std::string plain;
        const std::string* wire = &payload;
        if (zstd) {
            unzstd.decompress(payload, kCapSnapshot, plain);
            wire = &plain;
        }
        XBuffer b = XBuffer::load_verified<ViewSnapshot>(*wire, cap_snapshot());
        if (digest != nullptr) *digest = checks::snapshot_digest(b.root<ViewSnapshot>());
        return std::string();
    } catch (const codec::DecodeError& e) {
        return std::string("codec: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("verify: ") + e.what();
    }
}

void expect_refused(CheckLog& log, codec::Decompressor& unzstd, const char* name, const std::string& payload,
                    bool zstd) {
    const std::string how = try_accept(unzstd, payload, zstd);
    std::fprintf(stdout, "self-check: %-28s -> %s\n", name, how.empty() ? "ACCEPTED" : how.c_str());
    if (how.empty()) {
        log.fail(name, "accepted");
    } else {
        log.pass(name);
        log.note(name, how.substr(0, 160));
    }
}

void expect_same(CheckLog& log, codec::Decompressor& unzstd, const char* name, const std::string& payload, bool zstd,
                 std::uint64_t want) {
    std::uint64_t got = 0;
    const std::string how = try_accept(unzstd, payload, zstd, &got);
    std::string detail = how.empty() ? "" : how;
    if (how.empty() && got != want) {
        char buf[96];
        std::snprintf(buf, sizeof buf, "digest %016" PRIx64 " != %016" PRIx64, got, want);
        detail = buf;
    }
    std::fprintf(stdout, "self-check: %-28s -> %s (%zu bytes)\n", name, detail.empty() ? "verified, same content" : detail.c_str(),
                 payload.size());
    if (detail.empty()) {
        log.pass(name);
        log.note(name, std::to_string(payload.size()) + " bytes");
    } else {
        log.fail(name, detail);
    }
}

std::string flip(std::string s, std::size_t at, unsigned char mask = 1) {
    s[at] = static_cast<char>(static_cast<unsigned char>(s[at]) ^ mask);
    return s;
}

std::string drop_last(std::string s) {
    s.pop_back();
    return s;
}

bool write_results(const Options& opt, const CheckLog& log, const metrics::Recorder* rec) {
    if (opt.csv_dir.empty()) return true;
    if (!log.write_csv(join_path(opt.csv_dir, "client_checks.csv"))) return false;
    return rec == nullptr || rec->write_csv(join_path(opt.csv_dir, "client_metrics.csv"));
}

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> out;
    std::string field;
    std::istringstream in(line);
    while (std::getline(in, field, ',')) out.push_back(field);
    if (!line.empty() && line.back() == ',') out.emplace_back();
    return out;
}

bool read_file(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !f.bad();
}

}   // namespace

int run_self_check(const Options& opt) {
    CheckLog log;
    if (!codec::zstd_available()) {
        log.fail("zstd_available", "built without zstd (MAPDEMO_WITH_ZSTD=OFF)");
        log.print(stdout);
        write_results(opt, log, nullptr);
        return 1;
    }
    log.pass("zstd_available");
    codec::Decompressor unzstd;
    codec::Compressor zstd;

    XBuffer sample = build_sample(200);
    const std::uint64_t want = checks::snapshot_digest(sample.root<ViewSnapshot>());
    const std::string dense = sample.save_verified<ViewSnapshot>();
    const std::string compact = sample.save_compact<ViewSnapshot>(XCompactOptions{0});
    std::string frame;
    zstd.compress(compact, frame);

    // Positive controls: every encoding verifies and reads the same content.
    expect_same(log, unzstd, "accept_dense", dense, false, want);
    expect_same(log, unzstd, "accept_compact", compact, false, want);
    expect_same(log, unzstd, "accept_zstd", frame, true, want);
    {
        std::string plain;
        unzstd.decompress(frame, kCapSnapshot, plain);
        if (plain == compact) log.pass("zstd_round_trip_bytes");
        else log.fail("zstd_round_trip_bytes", "decompressed bytes differ from the compact wire");
    }
    if (compact.size() <= dense.size() && frame.size() < compact.size()) {
        log.pass("sizes_shrink");
        log.note("sizes_shrink", "dense=" + std::to_string(dense.size()) + " compact=" + std::to_string(compact.size()) +
                                     " zstd=" + std::to_string(frame.size()));
    } else {
        log.fail("sizes_shrink", "dense=" + std::to_string(dense.size()) + " compact=" + std::to_string(compact.size()) +
                                     " zstd=" + std::to_string(frame.size()));
    }
    {   // One kept buffer through every encoding, and through a rejected wire.
        SnapshotHolder h;
        std::string plain;
        unzstd.decompress(frame, kCapSnapshot, plain);
        std::string why;
        const std::string* order[] = {&dense, &compact, &plain, &dense};
        for (const std::string* w : order) {
            h.reload(*w);
            if (checks::snapshot_digest(h.view()) != want) why = "digest differs after reload";
        }
        try {
            h.reload(drop_last(compact));
            why = "truncated wire accepted by reload";
        } catch (const std::exception&) {
        }
        if (!h.empty()) why = "holder not empty after a rejected reload";
        h.reload(compact);
        if (checks::snapshot_digest(h.view()) != want) why = "digest differs after the reload that followed a rejection";
        if (why.empty()) log.pass("reload_across_encodings");
        else log.fail("reload_across_encodings", why);
    }

    // The XOffset wire: truncation, every header byte, the wrong schema.
    expect_refused(log, unzstd, "dense_truncated", drop_last(dense), false);
    expect_refused(log, unzstd, "compact_truncated", drop_last(compact), false);
    expect_refused(log, unzstd, "empty_payload", std::string(), false);
    expect_refused(log, unzstd, "wrong_schema", request_wire(), false);
    {
        const std::size_t header = std::min<std::size_t>(40, compact.size());
        std::size_t refused = 0;
        std::string first_accepted;
        for (std::size_t i = 0; i < header; ++i) {
            if (!try_accept(unzstd, flip(compact, i), false).empty()) ++refused;
            else if (first_accepted.empty()) first_accepted = "byte " + std::to_string(i);
        }
        std::fprintf(stdout, "self-check: %-28s -> %zu of %zu refused\n", "wire_header_bit_flips", refused, header);
        if (refused == header) {
            log.pass("wire_header_bit_flips");
            log.note("wire_header_bit_flips", std::to_string(header) + " header bytes, each refused");
        } else {
            log.fail("wire_header_bit_flips", "accepted a flip at " + first_accepted);
        }
    }

    // The zstd layer: refused before allocating, or the wire inside is refused.
    expect_refused(log, unzstd, "zstd_empty", std::string(), true);
    expect_refused(log, unzstd, "zstd_truncated", drop_last(frame), true);
    expect_refused(log, unzstd, "zstd_bad_magic", flip(frame, 0), true);
    expect_refused(log, unzstd, "zstd_trailing_byte", frame + std::string(1, '\0'), true);
    expect_refused(log, unzstd, "zstd_two_frames", frame + frame, true);
    expect_refused(log, unzstd, "zstd_no_content_size", codec::zstd_frame_without_size(compact), true);
    expect_refused(log, unzstd, "zstd_skippable_frame", codec::skippable_frame(compact), true);
    expect_refused(log, unzstd, "zstd_dense_wire_unwrapped", dense, true);
    {
        std::string over;
        zstd.compress(std::string(kCapSnapshot + 1, '\0'), over);
        expect_refused(log, unzstd, "zstd_over_cap", over, true);
    }
    {
        std::string inner;
        zstd.compress(drop_last(compact), inner);
        expect_refused(log, unzstd, "zstd_of_truncated_wire", inner, true);
        zstd.compress(request_wire(), inner);
        expect_refused(log, unzstd, "zstd_of_wrong_schema", inner, true);
    }

    log.print(stdout);
    if (!write_results(opt, log, nullptr)) throw std::runtime_error("cannot write client CSV files in " + opt.csv_dir);
    return log.all_ok() ? 0 : 1;
}

int run_replay(const Options& opt) {
    CheckLog log;
    metrics::Recorder rec("c");
    codec::Decompressor unzstd;
    SnapshotHolder holder;
    std::string payload, plain;

    std::ifstream index(join_path(opt.replay_dir, "corpus.csv"));
    if (!index) throw std::runtime_error("cannot read " + join_path(opt.replay_dir, "corpus.csv"));
    std::string line;
    std::getline(index, line);
    std::map<std::string, std::size_t> col;
    {
        const std::vector<std::string> names = split_csv(line);
        for (std::size_t i = 0; i < names.size(); ++i) col[names[i]] = i;
    }
    for (const char* need : {"request_id", "flags", "category", "encoding", "file", "net_bytes", "wire_bytes",
                             "wire_digest", "digest", "visible"})
        if (col.find(need) == col.end()) throw std::runtime_error(std::string("corpus.csv has no column ") + need);

    metrics::CsvFile out;
    const bool csv = !opt.csv_dir.empty();
    if (csv && !out.open(join_path(opt.csv_dir, "client_replay.csv"),
                         "request_id,category,encoding,net_bytes,wire_bytes,wire_digest,digest,decompress_us,load_us,result"))
        throw std::runtime_error("cannot write client_replay.csv in " + opt.csv_dir);

    std::uint64_t rows = 0, matched = 0;
    std::uint64_t by_enc[codec::kEncodingCount] = {};
    while (std::getline(index, line)) {
        if (line.empty()) continue;
        const std::vector<std::string> f = split_csv(line);
        auto at = [&](const char* name) -> const std::string& { return f.at(col[name]); };
        ++rows;
        const std::string tag = "request " + at("request_id");
        std::string why;
        double unzstd_us = -1, load_us = 0;
        std::uint64_t got_wire = 0, got_digest = 0;
        std::size_t wire_bytes = 0;
        const auto vf = codec::parse_view_flags(static_cast<std::uint16_t>(std::stoul(at("flags"))));
        if (!vf) why = "flags " + at("flags") + " not understood";
        else if (at("encoding") != codec::encoding_name(vf->encoding)) why = "encoding column disagrees with flags";
        else if (!read_file(join_path(opt.replay_dir, at("file").c_str()), payload)) why = "cannot read " + at("file");
        else if (std::to_string(payload.size()) != at("net_bytes")) why = "file size differs from net_bytes";
        if (why.empty()) {
            try {
                const std::string* wire = &payload;
                if (vf->encoding == codec::Encoding::Zstd) {
                    metrics::Stopwatch zw;
                    unzstd.decompress(payload, kCapSnapshot, plain);
                    unzstd_us = zw.us();
                    rec.add("c.decompress_us", unzstd_us);
                    wire = &plain;
                }
                wire_bytes = wire->size();
                got_wire = checks::wire_digest(*wire);
                double destroy_us = -1;
                load_us = holder.install(*wire, opt.buffers, &destroy_us);
                rec.add("c.load_us", load_us);
                const ViewSnapshot& s = holder.view();
                got_digest = checks::snapshot_digest(s);
                char hex[2][17];
                std::snprintf(hex[0], sizeof hex[0], "%016" PRIx64, got_wire);
                std::snprintf(hex[1], sizeof hex[1], "%016" PRIx64, got_digest);
                if (std::to_string(wire_bytes) != at("wire_bytes")) why = "wire_bytes differ";
                else if (at("wire_digest") != hex[0]) why = "wire digest differs";
                else if (at("digest") != hex[1]) why = "content digest differs";
                else if (std::to_string(s.request_id) != at("request_id")) why = "request_id not echoed";
                else if (std::to_string(s.units.size()) != at("visible")) why = "visible count differs";
                else if (vf->category == net::ViewCategory::Empty && s.units.size() != 0) why = "empty view has units";
                ++by_enc[static_cast<int>(vf->encoding)];
            } catch (const std::exception& e) {
                why = std::string("refused: ") + e.what();
            }
        }
        if (why.empty()) ++matched;
        else log.fail("replay_snapshots", tag + ": " + why);
        if (csv) {
            char row[512];
            std::snprintf(row, sizeof row, "%s,%s,%s,%zu,%zu,%016" PRIx64 ",%016" PRIx64 ",%.3f,%.3f,%s",
                          at("request_id").c_str(), at("category").c_str(), at("encoding").c_str(), payload.size(),
                          wire_bytes, got_wire, got_digest, unzstd_us < 0 ? 0.0 : unzstd_us, load_us,
                          why.empty() ? "match" : "FAIL");
            out.row(row);
        }
    }
    if (rows == 0) log.fail("replay_snapshots", "corpus.csv lists no snapshot");
    else log.pass("replay_snapshots", matched);
    log.note("replay_snapshots", "rows=" + std::to_string(rows) + " matched=" + std::to_string(matched) +
                                     " dense=" + std::to_string(by_enc[0]) + " compact=" + std::to_string(by_enc[1]) +
                                     " zstd=" + std::to_string(by_enc[2]));
    if (by_enc[0] && by_enc[1] && by_enc[2]) log.pass("replay_encodings");
    else log.fail("replay_encodings", "an encoding is missing from the corpus");
    holder.clear();
    std::fprintf(stdout, "replay: %" PRIu64 " of %" PRIu64 " snapshots verified with A's digests (%s)\n", matched, rows,
                 opt.replay_dir.c_str());
    log.print(stdout);
    if (!write_results(opt, log, &rec)) throw std::runtime_error("cannot write client CSV files in " + opt.csv_dir);
    return log.all_ok() ? 0 : 1;
}

}   // namespace mapdemo::client
