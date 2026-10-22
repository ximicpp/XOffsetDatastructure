// Map state demo: metrics bodies. OWNER: common-library thread.
//
// Percentiles are NEAREST-RANK over all samples of a metric: for p in (0, 100],
// the value at 1-based rank ceil(p / 100 * count) of the sorted samples
// (p50 of 4 samples = 2nd smallest, p95 of 20 samples = 19th). max is the
// largest sample; mean is the arithmetic mean. No samples -> all zero.
#include "mapdemo/metrics.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace mapdemo::metrics {
namespace {

double nearest_rank(const std::vector<double>& sorted, double p) {
    const std::size_t n = sorted.size();
    std::size_t rank = static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(n)));
    rank = std::clamp<std::size_t>(rank, 1, n);
    return sorted[rank - 1];
}

Summary summarize(std::vector<double> v) {
    Summary s;
    s.count = v.size();
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    double sum = 0;
    for (double x : v) sum += x;
    s.mean = sum / static_cast<double>(v.size());
    s.p50 = nearest_rank(v, 50.0);
    s.p95 = nearest_rank(v, 95.0);
    s.max = v.back();
    return s;
}

std::uint64_t steady_ns() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // namespace

void Recorder::add(std::string_view metric, double value) {
    auto it = samples_.find(metric);
    if (it == samples_.end()) it = samples_.emplace(std::string(metric), std::vector<double>{}).first;
    it->second.push_back(value);
}

Summary Recorder::summary(std::string_view metric) const {
    const auto it = samples_.find(metric);
    if (it == samples_.end()) return Summary{};
    return summarize(it->second);
}

void Recorder::print_line(std::FILE* out, std::initializer_list<std::string_view> metrics) const {
    std::fprintf(out, "%s", role_.c_str());
    for (std::string_view m : metrics) {
        const Summary s = summary(m);
        std::fprintf(out, " %.*s=%.1f/%.1f", static_cast<int>(m.size()), m.data(), s.mean, s.p95);
    }
    std::fprintf(out, "\n");
    std::fflush(out);
}

bool Recorder::write_csv(const std::string& path) const {
    CsvFile f;
    if (!f.open(path, "role,metric,count,mean,p50,p95,max")) return false;
    char line[512];
    for (const auto& [name, values] : samples_) {   // std::map: sorted by name
        const Summary s = summarize(values);
        std::snprintf(line, sizeof line, "%s,%s,%zu,%.3f,%.3f,%.3f,%.3f", role_.c_str(), name.c_str(), s.count,
                      s.mean, s.p50, s.p95, s.max);
        f.row(line);
    }
    return true;
}

void Stopwatch::restart() noexcept { t0_ = steady_ns(); }

double Stopwatch::us() const noexcept { return static_cast<double>(steady_ns() - t0_) / 1000.0; }

CsvFile::~CsvFile() {
    if (f_ != nullptr) std::fclose(f_);
}

bool CsvFile::open(const std::string& path, std::string_view header) {
    if (f_ != nullptr) std::fclose(f_);
    f_ = std::fopen(path.c_str(), "w");
    if (f_ == nullptr) return false;
    row(header);
    return true;
}

void CsvFile::row(std::string_view line) {
    if (f_ == nullptr) return;
    std::fwrite(line.data(), 1, line.size(), f_);
    std::fputc('\n', f_);
}

} // namespace mapdemo::metrics
