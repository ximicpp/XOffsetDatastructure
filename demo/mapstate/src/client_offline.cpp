// Map state demo: client modes that need no server. OWNER: client thread.
//
// --self-check: the snapshot path's rejection table on this platform. A
// snapshot built here is saved dense, compact and as a zstd frame; each
// must verify and read back the same content (positive controls), and each
// malformed variant must be refused by the codec (before allocation) or by
// load_verified. Nothing malformed may be accepted. The event stream gets
// the same treatment: every event encoding round-trips, and bad dictionary
// frames (another dictionary version, no id, no size, truncated, trailing
// bytes, over the cap, forged batch counts and lengths), a corrupt or
// foreign dictionary file and wrong wires are refused.
//
// --replay DIR: verifies every snapshot of a corpus that mapdemo_a
// --corpus-dir wrote on another machine and checks that the verified wire
// and the content read from it have the digests A logged. Targets that
// cannot reach the Linux servers (simulators) verify the servers' bytes
// this way.
#include "client_core.hpp"

#include <charconv>
#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <span>
#include <sstream>
#include <utility>

#if defined(MAPDEMO_WITH_ZSTD)
#include "xoffset_zstd_dict.hpp"
#endif

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
    std::string plain;
    const std::string* wire = &payload;
    if (zstd) {
        if (Status decoded = unzstd.decompress(payload, kCapSnapshot, plain); !decoded)
            return "codec: " + decoded.error().message;
        wire = &plain;
    }
    XResult<XBuffer> b = XBuffer::try_load_verified<ViewSnapshot>(*wire, cap_snapshot());
    if (!b) return "verify: " + b.error().message;
    if (digest != nullptr) *digest = checks::snapshot_digest(b->root<ViewSnapshot>());
    return std::string();
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


// ---- event stream ------------------------------------------------------------

// Sample UnitEvent wires like A's: ids 0, 100, 200, ..., short and long names.
std::vector<std::string> build_events(std::uint32_t count, std::uint32_t path_points = 0,
                                      XBuffer::MaximumCapacity max_bytes = cap_event()) {
    std::vector<std::string> wires;
    for (std::uint32_t i = 0; i < count; ++i) {
        XBuffer b = XBuffer::create<UnitEvent>(512, max_bytes);
        UnitEvent& e = b.root<UnitEvent>();
        e.tick = 1240;
        e.unit_id = i * kEventIdModulo;
        e.hp = std::int32_t(50 + (i * 7) % 51);
        e.pos = Point{float((i * 37) % 4000) + 0.5f, float((i * 53) % 4000) + 0.25f};
        e.behavior = static_cast<Behavior>(i % 3);
        e.next_waypoint = i % 4;
        const std::string name = i % 2 == 0 ? rules::short_name(e.unit_id) : rules::long_name(e.unit_id);
        e.name = std::string_view(name);
        const std::uint32_t points = path_points != 0 ? path_points : 4 + i % 3;
        e.path.reserve(points);
        for (std::uint32_t k = 0; k < points; ++k) e.path.push_back(Point{float((i * 11 + k * 97) % 4096), float((i * 13 + k * 89) % 4096)});
        wires.push_back(b.save_compact<UnitEvent>(XCompactOptions{0}));
    }
    return wires;
}

std::vector<std::string_view> views_of(const std::vector<std::string>& v) {
    return std::vector<std::string_view>(v.begin(), v.end());
}

// Decodes and verifies an Events payload; "" when every wire verified, else
// how it was refused. *digest: the digest of the re-packed wires.
std::string try_accept_events(codec::EventDecoder& dec, codec::EventEncoding e, std::string_view payload,
                              std::uint64_t* digest = nullptr) {
    std::vector<std::string_view> wires;
    if (Status s = dec.decode(e, payload, wires); !s) return "codec: " + s.error().message;
    for (std::string_view w : wires) {
        XResult<XBuffer> b =
            XBuffer::try_load_verified<UnitEvent>(std::as_bytes(std::span<const char>(w.data(), w.size())), cap_event());
        if (!b) return "verify: " + b.error().message;
    }
    if (digest != nullptr) {
        codec::EventEncoder raw;
        std::string packed;
        if (Status s = raw.encode(codec::EventEncoding::Raw, wires, packed); !s) return "repack: " + s.error().message;
        *digest = checks::wire_digest(packed);
    }
    return std::string();
}

