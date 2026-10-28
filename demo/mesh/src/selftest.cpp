// Device mesh demo: the self-test (xmesh_selftest). OWNER: types thread.
//
//   xmesh_selftest [--scales small,medium,large] [--limits] [--corpus-write DIR] [--corpus-check DIR]
//
// In memory, no network (INTERFACES.md section 6): the fingerprints of
// section 3; for every type, scale and save mode, state 0 and a route of
// six devices run by hand (save, receive, digest, model check, mutate),
// equal to replay() and digesting the same in both save modes; a route of
// kMaxRoute stops within data_limit / receive_cap / the mutation headroom;
// jobs through start_job / handle_hop / read_report, every EvilCase giving
// expected_verdict, and the other refusals of hop.hpp. --limits prints the
// measured maxima of the long routes (to set the limits in objects.cpp).
// --corpus-write DIR writes the corpus (corpus/README.md); --corpus-check
// DIR recomputes it and fails on any byte difference. Exit 0 when every
// check passed. Built with and without exceptions (xmesh_selftest_noexc).
#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "types/fingerprints.hpp"
#include "types/internal.hpp"
#include "types/model.hpp"
#include "xmesh/hop.hpp"
#include "xmesh/protocol.hpp"

using namespace xmesh;
using protocol::HopVerdict;

