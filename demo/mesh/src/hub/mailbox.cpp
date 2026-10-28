// Device mesh demo: cross-site jobs without a live link (INTERFACES.md
// section 6, mailbox). OWNER: network thread.
//
// The hub plans jobs between two sites with fixed device ids and writes
// one Job file per job into a mailbox directory. Each site runs its
// devices with `xo_peer --mailbox` over the files it has, the sites swap
// the files, and the hub scores every job afterwards from the reports and
// the hop files, with the same checks as the live relay (score.cpp) plus
// the wire digest of every hop file against its replay.
//
//   xo_hub --mailbox-plan DIR --devices FILE [--plan cross,chain,evil] [--types ...]
//          [--scales ...] [--save-modes ...] [--seed N] [--run LABEL]
//   xo_hub --mailbox-score DIR --out OUT [--run LABEL]
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <system_error>
#include "hub/hub.hpp"
#include "peer/control.hpp"
#include "peer/mailbox.hpp"

namespace xmesh::hub {

namespace {

namespace fs = std::filesystem;

struct SiteDevice {
    std::uint32_t id = 0;
    std::string name;
    std::string site;
};

std::uint64_t splitmix64(std::uint64_t& x) noexcept {
    std::uint64_t z = (x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

void say(const std::string& line) {
    std::fprintf(stderr, "[hub mailbox] %s\n", line.c_str());
    std::fflush(stderr);
}

bool read_text(const fs::path& path, std::string& out, std::uint64_t limit = std::uint64_t(64) << 20) {
    std::FILE* f = std::fopen(path.string().c_str(), "rb");
    if (f == nullptr) return false;
    out.clear();
    char buf[65536];
    bool ok = true;
    for (;;) {
        const std::size_t n = std::fread(buf, 1, sizeof buf, f);
        if (n == 0) break;
        if (out.size() + n > limit) {
            ok = false;
            break;
        }
        out.append(buf, n);
    }
    ok = ok && std::ferror(f) == 0;
    std::fclose(f);
    return ok;
}

bool write_text(const fs::path& path, std::string_view data) {
    std::FILE* f = std::fopen(path.string().c_str(), "wb");
    if (f == nullptr) return false;
    const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    return std::fclose(f) == 0 && ok;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t eol = text.find('\n', at);
        if (eol == std::string::npos) eol = text.size();
        std::string line = text.substr(at, eol - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(std::move(line));
        at = eol + 1;
    }
    return out;
}

std::vector<std::string> split_on(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::size_t at = 0;
    for (;;) {
        const std::size_t c = s.find(sep, at);
        out.push_back(s.substr(at, c == std::string::npos ? std::string::npos : c - at));
        if (c == std::string::npos) break;
        at = c + 1;
    }
    return out;
}

bool good_name(const std::string& n) {
    if (n.empty() || n.size() > 64) return false;
    for (char c : n)
        if (c < 0x21 || c > 0x7e || c == '>' || c == ',' || c == '"' || c == ':') return false;
    return true;
}

bool to_u64(const std::string& s, std::uint64_t& out) {
    if (s.empty() || s.size() > 20) return false;
    std::uint64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + std::uint64_t(c - '0');
    }
    out = v;
    return true;
}

// "ID NAME SITE" lines; '#' starts a comment.
Result<std::vector<SiteDevice>> read_devices(const fs::path& path) {
    std::string text;
    if (!read_text(path, text, 1 << 20)) return Error{"cannot read " + path.string()};
    std::vector<SiteDevice> devs;
    std::set<std::uint32_t> ids;
    std::set<std::string> names;
    int line_no = 0;
    for (std::string line : split_lines(text)) {
        ++line_no;
        if (const std::size_t h = line.find('#'); h != std::string::npos) line.resize(h);
        std::vector<std::string> w;
        std::size_t at = 0;
        while (at < line.size()) {
            while (at < line.size() && (line[at] == ' ' || line[at] == '\t')) ++at;
            std::size_t e = at;
            while (e < line.size() && line[e] != ' ' && line[e] != '\t') ++e;
            if (e > at) w.push_back(line.substr(at, e - at));
            at = e;
        }
        if (w.empty()) continue;
        std::uint64_t id = 0;
        const std::string where = path.string() + ":" + std::to_string(line_no);
        if (w.size() != 3 || !to_u64(w[0], id) || id == 0 || id > 0xffffffffull)
            return Error{where + ": expected ID NAME SITE"};
        if (!good_name(w[1]) || !good_name(w[2])) return Error{where + ": a name or site with a bad character"};
        if (!ids.insert(std::uint32_t(id)).second || !names.insert(w[1]).second)
            return Error{where + ": id or name used twice"};
        devs.push_back({std::uint32_t(id), w[1], w[2]});
    }
    return devs;
}

struct PlannedJob {
    JobKind kind = JobKind::Cross;
    JobSpec spec;
};

std::string plan_row(const PlannedJob& p) {
    std::string route;
    for (const auto& r : p.spec.route) route += (route.empty() ? "" : ">") + std::to_string(r.device_id) + ":" + r.name;
    return std::to_string(p.spec.job) + "," + kind_name(p.kind) + "," + std::to_string(p.spec.seed) + "," +
           type_name(p.spec.type) + "," + scale_name(p.spec.scale) + "," + save_mode_name(p.spec.save_mode) + "," +
           (p.kind == JobKind::Evil ? evil_name(p.spec.evil) : "") + "," + route;
}

constexpr const char* kPlanHeader = "job,kind,seed,type,scale,save_mode,evil,route";

Result<std::vector<PlannedJob>> read_plan(const fs::path& path) {
    std::string text;
    if (!read_text(path, text)) return Error{"cannot read " + path.string()};
    std::vector<std::string> lines = split_lines(text);
    if (lines.empty() || lines[0] != kPlanHeader) return Error{path.string() + ": not a mailbox plan"};
    std::vector<PlannedJob> jobs;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].empty()) continue;
        const std::string where = path.string() + ":" + std::to_string(i + 1);
        const std::vector<std::string> f = split_on(lines[i], ',');
        if (f.size() != 8) return Error{where + ": expected 8 fields"};
        PlannedJob p;
        if (!to_u64(f[0], p.spec.job) || p.spec.job != jobs.size() + 1) return Error{where + ": bad job number"};
        if (f[1] == "pairs") p.kind = JobKind::Pairs;
        else if (f[1] == "chain") p.kind = JobKind::Chain;
        else if (f[1] == "evil") p.kind = JobKind::Evil;
        else if (f[1] == "cross") p.kind = JobKind::Cross;
        else return Error{where + ": bad kind"};
        const auto t = parse_type(f[3]);
        const auto sc = parse_scale(f[4]);
        const auto m = parse_save_mode(f[5]);
        if (!to_u64(f[2], p.spec.seed) || !t || !sc || !m) return Error{where + ": bad seed, type, scale or save mode"};
        p.spec.type = *t;
        p.spec.scale = *sc;
        p.spec.save_mode = *m;
        if (p.kind == JobKind::Evil) {
            const auto e = parse_evil(f[6]);
            if (!e || *e == EvilCase::None) return Error{where + ": bad evil case"};
            p.spec.evil = *e;
        }
        for (const std::string& stop : split_on(f[7], '>')) {
            const std::size_t c = stop.find(':');
            std::uint64_t id = 0;
            if (c == std::string::npos || !to_u64(stop.substr(0, c), id) || id == 0 || id > 0xffffffffull)
                return Error{where + ": bad route"};
            p.spec.route.push_back({std::uint32_t(id), stop.substr(c + 1)});
        }
        if (p.spec.route.size() < 2 || p.spec.route.size() > kMaxRoute) return Error{where + ": bad route length"};
        jobs.push_back(std::move(p));
    }
    return jobs;
}

} // namespace

