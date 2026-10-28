// Device mesh demo: the reference model (plain std:: types) and the field
// lists both models share. OWNER: types thread.
//
// Every record of schema.hpp that holds a container has a twin here with
// the same member names, XString -> std::string, XVector -> std::vector,
// XSet -> std::set, XMap -> std::map. Records without containers (GeoPoint,
// Sample) are shared. The field lists below drive the content digest, the
// field-by-field comparison and nothing else, so a field missing here is a
// field neither checks: each list names every member of its XOFFSET_RECORD
// line, in the same order.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "xmesh/schema.hpp"

namespace xmesh::model {

template <class T>
using Set = std::set<T, std::less<>>;

struct HopMark {
    std::uint32_t hop{};
    std::string device;
};

struct Paragraph {
    std::uint32_t id{};
    ParaStyle style{};
    std::uint8_t level{};
    std::uint16_t reserved0{};
    std::string text;
};

struct Comment {
    std::uint64_t author{};
    std::int64_t time_ms{};
    std::string text;
    std::uint32_t paragraph_id{};
};

struct Document {
    std::uint64_t doc_id{};
    std::uint32_t revision{};
    std::uint32_t next_paragraph_id{};
    bool published{};
    std::uint8_t reserved0{};
    std::uint16_t reserved1{};
    std::string title;
    std::vector<Paragraph> paragraphs;
    Set<std::string> tags;
    std::map<std::uint64_t, Comment> comments;
    std::vector<HopMark> trail;
};

struct Address {
    GeoPoint geo;
    std::string name;
    std::string phone;
    std::string street;
    std::string city;
    std::uint32_t postcode{};
    std::uint32_t unit{};
};

struct LineItem {
    std::string sku;
    std::int32_t qty{};
    std::int64_t unit_price_cents{};
    std::string title;
    std::uint32_t discount_bp{};
};

struct Coupon {
    std::string code;
    std::uint32_t percent_off{};
    std::int64_t max_off_cents{};
};

struct Order {
    std::array<std::uint8_t, 16> order_no{};
    std::uint64_t customer_id{};
    std::int64_t created_ms{};
    OrderStatus status{};
    bool gift{};
    std::uint16_t reserved0{};
    std::uint32_t revision{};
    Address ship_to;
    std::int64_t total_cents{};
    std::vector<LineItem> lines;
    std::vector<Coupon> coupon;
    std::vector<HopMark> trail;
    std::uint32_t edit_count{};
};

struct SensorSeries {
    std::string device;
    SensorUnit unit{};
    std::int64_t start_ns{};
    std::int64_t period_ns{};
    std::vector<Sample> samples;
    std::uint32_t dropped{};
    std::array<double, 4> stats{};
    std::vector<std::int16_t> calibration;
    std::vector<HopMark> trail;
};

struct Value {
    ValueKind kind{};
    bool flag{};
    std::uint16_t reserved0{};
    std::int32_t i{};
    float f{};
    std::uint32_t version{};
    std::string text;
    std::vector<std::uint8_t> blob;
};

struct KvConfig {
    std::uint64_t config_id{};
    std::uint32_t generation{};
    std::uint32_t edit_count{};
    std::map<std::string, Value, std::less<>> entries;
    Set<std::int32_t> feature_flags;
    Set<std::int8_t> levels;
    std::vector<HopMark> trail;
};

struct Node {
    std::uint32_t id{};
    std::uint32_t parent{};
    std::array<float, 16> local{};
    std::string name;
    std::vector<std::uint32_t> children;
    NodeKind kind{};
    bool visible{};
    std::uint16_t layer{};
};

struct SceneTree {
    std::uint64_t scene_id{};
    std::uint32_t root{};
    std::uint32_t next_id{};
    std::vector<Node> nodes;
    std::map<std::string, std::uint32_t, std::less<>> by_name;
    std::vector<HopMark> trail;
    std::uint32_t edit_count{};
};

struct Member {
    std::string nick;
    Role role{};
    bool muted{};
    std::uint16_t reserved0{};
    std::int64_t joined_ms{};
};

struct Attachment {
    std::string name;
    std::string mime;
    std::uint64_t size{};
    std::array<std::uint8_t, 32> sha256{};
};

struct Message {
    std::uint64_t id{};
    std::uint64_t from{};
    std::int64_t time_ms{};
    std::string text;
    std::uint32_t edits{};
    std::vector<Attachment> attachments;
    Set<Role> visible_to;
};

struct ChatRoom {
    std::uint64_t room_id{};
    std::string topic;
    std::uint32_t edit_count{};
    std::map<std::uint64_t, Member> members;
    std::vector<Message> messages;
    std::map<std::uint64_t, std::uint64_t> read_upto;
    Set<std::uint64_t> pinned;
    std::vector<HopMark> trail;
    std::uint32_t reserved0{};
};

} // namespace xmesh::model

