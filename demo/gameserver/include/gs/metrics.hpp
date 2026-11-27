// Game server demo: counters and a latency histogram.
#ifndef GS_METRICS_HPP
#define GS_METRICS_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace gs {

// Counters several threads add to.
struct Counter {
    std::atomic<std::uint64_t> v{0};
    void add(std::uint64_t n = 1) { v.fetch_add(n, std::memory_order_relaxed); }
    std::uint64_t get() const { return v.load(std::memory_order_relaxed); }
};

// Log-linear buckets: 8 per power of two, from 1 us to about 70 s. One
// thread records; percentiles are within 1/8 of a power of two (about 9%).
class Histogram {
public:
    void record(std::uint64_t us) {
        ++count_;
        sum_ += us;
        max_ = std::max(max_, us);
        min_ = count_ == 1 ? us : std::min(min_, us);
        ++buckets_[index(us)];
    }
    void merge(const Histogram& o) {
        for (std::size_t i = 0; i < kBuckets; ++i) buckets_[i] += o.buckets_[i];
        if (o.count_) {
            min_ = count_ ? std::min(min_, o.min_) : o.min_;
            max_ = std::max(max_, o.max_);
        }
        count_ += o.count_;
        sum_ += o.sum_;
    }
    std::uint64_t count() const { return count_; }
    std::uint64_t max() const { return max_; }
    std::uint64_t min() const { return min_; }
    double mean() const { return count_ ? double(sum_) / double(count_) : 0.0; }
    // The upper edge of the bucket that holds quantile q.
    std::uint64_t percentile(double q) const {
        if (!count_) return 0;
        const std::uint64_t rank = std::uint64_t(std::ceil(q * double(count_)));
        std::uint64_t seen = 0;
        for (std::size_t i = 0; i < kBuckets; ++i) {
            seen += buckets_[i];
            if (seen >= rank) return std::min(upper(i), max_);
        }
        return max_;
    }

private:
    static constexpr std::size_t kSub = 8;
    static constexpr std::size_t kBuckets = 27 * kSub;
    static std::size_t index(std::uint64_t us) {
        if (us < kSub) return std::size_t(us);
        const int msb = 63 - __builtin_clzll(us);
        const std::size_t sub = std::size_t((us >> (msb - 3)) & (kSub - 1));
        const std::size_t i = std::size_t(msb - 2) * kSub + sub;
        return std::min(i, kBuckets - 1);
    }
    static std::uint64_t upper(std::size_t i) {
        if (i < kSub) return i;
        const std::size_t msb = i / kSub + 2;
        const std::uint64_t sub = i % kSub;
        return ((kSub + sub + 1) << (msb - 3)) - 1;
    }
    std::array<std::uint64_t, kBuckets> buckets_{};
    std::uint64_t count_ = 0;
    std::uint64_t sum_ = 0;
    std::uint64_t max_ = 0;
    std::uint64_t min_ = 0;
};

}  // namespace gs

#endif
