#pragma once
#include "views.hpp"
#include "xoffsetdatastructure.hpp"
#include <unordered_set>

namespace sc::xo_detail {
using namespace XOffsetDatastructure;
struct Attr { XString key; XVector<std::uint8_t> value; };
struct Record {
    std::uint64_t id{};
    std::uint32_t revision{};
    std::int64_t score{};
    XString name;
    XVector<std::uint32_t> samples;
    XVector<std::uint8_t> payload;
    XVector<Attr> attrs;
};
struct Snapshot {
    std::uint32_t schema_version{1};
    std::uint64_t epoch{};
    XVector<Record> records;
};

template<class V, class C> void assign_sequence(V& target, const C& source) {
    target.resize(source.size());
    std::copy(source.begin(), source.end(), target.begin());
}
inline void assign_attr(Attr& target, const sc::Attr& source) {
    target.key = std::string_view(source.key);
    assign_sequence(target.value, source.value);
}
inline void assign_record(Record& target, const sc::Record& source) {
    target.id = source.id;
    target.revision = source.revision;
    target.score = source.score;
    target.name = std::string_view(source.name);
    assign_sequence(target.samples, source.samples);
    assign_sequence(target.payload, source.payload);
    target.attrs.resize(source.attrs.size());
    for (std::size_t i = 0; i < source.attrs.size(); ++i) assign_attr(target.attrs[i], source.attrs[i]);
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
} // namespace sc::xo_detail

XOFFSET_REGISTER_SCHEMA_NAME(sc::xo_detail::Snapshot, "competitor.value-snapshot.v1")

namespace sc {
struct XOffsetAdapter {
    using Buffer = XOffsetDatastructure::TypedXBuffer<xo_detail::Snapshot>;
    using State = std::unique_ptr<Buffer>;
    static constexpr std::string_view library_id = "xoffset-mac-r3-v2";
    static constexpr std::string_view commit = "7539433ab96d69c4a07b1db5c0a13786da85cdcd";
    static constexpr std::string_view trust_mode = "T-trusted-typed-verified-owning-copy-Automatic;U-not-established";
    static constexpr std::string_view source_archive_sha256 = "5a8647736d1145676c503299d704bc37e2dd035d730ccc2ffca2e5e96a28c658";

    static auto policy() {
        return XOffsetDatastructure::XBuffer::maximum_capacity(native_arena_maximum)
            .with_growth(XOffsetDatastructure::XGrowPolicy::Automatic);
    }
    static xo_detail::Snapshot& root(const State& state) { return state->root(); }

    static State build(const Snapshot& input) {
        auto state = std::make_unique<Buffer>(Buffer::create(native_arena_initial, policy()));
        auto& s = root(state);
        s.schema_version = input.schema_version;
        s.epoch = input.epoch;
        s.records.reserve(input.records.size());
        for (const auto& source : input.records) {
            // Container records are constructed in their arena, never on the
            // stack with an allocator binding that cannot reach the arena.
            auto& target = s.records.emplace_back();
            xo_detail::assign_record(target, source);
        }
        return state;
    }
    static State load(std::span<const std::uint8_t> bytes) {
        if (bytes.size() > wire_limit) throw std::runtime_error("XOffset wire limit");
        auto state = std::make_unique<Buffer>(Buffer::load(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()), policy()));
        if (root(state).schema_version != 1) throw std::runtime_error("XOffset schema version");
        return state;
    }
    static std::string emit(const State& state) { return state->save_verified(); }
    static Snapshot export_state(const State& state) { return export_view(PlainSnapshotView{root(state)}); }
    static void query(const State& state, std::span<const Query> queries, std::span<QueryResult> results) {
        query_view(PlainSnapshotView{root(state)}, queries, results);
    }

    static void mutate(State& state, std::span<const Mutation> operations) {
        auto& s = root(state);
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
                        throw std::runtime_error("XOffset erase status mismatch");
                    if (found) removed.insert(op.id);
                }
                std::size_t write = 0;
                for (std::size_t read = 0; read < s.records.size(); ++read) {
                    if (removed.contains(s.records[read].id)) continue;
                    // Public record/member move assignments preserve arena
                    // bindings. XOffset's moves may copy nested values; no
                    // bytewise relocation or private allocator API is used.
                    if (write != read) s.records[write] = std::move(s.records[read]);
                    ++write;
                }
                s.records.resize(write);
                continue;
            }
            for (const auto& op : group) {
                Status actual = Status::ok;
                if (op.code == MutationCode::set_epoch) s.epoch = op.epoch;
                else {
                    const auto at = find_record(PlainSnapshotView{s}, op.id);
                    const bool found = at < s.records.size();
                    if (op.code == MutationCode::append_record) {
                        if (found) throw std::runtime_error("XOffset append duplicate id");
                        // Trace ids are increasing; retaining a legal stable
                        // insertion also covers small functional fixtures.
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
                        s.records.resize(std::size_t(old) + 1);
                        for (std::size_t i = old; i > pos; --i) s.records[i] = std::move(s.records[i - 1]);
                        xo_detail::assign_record(s.records[pos], op.record);
                    } else if (!found) actual = Status::not_found;
                    else {
                        auto& r = s.records[at];
                        switch (op.code) {
                        case MutationCode::update_score: r.score = op.score; r.revision = op.revision; break;
                        case MutationCode::replace_name: r.name = std::string_view(op.text); break;
                        case MutationCode::append_samples:
                            for (auto value : op.samples) r.samples.push_back(value);
                            break;
                        case MutationCode::replace_payload: r.payload.assign(op.bytes.begin(), op.bytes.end()); break;
                        case MutationCode::replace_attr:
                        case MutationCode::insert_attr: {
                            auto pos = xo_detail::attr_lower(r, op.text);
                            const bool exists = pos < r.attrs.size() && r.attrs[pos].key == std::string_view(op.text);
                            if (op.code == MutationCode::replace_attr) {
                                if (!exists) actual = Status::not_found;
                                else xo_detail::assign_sequence(r.attrs[pos].value, op.bytes);
                            } else {
                                if (exists) throw std::runtime_error("XOffset insert duplicate key");
                                const auto old = r.attrs.size();
                                r.attrs.resize(std::size_t(old) + 1);
                                for (std::size_t i = old; i > pos; --i) r.attrs[i] = std::move(r.attrs[i - 1]);
                                r.attrs[pos].key = std::string_view(op.text);
                                xo_detail::assign_sequence(r.attrs[pos].value, op.bytes);
                            }
                            break;
                        }
                        default: throw std::runtime_error("XOffset unknown mutation opcode");
                        }
                    }
                }
                if (actual != op.expected) throw std::runtime_error("XOffset mutation status mismatch");
            }
        }
    }
};
} // namespace sc
