#pragma once
#include "views.hpp"
#include <cista/serialization.h>
#include <unordered_set>

namespace sc::ci_detail {
struct Attr { cista::offset::string key; cista::offset::vector<std::uint8_t> value; };
struct Record {
    std::uint64_t id{};
    std::uint32_t revision{};
    std::int64_t score{};
    cista::offset::string name;
    cista::offset::vector<std::uint32_t> samples;
    cista::offset::vector<std::uint8_t> payload;
    cista::offset::vector<Attr> attrs;
};
struct Snapshot {
    std::uint32_t schema_version{1};
    std::uint64_t epoch{};
    cista::offset::vector<Record> records;
};

template<class V> void release_vector(V& v) noexcept {
    v.deallocate();
    // deallocate() intentionally leaves non-self-allocated blob views alone.
    // Their children have already been visited; forget the borrowed view now.
    v.reset();
}
inline void release_attr(Attr& a) noexcept {
    a.key.reset();
    release_vector(a.value);
}
inline void release_record(Record& r) noexcept {
    r.name.reset();
    release_vector(r.samples);
    release_vector(r.payload);
    for (auto& a : r.attrs) release_attr(a);
    release_vector(r.attrs);
}
inline void release_snapshot(Snapshot& s) noexcept {
    for (auto& r : s.records) release_record(r);
    release_vector(s.records);
}
template<class V, class C> void assign_sequence(V& target, const C& source) {
    target.clear();
    target.reserve(static_cast<typename V::size_type>(source.size()));
    for (auto value : source) target.push_back(value);
}
inline void assign_record(Record& target, const sc::Record& source) {
    target.id = source.id;
    target.revision = source.revision;
    target.score = source.score;
    target.name = std::string_view(source.name);
    assign_sequence(target.samples, source.samples);
    assign_sequence(target.payload, source.payload);
    target.attrs.reserve(static_cast<decltype(target.attrs)::size_type>(source.attrs.size()));
    for (const auto& a : source.attrs) {
        auto& native = target.attrs.emplace_back();
        native.key = std::string_view(a.key);
        assign_sequence(native.value, a.value);
    }
}
inline std::size_t attr_lower(const Record& r, std::string_view key) {
    std::size_t l = 0, h = r.attrs.size();
    while (l < h) {
        auto m = l + (h - l) / 2;
        const auto& x = r.attrs[m].key;
        if (wire::byte_less({x.data(), x.size()}, key)) l = m + 1; else h = m;
    }
    return l;
}

struct NativeState {
    std::unique_ptr<Snapshot> constructed;
    Bytes private_blob;
    Snapshot* value{};
    ~NativeState() {
        // A non-owning vector may contain a string/vector that acquired a heap
        // allocation after load. Its ordinary vector destructor skips those
        // elements. Explicit bottom-up cleanup handles both blob and heap cases
        // without freeing bytes owned by private_blob or freeing children twice.
        if (value) release_snapshot(*value);
    }
};
} // namespace sc::ci_detail

namespace sc {
struct CistaAdapter {
    using State = std::unique_ptr<ci_detail::NativeState>;
    static constexpr std::string_view library_id = "cista-v0.16";
    static constexpr std::string_view commit = "015b94e66c4c0d7c1ef29742383a024c84fbbba1";
    static constexpr std::string_view trust_mode = "T-trusted-offset-mode-NONE-structural-checks-private-blob;U-not-established";

    static State build(const Snapshot& input) {
        auto state = std::make_unique<ci_detail::NativeState>();
        state->constructed = std::make_unique<ci_detail::Snapshot>();
        state->value = state->constructed.get();
        auto& s = *state->value;
        s.schema_version = input.schema_version;
        s.epoch = input.epoch;
        s.records.reserve(static_cast<decltype(s.records)::size_type>(input.records.size()));
        for (const auto& source : input.records) ci_detail::assign_record(s.records.emplace_back(), source);
        return state;
    }
    static State load(std::span<const std::uint8_t> bytes) {
        if (bytes.size() < sizeof(ci_detail::Snapshot) || bytes.size() > wire_limit)
            throw std::runtime_error("cista wire framing/limit");
        auto state = std::make_unique<ci_detail::NativeState>();
        state->private_blob.assign(bytes.begin(), bytes.end());
        state->value = cista::offset::deserialize<ci_detail::Snapshot, cista::mode::NONE>(state->private_blob);
        if (state->value->schema_version != 1) throw std::runtime_error("cista schema version");
        return state;
    }
    static auto emit(const State& state) { return cista::serialize<cista::mode::NONE>(*state->value); }
    static Snapshot export_state(const State& state) { return export_view(PlainSnapshotView{*state->value}); }
    static void query(const State& state, std::span<const Query> queries, std::span<QueryResult> results) {
        query_view(PlainSnapshotView{*state->value}, queries, results);
    }

