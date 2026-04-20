#include "xoffsetdatastructure.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace XOffsetDatastructure;

#ifndef XOFFSET_BENCH_VERSION_LABEL
#define XOFFSET_BENCH_VERSION_LABEL "unknown"
#endif

namespace {

volatile std::uint64_t g_sink = 0;

struct BenchItem {
    std::int32_t id{0};
    std::int32_t qty{0};
    XString name;
};

struct VecIntRoot {
    XVector<std::int32_t> values;
};

struct VecItemRoot {
    XVector<BenchItem> values;
};

struct MapRoot {
    XMap<std::int32_t, XString> values;
};

struct StringRoot {
    XString scratch;
};

struct SnapshotRoot {
    XString title;
    std::int32_t level{0};
    XVector<BenchItem> items;
    XMap<std::int32_t, XString> names;
};

struct WorkloadResult {
    std::uint64_t checksum{0};
    std::size_t used_bytes{0};
    std::size_t wire_bytes{0};
};

struct BenchSpec {
    std::string_view name;
    std::size_t work_items;
    int warmups;
    int runs;
};

struct BenchRow {
    std::string_view name;
    std::size_t work_items;
    int runs;
    double median_ms;
    double mean_ms;
    double min_ms;
    double max_ms;
    double ns_per_item;
    std::uint64_t checksum;
    std::size_t used_bytes;
    std::size_t wire_bytes;
};

bool should_run(
    std::string_view name,
    const std::vector<std::string_view>& filters) {
    return filters.empty() ||
        std::find(filters.begin(), filters.end(), name) != filters.end();
}

std::string make_token(std::size_t index, std::size_t width) {
    std::string s(width, 'a');
    for (std::size_t i = 0; i < width; ++i) {
        s[i] = static_cast<char>('a' + ((index + i * 7) % 26));
    }
    return s;
}

std::vector<std::string> make_tokens(std::size_t count, std::size_t width) {
    std::vector<std::string> values;
    values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        values.push_back(make_token(i, width));
    }
    return values;
}

std::vector<std::string> make_churn_patterns() {
    std::vector<std::string> out;
    out.reserve(256);
    for (std::size_t i = 0; i < 256; ++i) {
        std::size_t width = 8 + (i % 5) * 24 + ((i * 13) % 11);
        out.push_back(make_token(i * 17, width));
    }
    return out;
}

std::uint64_t hash_bytes(std::string_view bytes) {
    std::uint64_t hash = 1469598103934665603ull;
    for (unsigned char ch : bytes) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }
    return hash;
}

void populate_snapshot(
    SnapshotRoot& root,
    const std::vector<std::string>& short_names,
    const std::vector<std::string>& long_names,
    std::size_t item_count,
    std::size_t map_count) {
    root.title = "snapshot-title";
    root.level = 77;

    for (std::size_t i = 0; i < item_count; ++i) {
        root.items.emplace_back();
        auto& item = root.items.back();
        item.id = static_cast<std::int32_t>(i);
        item.qty = static_cast<std::int32_t>((i % 9) + 1);
        item.name = short_names[i % short_names.size()].c_str();
    }

    for (std::size_t i = 0; i < map_count; ++i) {
        root.names.insert_or_assign(
            static_cast<std::int32_t>(i),
            long_names[i % long_names.size()].c_str());
    }
}

std::string build_snapshot_bytes(
    const std::vector<std::string>& short_names,
    const std::vector<std::string>& long_names,
    std::size_t item_count,
    std::size_t map_count,
    std::size_t reserve_bytes) {
    XBuffer xbuf(reserve_bytes, XBuffer::max_capacity(512 * 1024 * 1024));
    auto* root = xbuf.make<SnapshotRoot>();
    populate_snapshot(*root, short_names, long_names, item_count, map_count);
    return xbuf.save();
}

XBuffer build_fragmented_snapshot(
    const std::vector<std::string>& short_names,
    const std::vector<std::string>& long_names) {
    XBuffer xbuf(64 * 1024 * 1024, XBuffer::max_capacity(512 * 1024 * 1024));
    auto* root = xbuf.make<SnapshotRoot>();
    populate_snapshot(*root, short_names, long_names, 12000, 5000);

    for (std::size_t i = 0; i < root->items.size(); i += 3) {
        root->items[i].name = long_names[i % long_names.size()].c_str();
    }
    for (std::size_t i = 1; i < root->items.size(); i += 5) {
        root->items[i].name = short_names[i % short_names.size()].c_str();
    }
    for (std::size_t i = 0; i < 1500; ++i) {
        root->names.erase(static_cast<std::int32_t>(i * 2));
    }
    for (std::size_t i = 0; i < 1500; ++i) {
        root->names.insert_or_assign(
            50000 + static_cast<std::int32_t>(i),
            short_names[(i * 11) % short_names.size()].c_str());
    }
    for (std::size_t i = 0; i < 2000; ++i) {
        root->items.pop_back();
    }
    return xbuf;
}