namespace {

int g_failed = 0;
int g_passed = 0;

void check(const std::string& name, bool ok, const std::string& detail) {
    std::printf("check %-34s %s  %s\n", name.c_str(), ok ? "pass" : "FAIL", detail.c_str());
    std::fflush(stdout);
    if (ok) ++g_passed;
    else ++g_failed;
}

std::string fmt(const char* f, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

double ms_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

// The first failure of a sequence of expectations.
struct Expect {
    std::string first;
    bool ok() const { return first.empty(); }
    void operator()(bool cond, const std::string& what) {
        if (!cond && first.empty()) first = what;
    }
};

std::vector<Step> route_of(std::initializer_list<const char*> names) {
    std::vector<Step> r;
    for (const char* n : names) r.push_back(Step{std::uint32_t(r.size()), n});
    return r;
}

// Six devices, names as xo_peer builds them.
const std::vector<Step> kSix = route_of({"linux-x86_64-gcc", "windows-x86_64-msvc-noexc", "android-arm64-clang",
                                         "linux-arm64-clang-noexc", "windows-x86_64-mingw", "ios-arm64-clang"});

std::vector<Step> long_route(std::uint32_t stops) {
    std::vector<Step> r;
    for (std::uint32_t k = 0; k < stops; ++k) r.push_back(Step{k, fmt("device-%02u-%s", k, k % 2 ? "noexc" : "x")});
    return r;
}

std::uint64_t seed_for(TypeId t, Scale s) { return 0x5eed0000ull + std::uint64_t(t) * 16 + std::uint64_t(s); }

// ---- fingerprints and names -------------------------------------------------------------

void test_fingerprints() {
    const auto cur = fp::current();
    std::string bad;
    for (std::size_t i = 0; i < cur.size(); ++i) {
        const fp::Fingerprint& c = cur[i];
        const fp::Fingerprint& f = fp::kFrozen[i];
        if (!fp::same(c, f) && bad.empty())
            bad = fmt("%s: %u/%u %016" PRIx64 " %016" PRIx64 " != frozen %u/%u %016" PRIx64 " %016" PRIx64, c.name,
                      c.size, c.align, c.schema_hash, c.name_digest, f.size, f.align, f.schema_hash, f.name_digest);
    }
    check("fingerprints", bad.empty(), bad.empty() ? "11 roots equal INTERFACES.md section 3" : bad);

    // Every member is in the field lists the digest and the comparison walk.
    Expect ex;
#define XMESH_LISTED(T) ex(field_check::listed_bytes<T>() == sizeof(T), #T " field list does not cover the record");
    XMESH_LISTED(xmesh::Document) XMESH_LISTED(xmesh::Paragraph)
    XMESH_LISTED(xmesh::Comment) XMESH_LISTED(xmesh::Order) XMESH_LISTED(xmesh::Address)
    XMESH_LISTED(xmesh::LineItem) XMESH_LISTED(xmesh::Coupon) XMESH_LISTED(xmesh::GeoPoint)
    XMESH_LISTED(xmesh::SensorSeries) XMESH_LISTED(xmesh::Sample) XMESH_LISTED(xmesh::KvConfig)
    XMESH_LISTED(xmesh::Value) XMESH_LISTED(xmesh::SceneTree) XMESH_LISTED(xmesh::Node)
    XMESH_LISTED(xmesh::ChatRoom) XMESH_LISTED(xmesh::Member) XMESH_LISTED(xmesh::Attachment)
    XMESH_LISTED(xmesh::Message) XMESH_LISTED(xmesh::HopMark)
#undef XMESH_LISTED
    check("field_lists", ex.ok(), ex.ok() ? "every member walked by digest and comparison" : ex.first);
}

void test_names() {
    Expect ex;
    for (TypeId t : kAllTypes) {
        ex(parse_type(type_name(t)) == t, std::string("type name ") + type_name(t));
        std::string lower = type_name(t);
        for (char& c : lower) c = char(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        ex(parse_type(lower) == t, "lower-case type name " + lower);
    }
    for (Scale s : {Scale::Small, Scale::Medium, Scale::Large}) ex(parse_scale(scale_name(s)) == s, "scale name");
    for (SaveMode m : {SaveMode::Verified, SaveMode::Compact}) ex(parse_save_mode(save_mode_name(m)) == m, "mode name");
    for (EvilCase e : kAllEvilCases) ex(parse_evil(evil_name(e)) == e, std::string("evil name ") + evil_name(e));
    ex(!to_type(0) && !to_type(7) && to_type(6), "to_type range");
    ex(!to_scale(3) && !to_save_mode(2) && !to_evil(8) && to_evil(7), "to_* ranges");
    ex(!parse_type("Documents"), "parse_type accepted a wrong name");
    for (TypeId t : kAllTypes)
        for (Scale s : {Scale::Small, Scale::Medium, Scale::Large}) {
            ex(data_limit(t, s) <= protocol::kMaxDataBytes, "data_limit over kMaxDataBytes");
            ex(data_limit(t, s) + (1u << 20) < protocol::kMaxDataBytes, "no room above data_limit for the too_large case");
        }
    check("names_and_limits", ex.ok(), ex.ok() ? "names round-trip, wire value ranges, limits under 16 MiB" : ex.first);
}

// ---- one type, scale and mode by hand --------------------------------------------------

struct SizeRow {
    TypeId type;
    Scale scale;
    std::size_t verified = 0, compact = 0, used = 0;
    double make_ms = 0;
};
std::vector<SizeRow> g_sizes;

// Runs kSix by hand; returns the content digests per hop (for the
// comparison of the two save modes).
std::vector<std::uint64_t> run_by_hand(TypeId t, Scale s, SaveMode mode, Expect& ex, std::size_t& state0_bytes,
                                       std::size_t& state0_used) {
    const std::uint64_t seed = seed_for(t, s);
    std::vector<std::uint64_t> digests;
    Result<Object> obj = make_object(t, seed, s, kSix[0].device);
    ex(bool(obj), "make_object failed");
    if (!obj) return digests;
    state0_used = std::size_t(internal::used_bytes(*obj));
    std::uint64_t d = content_digest(*obj);
    Status m0 = check_against_model(*obj, seed, s, kSix, 0);
    ex(bool(m0), "state 0 differs from the model: " + (m0 ? std::string() : m0.error().message));
    Result<std::string> wire = save(*obj, mode);
    ex(bool(wire), "save failed");
    if (!wire) return digests;
    state0_bytes = wire->size();
    std::vector<HopExpect> mine{{d, protocol::wire_digest(*wire), std::uint32_t(wire->size())}};
    digests.push_back(d);
    for (std::uint32_t k = 1; k + 1 < kSix.size(); ++k) {
        Result<Object> in = receive_object(t, s, *wire);
        ex(bool(in), fmt("hop %u: receive failed: %s", k, in ? "" : in.error().message.c_str()));
        if (!in) return digests;
        ex(content_digest(*in) == d, fmt("hop %u: received content digest differs from the sent one", k));
        Status before = check_against_model(*in, seed, s, kSix, k - 1);
        ex(bool(before), fmt("hop %u: received state differs from the model: %s", k,
                             before ? "" : before.error().message.c_str()));
        Status changed = mutate(*in, seed, kSix[k]);
        ex(bool(changed), fmt("hop %u: mutate failed: %s", k, changed ? "" : changed.error().message.c_str()));
        if (!changed) return digests;
        Status after = check_against_model(*in, seed, s, kSix, k);
        ex(bool(after), fmt("hop %u: mutated state differs from the model: %s", k,
                            after ? "" : after.error().message.c_str()));
        d = content_digest(*in);
        ex(d != digests.back(), fmt("hop %u: the mutation changed nothing", k));
        wire = save(*in, mode);
        ex(bool(wire), "save failed");
        if (!wire) return digests;
        mine.push_back({d, protocol::wire_digest(*wire), std::uint32_t(wire->size())});
        digests.push_back(d);
    }
    Result<std::vector<HopExpect>> rep = replay(t, seed, s, mode, kSix);
    ex(bool(rep), "replay failed: " + (rep ? std::string() : rep.error().message));
    if (rep) {
        ex(rep->size() == mine.size(), "replay has another number of hops");
        for (std::size_t k = 0; k < mine.size() && k < rep->size(); ++k) {
            ex((*rep)[k].content_digest == mine[k].content_digest, fmt("replay hop %zu: content digest", k));
            ex((*rep)[k].wire_digest == mine[k].wire_digest && (*rep)[k].bytes == mine[k].bytes,
               fmt("replay hop %zu: wire bytes differ from the hops run by hand", k));
        }
    }
    // The model check must see a change: one value off fails it.
    if (Result<Object> again = receive_object(t, s, *wire)) {
        Object& o = *again;
        if (t == TypeId::Document) o.root<Document>().revision += 1;
        if (t == TypeId::Order) o.root<Order>().lines[0].qty += 1;
        if (t == TypeId::SensorSeries) o.root<SensorSeries>().samples[0].reserved0 = 1;
        if (t == TypeId::KvConfig) o.root<KvConfig>().config_id ^= 1;
        if (t == TypeId::SceneTree) o.root<SceneTree>().nodes[0].local[3] = 0.5f;
        if (t == TypeId::ChatRoom) o.root<ChatRoom>().messages.back().edits += 1;
        Status drift = check_against_model(o, seed, s, kSix, std::uint32_t(kSix.size() - 2));
        ex(!drift, "a changed value passed the model check");
    }
    return digests;
}

void test_types(const std::vector<Scale>& scales) {
    for (TypeId t : kAllTypes)
        for (Scale s : scales) {
            Expect ex;
            const auto t0 = std::chrono::steady_clock::now();
            SizeRow row{t, s};
            std::size_t used_c = 0;
            std::vector<std::uint64_t> dv = run_by_hand(t, s, SaveMode::Verified, ex, row.verified, row.used);
            std::vector<std::uint64_t> dc = run_by_hand(t, s, SaveMode::Compact, ex, row.compact, used_c);
            ex(dv == dc, "the two save modes digest differently");
            {
                const auto m0 = std::chrono::steady_clock::now();
                Result<Object> o = make_object(t, seed_for(t, s), s, "timing");
                row.make_ms = ms_since(m0);
            }
            g_sizes.push_back(row);
            check(fmt("route6 %s %s", type_name(t), scale_name(s)), ex.ok(),
                  ex.ok() ? fmt("state 0: %zu B verified, %zu B compact; 5 hops = model = replay, both modes; %.0f ms",
                                row.verified, row.compact, ms_since(t0))
                          : ex.first);
        }
}

// ---- long routes: the limits ------------------------------------------------------------------

void test_long_routes(const std::vector<Scale>& scales, bool print_limits) {
    const std::vector<Step> route = long_route(kMaxRoute);
    for (TypeId t : kAllTypes)
        for (Scale s : scales) {
            Expect ex;
            const auto t0 = std::chrono::steady_clock::now();
            std::size_t max_wire = 0;
            std::uint64_t max_used = 0, max_grow = 0;
            const std::uint64_t seed = seed_for(t, s) ^ 0x10;
            for (SaveMode mode : {SaveMode::Verified, SaveMode::Compact}) {
                Result<Object> obj = make_object(t, seed, s, route[0].device);
                ex(bool(obj), "make failed");
                if (!obj) break;
                Result<std::string> wire = save(*obj, mode);
                ex(bool(wire), "save failed");
                if (!wire) break;
                max_wire = std::max(max_wire, wire->size());
                for (std::uint32_t k = 1; k + 1 < route.size(); ++k) {
                    Result<Object> in = receive_object(t, s, *wire);
                    ex(bool(in), fmt("%s hop %u: receive: %s", save_mode_name(mode), k,
                                     in ? "" : in.error().message.c_str()));
                    if (!in) break;
                    const std::uint64_t before = internal::used_bytes(*in);
                    max_used = std::max(max_used, before);
                    Status changed = mutate(*in, seed, route[k]);
                    ex(bool(changed), fmt("%s hop %u: mutate: %s", save_mode_name(mode), k,
                                          changed ? "" : changed.error().message.c_str()));
                    if (!changed) break;
                    max_grow = std::max(max_grow, internal::used_bytes(*in) - before);
                    wire = save(*in, mode);
                    ex(bool(wire), "save failed");
                    if (!wire) break;
                    max_wire = std::max(max_wire, wire->size());
                    if (k + 2 == route.size()) {
                        Status m = check_against_model(*in, seed, s, route, k);
                        ex(bool(m), fmt("%s hop %u: model: %s", save_mode_name(mode), k, m ? "" : m.error().message.c_str()));
                    }
                }
            }
            const std::uint32_t cap = receive_cap(t, s);
            const std::uint32_t head = internal::mutation_headroom(t, s);
            ex(max_wire <= data_limit(t, s), fmt("a wire of %zu B is over data_limit %u", max_wire, data_limit(t, s)));
            ex(max_used + head <= cap, fmt("a payload of %" PRIu64 " B leaves less than the headroom under the cap", max_used));
            ex(max_grow <= head, fmt("a mutation grew the arena by %" PRIu64 " B, over the headroom %u", max_grow, head));
            if (print_limits)
                std::printf("limits %-12s %-6s max_wire %zu  max_used %" PRIu64 "  max_grow %" PRIu64
                            "  (data_limit %u, receive_cap %u, headroom %u)\n",
                            type_name(t), scale_name(s), max_wire, max_used, max_grow, data_limit(t, s), cap, head);
            check(fmt("route%u %s %s", kMaxRoute, type_name(t), scale_name(s)), ex.ok(),
                  ex.ok() ? fmt("max wire %zu/%u B, max payload %" PRIu64 " B, max growth %" PRIu64 "/%u B; %.0f ms",
                                max_wire, data_limit(t, s), max_used, max_grow, head, ms_since(t0))
                          : ex.first);
        }
}

// ---- jobs through hop.hpp ---------------------------------------------------------------------------

JobSpec job_of(std::uint64_t id, TypeId t, Scale s, SaveMode m, EvilCase e, std::uint32_t stops) {
    JobSpec j;
    j.job = id;
    j.seed = 0xabc000 + id;
    j.type = t;
    j.scale = s;
    j.save_mode = m;
    j.evil = e;
    for (std::uint32_t k = 0; k < stops; ++k) j.route.push_back(RouteEntry{10 + k, kSix[k % kSix.size()].device});
    return j;
}

std::vector<Step> steps_of(const JobSpec& j) {
    std::vector<Step> r;
    for (const RouteEntry& e : j.route) r.push_back(Step{std::uint32_t(r.size()), e.name});
    return r;
}

void test_jobs() {
    std::uint64_t id = 1;
    for (TypeId t : kAllTypes)
        for (SaveMode m : {SaveMode::Verified, SaveMode::Compact}) {
            Expect ex;
            const JobSpec j = job_of(id++, t, Scale::Small, m, EvilCase::None, 5);
            Result<std::vector<HopExpect>> rep = replay(t, j.seed, j.scale, m, steps_of(j));
            ex(bool(rep), "replay failed");
            Result<std::string> meta = make_job_meta(j);
            ex(bool(meta), "make_job_meta failed");
            if (!rep || !meta) {
                check(fmt("job %s %s", type_name(t), save_mode_name(m)), false, ex.first);
                continue;
            }
            HopOutcome out = start_job(*meta, j.route[0].device_id);
            for (std::uint32_t k = 0; k < j.route.size(); ++k) {
                ex(out.verdict == HopVerdict::Ok, fmt("stop %u: %s %s", k, protocol::verdict_name(out.verdict), out.error.c_str()));
                Result<ReportRow> r = read_report(out.report);
                ex(bool(r), fmt("stop %u: report unreadable", k));
                if (!r || out.verdict != HopVerdict::Ok) break;
                ex(r->job == j.job && r->verdict == HopVerdict::Ok && !r->expected_reject, fmt("stop %u: report fields", k));
                const std::uint32_t got = k == 0 ? 0 : k - 1;
                ex(r->hop == got, fmt("stop %u: report hop %u", k, r->hop));
                ex(r->content_digest == (*rep)[got].content_digest, fmt("stop %u: content digest != replay", k));
                if (k + 1 < j.route.size()) {
                    ex(r->sent_wire_digest == (*rep)[k].wire_digest && r->sent_bytes == (*rep)[k].bytes,
                       fmt("stop %u: sent wire != replay", k));
                    ex(out.next_device == j.route[k + 1].device_id, fmt("stop %u: next device", k));
                    // Delivered to the wrong device first: refused, nothing sent on.
                    if (k == 1) {
                        HopOutcome wrong = handle_hop(out.next_payload, 999);
                        ex(wrong.verdict == HopVerdict::Misrouted && wrong.next_payload.empty(), "misrouted hop not refused");
                        std::string damaged = out.next_payload;
                        damaged.back() = char(damaged.back() ^ 0x40);
                        HopOutcome dmg = handle_hop(damaged, j.route[k + 1].device_id);
                        ex(dmg.verdict == HopVerdict::WireDigest, fmt("damaged data part: %s", protocol::verdict_name(dmg.verdict)));
                    }
                    out = handle_hop(out.next_payload, j.route[k + 1].device_id);
                } else {
                    ex(r->sent_wire_digest == 0 && r->sent_bytes == 0 && out.next_payload.empty(),
                       "the last stop sent something on");
                }
            }
            check(fmt("job %s %s", type_name(t), save_mode_name(m)), ex.ok(),
                  ex.ok() ? "start_job + 4 handle_hop: every report = replay, last stop ends; misrouted and damaged refused"
                          : ex.first);
        }

    // The envelope and the meta.
    Expect ex;
    HopOutcome junk = handle_hop("not a hop payload at all", 1);
    ex(junk.verdict == HopVerdict::BadEnvelope && junk.report.empty(), "junk payload not refused as bad_envelope");
    const JobSpec j = job_of(900, TypeId::Order, Scale::Small, SaveMode::Verified, EvilCase::None, 3);
    Result<std::string> meta = make_job_meta(j);
    HopOutcome first = start_job(*meta, j.route[0].device_id);
    std::string bad_meta = first.next_payload;
    bad_meta[sizeof(protocol::HopEnvelope)] ^= 0x01;   // the HopMeta wire's magic
    HopOutcome bm = handle_hop(bad_meta, j.route[1].device_id);
    ex(bm.verdict == HopVerdict::BadMeta && bm.report.empty(), "damaged meta not refused as bad_meta");
    HopOutcome mis = start_job(*meta, 77);
    ex(mis.verdict == HopVerdict::Misrouted && !mis.report.empty(), "a Job for another device was started");
    JobSpec one = j;
    one.route.resize(1);
    ex(!make_job_meta(one), "a route of one stop was accepted");
    // A hop whose sender is the last stop: no next stop.
    JobSpec two = job_of(901, TypeId::Order, Scale::Small, SaveMode::Verified, EvilCase::None, 2);
    HopOutcome h0 = start_job(*make_job_meta(two), two.route[0].device_id);
    HopOutcome end = handle_hop(h0.next_payload, two.route[1].device_id);
    ex(end.verdict == HopVerdict::Ok && end.next_payload.empty(), "a two-stop job did not end at route[1]");
    check("job_refusals", ex.ok(), ex.ok() ? "bad envelope, bad meta, misrouted job, short route, two-stop end" : ex.first);
}

void test_evil(const std::vector<Scale>& scales) {
    std::uint64_t id = 1000;
    for (TypeId t : kAllTypes) {
        Expect ex;
        std::string seen;
        for (Scale s : scales) {
            if (s == Scale::Large) continue;
            for (EvilCase e : kAllEvilCases)
                for (SaveMode m : {SaveMode::Verified, SaveMode::Compact}) {
                    const JobSpec j = job_of(id++, t, s, m, e, 3);
                    HopOutcome first = start_job(*make_job_meta(j), j.route[0].device_id);
                    ex(first.verdict == HopVerdict::Ok, fmt("%s: route[0] %s %s", evil_name(e),
                                                            protocol::verdict_name(first.verdict), first.error.c_str()));
                    if (first.verdict != HopVerdict::Ok) continue;
                    HopOutcome got = handle_hop(first.next_payload, j.route[1].device_id);
                    Result<ReportRow> r = read_report(got.report);
                    const HopVerdict want = protocol::expected_verdict(e);
                    ex(got.verdict == want, fmt("%s %s %s: %s, expected %s (%s)", evil_name(e), scale_name(s),
                                                save_mode_name(m), protocol::verdict_name(got.verdict),
                                                protocol::verdict_name(want), got.error.c_str()));
                    ex(r && r->expected_reject && r->verdict == want, fmt("%s: report", evil_name(e)));
                    ex(got.next_payload.empty(), fmt("%s: sent on", evil_name(e)));
                    if (s == scales.front() && m == SaveMode::Verified)
                        seen += std::string(seen.empty() ? "" : "; ") + evil_name(e) + ": " +
                                got.error.substr(0, 60);
                }
        }
        check(fmt("evil %s", type_name(t)), ex.ok(), ex.ok() ? "7 cases x scales x modes refused as expected" : ex.first);
        if (t == TypeId::Document) std::printf("  evil reasons (Document): %s\n", seen.c_str());
    }
}

// A received object with too little room under its cap is refused before
// any change (a build without exceptions would otherwise abort).
void test_no_room() {
    Expect ex;
    Result<Object> obj = make_object(TypeId::ChatRoom, 5, Scale::Small, "maker");
    Result<std::string> wire = save(*obj, SaveMode::Compact);
    const auto cap = XBuffer::maximum_capacity(std::uint32_t(wire->size() + 512)).with_growth(XGrowPolicy::Automatic);
    XResult<XBuffer> tight = XBuffer::try_load_verified<ChatRoom>(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(wire->data()), wire->size()), cap);
    ex(bool(tight), "tight load failed");
    if (tight) {
        Object o(TypeId::ChatRoom, std::move(*tight));
        const std::uint64_t before = content_digest(o);
        Status s = mutate(o, 5, Step{1, "next"});
        ex(!s, "a mutation without room was not refused");
        ex(content_digest(o) == before, "the refused mutation changed the object");
        if (!s) std::printf("  no room: %s\n", s.error().message.c_str());
    }
    Result<Object> big = receive_object(TypeId::Order, Scale::Small, std::string(data_limit(TypeId::Order, Scale::Small) + 1, 'x'));
    ex(!big, "receive_object read a part over data_limit");
    check("no_room", ex.ok(), ex.ok() ? "mutate refuses without headroom, object unchanged; receive refuses over data_limit" : ex.first);
}

// ---- corpus ------------------------------------------------------------------------------------------

bool read_file(const std::string& path, std::string& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[65536];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}
bool write_file(const std::string& path, const std::string& data) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    return std::fclose(f) == 0 && ok;
}

// The corpus: index.txt (every job's HopExpects and every evil part's
// digest) and, for the small scale, the wires themselves.
struct Corpus {
    std::string index;
    std::vector<std::pair<std::string, std::string>> files;   // name, bytes
};

Result<Corpus> make_corpus() {
    Corpus c;
    c.index = "# xmesh corpus v1: demo/mesh/corpus/README.md\n";
    const std::vector<Step> route =
        route_of({"linux-x86_64-gcc", "windows-x86_64-msvc", "android-arm64-clang-noexc", "linux-arm64-gcc", "ios-arm64-clang"});
    std::string names;
    for (const Step& s : route) names += (names.empty() ? "" : ">") + s.device;
    for (TypeId t : kAllTypes)
        for (Scale s : {Scale::Small, Scale::Medium, Scale::Large})
            for (SaveMode m : {SaveMode::Verified, SaveMode::Compact}) {
                const std::uint64_t seed = seed_for(t, s) ^ 0xc0;
                const std::string job = std::string(type_name(t)) + "-" + scale_name(s) + "-" + save_mode_name(m);
                c.index += fmt("job %s seed %016" PRIx64 " route %s\n", job.c_str(), seed, names.c_str());
                // The hops as devices run them, keeping the wires of the small scale.
                Result<Object> obj = make_object(t, seed, s, route[0].device);
                if (!obj) return Error{job + ": " + obj.error().message};
                for (std::uint32_t k = 0;; ++k) {
                    if (k > 0) XMESH_TRY(mutate(*obj, seed, route[k]));
                    Result<std::string> w = save(*obj, m);
                    if (!w) return Error{job + ": " + w.error().message};
                    c.index += fmt("  hop %u content %016" PRIx64 " wire %016" PRIx64 " bytes %zu\n", k,
                                   content_digest(*obj), protocol::wire_digest(*w), w->size());
                    if (s == Scale::Small) c.files.emplace_back(fmt("%s-h%u.xmw", job.c_str(), k), *w);
                    if (k + 2 == route.size()) break;
                    obj = receive_object(t, s, *w);
                    if (!obj) return Error{job + ": " + obj.error().message};
                }
                for (EvilCase e : kAllEvilCases) {
                    if (s == Scale::Large) break;
                    Result<EvilPart> p = make_evil(t, seed, s, m, route[0].device, e);
                    if (!p) return Error{job + " " + evil_name(e) + ": " + p.error().message};
                    c.index += fmt("  evil %s claimed %016" PRIx64 " wire %016" PRIx64 " bytes %zu\n", evil_name(e),
                                   p->claimed_content_digest, protocol::wire_digest(p->wire), p->wire.size());
                }
            }
    return c;
}

int corpus(const std::string& dir, bool write) {
    const auto t0 = std::chrono::steady_clock::now();
    Result<Corpus> c = make_corpus();
    if (!c) {
        check("corpus", false, c.error().message);
        return 1;
    }
    std::size_t bytes = 0;
    for (const auto& f : c->files) bytes += f.second.size();
    if (write) {
        bool ok = write_file(dir + "/index.txt", c->index);
        for (const auto& f : c->files) ok = ok && write_file(dir + "/" + f.first, f.second);
        check("corpus_write", ok, fmt("%zu wires (%zu B) + index.txt to %s", c->files.size(), bytes, dir.c_str()));
        return ok ? 0 : 1;
    }
    std::string bad, got;
    if (!read_file(dir + "/index.txt", got)) bad = "no " + dir + "/index.txt";
    else if (got != c->index) {
        std::size_t i = 0;
        while (i < got.size() && i < c->index.size() && got[i] == c->index[i]) ++i;
        const std::size_t line = std::size_t(std::count(c->index.begin(), c->index.begin() + std::ptrdiff_t(i), '\n')) + 1;
        const std::size_t from = c->index.rfind('\n', i == 0 ? 0 : i - 1);
        const std::size_t start = from == std::string::npos ? 0 : from + 1;
        bad = fmt("index.txt line %zu differs; this build: %s", line,
                  c->index.substr(start, c->index.find('\n', start) - start).c_str());
    }
    for (const auto& f : c->files) {
        if (!bad.empty()) break;
        if (!read_file(dir + "/" + f.first, got)) bad = "missing " + f.first;
        else if (got != f.second) bad = f.first + " differs";
    }
    check("corpus_check", bad.empty(),
          bad.empty() ? fmt("index.txt + %zu wires (%zu B) byte-identical to %s; %.0f ms", c->files.size(), bytes,
                            dir.c_str(), ms_since(t0))
                      : bad);
    return bad.empty() ? 0 : 1;
}

void print_sizes() {
    std::printf("\nstate 0 sizes (bytes): type scale verified compact payload make_ms\n");
    for (const SizeRow& r : g_sizes)
        std::printf("size %-12s %-6s %9zu %9zu %9zu %8.2f\n", type_name(r.type), scale_name(r.scale), r.verified,
                    r.compact, r.used, r.make_ms);
}

} // namespace