namespace xmesh {

// The two families: the same member names, so one template builds and
// mutates either.
struct XFamily {
    using HopMark = xmesh::HopMark;
    using Document = xmesh::Document;
    using Order = xmesh::Order;
    using SensorSeries = xmesh::SensorSeries;
    using KvConfig = xmesh::KvConfig;
    using SceneTree = xmesh::SceneTree;
    using ChatRoom = xmesh::ChatRoom;
};
struct ModelFamily {
    using HopMark = model::HopMark;
    using Document = model::Document;
    using Order = model::Order;
    using SensorSeries = model::SensorSeries;
    using KvConfig = model::KvConfig;
    using SceneTree = model::SceneTree;
    using ChatRoom = model::ChatRoom;
};

// ---- field lists -------------------------------------------------------------
// fields(r, f) calls f(name, member) for every member; fields2(a, b, f)
// calls f(name, a.member, b.member) for an object and its model twin.

#define XMESH_F1(m) f(#m, r.m);
#define XMESH_F2(m) f(#m, a.m, b.m);
#define XMESH_TWIN_FIELDS(X, M, LIST)                                   \
    template <class F> void fields(const X& r, F& f) { LIST(XMESH_F1) } \
    template <class F> void fields(const M& r, F& f) { LIST(XMESH_F1) } \
    template <class F> void fields2(const X& a, const M& b, F& f) { LIST(XMESH_F2) }
#define XMESH_SHARED_FIELDS(X, LIST)                                    \
    template <class F> void fields(const X& r, F& f) { LIST(XMESH_F1) } \
    template <class F> void fields2(const X& a, const X& b, F& f) { LIST(XMESH_F2) }

#define XMESH_HOPMARK(F) F(hop) F(device)
#define XMESH_PARAGRAPH(F) F(id) F(style) F(level) F(reserved0) F(text)
#define XMESH_COMMENT(F) F(author) F(time_ms) F(text) F(paragraph_id)
#define XMESH_DOCUMENT(F) F(doc_id) F(revision) F(next_paragraph_id) F(published) F(reserved0) F(reserved1) \
    F(title) F(paragraphs) F(tags) F(comments) F(trail)
#define XMESH_GEOPOINT(F) F(lat) F(lon)
#define XMESH_ADDRESS(F) F(geo) F(name) F(phone) F(street) F(city) F(postcode) F(unit)
#define XMESH_LINEITEM(F) F(sku) F(qty) F(unit_price_cents) F(title) F(discount_bp)
#define XMESH_COUPON(F) F(code) F(percent_off) F(max_off_cents)
#define XMESH_ORDER(F) F(order_no) F(customer_id) F(created_ms) F(status) F(gift) F(reserved0) F(revision) \
    F(ship_to) F(total_cents) F(lines) F(coupon) F(trail) F(edit_count)
#define XMESH_SAMPLE(F) F(t_ns) F(value) F(quality) F(channel) F(flags) F(reserved0)
#define XMESH_SENSORSERIES(F) F(device) F(unit) F(start_ns) F(period_ns) F(samples) F(dropped) F(stats) \
    F(calibration) F(trail)
#define XMESH_VALUE(F) F(kind) F(flag) F(reserved0) F(i) F(f) F(version) F(text) F(blob)
#define XMESH_KVCONFIG(F) F(config_id) F(generation) F(edit_count) F(entries) F(feature_flags) F(levels) F(trail)
#define XMESH_NODE(F) F(id) F(parent) F(local) F(name) F(children) F(kind) F(visible) F(layer)
#define XMESH_SCENETREE(F) F(scene_id) F(root) F(next_id) F(nodes) F(by_name) F(trail) F(edit_count)
#define XMESH_MEMBER(F) F(nick) F(role) F(muted) F(reserved0) F(joined_ms)
#define XMESH_ATTACHMENT(F) F(name) F(mime) F(size) F(sha256)
#define XMESH_MESSAGE(F) F(id) F(from) F(time_ms) F(text) F(edits) F(attachments) F(visible_to)
#define XMESH_CHATROOM(F) F(room_id) F(topic) F(edit_count) F(members) F(messages) F(read_upto) F(pinned) \
    F(trail) F(reserved0)

XMESH_TWIN_FIELDS(xmesh::HopMark, model::HopMark, XMESH_HOPMARK)
XMESH_TWIN_FIELDS(xmesh::Paragraph, model::Paragraph, XMESH_PARAGRAPH)
XMESH_TWIN_FIELDS(xmesh::Comment, model::Comment, XMESH_COMMENT)
XMESH_TWIN_FIELDS(xmesh::Document, model::Document, XMESH_DOCUMENT)
XMESH_SHARED_FIELDS(xmesh::GeoPoint, XMESH_GEOPOINT)
XMESH_TWIN_FIELDS(xmesh::Address, model::Address, XMESH_ADDRESS)
XMESH_TWIN_FIELDS(xmesh::LineItem, model::LineItem, XMESH_LINEITEM)
XMESH_TWIN_FIELDS(xmesh::Coupon, model::Coupon, XMESH_COUPON)
XMESH_TWIN_FIELDS(xmesh::Order, model::Order, XMESH_ORDER)
XMESH_SHARED_FIELDS(xmesh::Sample, XMESH_SAMPLE)
XMESH_TWIN_FIELDS(xmesh::SensorSeries, model::SensorSeries, XMESH_SENSORSERIES)
XMESH_TWIN_FIELDS(xmesh::Value, model::Value, XMESH_VALUE)
XMESH_TWIN_FIELDS(xmesh::KvConfig, model::KvConfig, XMESH_KVCONFIG)
XMESH_TWIN_FIELDS(xmesh::Node, model::Node, XMESH_NODE)
XMESH_TWIN_FIELDS(xmesh::SceneTree, model::SceneTree, XMESH_SCENETREE)
XMESH_TWIN_FIELDS(xmesh::Member, model::Member, XMESH_MEMBER)
XMESH_TWIN_FIELDS(xmesh::Attachment, model::Attachment, XMESH_ATTACHMENT)
XMESH_TWIN_FIELDS(xmesh::Message, model::Message, XMESH_MESSAGE)
XMESH_TWIN_FIELDS(xmesh::ChatRoom, model::ChatRoom, XMESH_CHATROOM)

// The lists cover every member: a list missing one, or naming one twice,
// adds up to another size than the record (no record has padding).
namespace field_check {
struct Sum {
    std::size_t bytes = 0;
    template <class T>
    void operator()(const char*, const T&) { bytes += sizeof(T); }
};
template <class T>
std::size_t listed_bytes() {
    Sum s;
    fields(T{}, s);
    return s.bytes;
}
} // namespace field_check

} // namespace xmesh
