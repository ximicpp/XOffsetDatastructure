// Device mesh demo: a device's handling of a Job or a Hop (hop.hpp).
// STUB until the types thread fills it in (INTERFACES.md section 7).
#include "xmesh/hop.hpp"

namespace xmesh {
namespace {
HopOutcome not_implemented(protocol::HopVerdict verdict) {
    HopOutcome out;
    out.verdict = verdict;
    out.error = "hop handling: not implemented yet";
    return out;
}
} // namespace

Result<std::string> make_job_meta(const JobSpec&) { return Error{"make_job_meta: not implemented yet"}; }
HopOutcome start_job(std::string_view, std::uint32_t) { return not_implemented(protocol::HopVerdict::MakeFailed); }
HopOutcome handle_hop(std::string_view, std::uint32_t) { return not_implemented(protocol::HopVerdict::BadMeta); }
Result<ReportRow> read_report(std::string_view) { return Error{"read_report: not implemented yet"}; }

} // namespace xmesh