template <typename Fn>
BenchRow run_benchmark(const BenchSpec& spec, Fn&& fn) {
    using clock = std::chrono::steady_clock;
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(spec.runs));

    WorkloadResult last{};
    for (int i = 0; i < spec.warmups; ++i) {
        last = fn();
        g_sink ^= last.checksum +
                  static_cast<std::uint64_t>(last.used_bytes) +
                  static_cast<std::uint64_t>(last.wire_bytes);
    }

    double total_ms = 0.0;
    double min_ms = std::numeric_limits<double>::max();
    double max_ms = 0.0;
    for (int i = 0; i < spec.runs; ++i) {
        auto start = clock::now();
        last = fn();
        auto end = clock::now();
        double elapsed_ms =
            std::chrono::duration<double, std::milli>(end - start).count();
        samples.push_back(elapsed_ms);
        total_ms += elapsed_ms;
        min_ms = std::min(min_ms, elapsed_ms);
        max_ms = std::max(max_ms, elapsed_ms);
        g_sink ^= last.checksum +
                  static_cast<std::uint64_t>(last.used_bytes) +
                  static_cast<std::uint64_t>(last.wire_bytes) +
                  static_cast<std::uint64_t>(i + 1);
    }

    std::sort(samples.begin(), samples.end());
    double median_ms = samples[samples.size() / 2];
    double mean_ms = total_ms / static_cast<double>(samples.size());
    double ns_per_item = (median_ms * 1'000'000.0) /
        static_cast<double>(std::max<std::size_t>(spec.work_items, 1));

    return BenchRow{
        .name = spec.name,
        .work_items = spec.work_items,
        .runs = spec.runs,
        .median_ms = median_ms,
        .mean_ms = mean_ms,
        .min_ms = min_ms,
        .max_ms = max_ms,
        .ns_per_item = ns_per_item,
        .checksum = last.checksum,
        .used_bytes = last.used_bytes,
        .wire_bytes = last.wire_bytes
    };
}

void print_header() {
    std::cout
        << "version\tbenchmark\twork_items\truns\tmedian_ms\tmean_ms\tmin_ms\tmax_ms\t"
        << "median_ns_per_item\tchecksum\tused_bytes\twire_bytes\n";
}

void print_row(const BenchRow& row) {
    std::cout << XOFFSET_BENCH_VERSION_LABEL << '\t'
              << row.name << '\t'
              << row.work_items << '\t'
              << row.runs << '\t'
              << std::fixed << std::setprecision(3)
              << row.median_ms << '\t'
              << row.mean_ms << '\t'
              << row.min_ms << '\t'
              << row.max_ms << '\t'
              << std::setprecision(1)
              << row.ns_per_item << '\t'
              << row.checksum << '\t'
              << row.used_bytes << '\t'
              << row.wire_bytes << '\n';
}

template <typename Fn>
void run_selected(
    const BenchSpec& spec,
    const std::vector<std::string_view>& filters,
    Fn&& fn) {
    if (!should_run(spec.name, filters)) {
        return;
    }

    std::cerr << "[bench] " << spec.name << '\n';
    try {
        print_row(run_benchmark(spec, std::forward<Fn>(fn)));
    } catch (const std::exception& ex) {
        std::cerr << "[bench] failed: " << spec.name << ": " << ex.what() << '\n';
        throw;
    } catch (...) {
        std::cerr << "[bench] failed: " << spec.name << '\n';
        throw;
    }
}

} // namespace

