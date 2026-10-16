#ifndef VM_NETWORK_FRAME_MODEL_HPP
#define VM_NETWORK_FRAME_MODEL_HPP

// Experimental, single-threaded completion model. This describes ownership and
// progress rules; it is not an asynchronous networking-library implementation.
// Include the frozen production header before this header.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace vm_network {

using WireHeader = XOffsetDatastructure::XWireHeaderV1;
inline constexpr std::size_t wire_header_bytes = sizeof(WireHeader);
static_assert(wire_header_bytes == 88);

enum class Error {
    none, bad_magic, version, header_size, endian, flags, root_type, schema,
    payload_size, reserved_fields, root_offset, allocator_offset, max_wire,
    owner_capacity, inflight_budget, queue_budget, zero_progress, eof_header,
    eof_payload, transport_error, timeout, cancelled, invalid_graph,
    verifier_exception, invalid_completion, allocation_failed
};

inline const char* error_name(Error e) noexcept {
    switch (e) {
    case Error::none: return "none";
    case Error::bad_magic: return "bad_magic";
    case Error::version: return "version";
    case Error::header_size: return "header_size";
    case Error::endian: return "endian";
    case Error::flags: return "flags";
    case Error::root_type: return "root_type";
    case Error::schema: return "schema";
    case Error::payload_size: return "payload_size";
    case Error::reserved_fields: return "reserved_fields";
    case Error::root_offset: return "root_offset";
    case Error::allocator_offset: return "allocator_offset";
    case Error::max_wire: return "max_wire";
    case Error::owner_capacity: return "owner_capacity";
    case Error::inflight_budget: return "inflight_budget";
    case Error::queue_budget: return "queue_budget";
    case Error::zero_progress: return "zero_progress";
    case Error::eof_header: return "eof_header";
    case Error::eof_payload: return "eof_payload";
    case Error::transport_error: return "transport_error";
    case Error::timeout: return "timeout";
    case Error::cancelled: return "cancelled";
    case Error::invalid_graph: return "invalid_graph";
    case Error::verifier_exception: return "verifier_exception";
    case Error::invalid_completion: return "invalid_completion";
    case Error::allocation_failed: return "allocation_failed";
    }
    return "unknown";
}

struct Limits {
    std::size_t max_wire_bytes = 32u * 1024u * 1024u;
    std::size_t owner_capacity = 32u * 1024u * 1024u;
    std::size_t max_inflight_bytes = 128u * 1024u * 1024u;
    std::size_t max_queue_frames = 1;
};

struct Admission {
    Error error = Error::none;
    std::size_t wire_bytes = 0;
    explicit operator bool() const noexcept { return error == Error::none; }
};

// No allocation, aligned input only. The caller memcpy's its 88 received bytes
// to WireHeader first. Fixed fields and reservation semantics match F's
// load_verified<T>(); only the full graph loader may publish a typed root.
inline Admission admit_header(const WireHeader& h, const Limits& limits,
                              std::uint64_t root_type_id,
                              std::uint64_t schema_hash) noexcept {
    using namespace XOffsetDatastructure;
    if (!wire_magic_matches_v1(h.magic)) return {Error::bad_magic};
    if (h.format_major != XWIRE_FORMAT_MAJOR_V1 ||
        h.format_minor != XWIRE_FORMAT_MINOR_V1) return {Error::version};
    if (h.header_size != wire_header_bytes) return {Error::header_size};
    // Lengths are not trusted before supported endian is established.
    if (h.endian_tag != XWIRE_ENDIAN_TAG_V1) return {Error::endian};
    if (h.flags != XWIRE_FLAGS_V1) return {Error::flags};
    if (h.root_type_id != root_type_id) return {Error::root_type};
    if (h.schema_hash_lo != schema_hash || h.schema_hash_hi != 0)
        return {Error::schema};
    if (h.reserved_bytes < h.used_bytes ||
        h.reserved_bytes > XBufferCore::MAX_PAYLOAD ||
        h.used_bytes > XBufferCore::MAX_PAYLOAD) return {Error::payload_size};
    if (h.crc32c != 0 || h.reserved0 != 0) return {Error::reserved_fields};
    if (h.root_offset >= h.used_bytes) return {Error::root_offset};
    if (h.allocator_offset >= h.used_bytes) return {Error::allocator_offset};
    if (limits.max_wire_bytes < wire_header_bytes ||
        h.used_bytes > limits.max_wire_bytes - wire_header_bytes)
        return {Error::max_wire};
    if (h.used_bytes > limits.owner_capacity) return {Error::owner_capacity};
    // F with explicit MaximumCapacity deliberately ignores reserved_bytes as
    // a capacity request. Historical reserved_bytes > used_bytes remains legal.
    return {Error::none, wire_header_bytes + static_cast<std::size_t>(h.used_bytes)};
}

