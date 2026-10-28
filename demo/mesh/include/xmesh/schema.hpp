// Device mesh demo: the wire schema every device and the hub share.
// FROZEN INTERFACE (demo/mesh/INTERFACES.md section 3). A change here
// changes every wire and every device's handshake; it goes through the
// design thread (「跨设备数据互传 demo」) first.
//
// Six data roots, one per common shape of application data (not a game):
// Document, Order, SensorSeries, KvConfig, SceneTree, ChatRoom. Together
// they use every container the library has (XString short and long,
// XVector of scalars and of records, XSet of strings, integers and enums,
// XMap with integer and string keys), std::array, nested records, bool,
// float and double. Five control roots carry the protocol (protocol.hpp):
// PeerInfo, Welcome, Roster, HopMeta, HopReport.
//
// Portable scalars only (USER_GUIDE 2.2): fixed-width integers, float,
// double, bool and enums with a written underlying type. No long, size_t,
// long double, wchar_t, plain char keys or enums without an underlying
// type. Every record is laid out without implicit padding (checked below);
// a reserved field fills a gap where one would remain. No union or
// variant exists, so KvConfig::Value is a tag plus one field per kind, and
// an optional field is an XVector of length 0 or 1 (Order::coupon).
//
// Every data root ends its history in `trail`: one HopMark per mutation,
// naming the device that made it, so a received object shows the devices
// it went through.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "xoffsetdatastructure.hpp"

#define XMESH_SCHEMA_VERSION 1

