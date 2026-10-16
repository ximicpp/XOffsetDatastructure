// Follow-up evidence uses the unchanged historical helper and full schema checks.
#define XOFFSET_BENCH_VM 1
#define main historical_benchmark_main
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
#include "bench_vm_compare.cpp"
#pragma clang diagnostic pop
#undef main
#include <map>

#ifndef XOFFSET_BENCH_FOLLOWUP
#define XOFFSET_BENCH_FOLLOWUP 0
#endif
struct FollowupRoot {
    std::uint64_t marker{};
    XVector<std::uint32_t> values;
    XVector<std::uint32_t> other;
    XVector<XVector<std::uint32_t>> rows;
};
XOFFSET_REGISTER_SCHEMA_NAME(FollowupRoot, "bench.vm-followup.Root.v1")

namespace followup {
struct Config { std::string workload; std::size_t size{1024}, cycles{8}; bool enabled{}; };
struct Sample {
    Config config;
    std::map<std::string, double> metrics;
    std::map<std::string, Memory> memory;
    std::vector<double> operation_ms, regrow_ms;
    std::uint64_t checksum{root_marker};
    std::string timing_scope;
};
void mix(Sample& s, std::uint64_t v) { s.checksum = (s.checksum ^ v) * 1099511628211ULL; }
void capture(Sample& s, XBuffer& b) {
    s.metrics["used_end"] = b.arena()->used_end;
    s.metrics["free_bytes"] = b.get_free_memory();
    s.metrics["logical_bytes"] = b.get_size();
    s.metrics["accessible_bytes"] = b.accessible_bytes();
    s.metrics["reserved_bytes"] = b.reserved_bytes();
}
template<class F> double timed(F&& f) { auto t = Clock::now(); f(); return milliseconds(t, Clock::now()); }

Sample fragmentation(Config c) {
    Sample s; s.config = c; s.timing_scope = "allocation_fill_and_free_phases; full_value_checks_excluded";
    const auto n = c.size;
    const auto logical = n * 8 * KiB + MiB;
    XBuffer b(logical, XBuffer::maximum_capacity(logical));
    s.memory["before"] = memory_now();
    if (c.workload == "cache_split") {
        auto* root = b.make<FollowupRoot>(); root->marker = root_marker;
        root->rows.reserve(n * 4 + logical / 1024 + 2);
        s.metrics["initial_ms"] = timed([&] {
            for (std::size_t i = 0; i < n; ++i) {
                root->rows.emplace_back(); root->rows.back().reserve(1024);
                root->rows.back().push_back(value_at(i));
            }
        });
        s.metrics["free_ms"] = timed([&] { root->rows.clear(); });
        s.metrics["replacement_ms"] = timed([&] {
            for (std::size_t i = 0; i < n * 4; ++i) {
                root->rows.emplace_back(); root->rows.back().reserve(192);
                for (std::size_t j = 0; j < 192; ++j) root->rows.back().push_back(value_at(i * 192 + j));
            }
        });
        capture(s, b); s.memory["live"] = memory_now();
        s.metrics["cached_free_bytes"] = root->rows.allocator_state()->free_bytes;
        auto verify = [&](const FollowupRoot& r, bool checksum) {
            require(r.marker == root_marker && r.rows.size() == root->rows.size(), "cache root changed");
            for (std::size_t i = 0; i < n * 4; ++i) {
                require(r.rows[i].size() == 192, "cache row size changed");
                for (std::size_t j = 0; j < 192; ++j) {
                    require(r.rows[i][j] == value_at(i * 192 + j), "cache value changed");
                    if (checksum) mix(s, r.rows[i][j]);
                }
            }
        };
        verify(*root, true);
        s.metrics["live_payload_bytes"] = n * 4 * (192 * 4 + sizeof(XVector<std::uint32_t>));
        // A second request wave tests whether residual blocks are useful under
        // the same fixed logical limit. Its variable completed work is reported
        // separately and is deliberately excluded from elapsed_ms.
        std::size_t completed = 0;
        s.metrics["pressure_ms"] = timed([&] {
            for (;;) {
                require(root->rows.size() < root->rows.capacity(), "pressure exhausted metadata reserve");
                root->rows.emplace_back();
                try { root->rows.back().reserve(256); }
                catch (const XBadAlloc&) { root->rows.pop_back(); break; }
                for (std::size_t j = 0; j < 256; ++j) root->rows.back().push_back(value_at(completed * 256 + j));
                ++completed;
            }
        });
        for (std::size_t i = 0; i < completed; ++i) for (std::size_t j = 0; j < 256; ++j)
            require(root->rows[n * 4 + i][j] == value_at(i * 256 + j), "cache pressure value changed");
        s.metrics["pressure_successes"] = completed; s.metrics["pressure_failures"] = 1;
        s.metrics["pressure_verified_bytes"] = completed * 1024;
        s.metrics["pressure_used_end"] = b.arena()->used_end;
        s.memory["pressure"] = memory_now();
        auto wire = b.save_verified<FollowupRoot>();
        auto loaded = XBuffer::load_verified<FollowupRoot>(wire);
        verify(loaded.root<FollowupRoot>(), false);
        for (std::size_t i = 0; i < completed; ++i) for (std::size_t j = 0; j < 256; ++j)
            require(loaded.root<FollowupRoot>().rows[n * 4 + i][j] == value_at(i * 256 + j), "pressure wire changed");
        s.metrics["wire_bytes"] = wire.size();
    } else {
        b.make<VmBenchScalarRoot>()->value = root_marker;
        auto a = b.allocator<std::uint8_t>();
        std::vector<std::uint8_t*> first(n), second(n), guards(n), replacement;
        const bool coalesce = c.workload == "core_coalesce";
        s.metrics["initial_ms"] = timed([&] {
            for (std::size_t i = 0; i < n; ++i) {
                first[i] = a.allocate(coalesce ? 1024 : 4096);
                if (coalesce) second[i] = a.allocate(1024);
                guards[i] = a.allocate(64); std::memset(guards[i], int(i % 251), 64);
            }
        });
        s.metrics["initial_used_end"] = b.arena()->used_end;
        s.metrics["free_ms"] = timed([&] {
            for (std::size_t i = 0; i < n; ++i) {
                a.deallocate(first[i], coalesce ? 1024 : 4096);
                if (coalesce) a.deallocate(second[i], 1024);
            }
        });
        const auto count = n * (coalesce ? 1 : 4);
        const int bytes = coalesce ? 1536 : 768;
        replacement.resize(count);
        s.metrics["replacement_ms"] = timed([&] {
            for (std::size_t i = 0; i < count; ++i) {
                replacement[i] = a.allocate(bytes); std::memset(replacement[i], int(i % 251), bytes);
            }
        });
        capture(s, b); s.memory["live"] = memory_now();
        require(b.root<VmBenchScalarRoot>().value == root_marker, "raw root changed");
        for (std::size_t i = 0; i < n; ++i) for (int j = 0; j < 64; ++j) {
            require(guards[i][j] == i % 251, "guard changed"); mix(s, guards[i][j]);
        }
        for (std::size_t i = 0; i < count; ++i) for (int j = 0; j < bytes; ++j) {
            require(replacement[i][j] == i % 251, "replacement changed"); mix(s, replacement[i][j]);
        }
        s.metrics["live_payload_bytes"] = n * 64 + count * bytes;
        std::vector<std::uint8_t*> pressure; pressure.reserve(logical / 1024 + 1);
        s.metrics["pressure_ms"] = timed([&] {
            for (;;) {
                // A successful unique 1 KiB allocation cannot exceed this fixed
                // logical extent. Stop a broken no-progress allocator before
                // unbounded host logging; pressure is outside elapsed_ms.
                require(pressure.size() < logical / 1024 + 1, "pressure made no capacity progress");
                const auto available = b.get_free_memory();
                std::uint8_t* p{};
                try { p = a.allocate(1024); } catch (const XBadAlloc&) { break; }
                require(b.get_free_memory() < available, "pressure made no free-space progress");
                std::memset(p, int(pressure.size() % 251), 1024); pressure.push_back(p);
            }
        });
        for (std::size_t i = 0; i < pressure.size(); ++i) for (std::size_t j = 0; j < 1024; ++j)
            require(pressure[i][j] == i % 251, "raw pressure value changed");
        s.metrics["pressure_successes"] = pressure.size(); s.metrics["pressure_failures"] = 1;
        s.metrics["pressure_verified_bytes"] = pressure.size() * 1024;
        s.metrics["pressure_used_end"] = b.arena()->used_end;
        s.memory["pressure"] = memory_now();
    }
    s.metrics["elapsed_ms"] = s.metrics["initial_ms"] + s.metrics["free_ms"] + s.metrics["replacement_ms"];
    return s;
}

Sample churn(Config c) {
    Sample s; s.config = c;
    s.timing_scope = "create_or_verified_load_and_destroy_phases; value_checks_and_memory_queries_excluded";
    const bool loading = c.workload == "pool_load";
    const std::size_t cap = loading ? 128 * KiB : 64 * KiB;
    auto config = XBuffer::maximum_capacity(cap);
#if XOFFSET_BENCH_FOLLOWUP
    XBufferPool pool;
    if (c.enabled) { pool = XBufferPool(XBufferPool::Limits{c.size, c.size * cap, c.size * cap}); config = config.with_pool(pool); }
    auto stats = [&](const char* phase) {
        auto p = pool.stats(); std::string prefix = std::string("pool_") + phase + "_";
        s.metrics[prefix + "blocks"] = p.cached_blocks; s.metrics[prefix + "reserved_bytes"] = p.cached_reserved_bytes;
        s.metrics[prefix + "accessible_bytes"] = p.cached_accessible_bytes;
        s.metrics[prefix + "hits"] = p.hits; s.metrics[prefix + "misses"] = p.misses;
    };
#endif
    std::string fixture;
    if (loading) {
        XBuffer b(cap, XBuffer::maximum_capacity(cap));
        auto* root = b.make<VmBenchRoot>(); root->marker = root_marker;
        root->values.reserve(64 * KiB / 4);
        for (std::size_t i = 0; i < 64 * KiB / 4; ++i) root->values.push_back(value_at(i));
        (void)verify_values(b, 64 * KiB / 4); fixture = b.save_verified<VmBenchRoot>();
    }
    std::vector<std::optional<XBuffer>> live(c.size);
    s.memory["before"] = memory_now();
    for (std::size_t cycle = 0; cycle < c.cycles; ++cycle) {
        auto create = timed([&] {
            for (std::size_t i = 0; i < c.size; ++i) {
                if (loading) live[i].emplace(XBuffer::load_verified<VmBenchRoot>(fixture, config));
                else { live[i].emplace(64 * KiB, config); live[i]->make<VmBenchScalarRoot>()->value = root_marker ^ i; }
            }
        });
        capture(s, *live.front());
        s.memory[cycle ? "warm_live" : "cold_live"] = memory_now();
        s.metrics["live_reserved_bytes"] = live.front()->reserved_bytes() * c.size;
        std::uint64_t checksum = root_marker;
        for (std::size_t i = 0; i < c.size; ++i) {
            auto value = loading ? verify_values(*live[i], 64 * KiB / 4) : live[i]->root<VmBenchScalarRoot>().value;
            if (!loading) require(value == (root_marker ^ i), "churn root mismatch");
            checksum = (checksum ^ value) * 1099511628211ULL;
        }
        if (cycle) require(checksum == s.checksum, "churn checksum changed"); else s.checksum = checksum;
        auto destroy = timed([&] { for (auto& b : live) b.reset(); });
        s.metrics["create_ms"] += create; s.metrics["destroy_ms"] += destroy;
        s.operation_ms.push_back(create + destroy);
        s.metrics[cycle ? "warm_ms" : "cold_ms"] += create + destroy;
    }
    s.metrics["elapsed_ms"] = s.metrics["create_ms"] + s.metrics["destroy_ms"];
    s.metrics["warm_mean_ms"] = s.metrics["warm_ms"] / (c.cycles - 1);
    s.memory["idle"] = memory_now();
#if XOFFSET_BENCH_FOLLOWUP
    stats("idle");
    if (c.enabled) {
        auto state = pool.stats();
        require(state.cached_blocks == c.size && state.hits == (c.cycles - 1) * c.size && state.misses == c.size,
                "unexpected bounded pool reuse counters");
        require(state.cached_reserved_bytes <= c.size * cap && state.cached_accessible_bytes <= c.size * cap,
                "pool exceeded cache budget");
    }
    s.metrics["pool_clear_ms"] = timed([&] { pool.clear(); });
    stats("cleared");
    require(pool.stats().cached_blocks == 0 && pool.stats().cached_reserved_bytes == 0 && pool.stats().cached_accessible_bytes == 0,
            "pool clear retained idle ownership");
#endif
    s.memory["cleared"] = memory_now();
    return s;
}

Sample reclaim(Config c) {
    Sample s; s.config = c;
    s.timing_scope = "logical_shrink_optional_explicit_reclaim_then_regrow; zero_and_live_value_checks_excluded";
    const auto total = c.size;
    s.metrics["attempted_bytes"] = 0; s.metrics["discarded_bytes"] = 0; s.metrics["inaccessible_bytes"] = 0;
    XBuffer b(total, XBuffer::maximum_capacity(total));
    b.make<VmBenchScalarRoot>()->value = root_marker;
    auto a = b.allocator<std::uint8_t>(); auto* live = a.allocate(4096); std::memset(live, 0x5a, 4096);
    const auto base = b.get_address(); s.memory["before"] = memory_now();
    for (std::size_t cycle = 0; cycle < c.cycles; ++cycle) {
        s.metrics["shrink_ms"] += timed([&] { b.shrink_to_fit(); });
        const auto logical = b.get_size();
        s.metrics["shrink_logical_bytes"] = logical;
        s.metrics["shrink_accessible_bytes"] = b.accessible_bytes();
        s.memory["shrunk"] = memory_now();
        double reclaim_time{};
#if XOFFSET_BENCH_FOLLOWUP
        if (c.enabled) reclaim_time = timed([&] {
            auto result = b.reclaim_tail_pages();
            require(result.complete(), "explicit reclaim failed");
            s.metrics["attempted_bytes"] += result.attempted_bytes;
            s.metrics["discarded_bytes"] += result.discarded_bytes;
            s.metrics["inaccessible_bytes"] += result.inaccessible_bytes;
        });
#endif
        s.metrics["reclaim_ms"] += reclaim_time;
        s.metrics["reclaimed_accessible_bytes"] = b.accessible_bytes();
        s.memory["reclaimed"] = memory_now();
        require(b.get_address() == base && b.root<VmBenchScalarRoot>().value == root_marker, "reclaim changed root/address");
        for (std::size_t i = 0; i < 4096; ++i) require(live[i] == 0x5a, "reclaim changed live bytes");
        auto growth = timed([&] { require(b.grow(total - logical), "regrow failed"); });
        s.metrics["regrow_ms"] += growth; s.regrow_ms.push_back(growth);
        require(b.get_address() == base, "regrow moved base");
        s.memory["regrown"] = memory_now();
        for (std::size_t i = logical; i < total; ++i) require(b.data()[i] == 0, "regrow did not zero bytes");
        // Dirty the unused logical tail before the next iteration, outside timing.
        std::memset(b.data() + logical, 0x6b, total - logical);
    }
    for (std::size_t i = 0; i < 4096; ++i) mix(s, live[i]);
    capture(s, b); s.metrics["elapsed_ms"] = s.metrics["shrink_ms"] + s.metrics["reclaim_ms"] + s.metrics["regrow_ms"];
    return s;
}

Sample automatic(Config c) {
    Sample s; s.config = c;
    s.timing_scope = "append_loop_including_capacity_expansion_and_explicit_failure_retry; full_value_and_wire_checks_excluded";
    auto config = XBuffer::maximum_capacity(32 * MiB);
#if XOFFSET_BENCH_FOLLOWUP
    if (c.enabled) config = config.with_growth(XGrowPolicy::Automatic);
#endif
    XBuffer b(4096, config); auto* root = b.make<FollowupRoot>(); root->marker = root_marker;
    const auto address = b.get_address(); s.memory["before"] = memory_now();
    const bool interleaved = c.workload == "automatic_interleaved";
    s.metrics["allocation_retries"] = 0; s.metrics["logical_growth_events"] = 0;
    std::size_t previous = b.get_size();
    s.metrics["elapsed_ms"] = timed([&] {
        for (std::size_t i = 0; i < c.size; ++i) {
            auto append = [&](auto& vector, std::uint32_t value) {
                for (;;) {
                    try { vector.push_back(value); break; }
                    catch (const XBadAlloc&) {
                        ++s.metrics["allocation_retries"];
                        require(!c.enabled, "automatic mode required a retry");
                        require(b.grow(std::min(b.get_size(), b.maximum_capacity() - b.get_size())), "explicit growth exhausted cap");
                    }
                }
            };
            append(root->values, value_at(i));
            if (interleaved && i % 256 == 0) append(root->other, value_at(i / 256));
            if (b.get_size() != previous) { ++s.metrics["logical_growth_events"]; previous = b.get_size(); }
        }
    });
    capture(s, b); s.memory["live"] = memory_now();
    require(b.get_address() == address, "automatic base moved");
    auto verify = [&](const FollowupRoot& r, bool checksum) {
        require(r.marker == root_marker && r.values.size() == c.size, "automatic root mismatch");
        require(r.other.size() == (interleaved ? (c.size + 255) / 256 : 0), "automatic interleaved size mismatch");
        for (std::size_t i = 0; i < c.size; ++i) { require(r.values[i] == value_at(i), "automatic vector changed"); if (checksum) mix(s, r.values[i]); }
        for (std::size_t i = 0; i < r.other.size(); ++i) { require(r.other[i] == value_at(i), "automatic other changed"); if (checksum) mix(s, r.other[i]); }
    };
    verify(*root, true); auto wire = b.save_verified<FollowupRoot>();
    auto loaded = XBuffer::load_verified<FollowupRoot>(wire); verify(loaded.root<FollowupRoot>(), false);
    s.metrics["wire_bytes"] = wire.size();
    return s;
}

void print(const Sample& s) {
    std::cout << std::setprecision(17) << "{\"workload\":" << json_string(s.config.workload)
        << ",\"size\":" << s.config.size << ",\"cycles\":" << s.config.cycles
        << ",\"enabled\":" << (s.config.enabled ? "true" : "false")
        << ",\"semantic_checksum\":" << s.checksum << ",\"timing_scope\":" << json_string(s.timing_scope)
        << ",\"metrics\":{";
    bool comma = false;
    for (const auto& [key, value] : s.metrics) { if (comma) std::cout << ','; comma = true; std::cout << json_string(key) << ':' << value; }
    std::cout << "},\"memory\":{"; comma = false;
    for (const auto& [key, value] : s.memory) { if (comma) std::cout << ','; comma = true; std::cout << json_string(key) << ':'; print_memory(std::cout, value); }
    std::cout << "},\"operation_ms\":"; print_samples(std::cout, s.operation_ms);
    std::cout << ",\"regrow_ms\":"; print_samples(std::cout, s.regrow_ms); std::cout << "}\n";
}
} // namespace followup

int main(int argc, char** argv) {
    try {
        followup::Config c;
        for (int i = 1; i < argc; ++i) {
            require(i + 1 < argc, "flag needs value"); const std::string key = argv[i], value = argv[++i];
            if (key == "--workload") c.workload = value;
            else if (key == "--size") c.size = parse_size(value);
            else if (key == "--cycles") c.cycles = parse_size(value);
            else if (key == "--enabled") c.enabled = parse_size(value) != 0;
            else throw std::runtime_error("unknown option");
        }
        require(c.size > 0 && c.cycles > 1, "invalid size/cycles");
        require(XOFFSET_BENCH_FOLLOWUP || !c.enabled, "baseline does not support opt-in features");
        followup::Sample s;
        if (c.workload == "core_split" || c.workload == "core_coalesce" || c.workload == "cache_split") s = followup::fragmentation(c);
        else if (c.workload == "pool_create" || c.workload == "pool_load") s = followup::churn(c);
        else if (c.workload == "reclaim") s = followup::reclaim(c);
        else if (c.workload == "automatic" || c.workload == "automatic_interleaved") s = followup::automatic(c);
        else throw std::runtime_error("unknown workload");
        followup::print(s);
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
