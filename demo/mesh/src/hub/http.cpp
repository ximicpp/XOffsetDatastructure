// Device mesh demo: the hub's HTTP pages (INTERFACES.md section 6).
// OWNER: network thread.
//
//   /             dashboard: roster, jobs in flight, the pairs matrix, the
//                 latest finished jobs (refreshes every 2 s)
//   /join         join page: QR code of the join link and the xo_peer command
//   /status.json  the dashboard's data for scripts
#include <cstdio>
#include <set>
#include "hub/hub.hpp"
#include "hub/qr.hpp"
#include "xmesh/net.hpp"

namespace xmesh::hub {
namespace {

std::string json_string(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char b[8];
                    std::snprintf(b, sizeof b, "\\u%04x", c);
                    out += b;
                } else {
                    out += char(c);
                }
        }
    }
    return out + "\"";
}

std::string url_encode(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~' || c == ':') {
            out += char(c);
        } else {
            char b[4];
            std::snprintf(b, sizeof b, "%%%02X", c);
            out += b;
        }
    }
    return out;
}

bool is_loopback(const std::string& host) {
    return host.empty() || host == "localhost" || host.rfind("127.", 0) == 0 || host == "::1" || host == "[::1]" ||
           host == "0.0.0.0";
}

std::string route_names(const JobRun& j) {
    std::string s;
    for (const auto& r : j.spec.route) s += (s.empty() ? "" : " > ") + r.name;
    return s;
}

