// Device mesh demo: what one device does with a Job or a Hop, in memory.
// FROZEN INTERFACE (demo/mesh/INTERFACES.md section 5). Bodies live in
// src/hop.cpp, owned by the types thread. The device program (network),
// mailbox mode and the self-test all call these, so the checks a device
// runs are the same whether the bytes came over a socket or from a file.
//
// handle_hop checks, in this order, stopping at the first failure:
//   1. unpack_hop                                    -> BadEnvelope
//   2. HopMeta: try_load_verified with kControlCap; type, scale, save mode
//      and evil known; route non-empty and <= kMaxRoute; hop + 1 < route size
//                                                    -> BadMeta
//   3. route[hop + 1].device_id == this device       -> Misrouted
//   4. data part <= data_limit(type, scale)          -> TooLarge
//   5. wire_digest(data) == HopMeta::wire_digest     -> WireDigest
//   6. receive_object (load_verified, receive_cap)   -> LoadRejected
//   7. content_digest == HopMeta::content_digest     -> ContentDigest
//   8. check_against_model(..., upto = hop)          -> ModelMismatch
// then, unless this device is the last stop:
//   9. mutate(hop + 1)                               -> MutateFailed
//  10. save(save mode), pack_hop with hop + 1        -> SaveFailed
// and always writes a HopReport.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "xmesh/protocol.hpp"
#include "xmesh/result.hpp"
#include "xmesh/types.hpp"

namespace xmesh {

// A job as the hub plans it.
struct RouteEntry {
    std::uint32_t device_id = 0;
    std::string name;
};
struct JobSpec {
    std::uint64_t job = 0;
    std::uint64_t seed = 0;
    TypeId type = TypeId::Document;
    Scale scale = Scale::Small;
    SaveMode save_mode = SaveMode::Verified;
    EvilCase evil = EvilCase::None;
    std::vector<RouteEntry> route;   // 2 .. kMaxRoute stops; route[0] makes state 0
};

// The Job payload: HopMeta with hop 0, both digests 0 and evil as given.
Result<std::string> make_job_meta(const JobSpec& spec);

// What a device did with one Job or Hop.
struct HopOutcome {
    protocol::HopVerdict verdict = protocol::HopVerdict::Ok;
    std::string error;                 // empty when ok
    std::uint64_t job = 0;             // 0 when the meta could not be read
    std::string report;                // HopReport wire for the hub (empty when job == 0)
    std::string next_payload;          // Hop payload to send on; empty when ended or refused
    std::uint32_t next_device = 0;     // its receiver, route[hop + 1] of what is sent
    std::string summary;               // summary() of the object after this device, for logs
};

// route[0]: builds state 0 (or the evil part, HopMeta::evil != None) from
// a Job payload and packs hop 0.
HopOutcome start_job(std::string_view job_meta_wire, std::uint32_t my_device_id);

// route[k], k >= 1: the checks above on a received Hop payload.
HopOutcome handle_hop(std::string_view hop_payload, std::uint32_t my_device_id);

// The hub's reading of a Report payload (try_load_verified, kControlCap).
struct ReportRow {
    std::uint64_t job = 0;
    std::uint32_t hop = 0;
    protocol::HopVerdict verdict = protocol::HopVerdict::Ok;
    bool expected_reject = false;
    std::uint64_t content_digest = 0;
    std::uint64_t sent_wire_digest = 0;
    std::uint32_t data_bytes = 0;
    std::uint32_t sent_bytes = 0;
    std::uint32_t grown_bytes = 0;
    std::uint64_t verify_ns = 0, check_ns = 0, mutate_ns = 0, save_ns = 0;
    std::string error;
};
Result<ReportRow> read_report(std::string_view report_wire);

} // namespace xmesh