class ResourceLedger {
public:
    explicit ResourceLedger(Limits limits) : limits_(limits) {}
    const Limits& limits() const noexcept { return limits_; }
    std::size_t inflight_bytes() const noexcept { return inflight_bytes_; }
    std::size_t queue_frames() const noexcept { return queue_frames_; }
    std::size_t peak_inflight_bytes() const noexcept { return peak_inflight_bytes_; }
    std::size_t peak_queue_frames() const noexcept { return peak_queue_frames_; }

    bool acquire_queue() noexcept {
        if (queue_frames_ >= limits_.max_queue_frames) return false;
        ++queue_frames_;
        peak_queue_frames_ = std::max(peak_queue_frames_, queue_frames_);
        return true;
    }
    // Accounting is logical owning wire bytes. String terminators, allocator
    // metadata, fixed 88-byte header scratch and C++ context storage are outside
    // this wire budget; these counters are not heap/RSS instrumentation.
    bool acquire_wire(std::size_t bytes) noexcept {
        if (inflight_bytes_ > limits_.max_inflight_bytes ||
            bytes > limits_.max_inflight_bytes - inflight_bytes_) return false;
        inflight_bytes_ += bytes;
        peak_inflight_bytes_ = std::max(peak_inflight_bytes_, inflight_bytes_);
        return true;
    }
    void release_queue() noexcept { if (queue_frames_ != 0) --queue_frames_; }
    void release_wire(std::size_t bytes) noexcept { inflight_bytes_ -= bytes; }

private:
    Limits limits_;
    std::size_t inflight_bytes_ = 0, queue_frames_ = 0;
    std::size_t peak_inflight_bytes_ = 0, peak_queue_frames_ = 0;
};

enum class Progress { bytes, would_block, zero, eof, error, timeout, cancelled };
enum class ReceiveStage { header, payload, frame_ready, verifying, cancel_pending, terminal };

class ReceiveModel {
public:
    ReceiveModel(ResourceLedger& ledger, std::uint64_t type, std::uint64_t schema)
        : ledger_(ledger), type_(type), schema_(schema) {
        slot_ = ledger_.acquire_queue();
        if (!slot_) finish(Error::queue_budget);
    }
    ReceiveModel(const ReceiveModel&) = delete;
    ReceiveModel& operator=(const ReceiveModel&) = delete;
    // An adapter must retain this context until outstanding completion arrives.
    ~ReceiveModel() { release(); }

    std::uint64_t begin_read() noexcept {
        if (pending_ || terminal_ || (stage_ != ReceiveStage::header &&
                                     stage_ != ReceiveStage::payload)) return 0;
        pending_ = true;
        return active_token_ = ++next_token_;
    }
    std::size_t read_capacity() const noexcept {
        if (terminal_ || cancel_requested_) return 0;
        if (stage_ == ReceiveStage::header) return wire_header_bytes - header_progress_;
        if (stage_ == ReceiveStage::payload) return wire_.size() - wire_progress_;
        return 0;
    }

    void complete_read(std::uint64_t token, Progress progress,
                       std::span<const char> bytes = {}) {
        if (!pending_ || token != active_token_) { ++late_completions_; return; }
        pending_ = false;
        if (cancel_requested_ || progress == Progress::cancelled) {
            finish(Error::cancelled); return;
        }
        if (progress == Progress::would_block) { ++would_blocks_; return; }
        if (progress == Progress::zero) { finish(Error::zero_progress); return; }
        if (progress == Progress::eof) {
            finish(stage_ == ReceiveStage::header ? Error::eof_header : Error::eof_payload);
            return;
        }
        if (progress == Progress::error) { finish(Error::transport_error); return; }
        if (progress == Progress::timeout) { finish(Error::timeout); return; }
        if (bytes.empty()) { finish(Error::zero_progress); return; }
        if (bytes.size() > read_capacity()) { finish(Error::invalid_completion); return; }
        if (stage_ == ReceiveStage::header) {
            std::memcpy(header_bytes_ + header_progress_, bytes.data(), bytes.size());
            header_progress_ += bytes.size();
            transferred_ += bytes.size();
            if (header_progress_ == wire_header_bytes) accept_header();
        } else {
            std::memcpy(wire_.data() + wire_progress_, bytes.data(), bytes.size());
            wire_progress_ += bytes.size();
            transferred_ += bytes.size();
            if (wire_progress_ == wire_.size()) stage_ = ReceiveStage::frame_ready;
        }
    }