const char* kStyle = R"(<style>
:root{--bg:#fafafa;--fg:#1a1a1a;--muted:#666;--line:#ddd;--ok:#1a7f37;--bad:#c62828;--card:#fff}
@media (prefers-color-scheme:dark){:root{--bg:#121212;--fg:#e8e8e8;--muted:#9a9a9a;--line:#333;--ok:#4caf50;--bad:#ef5350;--card:#1d1d1d}}
body{background:var(--bg);color:var(--fg);font:14px/1.45 system-ui,sans-serif;margin:0;padding:16px;max-width:1200px}
h1{font-size:20px;margin:0 0 4px}h2{font-size:16px;margin:20px 0 8px}
.muted{color:var(--muted)}.ok{color:var(--ok)}.bad{color:var(--bad)}
table{border-collapse:collapse;background:var(--card);font-size:13px}td,th{border:1px solid var(--line);padding:3px 7px;text-align:left;vertical-align:top}
.wrap{overflow-x:auto}code,pre{font:12px ui-monospace,monospace}pre{background:var(--card);border:1px solid var(--line);padding:8px;overflow-x:auto;white-space:pre-wrap;word-break:break-all}
.qr{background:#fff;display:inline-block;padding:8px;border-radius:6px}.qr svg{max-width:100%;height:auto}
</style>)";

} // namespace

std::string html_escape(const std::string& text) {
    std::string out;
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

// http://HOST:HTTP_PORT/join?hub=HOST:PORT&run=LABEL. HOST: --advertise, else
// the host the browser used (when it is not a loopback name), else this
// machine's LAN address, else localhost.
std::string join_link(const State& s, const std::string& host_header) {
    std::string host = s.opt.advertise;
    if (host.empty()) {
        std::string h;
        std::uint16_t ignored = 0;
        if (!host_header.empty() && net::split_host_port(host_header, h, ignored, 80) && !is_loopback(h)) host = h;
    }
    if (host.empty()) host = s.lan_address;
    if (host.empty()) host = "localhost";
    const std::string bracketed = host.find(':') != std::string::npos ? "[" + host + "]" : host;
    return "http://" + bracketed + ":" + std::to_string(s.opt.http_port) + "/join?hub=" + url_encode(bracketed) + ":" +
           std::to_string(s.opt.port) + "&run=" + url_encode(s.opt.run);
}

std::string page_join(const State& s, const std::string& host_header) {
    const std::string link = join_link(s, host_header);
    const qr::Code code = qr::encode(link);
    std::string p = "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" "
                    "content=\"width=device-width,initial-scale=1\"><title>Join mesh " +
                    html_escape(s.opt.run) + "</title>" + kStyle + "</head><body>";
    p += "<h1>Join the device mesh &ldquo;" + html_escape(s.opt.run) + "&rdquo;</h1>";
    p += "<p class=\"muted\">Scan the code on the device, or copy the link. The device program takes the link as it "
         "is; it registers at the hub and then receives, checks, changes and sends on objects through the hub.</p>";
    p += "<div class=\"qr\">" + qr::to_svg(code, 6) + "</div>";
    p += "<p><a href=\"" + html_escape(link) + "\"><code>" + html_escape(link) + "</code></a></p>";
    p += "<h2>Start the device program</h2>";
    p += "<p>Linux, macOS:</p><pre>./xo_peer --join '" + html_escape(link) + "'</pre>";
    p += "<p>Windows:</p><pre>xo_peer.exe --join \"" + html_escape(link) + "\"</pre>";
    p += "<p>Android (adb):</p><pre>adb push xo_peer /data/local/tmp/ &amp;&amp; adb shell /data/local/tmp/xo_peer --join '" +
         html_escape(link) + "'</pre>";
    p += "<p class=\"muted\">The Android emulator reaches the host as 10.0.2.2: use "
         "<code>--hub 10.0.2.2:" + std::to_string(s.opt.port) + "</code> there. Add <code>--name NAME</code> to "
         "pick the device's name (default: os-arch-compiler).</p>";
    p += "<p><a href=\"/\">Dashboard</a></p></body></html>";
    return p;
}

std::string page_dashboard(const State& s) {
    std::string p = "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" "
                    "content=\"width=device-width,initial-scale=1\"><meta http-equiv=\"refresh\" content=\"2\">"
                    "<title>Mesh " + html_escape(s.opt.run) + "</title>" + kStyle + "</head><body>";
    const std::size_t done = s.passed + s.failed;
    p += "<h1>Device mesh &ldquo;" + html_escape(s.opt.run) + "&rdquo;</h1>";
    p += "<p class=\"muted\">" + html_escape(s.hub_build) + " &middot; <a href=\"/join\">join link and QR code</a>"
         " &middot; <a href=\"/status.json\">status.json</a></p>";
    if (!s.planned) {
        std::size_t present = 0;
        for (const auto& [id, d] : s.devices) present += d.present ? 1 : 0;
        p += "<p>Waiting for devices: " + std::to_string(present) + " registered" +
             (s.opt.expect > 0 ? ", starting at " + std::to_string(s.opt.expect) : std::string()) + ".</p>";
    } else {
        p += "<p>Jobs: <b>" + std::to_string(done) + "</b> of " + std::to_string(s.jobs.size()) + " done, <span class=\"ok\">" +
             std::to_string(s.passed) + " passed</span>, <span class=\"" + (s.failed ? "bad" : "muted") + "\">" +
             std::to_string(s.failed) + " failed</span>, " + std::to_string(s.running) + " running" +
             (s.finished ? " &middot; finished" : "") + ".</p>";
    }
    p += "<h2>Devices</h2><div class=\"wrap\"><table><tr><th>id</th><th>name</th><th>jobs passed</th><th>reports ok</th>"
         "<th>build</th></tr>";
    for (const auto& [id, d] : s.devices) {
        p += "<tr><td>" + std::to_string(d.id) + "</td><td>" + html_escape(d.name) +
             (d.present ? "" : " <span class=\"muted\">(left)</span>") + "</td><td>" + std::to_string(d.jobs_passed) +
             "/" + std::to_string(d.jobs) + "</td><td>" + std::to_string(d.reports_ok) + "/" +
             std::to_string(d.reports) + "</td><td class=\"muted\">" + html_escape(d.build) + "</td></tr>";
    }
    p += "</table></div>";

    p += "<h2>In flight</h2>";
    std::string rows;
    const std::uint64_t now = net::now_ms();
    for (const JobRun& j : s.jobs)
        if (j.state == JobState::Running)
            rows += "<tr><td>" + std::to_string(j.spec.job) + "</td><td>" + kind_name(j.kind) + "</td><td>" +
                    type_name(j.spec.type) + " " + scale_name(j.spec.scale) + " " + save_mode_name(j.spec.save_mode) +
                    (j.kind == JobKind::Evil ? std::string(" ") + evil_name(j.spec.evil) : "") + "</td><td>" +
                    html_escape(route_names(j)) + "</td><td>" + std::to_string(j.hops_forwarded) + "</td><td>" +
                    std::to_string(now - j.start_ms) + " ms</td></tr>";
    p += rows.empty() ? "<p class=\"muted\">none</p>"
                      : "<div class=\"wrap\"><table><tr><th>job</th><th>kind</th><th>object</th><th>route</th>"
                        "<th>hops</th><th>age</th></tr>" + rows + "</table></div>";

    // Pairs matrix: row = sender (route[0]), column = receiver (route[1]).
    p += "<h2>Pairs (sender &rarr; receiver, passed/jobs)</h2>";
    if (s.matrix.empty()) {
        p += "<p class=\"muted\">no pair jobs finished yet</p>";
    } else {
        std::set<std::uint32_t> ids;
        for (const auto& [key, cell] : s.matrix) {
            ids.insert(key.first);
            ids.insert(key.second);
        }
        p += "<div class=\"wrap\"><table><tr><th></th>";
        for (std::uint32_t c : ids) p += "<th>" + std::to_string(c) + "</th>";
        p += "</tr>";
        for (std::uint32_t r : ids) {
            const auto it = s.devices.find(r);
            p += "<tr><th>" + std::to_string(r) + " " + html_escape(it == s.devices.end() ? "" : it->second.name) + "</th>";
            for (std::uint32_t c : ids) {
                const auto m = s.matrix.find({r, c});
                if (m == s.matrix.end()) {
                    p += "<td class=\"muted\">" + std::string(r == c ? "&mdash;" : "") + "</td>";
                    continue;
                }
                const bool all = m->second.first == m->second.second;
                p += "<td class=\"" + std::string(all ? "ok" : "bad") + "\">" + std::to_string(m->second.second) + "/" +
                     std::to_string(m->second.first) + "</td>";
            }
            p += "</tr>";
        }
        p += "</table></div>";
    }

    p += "<h2>Latest jobs</h2>";
    if (s.recent.empty()) p += "<p class=\"muted\">none yet</p>";
    for (const Recent& r : s.recent)
        p += "<div class=\"" + std::string(r.passed ? "" : "bad") + "\"><code>" + std::to_string(r.job) + "</code> " +
             html_escape(r.text) + "</div>";
    p += "</body></html>";
    return p;
}

std::string status_json(const State& s) {
    std::string j = "{\"run\":" + json_string(s.opt.run) + ",\"hub_build\":" + json_string(s.hub_build) +
                    ",\"join\":" + json_string(join_link(s, "")) + ",\"planned\":" + (s.planned ? "true" : "false") +
                    ",\"finished\":" + (s.finished ? "true" : "false") + ",\"jobs\":" + std::to_string(s.jobs.size()) +
                    ",\"passed\":" + std::to_string(s.passed) + ",\"failed\":" + std::to_string(s.failed) +
                    ",\"running\":" + std::to_string(s.running) + ",\"roster_version\":" +
                    std::to_string(s.roster_version) + ",\"devices\":[";
    bool first = true;
    for (const auto& [id, d] : s.devices) {
        j += std::string(first ? "" : ",") + "{\"id\":" + std::to_string(d.id) + ",\"name\":" + json_string(d.name) +
             ",\"build\":" + json_string(d.build) + ",\"flags\":" + std::to_string(d.flags) + ",\"present\":" +
             (d.present ? "true" : "false") + ",\"jobs\":" + std::to_string(d.jobs) + ",\"jobs_passed\":" +
             std::to_string(d.jobs_passed) + ",\"reports\":" + std::to_string(d.reports) + ",\"reports_ok\":" +
             std::to_string(d.reports_ok) + "}";
        first = false;
    }
    j += "],\"inflight\":[";
    first = true;
    for (const JobRun& r : s.jobs)
        if (r.state == JobState::Running) {
            j += std::string(first ? "" : ",") + "{\"job\":" + std::to_string(r.spec.job) + ",\"kind\":\"" +
                 kind_name(r.kind) + "\",\"type\":\"" + type_name(r.spec.type) + "\",\"route\":" +
                 json_string(route_names(r)) + ",\"hops\":" + std::to_string(r.hops_forwarded) + "}";
            first = false;
        }
    j += "],\"matrix\":[";
    first = true;
    for (const auto& [key, cell] : s.matrix) {
        j += std::string(first ? "" : ",") + "{\"sender\":" + std::to_string(key.first) + ",\"receiver\":" +
             std::to_string(key.second) + ",\"jobs\":" + std::to_string(cell.first) + ",\"passed\":" +
             std::to_string(cell.second) + "}";
        first = false;
    }
    j += "],\"recent\":[";
    first = true;
    for (const Recent& r : s.recent) {
        j += std::string(first ? "" : ",") + "{\"job\":" + std::to_string(r.job) + ",\"passed\":" +
             (r.passed ? "true" : "false") + ",\"text\":" + json_string(r.text) + "}";
        first = false;
    }
    return j + "]}\n";
}

} // namespace xmesh::hub