int main(int argc, char** argv) {
    constexpr std::size_t kIntCount = 100000;
    constexpr std::size_t kItemCount = 20000;
    constexpr std::size_t kMapCount = 12000;
    constexpr std::size_t kGrowthIntCount = 100000;
    constexpr std::size_t kChurnCount = 120000;
    constexpr std::size_t kSnapshotItemCount = 16000;
    constexpr std::size_t kSnapshotMapCount = 6000;
    constexpr std::size_t kLoadBatch = 40;

    const auto short_names = make_tokens(2048, 24);
    const auto long_names = make_tokens(2048, 96);
    const auto churn_patterns = make_churn_patterns();
    const std::string snapshot_bytes = build_snapshot_bytes(
        short_names, long_names, kSnapshotItemCount, kSnapshotMapCount, 96 * 1024 * 1024);
    std::vector<std::string_view> filters;
    filters.reserve(static_cast<std::size_t>(std::max(argc - 1, 0)));
    for (int i = 1; i < argc; ++i) {
        filters.emplace_back(argv[i]);
    }

    print_header();

    run_selected(
        {"vector_int_append_prealloc", kIntCount, 2, 9}, filters,
        [&]() -> WorkloadResult {
            XBuffer xbuf(8 * 1024 * 1024);
            auto* root = xbuf.make<VecIntRoot>();
            std::uint64_t checksum = 0;
            for (std::size_t i = 0; i < kIntCount; ++i) {
                root->values.push_back(static_cast<std::int32_t>(i));
                checksum += static_cast<std::uint32_t>(root->values.back());
            }
            return {checksum, xbuf.stats().used_size, 0};
        });

    run_selected(
        {"vector_item_append_prealloc", kItemCount, 2, 7}, filters,
        [&]() -> WorkloadResult {
            XBuffer xbuf(64 * 1024 * 1024);
            auto* root = xbuf.make<VecItemRoot>();
            std::uint64_t checksum = 0;
            for (std::size_t i = 0; i < kItemCount; ++i) {
                root->values.emplace_back();
                auto& item = root->values.back();
                item.id = static_cast<std::int32_t>(i);
                item.qty = static_cast<std::int32_t>((i % 7) + 1);
                item.name = short_names[i % short_names.size()].c_str();
                checksum += static_cast<std::uint64_t>(item.id) +
                            static_cast<std::uint64_t>(item.name.size());
            }
            return {checksum, xbuf.stats().used_size, 0};
        });

    run_selected(
        {"map_insert_find_prealloc", kMapCount * 2, 2, 7}, filters,
        [&]() -> WorkloadResult {
            XBuffer xbuf(64 * 1024 * 1024);
            auto* root = xbuf.make<MapRoot>();
            std::uint64_t checksum = 0;
            for (std::size_t i = 0; i < kMapCount; ++i) {
                root->values.insert_or_assign(
                    static_cast<std::int32_t>(i),
                    short_names[i % short_names.size()].c_str());
            }
            for (std::size_t i = 0; i < kMapCount; ++i) {
                auto it = root->values.find(static_cast<std::int32_t>(i));
                checksum += static_cast<std::uint64_t>(it->second.size()) +
                            static_cast<std::uint64_t>(it->first);
            }
            return {checksum, xbuf.stats().used_size, 0};
        });

    run_selected(
        {"string_assign_churn", kChurnCount, 2, 9}, filters,
        [&]() -> WorkloadResult {
            XBuffer xbuf(8 * 1024 * 1024);
            auto* root = xbuf.make<StringRoot>();
            std::uint64_t checksum = 0;
            for (std::size_t i = 0; i < kChurnCount; ++i) {
                const auto& pattern = churn_patterns[i % churn_patterns.size()];
                root->scratch = pattern.c_str();
                checksum += static_cast<std::uint64_t>(root->scratch.size());
            }
            return {checksum, xbuf.stats().used_size, 0};
        });

    if (!filters.empty()) {
        run_selected(
            {"vector_int_append_growth", kGrowthIntCount, 2, 7}, filters,
            [&]() -> WorkloadResult {
                XBuffer xbuf(64 * 1024, XBuffer::max_capacity(128 * 1024 * 1024));
                auto* root = xbuf.make<VecIntRoot>();
                std::uint64_t checksum = 0;
                for (std::size_t i = 0; i < kGrowthIntCount; ++i) {
                    root->values.push_back(static_cast<std::int32_t>(i));
                    checksum += static_cast<std::uint32_t>(root->values.back());
                }
                return {checksum, xbuf.stats().used_size, 0};
            });
    }

    run_selected(
        {"build_and_save_snapshot", kSnapshotItemCount + kSnapshotMapCount, 1, 5}, filters,
        [&]() -> WorkloadResult {
            XBuffer xbuf(96 * 1024 * 1024, XBuffer::max_capacity(512 * 1024 * 1024));
            auto* root = xbuf.make<SnapshotRoot>();
            populate_snapshot(*root, short_names, long_names, kSnapshotItemCount, kSnapshotMapCount);
            std::size_t used_bytes = xbuf.stats().used_size;
            std::string bytes = xbuf.save();
            return {hash_bytes(bytes), used_bytes, bytes.size()};
        });

    run_selected(
        {"load_snapshot_batch", kLoadBatch, 2, 9}, filters,
        [&]() -> WorkloadResult {
            std::uint64_t checksum = 0;
            std::size_t used_bytes = 0;
            for (std::size_t i = 0; i < kLoadBatch; ++i) {
                XBuffer xbuf = XBuffer::load(snapshot_bytes);
                checksum += static_cast<std::uint64_t>(xbuf.get_size());
                if (i == 0) {
                    used_bytes = xbuf.stats().used_size;
                }
            }
            return {checksum, used_bytes, snapshot_bytes.size()};
        });

    run_selected(
        {"fragment_and_compact_snapshot", 12000, 1, 5}, filters,
        [&]() -> WorkloadResult {
            XBuffer fragmented = build_fragmented_snapshot(short_names, long_names);
            XBuffer compacted = XCompactor::compact<SnapshotRoot>(fragmented);
            std::size_t used_bytes = compacted.stats().used_size;
            std::string bytes = compacted.save();
            return {hash_bytes(bytes), used_bytes, bytes.size()};
        });

    return static_cast<int>(g_sink == 0xdeadbeefULL);
}
