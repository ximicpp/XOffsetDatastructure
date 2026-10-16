#ifndef VM_NETWORK_FRAME_MODEL_TESTS_HPP
#define VM_NETWORK_FRAME_MODEL_TESTS_HPP

#include "frame_model.hpp"
#include <array>
#include <stdexcept>
#include <vector>

namespace vm_network {

struct TestRecord {
    std::string id, parent_id, expected_reason, actual_reason;
    std::size_t read_bytes = 0, written_bytes = 0, receive_allocations = 0;
    bool published = false;
    std::size_t terminal_count = 0, active_bytes = 0, active_queue_frames = 0;
    std::size_t expected_read_bytes = 0, expected_written_bytes = 0;
    std::size_t expected_receive_allocations = 0;
    bool expected_published = false;
    std::size_t expected_terminal_count = 1, expected_active_bytes = 0;
    std::size_t expected_active_queue_frames = 0;
};

inline std::string test_record_json(const TestRecord& row) {
    const auto quoted = [](std::string_view value) {
        std::string out = "\"";
        constexpr char hex[] = "0123456789abcdef";
        for (const unsigned char c : value) {
            if (c == '\\' || c == '"') { out += '\\'; out += static_cast<char>(c); }
            else if (c < 0x20) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
            else out += static_cast<char>(c);
        }
        out += '"';
        return out;
    };
    const auto scope = row.id.starts_with("model.") ? "injected_model" :
                       row.id.starts_with("tcp.") ? "actual_tcp" :
                       row.id.starts_with("fixture.") ? "filesystem" : "unknown";
    std::string out = "{\"id\":" + quoted(row.id) + ",\"scope\":" + quoted(scope) +
        ",\"parent_id\":" + quoted(row.parent_id) +
        ",\"expected_reason\":" + quoted(row.expected_reason) +
        ",\"actual_reason\":" + quoted(row.actual_reason);
    const auto number = [&](const char* key, std::size_t value) {
        out += ",\""; out += key; out += "\":"; out += std::to_string(value);
    };
    const auto boolean = [&](const char* key, bool value) {
        out += ",\""; out += key; out += "\":"; out += value ? "true" : "false";
    };
    number("read_bytes", row.read_bytes); number("written_bytes", row.written_bytes);
    number("receive_allocations", row.receive_allocations); boolean("published", row.published);
    number("terminal_count", row.terminal_count); number("active_bytes", row.active_bytes);
    number("active_queue_frames", row.active_queue_frames);
    number("expected_read_bytes", row.expected_read_bytes);
    number("expected_written_bytes", row.expected_written_bytes);
    number("expected_receive_allocations", row.expected_receive_allocations);
    boolean("expected_published", row.expected_published);
    number("expected_terminal_count", row.expected_terminal_count);
    number("expected_active_bytes", row.expected_active_bytes);
    number("expected_active_queue_frames", row.expected_active_queue_frames);
    return out + "}";
}

// Static exact inventory: symbolic boundary names stay stable across fixture
// sizes. Even if two named boundaries coincide, both IDs are explicitly checked.
inline std::vector<std::string> frame_model_subcase_ids() {
    std::vector<std::string> ids;
    for (std::size_t split = 0; split <= wire_header_bytes; ++split)
        ids.push_back("model.header_split." + std::to_string(split));
    for (const char* cut : {"0", "1", "7", "8", "63", "64", "65", "mid", "last", "end"})
        ids.push_back(std::string("model.payload_split.") + cut);
    for (const char* frame : {"1", "2", "3"})
        ids.push_back(std::string("model.contiguous.") + frame);
    for (const char* field : {"magic", "major", "minor", "header_size", "endian", "flags",
                              "root_type", "schema_lo", "schema_hi", "reserved_lt_used",
                              "reserved_overflow", "crc", "reserved0", "root_offset", "allocator_offset"})
        ids.push_back(std::string("model.reject.") + field);
    for (const char* id : {
            "model.reserved_historical", "model.overflow.uint64", "model.overflow.max_payload",
            "model.max_wire.header_underflow", "model.max_wire.frame_underflow", "model.max_wire.exact",
            "model.owner.underflow", "model.owner.exact", "model.inflight.blocked",
            "model.inflight.first_cancelled", "model.inflight.recovered", "model.queue.blocked",
            "model.queue.first_cancelled", "model.queue.recovered", "model.short_io.receive",
            "model.short_io.send"}) ids.emplace_back(id);
    for (const char* stage : {"header", "payload"})
        for (const char* outcome : {"zero", "eof", "timeout", "error"})
            ids.push_back(std::string("model.read_outcome.") + stage + "." + outcome);
    ids.emplace_back("model.read_outcome.empty_eof");
    for (const char* outcome : {"zero", "eof", "timeout"})
        ids.push_back(std::string("model.send_outcome.") + outcome);
    for (const char* id : {
            "model.read_outcome.complete_then_eof", "model.graph.rejected", "model.graph.exception",
            "model.cancel_receive.before_pending", "model.cancel_receive.header_pending",
            "model.cancel_receive.payload_pending", "model.cancel_receive.before_submit",
            "model.cancel_receive.before_verify", "model.cancel_receive.prepublish",
            "model.cancel_send.before_pending", "model.cancel_send.header_pending",
            "model.cancel_send.payload_pending", "model.cancel_send.before_submit",
            "model.late.receive", "model.late.send", "model.output_lifetime",
            "model.partial.send_error", "model.partial.read_overreport"}) ids.emplace_back(id);
    return ids;
}

inline const std::vector<std::string>& frame_model_case_ids() {
    static const std::vector<std::string> ids{
        "model.header_all_splits", "model.payload_boundaries", "model.contiguous_frames",
        "model.header_validation", "model.length_overflow", "model.max_wire_budget",
        "model.owner_budget", "model.inflight_budget_restore", "model.queue_budget_restore",
        "model.short_io_would_block", "model.zero_progress_eof",
        "model.invalid_graph_no_publication", "model.cancel_receive_stages",
        "model.cancel_prepublish", "model.cancel_send_stages", "model.late_completion_once",
        "model.owning_output_lifetime", "model.partial_send_error"
    };
    return ids;
}

namespace model_test_detail {
inline void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error("frame model assertion: " + reason);
}
inline std::span<const char> bytes(const std::string& wire) {
    return {wire.data(), wire.size()};
}
inline WireHeader header_of(const std::string& wire) {
    require(wire.size() >= wire_header_bytes, "fixture header size");
    WireHeader h{};
    std::memcpy(&h, wire.data(), wire_header_bytes);
    return h;
}
inline std::string with_header(const std::string& wire, const WireHeader& h) {
    std::string result = wire;
    std::memcpy(result.data(), &h, wire_header_bytes);
    return result;
}
inline void feed_all(ReceiveModel& model, std::span<const char> input,
                     std::size_t chunk = std::numeric_limits<std::size_t>::max()) {
    std::size_t offset = 0;
    while (offset < input.size()) {
        const auto consumed = model.feed(input.subspan(offset, std::min(chunk, input.size() - offset)));
        require(consumed != 0, "feed made no progress at " + std::to_string(offset));
        offset += consumed;
    }
}
inline void released(const ResourceLedger& ledger) {
    require(ledger.inflight_bytes() == 0 && ledger.queue_frames() == 0,
            "terminal operation must return byte and queue budgets");
}
inline void expect_header_rejection(const std::string& wire, Limits limits,
                                    std::uint64_t type, std::uint64_t schema,
                                    Error expected, auto&& report) {
    ResourceLedger ledger(limits);
    ReceiveModel model(ledger, type, schema);
    require(model.feed(bytes(wire).first(wire_header_bytes)) == wire_header_bytes,
            "rejected header consumes exactly header");
    require(model.terminal() && model.error() == expected,
            std::string("expected header rejection ") + error_name(expected) +
            ", observed " + error_name(model.error()));
    require(model.transferred() == wire_header_bytes && model.allocations() == 0 &&
            model.verification_attempts() == 0 && !model.published(),
            "header rejection precedes payload allocation and publication");
    released(ledger);
    report(model, ledger);
}
} // namespace model_test_detail

