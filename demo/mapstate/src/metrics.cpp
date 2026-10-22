// Map state demo: metrics bodies. OWNER: common-library thread. STUB.
#include "mapdemo/metrics.hpp"
#include <stdexcept>

namespace mapdemo::metrics {
void Recorder::add(std::string_view metric, double value) { samples_[std::string(metric)].push_back(value); }
Summary Recorder::summary(std::string_view) const { throw std::logic_error("metrics: not implemented"); }
void Recorder::print_line(std::FILE*, std::initializer_list<std::string_view>) const {}
bool Recorder::write_csv(const std::string&) const { return false; }
void Stopwatch::restart() noexcept {}
double Stopwatch::us() const noexcept { return 0; }
CsvFile::~CsvFile() {}
bool CsvFile::open(const std::string&, std::string_view) { return false; }
void CsvFile::row(std::string_view) {}
} // namespace mapdemo::metrics
