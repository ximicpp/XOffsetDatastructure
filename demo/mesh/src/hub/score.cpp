// Device mesh demo: how the hub scores a job from its devices' reports,
// shared by the live relay (hub.cpp) and the mailbox score (mailbox.cpp).
// OWNER: network thread.
#include <cstdio>
#include "hub/hub.hpp"

namespace xmesh::hub {

const char* kind_name(JobKind kind) noexcept {
    switch (kind) {
        case JobKind::Pairs: return "pairs";
        case JobKind::Chain: return "chain";
        case JobKind::Evil: return "evil";
        case JobKind::Cross: return "cross";
    }
    return "?";
}

std::string csv_field(const std::string& s) {
    if (s.find_first_of(",\"\n\r") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else if (c == '\n' || c == '\r') out += ' ';
        else out += c;
    }
    return out + "\"";
}

std::string route_text(const JobSpec& spec) {
    std::string s;
    for (const auto& r : spec.route) s += (s.empty() ? "" : ">") + r.name;
    return s;
}

std::string hex64(std::uint64_t v) {
    char buf[24];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

Judgement judge(const JobRun& j) {
    const auto& route = j.spec.route;
    auto fail = [](std::string reason) { return Judgement{true, false, std::move(reason)}; };
    auto failure = [&](std::size_t pos) {
        const ReportRow& r = *j.reports[pos];
        return route[pos].name + " (position " + std::to_string(pos) + "): " + protocol::verdict_name(r.verdict) +
               (r.error.empty() ? std::string() : ": " + r.error);
    };
    if (j.kind == JobKind::Evil) {
        if (j.reports[0] && j.reports[0]->verdict != protocol::HopVerdict::Ok) return fail(failure(0));
        if (!j.reports[1]) return {};
        const ReportRow& r = *j.reports[1];
        const protocol::HopVerdict want = protocol::expected_verdict(j.spec.evil);
        if (r.verdict != want || !r.expected_reject)
            return fail(route[1].name + " gave " + protocol::verdict_name(r.verdict) +
                        (r.expected_reject ? "" : " without expected_reject") + ", expected " +
                        protocol::verdict_name(want) + (r.error.empty() ? "" : " (" + r.error + ")"));
        if (!j.reports[0]) return {};
        return {true, true, std::string("refused: ") + protocol::verdict_name(r.verdict)};
    }
    for (std::size_t pos = 0; pos < route.size(); ++pos)
        if (j.reports[pos] && j.reports[pos]->verdict != protocol::HopVerdict::Ok) return fail(failure(pos));
    for (const auto& r : j.reports)
        if (!r) return {};
    const std::size_t last = route.size() - 1;
    for (std::size_t pos = 0; pos <= last; ++pos) {
        const ReportRow& r = *j.reports[pos];
        const HopExpect& got_from = j.expect[pos == 0 ? 0 : pos - 1];
        if (r.content_digest != got_from.content_digest)
            return fail(route[pos].name + " (position " + std::to_string(pos) + "): content digest " +
                        hex64(r.content_digest) + ", replay " + hex64(got_from.content_digest));
        if (pos < last) {
            const HopExpect& sent = j.expect[pos];
            if (r.sent_wire_digest != sent.wire_digest || r.sent_bytes != sent.bytes)
                return fail(route[pos].name + " (position " + std::to_string(pos) + ") sent " +
                            std::to_string(r.sent_bytes) + " bytes, wire " + hex64(r.sent_wire_digest) + "; replay " +
                            std::to_string(sent.bytes) + " bytes, wire " + hex64(sent.wire_digest));
        } else if (r.sent_wire_digest != 0 || r.sent_bytes != 0) {
            return fail(route[pos].name + " sent on after the last stop");
        }
    }
    return {true, true, ""};
}

std::string job_csv_row(const JobRun& j, const std::string& ms) {
    const bool passed = j.state == JobState::Passed;
    const std::string evil = j.kind == JobKind::Evil ? evil_name(j.spec.evil) : "";
    return std::to_string(j.spec.job) + "," + kind_name(j.kind) + "," + type_name(j.spec.type) + "," +
           scale_name(j.spec.scale) + "," + save_mode_name(j.spec.save_mode) + "," + evil + "," +
           csv_field(route_text(j.spec)) + "," + (passed ? "pass" : "fail") + "," + csv_field(j.reason) + "," + ms;
}

std::vector<std::string> hop_csv_rows(const JobRun& j) {
    std::vector<std::string> rows;
    const std::size_t last = j.spec.route.size() - 1;
    for (std::size_t pos = 0; pos < j.reports.size(); ++pos) {
        if (!j.reports[pos]) continue;
        const ReportRow& r = *j.reports[pos];
        std::string content_ok, wire_ok;
        if (j.kind != JobKind::Evil && !j.expect.empty()) {
            content_ok = r.content_digest == j.expect[pos == 0 ? 0 : pos - 1].content_digest ? "1" : "0";
            wire_ok = pos < last ? (r.sent_wire_digest == j.expect[pos].wire_digest ? "1" : "0")
                                 : (r.sent_wire_digest == 0 ? "1" : "0");
        }
        rows.push_back(std::to_string(j.spec.job) + "," + std::to_string(pos) + "," +
                       csv_field(j.spec.route[pos].name) + "," + protocol::verdict_name(r.verdict) + "," +
                       (r.expected_reject ? "1" : "0") + "," + std::to_string(r.data_bytes) + "," +
                       std::to_string(r.sent_bytes) + "," + std::to_string(r.grown_bytes) + "," + content_ok + "," +
                       wire_ok + "," + std::to_string(r.verify_ns) + "," + std::to_string(r.check_ns) + "," +
                       std::to_string(r.mutate_ns) + "," + std::to_string(r.save_ns) + "," + csv_field(r.error));
    }
    return rows;
}

} // namespace xmesh::hub