void expect_events_refused(CheckLog& log, codec::EventDecoder& dec, const char* name, codec::EventEncoding e,
                           std::string_view payload) {
    const std::string how = try_accept_events(dec, e, payload);
    std::fprintf(stdout, "self-check: %-28s -> %s\n", name, how.empty() ? "ACCEPTED" : how.c_str());
    if (how.empty()) {
        log.fail(name, "accepted");
    } else {
        log.pass(name);
        log.note(name, how.substr(0, 160));
    }
}

void expect_refused_text(CheckLog& log, const char* name, const Status& s) {
    std::fprintf(stdout, "self-check: %-28s -> %s\n", name, s ? "ACCEPTED" : s.error().message.c_str());
    if (s) {
        log.fail(name, "accepted");
    } else {
        log.pass(name);
        log.note(name, s.error().message.substr(0, 160));
    }
}

#if defined(MAPDEMO_WITH_ZSTD)
// A one-event Dict payload with its frame changed by `f`.
template <class F>
std::string rewrite_dict_frame(const std::string& payload, F&& f) {
    // count (1 byte: 1), length (1-2 bytes), the frame.
    std::size_t pos = 1;
    std::uint32_t len = 0;
    if (!xoffset_zstd_dict::get_varint32(payload, pos, len)) return payload;
    std::string frame = payload.substr(pos, len);
    f(frame);
    std::string out(1, '\x01');
    xoffset_zstd_dict::put_varint(frame.size(), out);
    return out + frame;
}
#endif

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

// A whole decimal field; nullopt for anything else.
template <class T>
std::optional<T> parse_uint(const std::string& text) {
    T v{};
    const char* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, v);
    if (ec != std::errc() || ptr != end || text.empty()) return std::nullopt;
    return v;
}


