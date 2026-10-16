#!/usr/bin/env python3
"""Strict, consumer-only W1 prepared-v2 -> W2 N3 network generation.

No build or workload execution is performed here. The caller must resolve the
generated include to the frozen diagnostic header and bind all dependencies.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

SOURCE_SHA256 = "d4235d100ca1687e7834ad0383d20864b19347dc254e09b6ae2a2649b9617dcf"
HERE = Path(__file__).resolve().parent
_PREPARED_CENSUS = HERE.parent / "plan" / "anchor-census.json"
CENSUS = (_PREPARED_CENSUS if _PREPARED_CENSUS.is_file() else
          HERE.parent / "network-plan-v1" / "anchor-census.json")
CENSUS_SHA256 = "a3dbafac4dff100864f113e77872c958f79ecd7e98739f231724d1fa4f77556b"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def transform(source_text: str) -> tuple[str, list[dict]]:
    """Validate the 20 frozen anchors first, then apply exact replacements."""
    if sha256(source_text.encode()) != SOURCE_SHA256:
        raise ValueError("W1 prepared-v2 consumer SHA-256 mismatch")
    census_bytes = CENSUS.read_bytes()
    if sha256(census_bytes) != CENSUS_SHA256:
        raise ValueError("frozen 20-anchor census SHA-256 mismatch")
    census = json.loads(census_bytes)
    if census["source_sha256"] != SOURCE_SHA256 or len(census["anchors"]) != 20:
        raise ValueError("unexpected anchor census identity")
    anchors = []
    for anchor in census["anchors"]:
        observed = source_text.count(anchor["anchor"])
        if sha256(anchor["anchor"].encode()) != anchor["anchor_sha256"]:
            raise ValueError("anchor text/hash mismatch: " + anchor["id"])
        if observed != anchor["expected_count"] or observed != 1:
            raise ValueError("anchor cardinality mismatch: " + anchor["id"])
        anchors.append({key: anchor[key] for key in
                        ("id", "anchor", "anchor_sha256", "expected_count")})
        anchors[-1]["observed_count"] = observed
    result = source_text

    def replace(old: str, new: str) -> None:
        nonlocal result
        if result.count(old) != 1:
            raise ValueError("exact replacement cardinality mismatch: " + old[:100])
        result = result.replace(old, new, 1)

    # Angle include prevents a same-directory pristine header from silently
    # shadowing the explicit frozen diagnostic include path.
    replace('#include "xoffsetdatastructure.hpp"',
            '#include <xoffsetdatastructure.hpp>\n#include "network_support.hpp"')
    replace("struct IOProgress { std::size_t read=0,written=0,allocations=0,waits=0; bool published=false; std::size_t terminal_count=0; };",
            "struct IOProgress { std::size_t read=0,written=0,allocations=0,waits=0; bool published=false; std::size_t terminal_count=0; "
            "std::uint64_t n3_rx_size=0,n3_rx_capacity=0,n3_rx_init_extent=0,n3_header_copy_calls=0,n3_header_copy_bytes=0; };")
    replace("std::string wire(admission.wire_bytes,'\\0');++progress.allocations;std::memcpy(wire.data(),&header,H);",
            "std::string wire(admission.wire_bytes,'\\0');++progress.allocations;std::memcpy(wire.data(),&header,H);"
            "progress.n3_rx_size=wire.size();progress.n3_rx_capacity=wire.capacity();progress.n3_rx_init_extent=admission.wire_bytes;"
            "++progress.n3_header_copy_calls;progress.n3_header_copy_bytes+=H;")
    replace("FixtureResult fixture(const fs::path& output,std::size_t count,std::size_t* terminal_count=nullptr) {\n    TerminalEvent completion(terminal_count);",
            "FixtureResult fixture(const fs::path& output,std::size_t count,std::size_t* terminal_count=nullptr) {\n"
            "    std::optional<n3::Phase> n3_phase;std::optional<n3::Scope> n3_owner;"
            "n3_phase.emplace(\"fixture.build\");\n    TerminalEvent completion(terminal_count);")
    replace("auto owner=Buffer::create(4096,XBuffer::maximum_capacity(maximum).with_growth(XGrowPolicy::Automatic));auto& root=owner.root();",
            "auto owner=Buffer::create(4096,XBuffer::maximum_capacity(maximum).with_growth(XGrowPolicy::Automatic));"
            "n3_owner.emplace(owner.data());auto& root=owner.root();")
    replace("root.marker=input_marker;root.label=input_label;root.values.reserve(count);for(std::size_t i=0;i<count;++i)root.values.push_back(fixture_value(i));",
            "root.marker=input_marker;root.label=input_label;n3_phase.emplace(\"fixture.reserve\");"
            "root.values.reserve(count);for(std::size_t i=0;i<count;++i)root.values.push_back(fixture_value(i));")
    replace("verify_values(root,false,count);const auto checksum=semantic_checksum(root);const auto wire=owner.save_verified();",
            "verify_values(root,false,count);const auto checksum=semantic_checksum(root);n3_phase.emplace(\"fixture.save\");const auto wire=owner.save_verified();")
    replace("bool created=false;try{write_exclusive(pending,wire,&created);require(read_file(pending,maximum)==wire,\"fixture readback differs\");",
            "bool created=false;n3_phase.emplace(\"fixture.readback_verify\");try{write_exclusive(pending,wire,&created);require(read_file(pending,maximum)==wire,\"fixture readback differs\");")
    replace("fs::remove(pending);return {wire.size(),checksum};",
            "fs::remove(pending);n3_phase.emplace(\"fixture.teardown\");return {wire.size(),checksum};")
    replace("    std::size_t terminal_count=0;\n};\nstruct ServerResult {IOProgress io;std::uint64_t receive=0,load=0,process=0,save=0,release=0,send=0;};",
            "    std::size_t terminal_count=0;\n"
            "    std::uint64_t n3_server_saved_size=0,n3_server_saved_capacity=0,n3_client_output_capacity=0;\n"
            "    bool n3_server_strings_released=false;\n};\n"
            "struct ServerResult {IOProgress io;std::uint64_t receive=0,load=0,process=0,save=0,release=0,send=0;"
            "std::uint64_t n3_saved_size=0,n3_saved_capacity=0;bool n3_strings_released=false;};")
    replace("ServerResult serve_one(int fd,const vm_network::Limits& limits,std::size_t timeout,std::size_t count,const XBufferPool& pool,std::size_t chunk=chunk_bytes) {",
            "ServerResult serve_one(int fd,const vm_network::Limits& limits,std::size_t timeout,std::size_t count,const XBufferPool& pool,std::size_t chunk=chunk_bytes,const std::string& n3_cycle=\"unsupported\") {\n"
            "    std::optional<n3::Phase> n3_phase;std::optional<n3::Scope> n3_owner;"
            "n3_phase.emplace((\"server.\"+n3_cycle+\".receive\").c_str());")
    replace("        auto owner=Buffer::load_verified(wire,XBuffer::maximum_capacity(limits.owner_capacity).with_pool(pool));if constexpr(diagnostic)result.load=elapsed(point);",
            "        n3_phase.emplace((\"server.\"+n3_cycle+\".load\").c_str());"
            "auto owner=Buffer::load_verified(wire,XBuffer::maximum_capacity(limits.owner_capacity).with_pool(pool));"
            "n3_owner.emplace(owner.data());if constexpr(diagnostic)result.load=elapsed(point);")
    replace("result.io.published=true;if constexpr(diagnostic)point=Clock::now();process(owner.root(),count);if constexpr(diagnostic)result.process=elapsed(point);",
            "result.io.published=true;n3_phase.emplace((\"server.\"+n3_cycle+\".process\").c_str());"
            "if constexpr(diagnostic)point=Clock::now();process(owner.root(),count);if constexpr(diagnostic)result.process=elapsed(point);")
    replace("if constexpr(diagnostic)point=Clock::now();saved=owner.save_verified();if constexpr(diagnostic)result.save=elapsed(point);",
            "n3_phase.emplace((\"server.\"+n3_cycle+\".save\").c_str());if constexpr(diagnostic)point=Clock::now();"
            "saved=owner.save_verified();result.n3_saved_size=saved.size();result.n3_saved_capacity=saved.capacity();if constexpr(diagnostic)result.save=elapsed(point);")
    replace("require(saved.size()<=limits.max_wire_bytes,\"saved response exceeds max-wire\");if constexpr(diagnostic)point=Clock::now();",
            "require(saved.size()<=limits.max_wire_bytes,\"saved response exceeds max-wire\");"
            "n3_phase.emplace((\"server.\"+n3_cycle+\".release\").c_str());if constexpr(diagnostic)point=Clock::now();")
    replace("    // saved owns the immutable bytes; the source owner is already released.\n    if constexpr(diagnostic)point=Clock::now();send_exact(fd,saved,deadline,result.io,chunk);if constexpr(diagnostic)result.send=elapsed(point);return result;",
            "    // saved owns the immutable bytes; the source owner is already released.\n"
            "    n3_owner.reset();n3_phase.emplace((\"server.\"+n3_cycle+\".send\").c_str());"
            "if constexpr(diagnostic)point=Clock::now();send_exact(fd,saved,deadline,result.io,chunk);if constexpr(diagnostic)result.send=elapsed(point);"
            "n3_phase.emplace((\"server.\"+n3_cycle+\".strings_release\").c_str());return result;")
    replace("ServerResult result_;std::exception_ptr error_;std::thread thread_;",
            "ServerResult result_;std::exception_ptr error_;const bool n3_has_warmup_;std::thread thread_;")
    replace("void loop(){for(;;)", "void loop(){std::size_t n3_ordinal=0;for(;;)")
    replace("result_=serve_one(fd_,limits_,timeout_,count_,pool_,chunk_);",
            "const auto n3_cycle=n3_net::cycle_name(n3_has_warmup_,n3_ordinal++);"
            "result_=serve_one(fd_,limits_,timeout_,count_,pool_,chunk_,n3_cycle);result_.n3_strings_released=true;")
    replace("Worker(int fd,vm_network::Limits limits,std::size_t timeout,std::size_t count,XBufferPool pool,std::size_t chunk):fd_(fd),limits_(limits),timeout_(timeout),count_(count),chunk_(chunk),pool_(std::move(pool)),thread_([this]{loop();}){}",
            "Worker(int fd,vm_network::Limits limits,std::size_t timeout,std::size_t count,XBufferPool pool,std::size_t chunk,bool n3_has_warmup=false):fd_(fd),limits_(limits),timeout_(timeout),count_(count),chunk_(chunk),pool_(std::move(pool)),n3_has_warmup_(n3_has_warmup),thread_([this]{loop();}){}")
    replace("Sample exchange(Connection& connection,Worker& worker,const std::string& request,const vm_network::Limits& limits,std::size_t timeout,std::size_t count,const XBufferPool& client_pool,std::size_t chunk=chunk_bytes) {",
            "Sample exchange(Connection& connection,Worker& worker,const std::string& request,const vm_network::Limits& limits,std::size_t timeout,std::size_t count,const XBufferPool& client_pool,std::size_t chunk=chunk_bytes,const std::string& n3_cycle=\"unsupported\") {\n"
            "    std::optional<n3::Phase> n3_phase;std::optional<n3::Scope> n3_owner;"
            "n3_phase.emplace((\"client.\"+n3_cycle+\".send\").c_str());")
    replace("if constexpr(diagnostic)point=Clock::now();sample.output=receive_frame(connection.client.fd,limits,deadline,progress,chunk);sample.output_bytes=sample.output.size();if constexpr(diagnostic)sample.response_receive=elapsed(point);",
            "n3_phase.emplace((\"client.\"+n3_cycle+\".receive\").c_str());if constexpr(diagnostic)point=Clock::now();"
            "sample.output=receive_frame(connection.client.fd,limits,deadline,progress,chunk);sample.output_bytes=sample.output.size();"
            "sample.n3_client_output_capacity=sample.output.capacity();if constexpr(diagnostic)sample.response_receive=elapsed(point);")
    replace("if constexpr(diagnostic)point=Clock::now();verified.emplace(Buffer::load_verified(sample.output,XBuffer::maximum_capacity(limits.owner_capacity).with_pool(client_pool)));if constexpr(diagnostic)sample.client_load=elapsed(point);",
            "n3_phase.emplace((\"client.\"+n3_cycle+\".load\").c_str());if constexpr(diagnostic)point=Clock::now();"
            "verified.emplace(Buffer::load_verified(sample.output,XBuffer::maximum_capacity(limits.owner_capacity).with_pool(client_pool)));"
            "n3_owner.emplace(verified->data());if constexpr(diagnostic)sample.client_load=elapsed(point);")
    replace("if constexpr(diagnostic)point=Clock::now();verify_values(verified->root(),true,count);deadline.check();if constexpr(diagnostic)sample.client_validate=elapsed(point);sample.total=elapsed(start);",
            "n3_phase.emplace((\"client.\"+n3_cycle+\".validate\").c_str());if constexpr(diagnostic)point=Clock::now();"
            "verify_values(verified->root(),true,count);deadline.check();if constexpr(diagnostic)sample.client_validate=elapsed(point);sample.total=elapsed(start);")
    replace("point=Clock::now();verified.reset();sample.client_release=elapsed(point);point=Clock::now();const auto server=worker.finish();sample.server_completion_wait=elapsed(point);",
            "n3_phase.emplace((\"client.\"+n3_cycle+\".release\").c_str());point=Clock::now();verified.reset();sample.client_release=elapsed(point);"
            "n3_owner.reset();n3_phase.emplace((\"client.\"+n3_cycle+\".server_wait\").c_str());point=Clock::now();"
            "const auto server=worker.finish();sample.server_completion_wait=elapsed(point);"
            "sample.n3_server_saved_size=server.n3_saved_size;sample.n3_server_saved_capacity=server.n3_saved_capacity;"
            "sample.n3_server_strings_released=server.n3_strings_released;")
    replace("XBufferPool::Stats server_before{},server_after{},client_before{},client_after{};std::vector<Sample> samples;",
            "XBufferPool::Stats server_before{},server_after{},client_before{},client_after{};std::vector<Sample> samples;"
            "std::optional<n3_net::Cycle> n3_warm;XBufferPool::Stats n3_server_final{},n3_client_final{};"
            "std::uint64_t n3_ledger_bytes=0,n3_ledger_frames=0,n3_ledger_peak=0;")
    replace("RunResult run(const fs::path& input,const fs::path& directory,std::string mode,std::size_t iterations,vm_network::Limits limits,std::size_t timeout,std::size_t chunk=chunk_bytes) {",
            "RunResult run(const fs::path& input,const fs::path& directory,std::string mode,std::size_t iterations,vm_network::Limits limits,std::size_t timeout,std::size_t chunk=chunk_bytes) {\n"
            "    std::optional<n3::Phase> n3_phase;n3_phase.emplace(\"init.input\");")
    replace("{auto preflight=Buffer::load_verified(request,XBuffer::maximum_capacity(limits.owner_capacity));result.count=preflight.root().values.size();verify_values(preflight.root(),false,result.count);process(preflight.root(),result.count);result.checksum=semantic_checksum(preflight.root());}",
            "n3_phase.emplace(\"init.preflight\");{std::optional<n3::Scope> n3_owner;"
            "auto preflight=Buffer::load_verified(request,XBuffer::maximum_capacity(limits.owner_capacity));n3_owner.emplace(preflight.data());"
            "result.count=preflight.root().values.size();verify_values(preflight.root(),false,result.count);process(preflight.root(),result.count);result.checksum=semantic_checksum(preflight.root());}")
    replace('if(mode!="none"){const auto budget=limits.owner_capacity+chunk_bytes;server_pool=XBufferPool({1,budget,budget});client_pool=XBufferPool({1,budget,budget});}',
            'if(mode!="none"){const auto budget=limits.owner_capacity+chunk_bytes;n3_phase.emplace("init.pool.server");server_pool=XBufferPool({1,budget,budget});n3_phase.emplace("init.pool.client");client_pool=XBufferPool({1,budget,budget});}')
    replace("result.prepare=elapsed(point);point=Clock::now();auto connection=connect_loopback(timeout);",
            'result.prepare=elapsed(point);n3_phase.emplace("init.connection");point=Clock::now();auto connection=connect_loopback(timeout);')
    replace("Worker worker(connection.server.fd,limits,timeout,result.count,server_pool,chunk);result.connection=elapsed(point);",
            'Worker worker(connection.server.fd,limits,timeout,result.count,server_pool,chunk,mode=="warm");result.connection=elapsed(point);')
    replace('if(mode=="warm"){point=Clock::now();(void)exchange(connection,worker,request,limits,timeout,result.count,client_pool,chunk);result.warmup=elapsed(point);}',
            'if(mode=="warm"){point=Clock::now();{auto n3_warm_sample=exchange(connection,worker,request,limits,timeout,result.count,client_pool,chunk,"warm");'
            'result.n3_warm=n3_net::observe_cycle(n3_warm_sample,"warm");n3_phase.emplace("warm.output_archive");'
            'write_exclusive(directory/"n3-warm-output.wire",n3_warm_sample.output);n3_phase.emplace("warm.output_release");}'
            'result.warmup=elapsed(point);}')
    replace("result.server_before=server_pool.stats();result.client_before=client_pool.stats();result.samples.reserve(iterations);",
            'n3_phase.emplace("init.pool.stats");result.server_before=server_pool.stats();result.client_before=client_pool.stats();result.samples.reserve(iterations);')
    replace('for(std::size_t i=0;i<iterations;++i){if(mode=="cold"){point=Clock::now();server_pool.clear();client_pool.clear();result.cold_clear+=elapsed(point);}',
            'for(std::size_t i=0;i<iterations;++i){if(mode=="cold"){n3_phase.emplace(("cold.clear."+std::to_string(i)).c_str());point=Clock::now();server_pool.clear();client_pool.clear();result.cold_clear+=elapsed(point);}')
    replace("auto sample=exchange(connection,worker,request,limits,timeout,result.count,client_pool,chunk);",
            'auto sample=exchange(connection,worker,request,limits,timeout,result.count,client_pool,chunk,"work."+std::to_string(i));')
    replace('write_exclusive(directory/("output-"+std::to_string(i)+".wire"),sample.output);',
            'n3_phase.emplace(("work."+std::to_string(i)+".output_archive").c_str());write_exclusive(directory/("output-"+std::to_string(i)+".wire"),sample.output);')
    replace("std::string{}.swap(sample.output);result.samples.push_back(std::move(sample));}",
            'n3_phase.emplace(("work."+std::to_string(i)+".output_release").c_str());std::string{}.swap(sample.output);result.samples.push_back(std::move(sample));}')
    replace("result.server_after=server_pool.stats();result.client_after=client_pool.stats();",
            'n3_phase.emplace("before_final.stats");result.server_after=server_pool.stats();result.client_after=client_pool.stats();')
    replace("point=Clock::now();server_pool.clear();client_pool.clear();result.final_clear=elapsed(point);",
            'n3_phase.emplace("final.clear");point=Clock::now();server_pool.clear();client_pool.clear();result.final_clear=elapsed(point);'
            'n3_phase.emplace("diag.pool.check");result.n3_server_final=server_pool.stats();result.n3_client_final=client_pool.stats();')
    replace('std::string{}.swap(request);reservation.release();require(ledger.inflight_bytes()==0&&ledger.queue_frames()==0&&ledger.peak_inflight_bytes()==4*limits.max_wire_bytes,"logical run reservation not restored");return result;',
            'n3_phase.emplace("request.release");std::string{}.swap(request);reservation.release();require(ledger.inflight_bytes()==0&&ledger.queue_frames()==0&&ledger.peak_inflight_bytes()==4*limits.max_wire_bytes,"logical run reservation not restored");'
            'result.n3_ledger_bytes=ledger.inflight_bytes();result.n3_ledger_frames=ledger.queue_frames();result.n3_ledger_peak=ledger.peak_inflight_bytes();n3_phase.emplace("run.teardown");return result;')
    replace("const auto result=fixture(output,count);prefix(command);",
            "const auto result=fixture(output,count);n3_net::dump_fixture(output,count,result);prefix(command);")
    replace("run_json(run(input,output,pool,iterations,{wire,owner,inflight,1},timeout));",
            "const auto n3_result=run(input,output,pool,iterations,{wire,owner,inflight,1},timeout);n3_net::dump_run(output,n3_result);run_json(n3_result);")
    return result, anchors


def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",required=True,type=Path)
    parser.add_argument("--out",required=True,type=Path)
    parser.add_argument("--manifest",required=True,type=Path)
    args=parser.parse_args()
    generated,anchors=transform(args.source.read_text())
    if args.out.exists() or args.manifest.exists():
        raise ValueError("refuse existing generation output")
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.manifest.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(generated)
    args.manifest.write_text(json.dumps({
        "schema":"n3-network-generation-v1", "diagnostic":True,
        "timing_evidence":False, "source_sha256":SOURCE_SHA256,
        "generated_sha256":sha256(generated.encode()),
        "generator_sha256":sha256(Path(__file__).read_bytes()),
        "support_sha256":sha256((HERE/"network_support.hpp").read_bytes()),
        "census_sha256":CENSUS_SHA256,"anchors":anchors},indent=2)+"\n")


if __name__ == "__main__":
    main()