    // Consumes only the current header/body remainder. A caller with contiguous
    // frames advances by the returned amount, then supplies the unconsumed tail.
    std::size_t feed(std::span<const char> bytes) {
        const auto amount = std::min(bytes.size(), read_capacity());
        if (amount == 0) return 0;
        const auto token = begin_read();
        if (token == 0) return 0;
        complete_read(token, Progress::bytes, bytes.first(amount));
        return amount;
    }

    template<class Verifier, class BeforePublish>
    bool verify_and_publish(Verifier&& verifier, BeforePublish&& before_publish) {
        if (terminal_ || pending_ || stage_ != ReceiveStage::frame_ready) return false;
        stage_ = ReceiveStage::verifying;
        ++verification_attempts_;
        bool valid = false;
        try { valid = static_cast<bool>(std::invoke(verifier, std::as_const(wire_))); }
        catch (...) { finish(Error::verifier_exception); return false; }
        if (cancel_requested_) { finish(Error::cancelled); return false; }
        if (!valid) { finish(Error::invalid_graph); return false; }
        ++verified_;
        // Models a cancellation race at the single publication boundary.
        std::invoke(before_publish, *this);
        if (cancel_requested_) { finish(Error::cancelled); return false; }
        if (terminal_) return false;
        published_ = true;
        finish(Error::none);
        return true;
    }
    template<class Verifier>
    bool verify_and_publish(Verifier&& verifier) {
        return verify_and_publish(std::forward<Verifier>(verifier), [](ReceiveModel&) {});
    }

    void cancel() noexcept {
        if (terminal_) return;
        cancel_requested_ = true;
        if (stage_ == ReceiveStage::verifying) return;
        if (pending_) stage_ = ReceiveStage::cancel_pending;
        else finish(Error::cancelled);
    }
    void acknowledge_cancel(std::uint64_t token) {
        complete_read(token, Progress::cancelled);
    }
    ReceiveStage stage() const noexcept { return stage_; }
    bool terminal() const noexcept { return terminal_; }
    bool pending() const noexcept { return pending_; }
    bool published() const noexcept { return published_; }
    Error error() const noexcept { return error_; }
    std::size_t transferred() const noexcept { return transferred_; }
    // Successful full-frame owning-string materializations after admission,
    // not a global malloc counter. Header scratch itself never allocates.
    std::size_t allocations() const noexcept { return allocations_; }
    std::size_t held_wire_bytes() const noexcept { return reserved_wire_; }
    std::size_t buffer_bytes() const noexcept { return wire_.size(); }
    std::size_t terminal_count() const noexcept { return terminal_count_; }
    std::size_t late_completions() const noexcept { return late_completions_; }
    std::size_t would_blocks() const noexcept { return would_blocks_; }
    std::size_t verification_attempts() const noexcept { return verification_attempts_; }
    std::size_t verified() const noexcept { return verified_; }

private:
    void accept_header() {
        WireHeader header{};
        std::memcpy(&header, header_bytes_, wire_header_bytes);
        const auto admission = admit_header(header, ledger_.limits(), type_, schema_);
        if (!admission) { finish(admission.error); return; }
        if (!ledger_.acquire_wire(admission.wire_bytes)) {
            finish(Error::inflight_budget); return;
        }
        reserved_wire_ = admission.wire_bytes;
        try { wire_.resize(admission.wire_bytes); }
        catch (...) { finish(Error::allocation_failed); return; }
        ++allocations_;
        std::memcpy(wire_.data(), header_bytes_, wire_header_bytes);
        wire_progress_ = wire_header_bytes;
        stage_ = wire_progress_ == wire_.size()
            ? ReceiveStage::frame_ready : ReceiveStage::payload;
    }
    void release() noexcept {
        // Destroy the actual owning string before returning its budget.
        std::string().swap(wire_);
        if (reserved_wire_) { ledger_.release_wire(reserved_wire_); reserved_wire_ = 0; }
        if (slot_) { ledger_.release_queue(); slot_ = false; }
    }
    void finish(Error error) noexcept {
        if (terminal_) return;
        terminal_ = true;
        stage_ = ReceiveStage::terminal;
        error_ = error;
        ++terminal_count_;
        release();
    }
    ResourceLedger& ledger_;
    std::uint64_t type_, schema_, active_token_ = 0, next_token_ = 0;
    char header_bytes_[wire_header_bytes]{};
    std::string wire_;
    std::size_t header_progress_ = 0, wire_progress_ = 0, transferred_ = 0;
    std::size_t reserved_wire_ = 0, allocations_ = 0, terminal_count_ = 0;
    std::size_t late_completions_ = 0, would_blocks_ = 0;
    std::size_t verification_attempts_ = 0, verified_ = 0;
    ReceiveStage stage_ = ReceiveStage::header;
    Error error_ = Error::none;
    bool slot_ = false, pending_ = false, cancel_requested_ = false;
    bool terminal_ = false, published_ = false;
};

