// Device mesh demo: a device's handling of a Job or a Hop (hop.hpp).
// OWNER: types thread.
#include "xmesh/hop.hpp"

#include <chrono>
#include "types/internal.hpp"

namespace xmesh {
namespace {

using protocol::HopVerdict;

std::uint64_t now_ns() noexcept {
    return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count());
}

XBuffer::MaximumCapacity control_cap() {
    return XBuffer::maximum_capacity(protocol::kControlCap).with_growth(XGrowPolicy::Automatic);
}

std::span<const std::byte> bytes_of(std::string_view s) noexcept {
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}

// A HopMeta as plain values.
struct Meta {
    std::uint64_t job = 0, seed = 0, content_digest = 0, wire_digest = 0;
    std::uint32_t hop = 0;
    std::uint16_t type = 0;
    std::uint8_t scale = 0, save_mode = 0, evil = 0;
    std::vector<RouteEntry> route;
};

Result<std::string> write_meta(const Meta& m) {
    if (m.route.size() > kMaxRoute) return Error{"hop meta: route longer than " + std::to_string(kMaxRoute)};
#if XMESH_EXCEPTIONS
    try {
#endif
        XBuffer b = XBuffer::create<HopMeta>(4096, control_cap());
        HopMeta& h = b.root<HopMeta>();
        h.job = m.job;
        h.seed = m.seed;
        h.content_digest = m.content_digest;
        h.wire_digest = m.wire_digest;
        h.hop = m.hop;
        h.type = m.type;
        h.scale = m.scale;
        h.save_mode = m.save_mode;
        h.evil = m.evil;
        h.route.reserve(m.route.size());
        for (const RouteEntry& e : m.route) {
            RouteStop& s = h.route.emplace_back();
            s.device_id = e.device_id;
            s.name = std::string_view(e.name);
        }
        std::string wire = b.save_verified<HopMeta>();
        if (wire.size() > protocol::kMaxMetaBytes) return Error{"hop meta: over 64 KiB"};
        return wire;
#if XMESH_EXCEPTIONS
    } catch (const std::exception& e) {
        return Error{std::string("hop meta: ") + e.what()};
    }
#endif
}

Result<Meta> read_meta(std::string_view wire) {
    XResult<XBuffer> in = XBuffer::try_load_verified<HopMeta>(bytes_of(wire), control_cap());
    if (!in) return Error{"hop meta: " + in.error().message};
    const HopMeta& h = in->root<HopMeta>();
    Meta m;
    m.job = h.job;
    m.seed = h.seed;
    m.content_digest = h.content_digest;
    m.wire_digest = h.wire_digest;
    m.hop = h.hop;
    m.type = h.type;
    m.scale = h.scale;
    m.save_mode = h.save_mode;
    m.evil = h.evil;
    m.route.reserve(h.route.size());
    for (const RouteStop& s : h.route) m.route.push_back(RouteEntry{s.device_id, std::string(s.name.view())});
    return m;
}

// The checks of step 2 that need the loaded values.
std::string meta_problem(const Meta& m) {
    if (!to_type(m.type)) return "unknown type " + std::to_string(m.type);
    if (!to_scale(m.scale)) return "unknown scale " + std::to_string(m.scale);
    if (!to_save_mode(m.save_mode)) return "unknown save mode " + std::to_string(m.save_mode);
    if (!to_evil(m.evil)) return "unknown evil case " + std::to_string(m.evil);
    if (m.route.size() < 2 || m.route.size() > kMaxRoute)
        return "route of " + std::to_string(m.route.size()) + " stops (2.." + std::to_string(kMaxRoute) + ")";
    if (std::uint64_t(m.hop) + 1 >= m.route.size())
        return "hop " + std::to_string(m.hop) + " has no next stop in a route of " + std::to_string(m.route.size());
    return {};
}

std::vector<Step> steps_of(const Meta& m) {
    std::vector<Step> steps;
    steps.reserve(m.route.size());
    for (std::size_t k = 0; k < m.route.size(); ++k) steps.push_back(Step{std::uint32_t(k), m.route[k].name});
    return steps;
}

std::string write_report(const ReportRow& r) {
#if XMESH_EXCEPTIONS
    try {
#endif
        XBuffer b = XBuffer::create<HopReport>(4096, control_cap());
        HopReport& h = b.root<HopReport>();
        h.job = r.job;
        h.content_digest = r.content_digest;
        h.sent_wire_digest = r.sent_wire_digest;
        h.hop = r.hop;
        h.data_bytes = r.data_bytes;
        h.sent_bytes = r.sent_bytes;
        h.verdict = std::uint8_t(r.verdict);
        h.expected_reject = r.expected_reject ? 1 : 0;
        h.verify_ns = r.verify_ns;
        h.check_ns = r.check_ns;
        h.mutate_ns = r.mutate_ns;
        h.save_ns = r.save_ns;
        // Reports stay far below the 64 KiB frame limit.
        h.error = std::string_view(r.error).substr(0, 2000);
        h.grown_bytes = r.grown_bytes;
        return b.save_verified<HopReport>();
#if XMESH_EXCEPTIONS
    } catch (...) {
        return {};
    }
#endif
}

// Ends handling: the outcome with its report.
HopOutcome finish(ReportRow& row, HopVerdict verdict, std::string error, HopOutcome out = {}) {
    row.verdict = verdict;
    row.error = error;
    out.verdict = verdict;
    out.error = std::move(error);
    out.job = row.job;
    if (verdict != HopVerdict::Ok) {
        out.next_payload.clear();
        out.next_device = 0;
    }
    if (row.job != 0) out.report = write_report(row);
    return out;
}

} // namespace