namespace xmesh {
using namespace XOffsetDatastructure;

// One mutation of an object: the route position that made it and that
// device's name (protocol.hpp HopMeta::route).
struct HopMark {
    std::uint32_t hop{};
    XString device;
};

// ---- Document: a collaborative text document --------------------------------
enum class ParaStyle : std::uint8_t { Body = 0, Heading = 1, Quote = 2, Code = 3 };

struct Paragraph {
    std::uint32_t id{};
    ParaStyle style{};
    std::uint8_t level{};          // heading level or indent
    std::uint16_t reserved0{};     // 0
    XString text;
};

struct Comment {
    std::uint64_t author{};
    std::int64_t time_ms{};
    XString text;
    std::uint32_t paragraph_id{};
};

struct Document {
    std::uint64_t doc_id{};
    std::uint32_t revision{};
    std::uint32_t next_paragraph_id{};
    bool published{};
    std::uint8_t reserved0{};      // 0
    std::uint16_t reserved1{};     // 0
    XString title;
    XVector<Paragraph> paragraphs;
    XSet<XString> tags;
    XMap<std::uint64_t, Comment> comments;   // comment id -> comment
    XVector<HopMark> trail;
};

// ---- Order: an e-commerce order -----------------------------------------------
enum class OrderStatus : std::uint8_t { Created = 0, Paid = 1, Shipped = 2, Delivered = 3, Cancelled = 4 };

struct GeoPoint {
    double lat{};
    double lon{};
};

struct Address {
    GeoPoint geo;
    XString name;
    XString phone;
    XString street;
    XString city;
    std::uint32_t postcode{};
    std::uint32_t unit{};
};

struct LineItem {
    XString sku;
    std::int32_t qty{};
    std::int64_t unit_price_cents{};
    XString title;
    std::uint32_t discount_bp{};   // basis points
};

struct Coupon {
    XString code;
    std::uint32_t percent_off{};
    std::int64_t max_off_cents{};
};

struct Order {
    std::array<std::uint8_t, 16> order_no{};
    std::uint64_t customer_id{};
    std::int64_t created_ms{};
    OrderStatus status{};
    bool gift{};
    std::uint16_t reserved0{};     // 0
    std::uint32_t revision{};
    Address ship_to;
    std::int64_t total_cents{};
    XVector<LineItem> lines;
    XVector<Coupon> coupon;        // optional: 0 or 1 element
    XVector<HopMark> trail;
    std::uint32_t edit_count{};    // == trail size
};

// ---- SensorSeries: a time series (the large one) ----------------------------
enum class SensorUnit : std::uint32_t { Celsius = 0, Pascal = 1, Volt = 2, Lux = 3 };

struct Sample {
    std::int64_t t_ns{};
    double value{};
    std::uint32_t quality{};
    std::uint16_t channel{};
    std::uint8_t flags{};
    std::uint8_t reserved0{};      // 0
};

struct SensorSeries {
    XString device;
    SensorUnit unit{};
    std::int64_t start_ns{};
    std::int64_t period_ns{};
    XVector<Sample> samples;
    std::uint32_t dropped{};
    std::array<double, 4> stats{};   // min, max, sum, count of samples[].value
    XVector<std::int16_t> calibration;
    XVector<HopMark> trail;
};

// ---- KvConfig: typed key-value configuration ---------------------------------
enum class ValueKind : std::uint8_t { Int = 0, Real = 1, Text = 2, Blob = 3, Flag = 4 };

// 4-aligned on purpose: an XMap element is {key, value}, and an 8-aligned
// value after a 12-byte XString key would leave 4 padding bytes.
struct Value {
    ValueKind kind{};
    bool flag{};
    std::uint16_t reserved0{};     // 0
    std::int32_t i{};
    float f{};
    std::uint32_t version{};
    XString text;
    XVector<std::uint8_t> blob;
};

struct KvConfig {
    std::uint64_t config_id{};
    std::uint32_t generation{};
    std::uint32_t edit_count{};    // == trail size
    XMap<XString, Value> entries;
    XSet<std::int32_t> feature_flags;
    XSet<std::int8_t> levels;
    XVector<HopMark> trail;
};

// ---- SceneTree: a tree (or graph) by index, no pointers ----------------------
enum class NodeKind : std::uint8_t { Group = 0, Mesh = 1, Light = 2, Camera = 3 };

struct Node {
    std::uint32_t id{};
    std::uint32_t parent{};        // index into SceneTree::nodes; 0xffffffff for the root
    std::array<float, 16> local{}; // column-major 4x4 transform
    XString name;
    XVector<std::uint32_t> children;   // indices into SceneTree::nodes
    NodeKind kind{};
    bool visible{};
    std::uint16_t layer{};
};

struct SceneTree {
    std::uint64_t scene_id{};
    std::uint32_t root{};          // index of the root node
    std::uint32_t next_id{};
    XVector<Node> nodes;
    XMap<XString, std::uint32_t> by_name;   // node name -> index
    XVector<HopMark> trail;
    std::uint32_t edit_count{};    // == trail size
};

// ---- ChatRoom: a chat room with members and messages --------------------------
enum class Role : std::uint8_t { Member = 0, Moderator = 1, Owner = 2, Bot = 3 };

struct Member {
    XString nick;
    Role role{};
    bool muted{};
    std::uint16_t reserved0{};     // 0
    std::int64_t joined_ms{};
};

struct Attachment {
    XString name;
    XString mime;
    std::uint64_t size{};
    std::array<std::uint8_t, 32> sha256{};
};

struct Message {
    std::uint64_t id{};
    std::uint64_t from{};
    std::int64_t time_ms{};
    XString text;
    std::uint32_t edits{};
    XVector<Attachment> attachments;
    XSet<Role> visible_to;         // empty: everyone
};

struct ChatRoom {
    std::uint64_t room_id{};
    XString topic;
    std::uint32_t edit_count{};    // == trail size
    XMap<std::uint64_t, Member> members;     // user id -> member
    XVector<Message> messages;
    XMap<std::uint64_t, std::uint64_t> read_upto;   // user id -> last read message id
    XSet<std::uint64_t> pinned;    // message ids
    XVector<HopMark> trail;
    std::uint32_t reserved0{};     // 0
};

// ---- control roots (protocol.hpp) -------------------------------------------

// Hello meta: who the device is.
struct PeerInfo {
    XString name;                  // unique in the mesh, e.g. "win-msvc-noexc"
    XString build;                 // build_info() line
    std::uint32_t flags{};         // kPeerExceptions, kPeerEvil
    std::uint32_t max_frame{};     // largest data part it accepts, bytes
};

// HelloAck meta: the id the hub gave the device.
struct Welcome {
    std::uint32_t device_id{};
    std::uint32_t reserved0{};     // 0
    XString hub_build;
    XString run_label;
};

struct RosterEntry {
    std::uint32_t id{};
    std::uint32_t flags{};         // PeerInfo::flags
    XString name;
    XString build;
};

// Roster meta: every device in the mesh, sent on every join and leave.
struct Roster {
    std::uint64_t version{};
    XVector<RosterEntry> devices;
    std::uint32_t reserved0{};     // 0
};

struct RouteStop {
    std::uint32_t device_id{};
    XString name;
};

// Job and Hop meta. An object travels route[0] -> route[1] -> ... ->
// route[n]. route[0] makes state 0 (no mutation; its trail is
// HopMark{0, route[0].name}) and sends it with hop 0;
// route[k], 0 < k < n, receives hop k-1, checks it, applies mutation k
// (HopMark{k, route[k].name}) and sends hop k; route[n] checks hop n-1 and
// ends the job. A Job message is a HopMeta with hop 0 and both digests 0.
struct HopMeta {
    std::uint64_t job{};
    std::uint64_t seed{};
    std::uint64_t content_digest{};  // sender's content digest of the data part
    std::uint64_t wire_digest{};     // sender's wire_digest of the data part
    std::uint32_t hop{};             // route position of the sender
    std::uint16_t type{};            // TypeId (types.hpp)
    std::uint8_t scale{};            // Scale
    std::uint8_t save_mode{};        // SaveMode
    std::uint8_t evil{};             // 0, or the EvilCase the sender applied (protocol.hpp)
    std::uint8_t reserved0{};        // 0
    std::uint16_t reserved1{};       // 0
    XVector<RouteStop> route;
};

// Report meta: what a device did with one received Hop (or with a Job, as
// route[0]). Verdict values: protocol.hpp HopVerdict.
struct HopReport {
    std::uint64_t job{};
    std::uint64_t content_digest{};  // receiver's content digest of what it received (route[0]: of state 0)
    std::uint64_t sent_wire_digest{};  // wire_digest of the data part it sent on; 0 when it ended the job
    std::uint32_t hop{};             // hop of the received message (route[0]: 0)
    std::uint32_t data_bytes{};      // received data part
    std::uint32_t sent_bytes{};      // data part it sent on; 0 when it ended the job
    std::uint8_t verdict{};          // HopVerdict
    std::uint8_t expected_reject{};  // 1 when the received message carried an EvilCase
    std::uint16_t reserved0{};       // 0
    std::uint64_t verify_ns{};       // load_verified (route[0]: 0)
    std::uint64_t check_ns{};        // reference-model comparison
    std::uint64_t mutate_ns{};       // the mutation (route[0]: building state 0)
    std::uint64_t save_ns{};         // save_verified or save_compact
    XString error;                   // first failure, empty when verdict is ok
    std::uint32_t grown_bytes{};     // arena growth caused by the mutation
};

} // namespace xmesh