// The callback must return true only after a real full-graph load succeeds, and
// false for the caller's deliberately invalid structural graph. Exceptions are
// a different model outcome and cannot accidentally satisfy a negative test.
// Returned IDs identify model checks only, never real asynchronous networking.
template<class Verifier>
std::vector<std::string> run_frame_model_tests(const std::string& valid_wire,
                                              const std::string& invalid_graph_wire,
                                              Limits limits,
                                              std::uint64_t type,
                                              std::uint64_t schema,
                                              Verifier&& verify,
                                              std::vector<TestRecord>* records = nullptr) {
    using namespace model_test_detail;
    const auto header = header_of(valid_wire);
    require(valid_wire.size() > wire_header_bytes + 65, "fixture needs payload larger than 65 bytes");
    require(header.used_bytes == valid_wire.size() - wire_header_bytes, "fixture extent");
    require(static_cast<bool>(admit_header(header, limits, type, schema)), "fixture admission");
    require(static_cast<bool>(admit_header(header_of(invalid_graph_wire), limits, type, schema)),
            "invalid graph must retain an admissible header");
    std::vector<std::string> passed;
    std::vector<TestRecord> details;
    const auto record = [&](std::string id, const std::string& parent, const auto& model,
                            const ResourceLedger& ledger, Error expected,
                            std::size_t expected_read, std::size_t expected_write,
                            std::size_t expected_alloc, bool expected_published,
                            std::size_t active_bytes = 0, std::size_t active_queue = 0) {
        TestRecord row;
        row.id = std::move(id); row.parent_id = parent;
        row.expected_reason = error_name(expected); row.actual_reason = error_name(model.error());
        if constexpr (requires { model.allocations(); }) {
            row.read_bytes = model.transferred(); row.receive_allocations = model.allocations();
            row.published = model.published();
        } else row.written_bytes = model.transferred();
        row.terminal_count = model.terminal_count();
        row.active_bytes = ledger.inflight_bytes(); row.active_queue_frames = ledger.queue_frames();
        row.expected_read_bytes = expected_read; row.expected_written_bytes = expected_write;
        row.expected_receive_allocations = expected_alloc; row.expected_published = expected_published;
        row.expected_active_bytes = active_bytes; row.expected_active_queue_frames = active_queue;
        require(row.expected_reason == row.actual_reason && row.read_bytes == expected_read &&
                row.written_bytes == expected_write && row.receive_allocations == expected_alloc &&
                row.published == expected_published && row.terminal_count == 1 &&
                row.active_bytes == active_bytes && row.active_queue_frames == active_queue,
                row.id + " expected/actual detailed acceptance differs");
        details.push_back(std::move(row));
    };
    const auto run = [&](const char* id, auto&& body) {
        try { body(); }
        catch (const std::exception& e) {
            throw std::runtime_error(std::string(id) + ": " + e.what());
        }
        passed.emplace_back(id);
    };

    run("model.header_all_splits", [&] {
        // Include both zero-length endpoints, with all 87 interior boundaries.
        for (std::size_t split = 0; split <= wire_header_bytes; ++split) {
            ResourceLedger ledger(limits);
            ReceiveModel model(ledger, type, schema);
            const auto input = bytes(valid_wire);
            if (split) feed_all(model, input.first(split));
            require(model.allocations() == (split == wire_header_bytes ? 1u : 0u),
                    "no allocation before complete admitted header");
            feed_all(model, input.subspan(split));
            require(model.transferred() == valid_wire.size(), "all split bytes consumed");
            require(model.verify_and_publish(verify) && model.published(), "split publication");
            require(model.terminal_count() == 1, "split terminal once");
            released(ledger);
            record("model.header_split." + std::to_string(split), "model.header_all_splits",
                   model, ledger, Error::none, valid_wire.size(), 0, 1, true);
        }
    });

    run("model.payload_boundaries", [&] {
        const auto payload = valid_wire.size() - wire_header_bytes;
        const std::array<std::pair<const char*, std::size_t>, 10> cuts{{
            {"0", 0}, {"1", 1}, {"7", 7}, {"8", 8}, {"63", 63}, {"64", 64},
            {"65", 65}, {"mid", payload / 2}, {"last", payload - 1}, {"end", payload}}};
        for (const auto& [name, cut] : cuts) {
            ResourceLedger ledger(limits);
            ReceiveModel model(ledger, type, schema);
            feed_all(model, bytes(valid_wire).first(wire_header_bytes + cut));
            require(!model.published() && model.verification_attempts() == 0,
                    "unverified complete/incomplete frame is unpublished");
            feed_all(model, bytes(valid_wire).subspan(wire_header_bytes + cut));
            require(model.verify_and_publish(verify), "payload split publication");
            released(ledger);
            record(std::string("model.payload_split.") + name, "model.payload_boundaries",
                   model, ledger, Error::none, valid_wire.size(), 0, 1, true);
        }
    });

    run("model.contiguous_frames", [&] {
        const auto stream = valid_wire + valid_wire + valid_wire;
        std::size_t offset = 0;
        ResourceLedger ledger(limits);
        for (unsigned frame = 0; frame != 3; ++frame) {
            ReceiveModel model(ledger, type, schema);
            while (model.stage() != ReceiveStage::frame_ready) {
                const auto consumed = model.feed(bytes(stream).subspan(offset));
                require(consumed != 0, "contiguous frame forward progress");
                offset += consumed;
            }
            require(offset == (frame + 1) * valid_wire.size(), "no next-frame overread");
            require(model.verify_and_publish(verify), "contiguous frame verification");
            released(ledger);
            record("model.contiguous." + std::to_string(frame + 1), "model.contiguous_frames",
                   model, ledger, Error::none, valid_wire.size(), 0, 1, true);
        }
        require(offset == stream.size(), "contiguous stream exact consumption");
    });

    run("model.header_validation", [&] {
        const auto reject = [&](const char* field, auto&& mutate, Error expected) {
            auto h = header;
            mutate(h);
            expect_header_rejection(with_header(valid_wire, h), limits, type, schema, expected,
                [&](const auto& model, const auto& ledger) {
                    record(std::string("model.reject.") + field, "model.header_validation",
                           model, ledger, expected, wire_header_bytes, 0, 0, false);
                });
        };
        reject("magic", [](auto& h) { h.magic[0] ^= 1; }, Error::bad_magic);
        reject("major", [](auto& h) { ++h.format_major; }, Error::version);
        reject("minor", [](auto& h) { ++h.format_minor; }, Error::version);
        reject("header_size", [](auto& h) { --h.header_size; }, Error::header_size);
        reject("endian", [](auto& h) { h.endian_tag ^= 1; }, Error::endian);
        reject("flags", [](auto& h) { h.flags = 1; }, Error::flags);
        reject("root_type", [](auto& h) { h.root_type_id ^= 1; }, Error::root_type);
        reject("schema_lo", [](auto& h) { h.schema_hash_lo ^= 1; }, Error::schema);
        reject("schema_hi", [](auto& h) { h.schema_hash_hi = 1; }, Error::schema);
        reject("reserved_lt_used", [](auto& h) { h.reserved_bytes = h.used_bytes - 1; }, Error::payload_size);
        reject("reserved_overflow", [](auto& h) { h.reserved_bytes = std::numeric_limits<std::uint64_t>::max(); },
               Error::payload_size);
        reject("crc", [](auto& h) { h.crc32c = 1; }, Error::reserved_fields);
        reject("reserved0", [](auto& h) { h.reserved0 = 1; }, Error::reserved_fields);
        reject("root_offset", [](auto& h) { h.root_offset = h.used_bytes; }, Error::root_offset);
        reject("allocator_offset", [](auto& h) { h.allocator_offset = h.used_bytes; }, Error::allocator_offset);
        // F accepts historical reservation larger than transmitted used_bytes,
        // even when the explicit owner cap is exactly used_bytes.
        auto historical_header = header;
        historical_header.reserved_bytes = header.used_bytes + 8;
        auto exact_owner = limits;
        exact_owner.owner_capacity = static_cast<std::size_t>(header.used_bytes);
        const auto historical_wire = with_header(valid_wire, historical_header);
        ResourceLedger ledger(exact_owner);
        ReceiveModel model(ledger, type, schema);
        feed_all(model, bytes(historical_wire));
        require(model.verify_and_publish(verify), "F historical reserved_bytes compatibility");
        released(ledger);
        record("model.reserved_historical", "model.header_validation", model, ledger,
               Error::none, valid_wire.size(), 0, 1, true);
    });

    run("model.length_overflow", [&] {
        auto h = header;
        h.used_bytes = std::numeric_limits<std::uint64_t>::max();
        h.reserved_bytes = h.used_bytes;
        expect_header_rejection(with_header(valid_wire, h), limits, type, schema, Error::payload_size,
            [&](const auto& model, const auto& ledger) {
                record("model.overflow.uint64", "model.length_overflow", model, ledger,
                       Error::payload_size, wire_header_bytes, 0, 0, false);
            });
        h = header;
        h.used_bytes = static_cast<std::uint64_t>(XOffsetDatastructure::XBufferCore::MAX_PAYLOAD) + 1;
        h.reserved_bytes = h.used_bytes;
        expect_header_rejection(with_header(valid_wire, h), limits, type, schema, Error::payload_size,
            [&](const auto& model, const auto& ledger) {
                record("model.overflow.max_payload", "model.length_overflow", model, ledger,
                       Error::payload_size, wire_header_bytes, 0, 0, false);
            });
    });

    run("model.max_wire_budget", [&] {
        auto small = limits;
        small.max_wire_bytes = wire_header_bytes - 1;
        expect_header_rejection(valid_wire, small, type, schema, Error::max_wire,
            [&](const auto& model, const auto& ledger) {
                record("model.max_wire.header_underflow", "model.max_wire_budget", model, ledger,
                       Error::max_wire, wire_header_bytes, 0, 0, false);
            });
        small.max_wire_bytes = valid_wire.size() - 1;
        expect_header_rejection(valid_wire, small, type, schema, Error::max_wire,
            [&](const auto& model, const auto& ledger) {
                record("model.max_wire.frame_underflow", "model.max_wire_budget", model, ledger,
                       Error::max_wire, wire_header_bytes, 0, 0, false);
            });
        small.max_wire_bytes = valid_wire.size();
        ResourceLedger ledger(small);
        ReceiveModel model(ledger, type, schema);
        feed_all(model, bytes(valid_wire));
        require(model.verify_and_publish(verify), "exact max-wire boundary allowed");
        released(ledger);
        record("model.max_wire.exact", "model.max_wire_budget", model, ledger,
               Error::none, valid_wire.size(), 0, 1, true);
    });

    run("model.owner_budget", [&] {
        auto small = limits;
        small.owner_capacity = static_cast<std::size_t>(header.used_bytes) - 1;
        expect_header_rejection(valid_wire, small, type, schema, Error::owner_capacity,
            [&](const auto& model, const auto& ledger) {
                record("model.owner.underflow", "model.owner_budget", model, ledger,
                       Error::owner_capacity, wire_header_bytes, 0, 0, false);
            });
        small.owner_capacity = static_cast<std::size_t>(header.used_bytes);
        ResourceLedger ledger(small);
        ReceiveModel model(ledger, type, schema);
        feed_all(model, bytes(valid_wire));
        require(model.verify_and_publish(verify), "exact owner boundary allowed");
        released(ledger);
        record("model.owner.exact", "model.owner_budget", model, ledger,
               Error::none, valid_wire.size(), 0, 1, true);
    });

    run("model.inflight_budget_restore", [&] {
        auto bounded = limits;
        bounded.max_queue_frames = 2;
        bounded.max_inflight_bytes = valid_wire.size();
        ResourceLedger ledger(bounded);
        ReceiveModel first(ledger, type, schema);
        feed_all(first, bytes(valid_wire).first(wire_header_bytes));
        require(ledger.inflight_bytes() == valid_wire.size(), "first receive reserves full frame");
        ReceiveModel blocked(ledger, type, schema);
        feed_all(blocked, bytes(valid_wire).first(wire_header_bytes));
        require(blocked.error() == Error::inflight_budget && blocked.allocations() == 0,
                "inflight rejection occurs before allocation");
        require(ledger.inflight_bytes() == valid_wire.size() && ledger.queue_frames() == 1,
                "rejection preserves first operation resources");
        record("model.inflight.blocked", "model.inflight_budget_restore", blocked, ledger,
               Error::inflight_budget, wire_header_bytes, 0, 0, false, valid_wire.size(), 1);
        first.cancel();
        released(ledger);
        record("model.inflight.first_cancelled", "model.inflight_budget_restore", first, ledger,
               Error::cancelled, wire_header_bytes, 0, 1, false);
        ReceiveModel recovered(ledger, type, schema);
        feed_all(recovered, bytes(valid_wire));
        require(recovered.verify_and_publish(verify), "receive resumes after budget release");
        released(ledger);
        record("model.inflight.recovered", "model.inflight_budget_restore", recovered, ledger,
               Error::none, valid_wire.size(), 0, 1, true);
        require(ledger.peak_inflight_bytes() == valid_wire.size(), "bounded inflight peak");
    });

    run("model.queue_budget_restore", [&] {
        auto bounded = limits;
        bounded.max_queue_frames = 1;
        ResourceLedger ledger(bounded);
        ReceiveModel first(ledger, type, schema);
        ReceiveModel blocked(ledger, type, schema);
        require(blocked.terminal() && blocked.error() == Error::queue_budget &&
                blocked.transferred() == 0 && blocked.allocations() == 0,
                "queue full rejects new submission before any transport");
        require(ledger.queue_frames() == 1, "blocked queue operation does not steal slot");
        record("model.queue.blocked", "model.queue_budget_restore", blocked, ledger,
               Error::queue_budget, 0, 0, 0, false, 0, 1);
        first.cancel();
        released(ledger);
        record("model.queue.first_cancelled", "model.queue_budget_restore", first, ledger,
               Error::cancelled, 0, 0, 0, false);
        ReceiveModel recovered(ledger, type, schema);
        feed_all(recovered, bytes(valid_wire));
        require(recovered.verify_and_publish(verify), "queue resumes after cancellation");
        released(ledger);
        record("model.queue.recovered", "model.queue_budget_restore", recovered, ledger,
               Error::none, valid_wire.size(), 0, 1, true);
        require(ledger.peak_queue_frames() == 1, "bounded queue peak");
    });

    run("model.short_io_would_block", [&] {
        ResourceLedger ledger(limits);
        ReceiveModel model(ledger, type, schema);
        for (unsigned wake = 0; wake != 3; ++wake) {
            const auto token = model.begin_read();
            require(token != 0, "would-block read token");
            model.complete_read(token, Progress::would_block);
            require(model.transferred() == 0 && !model.terminal() && !model.pending(),
                    "would-block returns control without implicit busy retry");
        }
        feed_all(model, bytes(valid_wire), 1);
        require(model.would_blocks() == 3 && model.verify_and_publish(verify),
                "one-byte short reads recover after would-block");
        released(ledger);
        record("model.short_io.receive", "model.short_io_would_block", model, ledger,
               Error::none, valid_wire.size(), 0, 1, true);
        SendModel send(ledger, std::make_shared<const std::string>(valid_wire));
        for (unsigned wake = 0; wake != 3; ++wake) {
            const auto token = send.begin_write();
            send.complete_write(token, Progress::would_block);
            require(send.transferred() == 0 && !send.terminal(), "write would-block no progress");
        }
        while (!send.terminal()) {
            const auto token = send.begin_write();
            const auto count = std::min<std::size_t>(7, send.remaining_bytes().size());
            send.complete_write(token, Progress::bytes, count);
        }
        require(send.completed() && send.error() == Error::none &&
                send.transferred() == valid_wire.size() && send.would_blocks() == 3,
                "short writes resume from exact offset");
        released(ledger);
        record("model.short_io.send", "model.short_io_would_block", send, ledger,
               Error::none, 0, valid_wire.size(), 0, false);
    });

    run("model.zero_progress_eof", [&] {
        const std::array<Progress, 4> outcomes{Progress::zero, Progress::eof,
                                             Progress::timeout, Progress::error};
        for (const bool payload_stage : {false, true}) {
            for (const auto outcome : outcomes) {
                ResourceLedger ledger(limits);
                ReceiveModel model(ledger, type, schema);
                const auto prefix = payload_stage ? wire_header_bytes + 1 : 1;
                feed_all(model, bytes(valid_wire).first(prefix));
                model.complete_read(model.begin_read(), outcome);
                const auto expected = outcome == Progress::zero ? Error::zero_progress :
                    outcome == Progress::timeout ? Error::timeout :
                    outcome == Progress::error ? Error::transport_error :
                    payload_stage ? Error::eof_payload : Error::eof_header;
                require(model.terminal() && model.error() == expected &&
                        model.transferred() == prefix && model.terminal_count() == 1 &&
                        !model.published() && model.verification_attempts() == 0,
                        "zero/EOF/deadline/error retains exact progress and cause");
                released(ledger);
                const char* outcome_name = outcome == Progress::zero ? "zero" :
                    outcome == Progress::eof ? "eof" : outcome == Progress::timeout ? "timeout" : "error";
                record(std::string("model.read_outcome.") + (payload_stage ? "payload." : "header.") + outcome_name,
                       "model.zero_progress_eof", model, ledger, expected, prefix, 0,
                       payload_stage ? 1 : 0, false);
            }
        }
        ResourceLedger ledger(limits);
        ReceiveModel empty_eof(ledger, type, schema);
        empty_eof.complete_read(empty_eof.begin_read(), Progress::eof);
        require(empty_eof.error() == Error::eof_header && empty_eof.transferred() == 0,
                "empty stream is explicit header EOF");
        released(ledger);
        record("model.read_outcome.empty_eof", "model.zero_progress_eof", empty_eof, ledger,
               Error::eof_header, 0, 0, 0, false);
        for (const auto outcome : {Progress::zero, Progress::eof, Progress::timeout}) {
            SendModel send(ledger, std::make_shared<const std::string>(valid_wire));
            send.complete_write(send.begin_write(), outcome);
            require(send.error() == (outcome == Progress::zero ? Error::zero_progress :
                                    outcome == Progress::eof ? Error::transport_error : Error::timeout) &&
                    send.transferred() == 0 && !send.completed(), "send zero/EOF/deadline cause");
            released(ledger);
            const char* name = outcome == Progress::zero ? "zero" : outcome == Progress::eof ? "eof" : "timeout";
            record(std::string("model.send_outcome.") + name, "model.zero_progress_eof", send, ledger,
                   outcome == Progress::zero ? Error::zero_progress :
                   outcome == Progress::eof ? Error::transport_error : Error::timeout, 0, 0, 0, false);
        }
        ReceiveModel complete(ledger, type, schema);
        feed_all(complete, bytes(valid_wire));
        require(complete.verify_and_publish(verify), "complete frame does not need EOF");
        complete.complete_read(0, Progress::eof);
        require(complete.published() && complete.error() == Error::none &&
                complete.terminal_count() == 1, "post-frame EOF cannot reverse success");
        released(ledger);
        record("model.read_outcome.complete_then_eof", "model.zero_progress_eof", complete, ledger,
               Error::none, valid_wire.size(), 0, 1, true);
    });

    run("model.invalid_graph_no_publication", [&] {
        ResourceLedger ledger(limits);
        ReceiveModel invalid(ledger, type, schema);
        feed_all(invalid, bytes(invalid_graph_wire));
        require(invalid.stage() == ReceiveStage::frame_ready && !invalid.published(),
                "complete invalid graph held as untyped staging bytes");
        require(!invalid.verify_and_publish(verify) && invalid.error() == Error::invalid_graph &&
                invalid.transferred() == invalid_graph_wire.size() &&
                invalid.verification_attempts() == 1 && invalid.verified() == 0 &&
                !invalid.published(), "bad graph fails for expected verification reason");
        released(ledger);
        record("model.graph.rejected", "model.invalid_graph_no_publication", invalid, ledger,
               Error::invalid_graph, invalid_graph_wire.size(), 0, 1, false);
        ReceiveModel exception(ledger, type, schema);
        feed_all(exception, bytes(valid_wire));
        require(!exception.verify_and_publish([](const std::string&) -> bool {
                    throw std::runtime_error("deliberate verifier exception");
                }) && exception.error() == Error::verifier_exception && !exception.published(),
                "arbitrary exception is distinct from invalid-graph success");
        released(ledger);
        record("model.graph.exception", "model.invalid_graph_no_publication", exception, ledger,
               Error::verifier_exception, valid_wire.size(), 0, 1, false);
    });

    run("model.cancel_receive_stages", [&] {
        for (const auto prefix : {std::size_t{0}, std::size_t{1}, wire_header_bytes + 1}) {
            ResourceLedger ledger(limits);
            ReceiveModel model(ledger, type, schema);
            if (prefix) feed_all(model, bytes(valid_wire).first(prefix));
            const auto token = model.begin_read();
            const auto held = ledger.inflight_bytes();
            const auto buffered = model.buffer_bytes();
            model.cancel();
            require(!model.terminal() && model.pending() && ledger.queue_frames() == 1 &&
                    ledger.inflight_bytes() == held && model.buffer_bytes() == buffered,
                    "cancel request retains pending transport buffer and budgets");
            model.complete_read(token, Progress::bytes, bytes(valid_wire).subspan(prefix, 1));
            require(model.terminal() && model.error() == Error::cancelled &&
                    model.transferred() == prefix && !model.published() &&
                    model.terminal_count() == 1, "cancel completion cannot advance or publish");
            released(ledger);
            const char* name = prefix == 0 ? "before_pending" : prefix == 1 ? "header_pending" : "payload_pending";
            record(std::string("model.cancel_receive.") + name, "model.cancel_receive_stages",
                   model, ledger, Error::cancelled, prefix, 0, prefix > wire_header_bytes ? 1 : 0, false);
        }
        ResourceLedger ledger(limits);
        ReceiveModel before(ledger, type, schema);
        before.cancel();
        require(before.error() == Error::cancelled && before.transferred() == 0,
                "cancel before first read");
        released(ledger);
        record("model.cancel_receive.before_submit", "model.cancel_receive_stages", before, ledger,
               Error::cancelled, 0, 0, 0, false);
        ReceiveModel complete(ledger, type, schema);
        feed_all(complete, bytes(valid_wire));
        complete.cancel();
        require(complete.error() == Error::cancelled && !complete.verify_and_publish(verify) &&
                complete.verification_attempts() == 0, "cancel full frame before verification");
        released(ledger);
        record("model.cancel_receive.before_verify", "model.cancel_receive_stages", complete, ledger,
               Error::cancelled, valid_wire.size(), 0, 1, false);
    });

    run("model.cancel_prepublish", [&] {
        ResourceLedger ledger(limits);
        ReceiveModel model(ledger, type, schema);
        feed_all(model, bytes(valid_wire));
        require(!model.verify_and_publish(verify, [](ReceiveModel& operation) { operation.cancel(); }),
                "cancel at publication boundary rejects commit");
        require(model.error() == Error::cancelled && model.verified() == 1 &&
                !model.published() && model.terminal_count() == 1,
                "fully verified cancelled frame remains unpublished");
        released(ledger);
        record("model.cancel_receive.prepublish", "model.cancel_prepublish", model, ledger,
               Error::cancelled, valid_wire.size(), 0, 1, false);
    });

    run("model.cancel_send_stages", [&] {
        for (const auto prefix : {std::size_t{0}, std::size_t{1}, wire_header_bytes + 1}) {
            ResourceLedger ledger(limits);
            auto owned = std::make_shared<const std::string>(valid_wire);
            std::weak_ptr<const std::string> weak = owned;
            SendModel send(ledger, owned);
            owned.reset();
            if (prefix) send.complete_write(send.begin_write(), Progress::bytes, prefix);
            const auto token = send.begin_write();
            send.cancel();
            require(!send.terminal() && send.pending() && !weak.expired() &&
                    ledger.inflight_bytes() == valid_wire.size(),
                    "cancel pending send retains immutable owning output");
            send.acknowledge_cancel(token);
            require(send.terminal() && send.error() == Error::cancelled && !send.completed() &&
                    send.transferred() == prefix && weak.expired(),
                    "send cancellation releases only after completion acknowledgement");
            released(ledger);
            const char* name = prefix == 0 ? "before_pending" : prefix == 1 ? "header_pending" : "payload_pending";
            record(std::string("model.cancel_send.") + name, "model.cancel_send_stages", send, ledger,
                   Error::cancelled, 0, prefix, 0, false);
        }
        ResourceLedger ledger(limits);
        SendModel before(ledger, std::make_shared<const std::string>(valid_wire));
        before.cancel();
        require(before.error() == Error::cancelled && before.transferred() == 0 &&
                before.begin_write() == 0, "cancel before send cannot submit later");
        released(ledger);
        record("model.cancel_send.before_submit", "model.cancel_send_stages", before, ledger,
               Error::cancelled, 0, 0, 0, false);
    });

    run("model.late_completion_once", [&] {
        ResourceLedger ledger(limits);
        ReceiveModel model(ledger, type, schema);
        const auto first = model.begin_read();
        model.complete_read(first, Progress::bytes, bytes(valid_wire).first(1));
        const auto second = model.begin_read();
        model.complete_read(first, Progress::bytes, bytes(valid_wire).first(1));
        require(model.pending() && model.transferred() == 1 && model.late_completions() == 1,
                "stale token does not consume the current operation");
        model.cancel();
        model.acknowledge_cancel(second);
        model.complete_read(second, Progress::bytes, bytes(valid_wire).first(1));
        model.acknowledge_cancel(second);
        require(model.error() == Error::cancelled && model.terminal_count() == 1 &&
                model.late_completions() == 3 && !model.published(),
                "duplicate and late reads cannot resurrect publication");
        released(ledger);
        record("model.late.receive", "model.late_completion_once", model, ledger,
               Error::cancelled, 1, 0, 0, false);
        SendModel send(ledger, std::make_shared<const std::string>(valid_wire));
        const auto token = send.begin_write();
        send.cancel();
        send.acknowledge_cancel(token);
        send.complete_write(token, Progress::bytes, valid_wire.size());
        require(send.error() == Error::cancelled && send.terminal_count() == 1 &&
                send.transferred() == 0 && send.late_completions() == 1 && !send.completed(),
                "late send success cannot replace cancellation");
        released(ledger);
        record("model.late.send", "model.late_completion_once", send, ledger,
               Error::cancelled, 0, 0, 0, false);
    });

    run("model.owning_output_lifetime", [&] {
        ResourceLedger ledger(limits);
        auto producer_storage = std::make_unique<std::string>(valid_wire);
        auto output = std::make_shared<const std::string>(*producer_storage);
        const auto* original_data = output->data();
        std::weak_ptr<const std::string> weak = output;
        SendModel send(ledger, output);
        output.reset();
        producer_storage.reset();
        std::string observed;
        while (!send.terminal()) {
            const auto token = send.begin_write();
            const auto remaining = send.remaining_bytes();
            require(!weak.expired() && remaining.data() == original_data + send.transferred(),
                    "owning output address stable across pending writes");
            const auto n = std::min<std::size_t>(13, remaining.size());
            observed.append(remaining.substr(0, n));
            send.complete_write(token, Progress::bytes, n);
        }
        require(observed == valid_wire && send.completed() && weak.expired(),
                "producer release does not alter owning output; final completion releases it");
        released(ledger);
        record("model.output_lifetime", "model.owning_output_lifetime", send, ledger,
               Error::none, 0, valid_wire.size(), 0, false);
    });

    run("model.partial_send_error", [&] {
        ResourceLedger ledger(limits);
        SendModel send(ledger, std::make_shared<const std::string>(valid_wire));
        send.complete_write(send.begin_write(), Progress::bytes, 7);
        send.complete_write(send.begin_write(), Progress::error);
        require(send.error() == Error::transport_error && send.transferred() == 7 &&
                !send.completed() && send.begin_write() == 0 && send.terminal_count() == 1,
                "partial send error keeps progress and never retransmits full message");
        released(ledger);
        record("model.partial.send_error", "model.partial_send_error", send, ledger,
               Error::transport_error, 0, 7, 0, false);
        ReceiveModel excessive(ledger, type, schema);
        excessive.complete_read(excessive.begin_read(), Progress::bytes, bytes(valid_wire));
        require(excessive.error() == Error::invalid_completion && excessive.allocations() == 0 &&
                excessive.transferred() == 0, "adapter overreport cannot overrun header buffer");
        released(ledger);
        record("model.partial.read_overreport", "model.partial_send_error", excessive, ledger,
               Error::invalid_completion, 0, 0, 0, false);
    });

    require(passed == frame_model_case_ids(), "exact preregistered model case set");
    std::vector<std::string> detail_ids;
    for (const auto& row : details) detail_ids.push_back(row.id);
    require(detail_ids == frame_model_subcase_ids(), "exact preregistered model subcase set");
    if (records) *records = std::move(details);
    return passed;
}

} // namespace vm_network
#endif