// The event stream's positive and refusal cases (codec.hpp, events).
void run_event_self_check(CheckLog& log, const std::string& wrong_schema_wire) {
    std::uint32_t dict_id = 0;
    Result<std::string> dict = codec::load_event_dictionary(std::string(), &dict_id);
    if (!dict) {
        log.fail("events_dictionary", dict.error().message);
        return;
    }
    log.pass("events_dictionary");
    log.note("events_dictionary", "id=" + std::to_string(dict_id) + " file_bytes=" + std::to_string(dict->size()));
    codec::EventEncoder enc;
    codec::EventDecoder dec;
    if (Status s = enc.set_dictionary(*dict); !s) { log.fail("events_dictionary", s.error().message); return; }
    if (Status s = dec.set_dictionary(*dict); !s) { log.fail("events_dictionary", s.error().message); return; }

    const std::vector<std::string> wires = build_events(20);
    const std::vector<std::string_view> views = views_of(wires);
    std::size_t raw_bytes = 0;
    for (const std::string& w : wires) raw_bytes += w.size();
    std::string raw;
    if (Status s = enc.encode(codec::EventEncoding::Raw, views, raw); !s) { log.fail("accept_events_raw", s.error().message); return; }
    const std::uint64_t want = checks::wire_digest(raw);

    // Positive controls: every encoding gives back the same wires.
    std::size_t per_event[codec::kEventEncodingCount] = {};
    std::string payloads[codec::kEventEncodingCount];
    static const char* const kAccept[] = {"accept_events_raw", "accept_events_zstd", "accept_events_zdict",
                                          "accept_events_batch"};
    for (int i = 0; i < codec::kEventEncodingCount; ++i) {
        const auto e = static_cast<codec::EventEncoding>(i);
        std::string& p = payloads[i];
        std::string why;
        std::uint64_t got = 0;
        if (Status s = enc.encode(e, views, p); !s) why = "encode: " + s.error().message;
        else why = try_accept_events(dec, e, p, &got);
        if (why.empty() && got != want) why = "wires differ after the round trip";
        per_event[i] = p.size() / wires.size();
        std::fprintf(stdout, "self-check: %-28s -> %s (%zu bytes, 20 events)\n", kAccept[i],
                     why.empty() ? "verified, same wires" : why.c_str(), p.size());
        if (why.empty()) {
            log.pass(kAccept[i]);
            log.note(kAccept[i], std::to_string(p.size()) + " bytes");
        } else {
            log.fail(kAccept[i], why);
        }
    }
    {
        const std::string detail = "bytes per event: wire=" + std::to_string(raw_bytes / wires.size()) +
                                   " zstd=" + std::to_string(per_event[1]) + " zdict=" + std::to_string(per_event[2]) +
                                   " batch=" + std::to_string(per_event[3]);
        if (per_event[1] < raw_bytes / wires.size() && per_event[2] < per_event[1] && per_event[3] < per_event[2]) {
            log.pass("events_sizes_shrink");
            log.note("events_sizes_shrink", detail);
        } else {
            log.fail("events_sizes_shrink", detail);
        }
    }

    // Raw layout and the wires inside.
    using E = codec::EventEncoding;
    expect_events_refused(log, dec, "events_raw_count_zero", E::Raw, std::string(1, '\0'));
    expect_events_refused(log, dec, "events_raw_truncated", E::Raw, drop_last(payloads[0]));
    expect_events_refused(log, dec, "events_raw_trailing_byte", E::Raw, payloads[0] + std::string(1, '\0'));
    {
        std::string wrong;
        codec::EventEncoder plain;
        const std::vector<std::string_view> one{wrong_schema_wire};
        if (Status s = plain.encode(E::Raw, one, wrong); !s) log.fail("events_raw_wrong_schema", s.error().message);
        else expect_events_refused(log, dec, "events_raw_wrong_schema", E::Raw, wrong);
        const std::vector<std::string> trunc{drop_last(wires[3])};
        if (Status s = enc.encode(E::Dict, views_of(trunc), wrong); !s) log.fail("events_zdict_truncated_wire", s.error().message);
        else expect_events_refused(log, dec, "events_zdict_truncated_wire", E::Dict, wrong);
    }
#if defined(MAPDEMO_WITH_ZSTD)
    namespace zd = xoffset_zstd_dict;
    {   // zstd without a dictionary: the snapshot rules (content size required).
        Result<std::string> unsized = codec::zstd_frame_without_size(wires[0]);
        if (!unsized) log.fail("events_zstd_no_content_size", unsized.error().message);
        else {
            std::string p(1, '\x01');
            zd::put_varint(unsized->size(), p);
            expect_events_refused(log, dec, "events_zstd_no_content_size", E::Zstd, p + *unsized);
        }
    }
    // Dictionary versions: frames from dictionary id+1 are refused.
    {
        Result<std::string> other = codec::relabel_dictionary(*dict, dict_id + 1);
        codec::EventEncoder enc2;
        Status set = other ? enc2.set_dictionary(*other) : Status(Error{other.error().message});
        std::string p;
        if (!set) {
            log.fail("events_dict_version_mismatch", set.error().message);
        } else {
            if (Status s = enc2.encode(E::Dict, views, p); s) expect_events_refused(log, dec, "events_dict_version_mismatch", E::Dict, p);
            if (Status s = enc2.encode(E::Batch, views, p); s) expect_events_refused(log, dec, "events_batch_version_mismatch", E::Batch, p);
        }
    }
    // One lean frame, damaged.
    std::string one;
    const std::vector<std::string_view> first{views[0]};
    if (Status s = enc.encode(E::Dict, first, one); !s) {
        log.fail("events_dict_frame", s.error().message);
    } else {
        expect_events_refused(log, dec, "events_dict_no_id", E::Dict, rewrite_dict_frame(one, [](std::string& f) {
            f[0] = static_cast<char>(static_cast<unsigned char>(f[0]) & ~3u);
            f.erase(1, 1);
        }));
        expect_events_refused(log, dec, "events_dict_no_content_size", E::Dict, rewrite_dict_frame(one, [](std::string& f) {
            const unsigned char fhd = static_cast<unsigned char>(f[0]);
            f[0] = static_cast<char>(fhd & ~0xe0u);
            f.erase(2, 1);
            f.insert(1, 1, '\0');
        }));
        expect_events_refused(log, dec, "events_dict_truncated", E::Dict,
                              rewrite_dict_frame(one, [](std::string& f) { f.pop_back(); }));
        expect_events_refused(log, dec, "events_dict_trailing_byte", E::Dict,
                              rewrite_dict_frame(one, [](std::string& f) { f.push_back('\0'); }));
        expect_events_refused(log, dec, "events_dict_standard_frame", E::Dict,
                              rewrite_dict_frame(one, [](std::string& f) { f.insert(0, "\x28\xb5\x2f\xfd", 4); }));
    }
    {   // A wire over kCapEvent: refused from the frame header, before decoding.
        const std::vector<std::string> big = build_events(1, 600, XBuffer::maximum_capacity(2 * kCapEvent).with_growth(XGrowPolicy::Automatic));
        std::string p;
        if (Status s = enc.encode(E::Dict, views_of(big), p); !s) log.fail("events_dict_over_cap", s.error().message);
        else expect_events_refused(log, dec, "events_dict_over_cap", E::Dict, p);
    }
    {   // Batch frames the encoder would not write.
        zd::Dictionary d;
        zd::Encoder zenc;
        if (zd::Status s = zd::Dictionary::parse(*dict, d); !s || !zenc.set_dictionary(d)) {
            log.fail("events_batch_over_count", "dictionary");
        } else {
            std::vector<std::string> many = build_events(std::uint32_t(kMaxEventsPerFrame) + 1);
            std::string f;
            if (zd::Status s = zenc.encode_batch(views_of(many), f); !s) log.fail("events_batch_over_count", s.message);
            else expect_events_refused(log, dec, "events_batch_over_count", E::Batch, f);
        }
        Result<std::string> unbound = codec::unbind_dictionary(*dict);
        zd::Dictionary u;
        zd::Encoder forger;
        if (!unbound || !zd::Dictionary::parse(*unbound, u) || !forger.set_dictionary(u)) {
            log.fail("events_batch_forged_length", "unbound dictionary");
        } else {
            std::string f;
            // Count 2, a first length that runs past the content.
            if (zd::Status s = forger.encode(std::string("\x02\x30" "0123456789abcdef0123", 22), f); s)
                expect_events_refused(log, dec, "events_batch_forged_length", E::Batch, f);
            // Count 1, a message shorter than a wire header.
            if (zd::Status s = forger.encode(std::string("\x01\x04" "abcd", 6), f); s)
                expect_events_refused(log, dec, "events_batch_short_message", E::Batch, f);
        }
    }
    // The dictionary file itself.
    {
        std::string bad = *dict;
        bad[bad.size() / 2] = static_cast<char>(bad[bad.size() / 2] ^ 1);
        codec::EventDecoder d2;
        expect_refused_text(log, "events_dict_file_corrupt", d2.set_dictionary(bad));
        Result<std::string> foreign =
            codec::rebind_dictionary(*dict, wire_root_type_id_v<ViewSnapshot>(), wire_schema_hash_v<ViewSnapshot>());
        if (!foreign) log.fail("events_dict_wrong_schema", foreign.error().message);
        else expect_refused_text(log, "events_dict_wrong_schema", d2.set_dictionary(*foreign));
    }
#endif
}

}   // namespace

