#ifndef XOFFSET_EXPERIMENT_N3_NETWORK_SUPPORT_HPP
#define XOFFSET_EXPERIMENT_N3_NETWORK_SUPPORT_HPP

// Consumer-only observations. These records deliberately make no claim about
// malloc usable bytes, libc memset, kernel copies, or formal timing evidence.
#include "n3_runtime.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <unistd.h>

namespace n3_net {

static_assert(sizeof(XOffsetDatastructure::XWireHeaderV1)==88);
static_assert(sizeof(XOffsetDatastructure::detail::WireValidationRange)==16);
static_assert(sizeof(std::uint32_t)==4 && sizeof(std::size_t)==8);

struct Cycle {
    std::string cycle;
    std::uint64_t input_bytes=0,output_bytes=0;
    std::uint64_t server_saved_size=0,server_saved_capacity=0;
    std::uint64_t client_output_capacity=0;
    bool server_strings_released=false;
    // Copying the original IOProgress, including scalar additions, preserves
    // endpoint synchronization and keeps warm observations non-owning.
    struct Endpoint {
        std::uint64_t read=0,written=0,allocations=0,waits=0,terminal_count=0;
        bool published=false;
        std::uint64_t rx_size=0,rx_capacity=0,rx_init_extent=0;
        std::uint64_t header_copy_calls=0,header_copy_bytes=0;
    } server,client;
};

template<class IO> inline Cycle::Endpoint endpoint(const IO& io) {
    return {io.read,io.written,io.allocations,io.waits,io.terminal_count,
            io.published,io.n3_rx_size,io.n3_rx_capacity,io.n3_rx_init_extent,
            io.n3_header_copy_calls,io.n3_header_copy_bytes};
}

template<class Sample> inline Cycle observe_cycle(const Sample& sample,
                                                 std::string cycle) {
    return {std::move(cycle),sample.input_bytes,sample.output_bytes,
            sample.n3_server_saved_size,sample.n3_server_saved_capacity,
            sample.n3_client_output_capacity,sample.n3_server_strings_released,
            endpoint(sample.server_io),endpoint(sample.client_io)};
}

inline std::string cycle_name(bool has_warmup,std::size_t ordinal) {
    if(has_warmup&&ordinal==0)return "warm";
    return "work."+std::to_string(ordinal-(has_warmup?1:0));
}

inline void boolean(std::ostream& out,bool value) {out<<(value?"true":"false");}
inline void endpoint_json(std::ostream& out,const Cycle::Endpoint& e) {
    out<<"{\"read\":"<<e.read<<",\"written\":"<<e.written
       <<",\"allocations\":"<<e.allocations<<",\"waits\":"<<e.waits
       <<",\"terminal_count\":"<<e.terminal_count<<",\"published\":";
    boolean(out,e.published);
    out<<",\"rx_size\":"<<e.rx_size<<",\"rx_capacity\":"<<e.rx_capacity
       <<",\"rx_init_extent\":"<<e.rx_init_extent
       <<",\"header_copy_calls\":"<<e.header_copy_calls
       <<",\"header_copy_bytes\":"<<e.header_copy_bytes<<'}';
}
inline void cycle_json(std::ostream& out,const Cycle& c) {
    out<<"{\"cycle\":";n3::detail::write_string(out,c.cycle.c_str());
    out<<",\"input_bytes\":"<<c.input_bytes<<",\"output_bytes\":"<<c.output_bytes
       <<",\"server_saved_size\":"<<c.server_saved_size
       <<",\"server_saved_capacity\":"<<c.server_saved_capacity
       <<",\"client_output_capacity\":"<<c.client_output_capacity
       <<",\"server_strings_released\":";boolean(out,c.server_strings_released);
    out<<",\"server\":";endpoint_json(out,c.server);
    out<<",\"client\":";endpoint_json(out,c.client);out<<'}';
}
template<class Stats> inline void stats_json(std::ostream& out,const Stats& s) {
    out<<"{\"cached_blocks\":"<<s.cached_blocks
       <<",\"cached_reserved_bytes\":"<<s.cached_reserved_bytes
       <<",\"cached_accessible_bytes\":"<<s.cached_accessible_bytes
       <<",\"hits\":"<<s.hits<<",\"misses\":"<<s.misses<<'}';
}
template<class Stats> inline void pair_json(std::ostream& out,const Stats& s,
                                           const Stats& c) {
    out<<"{\"server\":";stats_json(out,s);out<<",\"client\":";
    stats_json(out,c);out<<'}';
}
inline std::uint64_t page_size() {
    const auto value=::sysconf(_SC_PAGESIZE);
    if(value<=0)throw std::runtime_error("N3 network page-size query failed");
    return static_cast<std::uint64_t>(value);
}
inline std::ofstream output(const std::filesystem::path& path) {
    // The run directory and fixture pending protocol already prevent reuse of
    // the owning wire outputs. Refuse clobbering diagnostic evidence as well.
    if(std::filesystem::exists(path))throw std::runtime_error("N3 evidence already exists");
    std::ofstream stream(path,std::ios::binary|std::ios::out);
    if(!stream)throw std::runtime_error("N3 evidence open failed");
    return stream;
}
inline void finish(std::ofstream& stream) {
    stream.flush();if(!stream)throw std::runtime_error("N3 evidence write failed");
    stream.close();if(!stream)throw std::runtime_error("N3 evidence close failed");
}
inline void dump_trace(const std::filesystem::path& path) {
    auto stream=output(path);n3::dump(stream);finish(stream);
}
template<class Result> inline void dump_run(const std::filesystem::path& directory,
                                            const Result& result) {
    // Called by main only after run returned: original Worker destructor has
    // joined the emitter, and all original pool handles have been destroyed.
    dump_trace(directory/"n3-events.json");
    auto out=output(directory/"n3-observation.json");
    out<<"{\"schema\":\"n3-network-raw-v1\",\"diagnostic\":true,"
          "\"timing_evidence\":false,\"joined\":true,\"command\":\"run\",\"pool\":";
    n3::detail::write_string(out,result.pool.c_str());
    out<<",\"elements\":"<<result.count<<",\"page_size\":"<<page_size()
       <<",\"cycles\":[";
    bool first=true;
    if(result.n3_warm){cycle_json(out,*result.n3_warm);first=false;}
    for(std::size_t index=0;index<result.samples.size();++index){
        if(!first)out<<',';first=false;
        cycle_json(out,observe_cycle(result.samples[index],"work."+std::to_string(index)));
    }
    out<<"],\"before_final\":";pair_json(out,result.server_after,result.client_after);
    out<<",\"after_final\":";pair_json(out,result.n3_server_final,result.n3_client_final);
    out<<",\"logical_ledger\":{\"inflight_bytes\":"<<result.n3_ledger_bytes
       <<",\"queue_frames\":"<<result.n3_ledger_frames
       <<",\"peak_inflight_bytes\":"<<result.n3_ledger_peak<<"}}\n";finish(out);
}
template<class Result> inline void dump_fixture(const std::filesystem::path& wire,
                                               std::size_t count,const Result& result) {
    dump_trace(wire.string()+".n3.json");auto out=output(wire.string()+".obs.json");
    out<<"{\"schema\":\"n3-network-raw-v1\",\"diagnostic\":true,"
          "\"timing_evidence\":false,\"joined\":true,\"command\":\"fixture\","
          "\"elements\":"<<count<<",\"frame_bytes\":"<<result.bytes
       <<",\"page_size\":"<<page_size()<<"}\n";finish(out);
}
} // namespace n3_net
#endif