    static void mutate(State& state, std::span<const Mutation> operations) {
        auto& s = *state->value;
        std::size_t offset = 0;
        while (offset < operations.size()) {
            auto stop = offset + 1;
            while (stop < operations.size() && operations[stop].stage == operations[offset].stage) ++stop;
            const auto group = operations.subspan(offset, stop - offset);
            offset = stop;
            if (group.front().code == MutationCode::erase_record) {
                std::unordered_set<std::uint64_t> removed;
                removed.reserve(group.size());
                for (const auto& op : group) {
                    const auto at = find_record(PlainSnapshotView{s}, op.id);
                    const bool found = at < s.records.size() && !removed.contains(op.id);
                    if ((found ? Status::ok : Status::not_found) != op.expected)
                        throw std::runtime_error("cista erase status mismatch");
                    if (found) removed.insert(op.id);
                }
                std::size_t write = 0;
                for (std::size_t read = 0; read < s.records.size(); ++read) {
                    if (removed.contains(s.records[read].id)) {
                        ci_detail::release_record(s.records[read]);
                        continue;
                    }
                    if (write != read) {
                        ci_detail::release_record(s.records[write]);
                        s.records[write] = std::move(s.records[read]);
                    }
                    ++write;
                }
                // v0.16 resize(smaller) only changes used_size and skips
                // destructors. The public tail-erase destroys moved-from/tidy
                // elements; every move above uses container/member contracts.
                if (write != s.records.size()) s.records.erase(s.records.begin() + write, s.records.end());
                continue;
            }
            for (const auto& op : group) {
                Status actual = Status::ok;
                if (op.code == MutationCode::set_epoch) s.epoch = op.epoch;
                else {
                    const auto at = find_record(PlainSnapshotView{s}, op.id);
                    const bool found = at < s.records.size();
                    if (op.code == MutationCode::append_record) {
                        if (found) throw std::runtime_error("cista append duplicate id");
                        std::size_t pos = s.records.size();
                        if (pos && s.records.back().id >= op.record.id) {
                            std::size_t l = 0, h = pos;
                            while (l < h) {
                                auto m = l + (h - l) / 2;
                                if (s.records[m].id < op.record.id) l = m + 1; else h = m;
                            }
                            pos = l;
                        }
                        const auto old = s.records.size();
                        s.records.emplace_back();
                        for (std::size_t i = old; i > pos; --i) {
                            ci_detail::release_record(s.records[i]);
                            s.records[i] = std::move(s.records[i - 1]);
                        }
                        ci_detail::release_record(s.records[pos]);
                        ci_detail::assign_record(s.records[pos], op.record);
                    } else if (!found) actual = Status::not_found;
                    else {
                        auto& r = s.records[at];
                        switch (op.code) {
                        case MutationCode::update_score: r.score = op.score; r.revision = op.revision; break;
                        case MutationCode::replace_name: r.name = std::string_view(op.text); break;
                        case MutationCode::append_samples:
                            for (auto value : op.samples) r.samples.push_back(value);
                            break;
                        case MutationCode::replace_payload: ci_detail::assign_sequence(r.payload, op.bytes); break;
                        case MutationCode::replace_attr:
                        case MutationCode::insert_attr: {
                            auto pos = ci_detail::attr_lower(r, op.text);
                            const bool exists = pos < r.attrs.size() && r.attrs[pos].key == std::string_view(op.text);
                            if (op.code == MutationCode::replace_attr) {
                                if (!exists) actual = Status::not_found;
                                else ci_detail::assign_sequence(r.attrs[pos].value, op.bytes);
                            } else {
                                if (exists) throw std::runtime_error("cista insert duplicate key");
                                ci_detail::Attr value;
                                value.key = std::string_view(op.text);
                                ci_detail::assign_sequence(value.value, op.bytes);
                                if (r.attrs.empty()) r.attrs.push_back(std::move(value));
                                else r.attrs.insert(r.attrs.begin() + pos, std::move(value));
                            }
                            break;
                        }
                        default: throw std::runtime_error("cista unknown mutation opcode");
                        }
                    }
                }
                if (actual != op.expected) throw std::runtime_error("cista mutation status mismatch");
            }
        }
    }
};
} // namespace sc