int mailbox_plan(const Options& opt, const std::string& dir_text, const std::string& devices_file) {
    Result<std::vector<SiteDevice>> devs = read_devices(devices_file);
    if (!devs) {
        say("error: " + devs.error().message);
        return 2;
    }
    std::vector<RouteEntry> a, b;   // site 1, site 2
    std::string site1, site2;
    for (const SiteDevice& d : *devs) {
        if (site1.empty()) site1 = d.site;
        if (d.site == site1) {
            a.push_back({d.id, d.name});
        } else {
            if (site2.empty()) site2 = d.site;
            if (d.site != site2) {
                say("error: more than two sites (" + site1 + ", " + site2 + ", " + d.site + ")");
                return 2;
            }
            b.push_back({d.id, d.name});
        }
    }
    if (a.size() < 2 || b.empty()) {
        say("error: a mailbox plan needs at least 2 devices on the first site and 1 on the second");
        return 2;
    }
    const fs::path dir(dir_text);
    std::error_code ec;
    if (fs::exists(dir / "box", ec)) {
        say("error: " + (dir / "box").string() + " exists (an earlier plan: look before removing it)");
        return 2;
    }
    fs::create_directories(dir / "box", ec);
    if (ec) {
        say("error: cannot create " + (dir / "box").string() + ": " + ec.message());
        return 1;
    }

    std::vector<PlannedJob> jobs;
    auto add = [&](JobKind kind, TypeId type, Scale scale, SaveMode mode, EvilCase evil, std::vector<RouteEntry> route) {
        PlannedJob p;
        p.kind = kind;
        p.spec.job = jobs.size() + 1;
        std::uint64_t x = opt.seed * 0x100000001B3ull + p.spec.job;
        p.spec.seed = splitmix64(x);
        p.spec.type = type;
        p.spec.scale = scale;
        p.spec.save_mode = mode;
        p.spec.evil = evil;
        p.spec.route = std::move(route);
        jobs.push_back(std::move(p));
    };
    // (scale, save mode) combinations; pair (i, k) takes combination (i + k) mod n, so every
    // device of either site meets every combination.
    std::vector<std::pair<Scale, SaveMode>> combos;
    for (Scale sc : opt.scales)
        for (SaveMode m : opt.save_modes) combos.push_back({sc, m});
    if (opt.plan_cross)
        for (std::size_t i = 0; i < a.size(); ++i)
            for (std::size_t k = 0; k < b.size(); ++k) {
                const auto [sc, m] = combos[(i + k) % combos.size()];
                const RouteEntry& other = a[(i + 1) % a.size()];
                for (TypeId t : opt.types) {
                    add(JobKind::Cross, t, sc, m, EvilCase::None, {a[i], b[k], other});
                    add(JobKind::Cross, t, sc, m, EvilCase::None, {b[k], a[i], other});
                }
            }
    if (opt.plan_chain && b.size() + 2 <= kMaxRoute) {
        // Site 1 -> every site-2 device once (shuffled) -> another site-1 device.
        std::size_t ti = 0;
        for (TypeId t : opt.types) {
            std::vector<RouteEntry> route{a[ti % a.size()]};
            std::vector<RouteEntry> perm = b;
            std::uint64_t x = opt.seed ^ (std::uint64_t(t) << 32) ^ 0x5eedull;
            for (std::size_t i = perm.size(); i > 1; --i) std::swap(perm[i - 1], perm[splitmix64(x) % i]);
            route.insert(route.end(), perm.begin(), perm.end());
            route.push_back(a[(ti + 1) % a.size()]);
            add(JobKind::Chain, t, opt.scales[ti % opt.scales.size()],
                opt.save_modes[(ti / opt.scales.size()) % opt.save_modes.size()], EvilCase::None, std::move(route));
            ++ti;
        }
    }
    if (opt.plan_evil)
        // Two evil cases per pair and direction; over the other site's devices every
        // receiver meets every case.
        for (std::size_t i = 0; i < a.size(); ++i)
            for (std::size_t k = 0; k < b.size(); ++k)
                for (std::size_t c = 0; c < 2; ++c) {
                    const EvilCase e = kAllEvilCases[(2 * i + k + c) % kAllEvilCases.size()];
                    add(JobKind::Evil, kAllTypes[(jobs.size() + 1) % kAllTypes.size()], opt.scales.front(),
                        opt.save_modes.front(), e, {a[i], b[k]});
                    add(JobKind::Evil, kAllTypes[(jobs.size() + 1) % kAllTypes.size()], opt.scales.front(),
                        opt.save_modes.front(), e, {b[k], a[i]});
                }
    if (jobs.empty()) {
        say("error: nothing planned");
        return 2;
    }

    std::string plan = std::string(kPlanHeader) + "\n";
    std::size_t counts[kJobKinds] = {};
    std::size_t to_site2 = 0;
    std::set<std::uint32_t> site2_ids;
    for (const RouteEntry& r : b) site2_ids.insert(r.device_id);
    for (const PlannedJob& p : jobs) {
        Result<std::string> meta = make_job_meta(p.spec);
        if (!meta) {
            say("error: make_job_meta for job " + std::to_string(p.spec.job) + ": " + meta.error().message);
            return 1;
        }
        const std::uint32_t to = p.spec.route[0].device_id;
        if (!write_text(dir / "box" / mailbox::job_file(p.spec.job, to), *meta)) {
            say("error: cannot write the Job file of job " + std::to_string(p.spec.job));
            return 1;
        }
        to_site2 += site2_ids.count(to);
        plan += plan_row(p) + "\n";
        ++counts[std::size_t(p.kind)];
    }
    std::string dev_text = "# id name site (" + opt.run + ")\n";
    for (const SiteDevice& d : *devs) dev_text += std::to_string(d.id) + " " + d.name + " " + d.site + "\n";
    if (!write_text(dir / "plan.csv", plan) || !write_text(dir / "devices.txt", dev_text)) {
        say("error: cannot write plan.csv or devices.txt in " + dir.string());
        return 1;
    }
    say("planned " + std::to_string(jobs.size()) + " jobs for " + std::to_string(a.size()) + " " + site1 + " and " +
        std::to_string(b.size()) + " " + site2 + " devices: " + std::to_string(counts[std::size_t(JobKind::Cross)]) +
        " cross, " + std::to_string(counts[std::size_t(JobKind::Chain)]) + " chain, " +
        std::to_string(counts[std::size_t(JobKind::Evil)]) + " evil; " + std::to_string(to_site2) +
        " start on " + site2 + "; Job files in " + (dir / "box").string());
    return 0;
}