// Field lists, in declaration order: the C++20 device builds read the schema
// from these lines; the C++26 hub and Linux peers check them against
// reflection, so both backends compute the same wire_schema_hash_v (the
// Hello handshake compares it).
XOFFSET_RECORD(xmesh::HopMark, hop, device)
XOFFSET_RECORD(xmesh::Paragraph, id, style, level, reserved0, text)
XOFFSET_RECORD(xmesh::Comment, author, time_ms, text, paragraph_id)
XOFFSET_RECORD(xmesh::Document, doc_id, revision, next_paragraph_id, published, reserved0, reserved1, title, paragraphs, tags, comments, trail)
XOFFSET_RECORD(xmesh::GeoPoint, lat, lon)
XOFFSET_RECORD(xmesh::Address, geo, name, phone, street, city, postcode, unit)
XOFFSET_RECORD(xmesh::LineItem, sku, qty, unit_price_cents, title, discount_bp)
XOFFSET_RECORD(xmesh::Coupon, code, percent_off, max_off_cents)
XOFFSET_RECORD(xmesh::Order, order_no, customer_id, created_ms, status, gift, reserved0, revision, ship_to, total_cents, lines, coupon, trail, edit_count)
XOFFSET_RECORD(xmesh::Sample, t_ns, value, quality, channel, flags, reserved0)
XOFFSET_RECORD(xmesh::SensorSeries, device, unit, start_ns, period_ns, samples, dropped, stats, calibration, trail)
XOFFSET_RECORD(xmesh::Value, kind, flag, reserved0, i, f, version, text, blob)
XOFFSET_RECORD(xmesh::KvConfig, config_id, generation, edit_count, entries, feature_flags, levels, trail)
XOFFSET_RECORD(xmesh::Node, id, parent, local, name, children, kind, visible, layer)
XOFFSET_RECORD(xmesh::SceneTree, scene_id, root, next_id, nodes, by_name, trail, edit_count)
XOFFSET_RECORD(xmesh::Member, nick, role, muted, reserved0, joined_ms)
XOFFSET_RECORD(xmesh::Attachment, name, mime, size, sha256)
XOFFSET_RECORD(xmesh::Message, id, from, time_ms, text, edits, attachments, visible_to)
XOFFSET_RECORD(xmesh::ChatRoom, room_id, topic, edit_count, members, messages, read_upto, pinned, trail, reserved0)
XOFFSET_RECORD(xmesh::PeerInfo, name, build, flags, max_frame)
XOFFSET_RECORD(xmesh::Welcome, device_id, reserved0, hub_build, run_label)
XOFFSET_RECORD(xmesh::RosterEntry, id, flags, name, build)
XOFFSET_RECORD(xmesh::Roster, version, devices, reserved0)
XOFFSET_RECORD(xmesh::RouteStop, device_id, name)
XOFFSET_RECORD(xmesh::HopMeta, job, seed, content_digest, wire_digest, hop, type, scale, save_mode, evil, reserved0, reserved1, route)
XOFFSET_RECORD(xmesh::HopReport, job, content_digest, sent_wire_digest, hop, data_bytes, sent_bytes, verdict, expected_reject, reserved0, verify_ns, check_ns, mutate_ns, save_ns, error, grown_bytes)