class SendModel {
public:
    SendModel(ResourceLedger& ledger, std::shared_ptr<const std::string> output)
        : ledger_(ledger), output_(std::move(output)) {
        slot_ = ledger_.acquire_queue();
        if (!slot_) { finish(Error::queue_budget); return; }
        if (!output_ || output_->empty()) { finish(Error::invalid_completion); return; }
        if (output_->size() > ledger_.limits().max_wire_bytes) {
            finish(Error::max_wire); return;
        }
        if (!ledger_.acquire_wire(output_->size())) { finish(Error::inflight_budget); return; }
        reserved_wire_ = output_->size();
        total_bytes_ = output_->size();
    }
    SendModel(const SendModel&) = delete;
    SendModel& operator=(const SendModel&) = delete;
    ~SendModel() { release(); }
    std::uint64_t begin_write() noexcept {
        if (terminal_ || pending_) return 0;
        pending_ = true;
        return active_token_ = ++next_token_;
    }
    std::string_view remaining_bytes() const noexcept {
        if (!output_) return {};
        return std::string_view(*output_).substr(offset_);
    }
    void complete_write(std::uint64_t token, Progress progress, std::size_t bytes = 0) {
        if (!pending_ || token != active_token_) { ++late_completions_; return; }
        pending_ = false;
        if (cancel_requested_ || progress == Progress::cancelled) {
            finish(Error::cancelled); return;
        }
        if (progress == Progress::would_block) { ++would_blocks_; return; }
        if (progress == Progress::zero || (progress == Progress::bytes && bytes == 0)) {
            finish(Error::zero_progress); return;
        }
        if (progress == Progress::timeout) { finish(Error::timeout); return; }
        if (progress == Progress::eof || progress == Progress::error) {
            finish(Error::transport_error); return;
        }
        if (bytes > remaining_bytes().size()) { finish(Error::invalid_completion); return; }
        offset_ += bytes;
        if (offset_ == total_bytes_) { completed_ = true; finish(Error::none); }
    }
    void cancel() noexcept {
        if (terminal_) return;
        cancel_requested_ = true;
        if (!pending_) finish(Error::cancelled);
    }
    void acknowledge_cancel(std::uint64_t token) {
        complete_write(token, Progress::cancelled);
    }
    bool terminal() const noexcept { return terminal_; }
    bool pending() const noexcept { return pending_; }
    bool completed() const noexcept { return completed_; }
    Error error() const noexcept { return error_; }
    std::size_t transferred() const noexcept { return offset_; }
    std::size_t total_bytes() const noexcept { return total_bytes_; }
    std::size_t held_wire_bytes() const noexcept { return reserved_wire_; }
    std::size_t terminal_count() const noexcept { return terminal_count_; }
    std::size_t late_completions() const noexcept { return late_completions_; }
    std::size_t would_blocks() const noexcept { return would_blocks_; }

private:
    void release() noexcept {
        output_.reset();
        if (reserved_wire_) { ledger_.release_wire(reserved_wire_); reserved_wire_ = 0; }
        if (slot_) { ledger_.release_queue(); slot_ = false; }
    }
    void finish(Error error) noexcept {
        if (terminal_) return;
        terminal_ = true;
        error_ = error;
        ++terminal_count_;
        release();
    }
    ResourceLedger& ledger_;
    std::shared_ptr<const std::string> output_;
    std::uint64_t active_token_ = 0, next_token_ = 0;
    std::size_t reserved_wire_ = 0, offset_ = 0, total_bytes_ = 0;
    std::size_t terminal_count_ = 0, late_completions_ = 0, would_blocks_ = 0;
    Error error_ = Error::none;
    bool slot_ = false, terminal_ = false, pending_ = false;
    bool cancel_requested_ = false, completed_ = false;
};

} // namespace vm_network
#endif
