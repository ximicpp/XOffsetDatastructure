// Device mesh demo: the hub's state, shared by the relay loop (hub.cpp) and
// the HTTP pages (http.cpp). OWNER: network thread.
#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "xmesh/hop.hpp"
#include "xmesh/types.hpp"

namespace xmesh::hub {

struct Options {
    std::string run = "mesh";
    std::string out_dir;
    std::string bind = "0.0.0.0";
    std::uint16_t port = protocol::kDefaultPort;
    std::uint16_t http_port = protocol::kDefaultHttpPort;   // 0: no HTTP
    std::string advertise;          // host in the join link ("" = from the request, then the LAN address)
    int expect = 0;                 // start when this many devices registered (0: after --wait-s)
    int wait_s = 120;
    bool plan_pairs = true, plan_chain = true, plan_evil = true;
    std::vector<TypeId> types{kAllTypes.begin(), kAllTypes.end()};
    std::vector<Scale> scales{Scale::Small, Scale::Medium};
    std::vector<SaveMode> save_modes{SaveMode::Verified, SaveMode::Compact};
    std::uint64_t seed = 1;
    int max_inflight = 8;
    int max_per_device = 2;         // jobs in flight that one device takes part in
    int job_timeout_s = 60;
    bool linger = false;
    bool archive = true;
};

enum class JobKind : std::uint8_t { Pairs, Chain, Evil };
const char* kind_name(JobKind kind) noexcept;

struct DeviceRec {
    std::uint32_t id = 0;
    std::string name;
    std::string build;
    std::uint32_t flags = 0;
    std::uint64_t joined_ms = 0;    // wall clock
    std::uint64_t left_ms = 0;      // 0 while present
    bool present = false;
    std::uint64_t reports = 0, reports_ok = 0;
    std::uint64_t jobs = 0, jobs_passed = 0;
    std::uint64_t verify_ns = 0, check_ns = 0, mutate_ns = 0, save_ns = 0;   // sums over ok reports
    std::uint64_t inflight = 0;     // running jobs this device is in
};

enum class JobState : std::uint8_t { Pending, Running, Passed, Failed };

struct JobRun {
    JobSpec spec;
    JobKind kind = JobKind::Pairs;
    JobState state = JobState::Pending;
    std::vector<HopExpect> expect;  // replay (normal jobs)
    std::vector<std::optional<ReportRow>> reports;   // by route position
    std::uint32_t hops_forwarded = 0;
    std::uint64_t start_ms = 0, end_ms = 0;   // steady clock
    std::string reason;
    std::string last_payload;       // last Hop payload forwarded (for the summary)
};

struct Recent {
    std::uint64_t job = 0;
    std::string text;               // kind, type, route, result, summary
    bool passed = false;
};

struct State {
    Options opt;
    std::string hub_build;
    std::uint64_t start_wall_ms = 0;
    std::uint64_t start_ms = 0;
    std::map<std::uint32_t, DeviceRec> devices;
    std::uint32_t next_device_id = 1;
    std::uint64_t roster_version = 0;
    bool planned = false;
    std::vector<JobRun> jobs;       // index = job id - 1
    std::deque<std::size_t> pending;
    std::size_t running = 0, passed = 0, failed = 0;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::pair<std::uint64_t, std::uint64_t>> matrix;   // jobs, passed
    std::deque<Recent> recent;      // newest first, at most 30
    std::string lan_address;
    bool finished = false;
};

// HTTP pages (http.cpp). host_header: the request's Host header.
std::string page_dashboard(const State& s);
std::string page_join(const State& s, const std::string& host_header);
std::string status_json(const State& s);
std::string join_link(const State& s, const std::string& host_header);

std::string html_escape(const std::string& text);

int run(const Options& opt);

} // namespace xmesh::hub