int main(int argc, char** argv) {
    std::vector<Scale> scales{Scale::Small, Scale::Medium, Scale::Large};
    bool limits = false;
    std::string corpus_write, corpus_check;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--scales" && i + 1 < argc) {
            scales.clear();
            std::string list = argv[++i];
            for (std::size_t p = 0; p <= list.size();) {
                const std::size_t q = std::min(list.find(',', p), list.size());
                const auto s = parse_scale(std::string_view(list).substr(p, q - p));
                if (!s) {
                    std::fprintf(stderr, "unknown scale in %s\n", list.c_str());
                    return 2;
                }
                scales.push_back(*s);
                p = q + 1;
            }
        } else if (a == "--limits") {
            limits = true;
        } else if (a == "--corpus-write" && i + 1 < argc) {
            corpus_write = argv[++i];
        } else if (a == "--corpus-check" && i + 1 < argc) {
            corpus_check = argv[++i];
        } else {
            std::fprintf(stderr,
                         "usage: %s [--scales small,medium,large] [--limits] [--corpus-write DIR] [--corpus-check DIR]\n",
                         argv[0]);
            return 2;
        }
    }
    std::printf("xmesh_selftest: exceptions %s, schema version %d\n", XMESH_EXCEPTIONS ? "on" : "off",
                XMESH_SCHEMA_VERSION);
    if (!corpus_write.empty()) return corpus(corpus_write, true);
    const auto t0 = std::chrono::steady_clock::now();
    test_fingerprints();
    test_names();
    test_types(scales);
    test_long_routes(scales, limits);
    test_jobs();
    test_evil(scales);
    test_no_room();
    if (!corpus_check.empty()) corpus(corpus_check, false);
    print_sizes();
    std::printf("\nRESULT %s: %d checks passed, %d failed, %.1f s\n", g_failed ? "FAIL" : "PASS", g_passed, g_failed,
                ms_since(t0) / 1000.0);
    return g_failed ? 1 : 0;
}