Result<std::string> make_job_meta(const JobSpec& spec) {
    if (spec.route.size() < 2 || spec.route.size() > kMaxRoute)
        return Error{"job: a route has 2.." + std::to_string(kMaxRoute) + " stops"};
    Meta m;
    m.job = spec.job;
    m.seed = spec.seed;
    m.hop = 0;
    m.type = std::uint16_t(spec.type);
    m.scale = std::uint8_t(spec.scale);
    m.save_mode = std::uint8_t(spec.save_mode);
    m.evil = std::uint8_t(spec.evil);
    m.route = spec.route;
    return write_meta(m);
}

HopOutcome start_job(std::string_view job_meta_wire, std::uint32_t my_device_id) {
    ReportRow row;
    Result<Meta> meta = read_meta(job_meta_wire);
    if (!meta) return finish(row, HopVerdict::BadMeta, meta.error().message);
    Meta& m = *meta;
    row.job = m.job;
    row.hop = 0;
    if (std::string p = meta_problem(m); !p.empty()) return finish(row, HopVerdict::BadMeta, "job: " + p);
    if (m.hop != 0 || m.content_digest != 0 || m.wire_digest != 0)
        return finish(row, HopVerdict::BadMeta, "job: a Job has hop 0 and both digests 0");
    if (m.route[0].device_id != my_device_id)
        return finish(row, HopVerdict::Misrouted, "job: route[0] is device " + std::to_string(m.route[0].device_id));

    const TypeId type = TypeId(m.type);
    const Scale scale = Scale(m.scale);
    const SaveMode mode = SaveMode(m.save_mode);
    const EvilCase evil = EvilCase(m.evil);
    HopOutcome out;
    std::string data;
    std::uint64_t t0 = now_ns();
    if (evil != EvilCase::None) {
        Result<EvilPart> part = make_evil(type, m.seed, scale, mode, m.route[0].name, evil);
        if (!part) return finish(row, HopVerdict::MakeFailed, part.error().message);
        row.mutate_ns = now_ns() - t0;
        data = std::move(part->wire);
        row.content_digest = part->claimed_content_digest;
        out.summary = std::string(type_name(type)) + " evil part: " + evil_name(evil);
    } else {
        Result<Object> obj = make_object(type, m.seed, scale, m.route[0].name);
        if (!obj) return finish(row, HopVerdict::MakeFailed, obj.error().message);
        row.mutate_ns = now_ns() - t0;
        row.content_digest = content_digest(*obj);
        t0 = now_ns();
        Result<std::string> w = save(*obj, mode);
        if (!w) return finish(row, HopVerdict::SaveFailed, w.error().message);
        row.save_ns = now_ns() - t0;
        data = std::move(*w);
        out.summary = summary(*obj);
    }
    Meta next = m;
    next.hop = 0;
    next.content_digest = row.content_digest;
    next.wire_digest = protocol::wire_digest(data);
    Result<std::string> next_meta = write_meta(next);
    if (!next_meta) return finish(row, HopVerdict::SaveFailed, next_meta.error().message);
    row.sent_wire_digest = next.wire_digest;
    row.sent_bytes = std::uint32_t(data.size());
    out.next_payload = protocol::pack_hop(*next_meta, data);
    out.next_device = m.route[1].device_id;
    return finish(row, HopVerdict::Ok, {}, std::move(out));
}

