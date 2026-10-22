// Map state demo: timing/size samples and CSV output.
// FROZEN INTERFACE (demo/mapstate/INTERFACES.md). Bodies in src/metrics.cpp,
// owned by the common-library thread.
//
// Metric names: "<role>.<name>_us" (microseconds, process-local steady clock),
// "<role>.<name>_bytes", "<role>.<name>_count". The list each process records
// is fixed in INTERFACES.md section 6.
#pragma once
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace mapdemo::metrics {

struct Summary {
    std::size_t count = 0;
    double mean = 0, p50 = 0, p95 = 0, max = 0;
};

class Recorder {
public:
    explicit Recorder(std::string role) : role_(std::move(role)) {}
    const std::string& role() const noexcept { return role_; }
    void add(std::string_view metric, double value);
    Summary summary(std::string_view metric) const;
    // One console line: "<role> metric=mean/p95 ..." for the named metrics.
    void print_line(std::FILE* out, std::initializer_list<std::string_view> metrics) const;
    // Header: role,metric,count,mean,p50,p95,max (one row per metric, sorted).
    bool write_csv(const std::string& path) const;

private:
    std::string role_;
    std::map<std::string, std::vector<double>, std::less<>> samples_;
};

class Stopwatch {
public:
    Stopwatch() noexcept { restart(); }
    void restart() noexcept;
    double us() const noexcept;   // elapsed microseconds since restart
private:
    std::uint64_t t0_ = 0;
};

// Plain CSV file: the caller passes the header once and preformatted rows.
class CsvFile {
public:
    CsvFile() = default;
    ~CsvFile();
    CsvFile(const CsvFile&) = delete;
    CsvFile& operator=(const CsvFile&) = delete;
    bool open(const std::string& path, std::string_view header);
    void row(std::string_view line);   // appends '\n'
private:
    std::FILE* f_ = nullptr;
};

} // namespace mapdemo::metrics