Result<int> run_self_check(const Options& opt) {
    CheckLog log;
    if (!codec::zstd_available()) {
        log.fail("zstd_available", "built without zstd (MAPDEMO_WITH_ZSTD=OFF)");
        log.print(stdout);
        if (!write_results(opt, log, nullptr)) return Error{"cannot write client CSV files in " + opt.csv_dir};
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
    MAPDEMO_TRY(zstd.compress(compact, frame));

    // Positive controls: every encoding verifies and reads the same content.
    expect_same(log, unzstd, "accept_dense", dense, false, want);
    expect_same(log, unzstd, "accept_compact", compact, false, want);
    expect_same(log, unzstd, "accept_zstd", frame, true, want);
    {
        std::string plain;
        Status decoded = unzstd.decompress(frame, kCapSnapshot, plain);
        if (!decoded) log.fail("zstd_round_trip_bytes", decoded.error().message);
        else if (plain == compact) log.pass("zstd_round_trip_bytes");
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
        std::string why;
        if (Status decoded = unzstd.decompress(frame, kCapSnapshot, plain); !decoded) why = decoded.error().message;
        const std::string* order[] = {&dense, &compact, &plain, &dense};
        for (const std::string* w : order) {
            if (!why.empty()) break;
            if (Status loaded = h.reload(*w); !loaded) why = "reload refused a valid wire: " + loaded.error().message;
            else if (checks::snapshot_digest(h.view()) != want) why = "digest differs after reload";
        }
        if (why.empty()) {
            if (h.reload(drop_last(compact))) why = "truncated wire accepted by reload";
            else if (!h.empty()) why = "holder not empty after a rejected reload";
            else if (Status loaded = h.reload(compact); !loaded)
                why = "reload after a rejection refused a valid wire: " + loaded.error().message;
            else if (checks::snapshot_digest(h.view()) != want)
                why = "digest differs after the reload that followed a rejection";
        }
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
    {
        Result<std::string> unsized = codec::zstd_frame_without_size(compact);
        if (!unsized) return std::move(unsized).error();
        expect_refused(log, unzstd, "zstd_no_content_size", *unsized, true);
    }
    expect_refused(log, unzstd, "zstd_skippable_frame", codec::skippable_frame(compact), true);
    expect_refused(log, unzstd, "zstd_dense_wire_unwrapped", dense, true);
    {
        std::string over;
        MAPDEMO_TRY(zstd.compress(std::string(kCapSnapshot + 1, '\0'), over));
        expect_refused(log, unzstd, "zstd_over_cap", over, true);
    }
    {
        std::string inner;
        MAPDEMO_TRY(zstd.compress(drop_last(compact), inner));
        expect_refused(log, unzstd, "zstd_of_truncated_wire", inner, true);
        MAPDEMO_TRY(zstd.compress(request_wire(), inner));
        expect_refused(log, unzstd, "zstd_of_wrong_schema", inner, true);
    }

    run_event_self_check(log, request_wire());

    log.print(stdout);
    if (!write_results(opt, log, nullptr)) return Error{"cannot write client CSV files in " + opt.csv_dir};
    return log.all_ok() ? 0 : 1;
}

Result<int> run_replay(const Options& opt) {
    CheckLog log;
    metrics::Recorder rec("c");
    codec::Decompressor unzstd;
    SnapshotHolder holder;
    std::string payload, plain;

    std::ifstream index(join_path(opt.replay_dir, "corpus.csv"));
    if (!index) return Error{"cannot read " + join_path(opt.replay_dir, "corpus.csv")};
    std::string line;
    std::getline(index, line);
    std::map<std::string, std::size_t> col;
    {
        const std::vector<std::string> names = split_csv(line);
        for (std::size_t i = 0; i < names.size(); ++i) col[names[i]] = i;
    }
    for (const char* need : {"request_id", "flags", "category", "encoding", "file", "net_bytes", "wire_bytes",
                             "wire_digest", "digest", "visible"})
        if (col.find(need) == col.end()) return Error{std::string("corpus.csv has no column ") + need};

    metrics::CsvFile out;
    const bool csv = !opt.csv_dir.empty();
    if (csv && !out.open(join_path(opt.csv_dir, "client_replay.csv"),
                         "request_id,category,encoding,net_bytes,wire_bytes,wire_digest,digest,decompress_us,load_us,result"))
        return Error{"cannot write client_replay.csv in " + opt.csv_dir};

    std::uint64_t rows = 0, matched = 0;
    std::uint64_t by_enc[codec::kEncodingCount] = {};
    while (std::getline(index, line)) {
        if (line.empty()) continue;
        const std::vector<std::string> f = split_csv(line);
        static const std::string kMissing;
        // A short row reads as empty fields, which then fail their checks.
        auto at = [&](const char* name) -> const std::string& {
            const std::size_t i = col[name];
            return i < f.size() ? f[i] : kMissing;
        };
        ++rows;
        const std::string tag = "request " + at("request_id");
        std::string why;
        double unzstd_us = -1, load_us = 0;
        std::uint64_t got_wire = 0, got_digest = 0;
        std::size_t wire_bytes = 0;
        const auto flags = parse_uint<std::uint16_t>(at("flags"));
        const auto vf = flags ? codec::parse_view_flags(*flags) : std::nullopt;
        if (!vf) why = "flags " + at("flags") + " not understood";
        else if (at("encoding") != codec::encoding_name(vf->encoding)) why = "encoding column disagrees with flags";
        else if (!read_file(join_path(opt.replay_dir, at("file").c_str()), payload)) why = "cannot read " + at("file");
        else if (std::to_string(payload.size()) != at("net_bytes")) why = "file size differs from net_bytes";
        // Decode and verify; a refusal reads "refused: " + the codec's or load_verified's text.
        bool installed = false;
        const std::string* wire = &payload;
        if (why.empty() && vf->encoding == codec::Encoding::Zstd) {
            metrics::Stopwatch zw;
            if (Status decoded = unzstd.decompress(payload, kCapSnapshot, plain); !decoded) {
                why = "refused: " + decoded.error().message;
            } else {
                unzstd_us = zw.us();
                rec.add("c.decompress_us", unzstd_us);
                wire = &plain;
            }
        }
        if (why.empty()) {
            wire_bytes = wire->size();
            got_wire = checks::wire_digest(*wire);
            double destroy_us = -1;
            Result<double> loaded = holder.install(*wire, opt.buffers, &destroy_us);
            if (!loaded) {
                why = "refused: " + loaded.error().message;
            } else {
                load_us = *loaded;
                rec.add("c.load_us", load_us);
                installed = true;
            }
        }
        if (installed) {
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
    if (!write_results(opt, log, &rec)) return Error{"cannot write client CSV files in " + opt.csv_dir};
    return log.all_ok() ? 0 : 1;
}

}   // namespace mapdemo::client