HopOutcome handle_hop(std::string_view hop_payload, std::uint32_t my_device_id) {
    ReportRow row;
    // 1. envelope
    Result<protocol::HopParts> parts = protocol::unpack_hop(hop_payload);
    if (!parts) return finish(row, HopVerdict::BadEnvelope, parts.error().message);
    // 2. meta
    Result<Meta> meta = read_meta(parts->meta);
    if (!meta) return finish(row, HopVerdict::BadMeta, meta.error().message);
    Meta& m = *meta;
    row.job = m.job;
    row.hop = m.hop;
    row.expected_reject = m.evil != 0;
    row.data_bytes = std::uint32_t(parts->data.size());
    if (std::string p = meta_problem(m); !p.empty()) return finish(row, HopVerdict::BadMeta, p);
    const TypeId type = TypeId(m.type);
    const Scale scale = Scale(m.scale);
    const SaveMode mode = SaveMode(m.save_mode);
    const std::uint32_t me = m.hop + 1;
    // 3. route
    if (m.route[me].device_id != my_device_id)
        return finish(row, HopVerdict::Misrouted,
                      "route[" + std::to_string(me) + "] is device " + std::to_string(m.route[me].device_id) +
                          ", not " + std::to_string(my_device_id));
    // 4. size, before anything reads the data
    if (parts->data.size() > data_limit(type, scale))
        return finish(row, HopVerdict::TooLarge,
                      std::to_string(parts->data.size()) + " bytes over the " + type_name(type) + " " +
                          scale_name(scale) + " limit of " + std::to_string(data_limit(type, scale)));
    // 5. the bytes as sent
    if (protocol::wire_digest(parts->data) != m.wire_digest)
        return finish(row, HopVerdict::WireDigest, "the data part's bytes differ from the sender's wire digest");
    // 6. verified load
    std::uint64_t t0 = now_ns();
    Result<Object> obj = receive_object(type, scale, parts->data);
    row.verify_ns = now_ns() - t0;
    if (!obj) return finish(row, HopVerdict::LoadRejected, obj.error().message);
    // 7. content
    t0 = now_ns();
    row.content_digest = content_digest(*obj);
    if (row.content_digest != m.content_digest) {
        row.check_ns = now_ns() - t0;
        return finish(row, HopVerdict::ContentDigest, "content digest differs from the sender's");
    }
    // 8. reference model
    const std::vector<Step> steps = steps_of(m);
    Status model = check_against_model(*obj, m.seed, scale, steps, m.hop);
    row.check_ns = now_ns() - t0;
    if (!model) return finish(row, HopVerdict::ModelMismatch, model.error().message);

    HopOutcome out;
    if (me + 1 == m.route.size()) {             // the last stop ends the job
        out.summary = summary(*obj);
        return finish(row, HopVerdict::Ok, {}, std::move(out));
    }
    // 9. mutation me
    const std::uint64_t before = internal::used_bytes(*obj);
    t0 = now_ns();
    Status changed = mutate(*obj, m.seed, steps[me]);
    row.mutate_ns = now_ns() - t0;
    const std::uint64_t after = internal::used_bytes(*obj);
    row.grown_bytes = std::uint32_t(after > before ? after - before : 0);
    if (!changed) return finish(row, HopVerdict::MutateFailed, changed.error().message);
    // 10. send on
    t0 = now_ns();
    Result<std::string> data = save(*obj, mode);
    row.save_ns = now_ns() - t0;
    if (!data) return finish(row, HopVerdict::SaveFailed, data.error().message);
    Meta next = m;
    next.hop = me;
    next.evil = 0;
    next.content_digest = content_digest(*obj);
    next.wire_digest = protocol::wire_digest(*data);
    Result<std::string> next_meta = write_meta(next);
    if (!next_meta) return finish(row, HopVerdict::SaveFailed, next_meta.error().message);
    row.sent_wire_digest = next.wire_digest;
    row.sent_bytes = std::uint32_t(data->size());
    out.summary = summary(*obj);
    out.next_payload = protocol::pack_hop(*next_meta, *data);
    out.next_device = m.route[me + 1].device_id;
    return finish(row, HopVerdict::Ok, {}, std::move(out));
}

Result<ReportRow> read_report(std::string_view report_wire) {
    XResult<XBuffer> in = XBuffer::try_load_verified<HopReport>(bytes_of(report_wire), control_cap());
    if (!in) return Error{"report: " + in.error().message};
    const HopReport& h = in->root<HopReport>();
    if (h.verdict > std::uint8_t(HopVerdict::MakeFailed)) return Error{"report: unknown verdict " + std::to_string(h.verdict)};
    ReportRow r;
    r.job = h.job;
    r.hop = h.hop;
    r.verdict = HopVerdict(h.verdict);
    r.expected_reject = h.expected_reject != 0;
    r.content_digest = h.content_digest;
    r.sent_wire_digest = h.sent_wire_digest;
    r.data_bytes = h.data_bytes;
    r.sent_bytes = h.sent_bytes;
    r.grown_bytes = h.grown_bytes;
    r.verify_ns = h.verify_ns;
    r.check_ns = h.check_ns;
    r.mutate_ns = h.mutate_ns;
    r.save_ns = h.save_ns;
    r.error = std::string(h.error.view());
    return r;
}

} // namespace xmesh