int mailbox_score(const Options& opt, const std::string& dir_text) {
    const fs::path dir(dir_text), box = fs::path(dir_text) / "box", out(opt.out_dir);
    Result<std::vector<PlannedJob>> plan = read_plan(dir / "plan.csv");
    if (!plan) {
        say("error: " + plan.error().message);
        return 2;
    }
    Result<std::vector<SiteDevice>> devs = read_devices(dir / "devices.txt");
    if (!devs) {
        say("error: " + devs.error().message);
        return 2;
    }
    std::error_code ec;
    fs::create_directories(out, ec);
    if (ec) {
        say("error: cannot create " + out.string() + ": " + ec.message());
        return 1;
    }
    // Every mailbox file, by job.
    std::map<std::uint64_t, std::vector<mailbox::FileName>> files;
    fs::directory_iterator it(box, ec);
    if (ec) {
        say("error: cannot list " + box.string() + ": " + ec.message());
        return 1;
    }
    for (const fs::directory_iterator end; it != end; it.increment(ec)) {
        if (ec) break;
        if (const auto n = mailbox::parse(it->path().filename().string())) files[n->job].push_back(*n);
    }

    std::map<std::uint32_t, DeviceRec> devices;
    for (const SiteDevice& d : *devs) {
        DeviceRec r;
        r.id = d.id;
        r.name = d.name;
        std::string build;
        if (read_text(box / mailbox::build_file(d.id), build, 4096)) {
            build = build.substr(0, build.find('\n'));
            r.build = build + " (" + d.site + ", mailbox)";
        } else {
            r.build = "no build line (" + d.site + ", mailbox: never ran)";
        }
        devices[d.id] = r;
    }

    std::string jobs_csv = std::string(kJobsCsvHeader) + "\n", hops_csv = std::string(kHopsCsvHeader) + "\n";
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::pair<std::uint64_t, std::uint64_t>> matrix;
    std::size_t passed = 0, failed = 0;
    std::size_t by_kind[kJobKinds][2] = {};
    for (const PlannedJob& p : *plan) {
        JobRun j;
        j.spec = p.spec;
        j.kind = p.kind;
        j.state = JobState::Running;
        const auto& route = j.spec.route;
        j.reports.resize(route.size());
        std::string reason;
        bool unscorable = false;
        if (j.kind != JobKind::Evil) {
            std::vector<Step> steps;
            for (std::uint32_t k = 0; k < route.size(); ++k) steps.push_back({k, route[k].name});
            Result<std::vector<HopExpect>> e = replay(j.spec.type, j.spec.seed, j.spec.scale, j.spec.save_mode, steps);
            if (!e || e->size() + 1 != route.size()) {
                reason = "hub replay: " + (e ? std::string("wrong hop count") : e.error().message);
                unscorable = true;
            } else {
                j.expect = std::move(*e);
            }
        }
        const std::size_t positions = j.kind == JobKind::Evil ? 2 : route.size();
        std::string errors;   // .xmerr texts
        for (std::size_t pos = 0; pos < positions && !unscorable; ++pos) {
            const std::uint32_t id = route[pos].device_id;
            std::string data;
            if (read_text(box / mailbox::error_file(j.spec.job, std::uint32_t(pos), id), data, 8192)) {
                while (!data.empty() && data.back() == '\n') data.pop_back();
                errors += (errors.empty() ? "" : "; ") + route[pos].name + ": " + data;
            }
            if (!read_text(box / mailbox::report_file(j.spec.job, std::uint32_t(pos), id), data, 64 << 10)) continue;
            DeviceRec& d = devices[id];
            ++d.reports;
            Result<ReportRow> row = read_report(data);
            if (!row) {
                reason = "unreadable report of " + route[pos].name + ": " + row.error().message;
                break;
            }
            const std::uint32_t want_hop = pos == 0 ? 0 : std::uint32_t(pos - 1);
            if (row->job != j.spec.job || row->hop != want_hop) {
                reason = "report of " + route[pos].name + " names job " + std::to_string(row->job) + " hop " +
                         std::to_string(row->hop);
                break;
            }
            if (row->verdict == protocol::HopVerdict::Ok) {
                ++d.reports_ok;
                d.verify_ns += row->verify_ns;
                d.check_ns += row->check_ns;
                d.mutate_ns += row->mutate_ns;
                d.save_ns += row->save_ns;
            }
            j.reports[pos] = std::move(*row);
        }
        bool pass = false;
        if (reason.empty()) {
            const Judgement v = judge(j);
            if (!v.done) {
                std::string missing;
                for (std::size_t pos = 0; pos < positions; ++pos)
                    if (!j.reports[pos]) missing += (missing.empty() ? "" : ", ") + route[pos].name;
                reason = "no report from " + missing + (errors.empty() ? "" : " (" + errors + ")");
            } else {
                pass = v.passed;
                reason = v.reason;
            }
        }
        // The hop files: no more than the route has; for a normal job each one's data
        // part is the replay's, byte for byte (wire digest and length).
        if (pass) {
            const std::uint32_t hops = j.kind == JobKind::Evil ? 1 : std::uint32_t(route.size() - 1);
            for (const mailbox::FileName& f : files[j.spec.job])
                if (f.kind == mailbox::FileKind::Hop && (f.number >= hops || f.device != route[f.number + 1].device_id)) {
                    pass = false;
                    reason = "unexpected hop file " + mailbox::hop_file(j.spec.job, f.number, f.device) +
                             (j.kind == JobKind::Evil ? " (the evil object went on)" : "");
                    break;
                }
        }
        if (pass && j.kind != JobKind::Evil) {
            for (std::uint32_t k = 0; k + 1 < route.size() && pass; ++k) {
                const std::string name = mailbox::hop_file(j.spec.job, k, route[k + 1].device_id);
                std::string payload;
                if (!read_text(box / name, payload, protocol::max_payload(protocol::MsgType::Hop))) {
                    pass = false;
                    reason = "hop file " + name + " missing";
                    break;
                }
                Result<protocol::HopParts> parts = protocol::unpack_hop(payload);
                if (!parts) {
                    pass = false;
                    reason = name + ": " + parts.error().message;
                } else if (protocol::wire_digest(parts->data) != j.expect[k].wire_digest ||
                           parts->data.size() != j.expect[k].bytes) {
                    pass = false;
                    reason = name + ": data part " + std::to_string(parts->data.size()) + " bytes, wire " +
                             hex64(protocol::wire_digest(parts->data)) + "; replay " +
                             std::to_string(j.expect[k].bytes) + " bytes, wire " + hex64(j.expect[k].wire_digest);
                }
            }
        }
        j.state = pass ? JobState::Passed : JobState::Failed;
        j.reason = reason;
        ++(pass ? passed : failed);
        ++by_kind[std::size_t(j.kind)][1];
        if (pass) ++by_kind[std::size_t(j.kind)][0];
        std::set<std::uint32_t> seen;
        for (const auto& r : route) {
            if (!seen.insert(r.device_id).second) continue;
            DeviceRec& d = devices[r.device_id];
            ++d.jobs;
            if (pass) ++d.jobs_passed;
        }
        if (j.kind == JobKind::Pairs || j.kind == JobKind::Cross) {
            auto& cell = matrix[{route[0].device_id, route[1].device_id}];
            ++cell.first;
            if (pass) ++cell.second;
        }
        jobs_csv += job_csv_row(j, "") + "\n";
        for (const std::string& row : hop_csv_rows(j)) hops_csv += row + "\n";
    }

    std::string dev_csv = "id,name,build,flags,joined_ms,left_ms\n", mat_csv = "sender,receiver,jobs,passed\n";
    for (const auto& [id, d] : devices)
        dev_csv += std::to_string(d.id) + "," + csv_field(d.name) + "," + csv_field(d.build) + ",0,0,0\n";
    for (const auto& [key, cell] : matrix)
        mat_csv += csv_field(devices[key.first].name) + "," + csv_field(devices[key.second].name) + "," +
                   std::to_string(cell.first) + "," + std::to_string(cell.second) + "\n";
    std::string sum = "run " + opt.run + " (mailbox " + dir.string() + "), xo_hub " + control::build_info() + "\n";
    for (const auto& [id, d] : devices) {
        auto avg_us = [&](std::uint64_t ns) {
            char b[32];
            std::snprintf(b, sizeof b, "%.1f", d.reports_ok ? double(ns) / double(d.reports_ok) / 1000.0 : 0.0);
            return std::string(b);
        };
        sum += "device " + std::to_string(d.id) + " " + d.name + ": jobs " + std::to_string(d.jobs_passed) + "/" +
               std::to_string(d.jobs) + " passed, reports " + std::to_string(d.reports_ok) + " ok of " +
               std::to_string(d.reports) + ", mean us verify " + avg_us(d.verify_ns) + " check " + avg_us(d.check_ns) +
               " mutate " + avg_us(d.mutate_ns) + " save " + avg_us(d.save_ns) + " | " + d.build + "\n";
    }
    for (std::size_t k = 0; k < kJobKinds; ++k)
        if (by_kind[k][1] != 0)
            sum += std::string("kind ") + kind_name(JobKind(k)) + ": " + std::to_string(by_kind[k][0]) + "/" +
                   std::to_string(by_kind[k][1]) + " passed\n";
    sum += "jobs " + std::to_string(plan->size()) + ", passed " + std::to_string(passed) + ", failed " +
           std::to_string(failed) + ", not finished 0\n";
    const bool pass = !plan->empty() && failed == 0;
    sum += pass ? "RESULT PASS\n" : "RESULT FAIL\n";
    if (!write_text(out / "jobs.csv", jobs_csv) || !write_text(out / "hops.csv", hops_csv) ||
        !write_text(out / "devices.csv", dev_csv) || !write_text(out / "matrix.csv", mat_csv) ||
        !write_text(out / "summary.txt", sum)) {
        say("error: cannot write the results into " + out.string());
        return 1;
    }
    std::fputs(sum.c_str(), stderr);
    return pass ? 0 : 1;
}

} // namespace xmesh::hub