XOFFSET_REGISTER_SCHEMA_NAME(xmesh::Document, "xmesh.Document.v1")
XOFFSET_REGISTER_SCHEMA_NAME(xmesh::Order, "xmesh.Order.v1")
XOFFSET_REGISTER_SCHEMA_NAME(xmesh::SensorSeries, "xmesh.SensorSeries.v1")
XOFFSET_REGISTER_SCHEMA_NAME(xmesh::KvConfig, "xmesh.KvConfig.v1")
XOFFSET_REGISTER_SCHEMA_NAME(xmesh::SceneTree, "xmesh.SceneTree.v1")
XOFFSET_REGISTER_SCHEMA_NAME(xmesh::ChatRoom, "xmesh.ChatRoom.v1")
XOFFSET_REGISTER_SCHEMA_NAME(xmesh::PeerInfo, "xmesh.PeerInfo.v1")
XOFFSET_REGISTER_SCHEMA_NAME(xmesh::Welcome, "xmesh.Welcome.v1")
XOFFSET_REGISTER_SCHEMA_NAME(xmesh::Roster, "xmesh.Roster.v1")
XOFFSET_REGISTER_SCHEMA_NAME(xmesh::HopMeta, "xmesh.HopMeta.v1")
XOFFSET_REGISTER_SCHEMA_NAME(xmesh::HopReport, "xmesh.HopReport.v1")

XOFFSET_REQUIRE_NO_PADDING(xmesh::Document);
XOFFSET_REQUIRE_NO_PADDING(xmesh::Order);
XOFFSET_REQUIRE_NO_PADDING(xmesh::SensorSeries);
XOFFSET_REQUIRE_NO_PADDING(xmesh::KvConfig);
XOFFSET_REQUIRE_NO_PADDING(xmesh::SceneTree);
XOFFSET_REQUIRE_NO_PADDING(xmesh::ChatRoom);
XOFFSET_REQUIRE_NO_PADDING(xmesh::PeerInfo);
XOFFSET_REQUIRE_NO_PADDING(xmesh::Welcome);
XOFFSET_REQUIRE_NO_PADDING(xmesh::Roster);
XOFFSET_REQUIRE_NO_PADDING(xmesh::HopMeta);
XOFFSET_REQUIRE_NO_PADDING(xmesh::HopReport);
