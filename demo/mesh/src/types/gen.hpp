// Device mesh demo: state 0 and the mutations of the six types, written
// once for both models (Fam = XFamily or ModelFamily). OWNER: types thread.
//
// Rules (types.hpp): every random draw is a statement of its own; integer
// arithmetic only; a float or double is an integer divided by a power of
// two (exact); the same container calls in the same order on both models
// (reserve included: capacity is part of the wire). References into a
// container are not held across a call that changes that container.
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include "ops.hpp"
#include "xmesh/types.hpp"

namespace xmesh::gen {

using ops::Rng;

inline std::uint32_t pick(Scale sc, std::uint32_t small, std::uint32_t medium, std::uint32_t large) noexcept {
    return sc == Scale::Small ? small : sc == Scale::Medium ? medium : large;
}

template <class Fam, class Root>
void mark(Root& r, std::uint32_t hop, std::string_view device) {
    auto& m = ops::push(r.trail);
    m.hop = hop;
    ops::set(m.device, device);
}

// ---- Document ---------------------------------------------------------------------

inline std::string tag_name(Rng& r) {
    static constexpr std::string_view kTags[] = {"draft", "final", "legal", "team", "urgent", "q3", "review",
                                                 "数据", "café", "spec", "howto", "old"};
    const std::uint32_t t = r.below(sizeof kTags / sizeof kTags[0]);
    const std::uint32_t n = r.below(60);
    return std::string(kTags[t]) + "-" + std::to_string(n);
}

template <class Fam>
void doc_paragraph(typename Fam::Document& d, Rng& r) {
    auto& p = ops::push(d.paragraphs);
    p.id = d.next_paragraph_id++;
    p.style = ParaStyle(r.below(4));
    p.level = std::uint8_t(r.below(4));
    ops::set(p.text, ops::text(r, 0, 240));
}

template <class Fam>
void doc_comment(typename Fam::Document& d, Rng& r) {
    const std::uint64_t key = r.next() >> 40;
    auto& c = ops::get(d.comments, key);
    c.author = 1000 + r.below(500);
    c.time_ms = 1767225600000 + std::int64_t(r.below(1000000000));
    ops::set(c.text, ops::text(r, 4, 120));
    const std::uint32_t at = r.below(std::uint32_t(d.paragraphs.size()));
    c.paragraph_id = d.paragraphs.size() ? d.paragraphs[at].id : 0;
}

template <class Fam>
void build_document(typename Fam::Document& d, Rng& r, Scale sc) {
    const std::uint32_t paras = pick(sc, 10, 380, 6000);
    const std::uint32_t tags = pick(sc, 3, 24, 120);
    const std::uint32_t comments = pick(sc, 2, 40, 600);
    d.doc_id = r.next();
    d.published = r.chance(1, 2);
    ops::set(d.title, "Document " + ops::hex(d.doc_id & 0xffffff, 6) + ": " + ops::text(r, 4, 40));
    d.paragraphs.reserve(paras);
    for (std::uint32_t i = 0; i < paras; ++i) doc_paragraph<Fam>(d, r);
    for (std::uint32_t i = 0; i < tags; ++i) ops::add(d.tags, tag_name(r));
    for (std::uint32_t i = 0; i < comments; ++i) doc_comment<Fam>(d, r);
}

template <class Fam>
void mutate_document(typename Fam::Document& d, Rng& r) {
    d.revision += 1;
    const std::uint32_t steps = 3 + r.below(3);
    for (std::uint32_t s = 0; s < steps; ++s) {
        const std::uint32_t op = r.below(11);
        const std::uint32_t n = std::uint32_t(d.paragraphs.size());
        if (op <= 2 && n) {                       // rewrite a paragraph: short (inline) or long
            const std::uint32_t i = r.below(n);
            const bool shorter = r.chance(1, 4);
            ops::set(d.paragraphs[i].text, shorter ? ops::text(r, 0, 8) : ops::text(r, 20, 320));
        } else if (op == 3 || (op <= 2 && !n)) {   // new paragraph at the end
            doc_paragraph<Fam>(d, r);
        } else if (op == 4 && n > 1) {             // delete one from the middle
            ops::erase_at(d.paragraphs, r.below(n));
        } else if (op == 5 && n) {                 // restyle
            const std::uint32_t i = r.below(n);
            d.paragraphs[i].style = ParaStyle(r.below(4));
            d.paragraphs[i].level = std::uint8_t(r.below(4));
        } else if (op == 6) {
            ops::add(d.tags, tag_name(r));
        } else if (op == 7 && d.tags.size()) {
            const std::string t = ops::key_of(*ops::nth(d.tags, r.below(std::uint32_t(d.tags.size()))));
            ops::remove(d.tags, t);
        } else if (op == 8) {
            doc_comment<Fam>(d, r);
        } else if (op == 9 && d.comments.size()) {
            const std::uint64_t k = ops::nth(d.comments, r.below(std::uint32_t(d.comments.size())))->first;
            ops::erase_key(d.comments, k);
        } else if (op == 10) {
            d.published = !d.published;
        }
    }
}

// ---- Order --------------------------------------------------------------------------

template <class Fam>
void order_line(typename Fam::Order& o, Rng& r) {
    auto& l = ops::push(o.lines);
    const std::uint32_t sku = r.below(1000000);
    ops::set(l.sku, "SKU-" + ops::dec(sku, 6));
    l.qty = std::int32_t(r.range(1, 20));
    l.unit_price_cents = r.range(99, 99999);
    ops::set(l.title, ops::text(r, 5, 60));
    l.discount_bp = std::uint32_t(r.below(4) == 0 ? r.below(3000) : 0);
}

template <class Fam>
void order_coupon(typename Fam::Order& o, Rng& r) {
    auto& c = ops::push(o.coupon);
    ops::set(c.code, "SAVE" + std::to_string(5 + r.below(30)));
    c.percent_off = 5 + r.below(30);
    c.max_off_cents = r.range(500, 5000);
}

template <class Order>
void order_total(Order& o) {
    std::int64_t sum = 0;
    for (const auto& l : o.lines)
        sum += std::int64_t(l.qty) * l.unit_price_cents * (10000 - std::int64_t(l.discount_bp)) / 10000;
    if (o.coupon.size()) {
        const std::int64_t off = sum * std::int64_t(o.coupon[0].percent_off) / 100;
        sum -= std::min(off, o.coupon[0].max_off_cents);
    }
    o.total_cents = sum;
}

template <class Fam>
void order_street(typename Fam::Order& o, Rng& r) {
    const std::uint32_t no = 1 + r.below(999);
    // Short streets stay inline in the string head; long ones take a block.
    if (r.chance(1, 3)) ops::set(o.ship_to.street, std::to_string(no) + " Elm");
    else ops::set(o.ship_to.street, std::to_string(no) + " " + ops::text(r, 10, 40) + " Road");
}

template <class Fam>
void build_order(typename Fam::Order& o, Rng& r, Scale sc) {
    const std::uint32_t lines = pick(sc, 6, 800, 13000);
    for (auto& b : o.order_no) b = std::uint8_t(r.below(256));
    o.customer_id = r.next() >> 8;
    o.created_ms = 1767225600000 + std::int64_t(r.below(1000000000));
    o.status = OrderStatus::Created;
    o.gift = r.chance(1, 5);
    const std::int64_t lat = r.range(-90ll << 20, 90ll << 20);
    const std::int64_t lon = r.range(-180ll << 20, 180ll << 20);
    o.ship_to.geo.lat = double(lat) / double(1 << 20);
    o.ship_to.geo.lon = double(lon) / double(1 << 20);
    ops::set(o.ship_to.name, ops::text(r, 3, 24));
    const std::uint32_t phone = r.below(1000000000);
    ops::set(o.ship_to.phone, "+86 1" + ops::dec(phone, 10));
    order_street<Fam>(o, r);
    ops::set(o.ship_to.city, r.chance(1, 2) ? "Shenzhen" : "San Francisco");
    o.ship_to.postcode = r.below(1000000);
    o.ship_to.unit = r.below(3000);
    o.lines.reserve(lines);
    for (std::uint32_t i = 0; i < lines; ++i) order_line<Fam>(o, r);
    if (r.chance(1, 2)) order_coupon<Fam>(o, r);
    order_total(o);
}

template <class Fam>
void mutate_order(typename Fam::Order& o, Rng& r) {
    o.revision += 1;
    const std::uint32_t steps = 2 + r.below(3);
    for (std::uint32_t s = 0; s < steps; ++s) {
        const std::uint32_t op = r.below(8);
        const std::uint32_t n = std::uint32_t(o.lines.size());
        if (op <= 1 && n) {
            const std::uint32_t i = r.below(n);
            o.lines[i].qty = std::int32_t(r.range(1, 20));
        } else if (op == 2 || (op <= 1 && !n)) {
            order_line<Fam>(o, r);
        } else if (op == 3 && n > 1) {
            ops::erase_at(o.lines, r.below(n));
        } else if (op == 4) {
            order_street<Fam>(o, r);
        } else if (op == 5) {                       // the optional field: present <-> absent
            if (o.coupon.size()) ops::erase_at(o.coupon, 0);
            else order_coupon<Fam>(o, r);
        } else if (op == 6) {
            if (o.status != OrderStatus::Delivered && o.status != OrderStatus::Cancelled) {
                if (r.chance(1, 10)) o.status = OrderStatus::Cancelled;
                else o.status = OrderStatus(std::uint8_t(o.status) + 1);
            }
        } else {
            o.gift = !o.gift;
        }
    }
    order_total(o);
}

// ---- SensorSeries ---------------------------------------------------------------------

// Values are k / 16 with |k| <= 2^20, so every sum of up to 2^28 of them is
// exact in a double, in any order.
inline std::int64_t walk_step(Rng& r, std::int64_t k) {
    k += r.range(-64, 64);
    if (k > (1 << 20)) k = 1 << 20;
    if (k < -(1 << 20)) k = -(1 << 20);
    return k;
}

template <class Series>
void sensor_stats(Series& s) {
    double mn = 0, mx = 0, sum = 0;
    bool first = true;
    for (const auto& x : s.samples) {
        if (first || x.value < mn) mn = x.value;
        if (first || x.value > mx) mx = x.value;
        sum += x.value;
        first = false;
    }
    s.stats[0] = mn;
    s.stats[1] = mx;
    s.stats[2] = sum;
    s.stats[3] = double(s.samples.size());
}

inline Sample sensor_sample(Rng& r, std::int64_t t_ns, std::int64_t k) {
    Sample x;
    x.t_ns = t_ns;
    x.value = double(k) / 16.0;
    x.quality = r.below(101);
    x.channel = std::uint16_t(r.below(8));
    x.flags = std::uint8_t(r.below(4));
    return x;
}

template <class Fam>
void build_sensor(typename Fam::SensorSeries& s, Rng& r, Scale sc) {
    const std::uint32_t n = pick(sc, 100, 2700, 174000);
    const std::uint32_t cal = pick(sc, 8, 64, 256);
    const std::uint64_t serial = r.next() & 0xffffffff;
    ops::set(s.device, "sensor-" + ops::hex(serial, 8) + "-" + ops::text(r, 0, 20));
    s.unit = SensorUnit(r.below(4));
    s.period_ns = std::int64_t(1000000) * (1 + r.below(100));
    s.start_ns = 1767225600000000000ll + std::int64_t(r.below(1000000000));
    s.samples.reserve(n);
    std::int64_t k = r.range(-1000, 1000);
    for (std::uint32_t i = 0; i < n; ++i) {
        k = walk_step(r, k);
        s.samples.push_back(sensor_sample(r, s.start_ns + std::int64_t(i) * s.period_ns, k));
    }
    s.calibration.reserve(cal);
    for (std::uint32_t i = 0; i < cal; ++i) s.calibration.push_back(std::int16_t(std::int32_t(r.below(65536)) - 32768));
    sensor_stats(s);
}

// A rolling window: the oldest samples leave, as many new ones arrive, so
// the samples vector never grows past its capacity (no relocation of the
// big block, on any hop).
template <class Fam>
void mutate_sensor(typename Fam::SensorSeries& s, Rng& r) {
    const std::uint32_t n = std::uint32_t(s.samples.size());
    const std::uint32_t roll = std::min<std::uint32_t>(1 + r.below(8), n ? n - 1 : 0);
    if (roll) {
        const std::int64_t cut = s.samples[roll].t_ns;
        ops::erase_if(s.samples, [cut](const Sample& x) { return x.t_ns < cut; });
        std::int64_t k = s.samples.size() ? std::int64_t(s.samples.back().value * 16.0) : 0;
        std::int64_t t = s.samples.size() ? s.samples.back().t_ns : s.start_ns;
        for (std::uint32_t i = 0; i < roll; ++i) {
            k = walk_step(r, k);
            t += s.period_ns;
            s.samples.push_back(sensor_sample(r, t, k));
        }
        s.start_ns = s.samples[0].t_ns;
    }
    const std::uint32_t edits = 1 + r.below(4);
    for (std::uint32_t e = 0; e < edits && s.samples.size(); ++e) {
        const std::uint32_t i = r.below(std::uint32_t(s.samples.size()));
        s.samples[i].quality = r.below(101);
        s.samples[i].flags = std::uint8_t(s.samples[i].flags ^ 1u);
    }
    s.dropped += r.below(3);
    const std::uint32_t c = std::uint32_t(s.calibration.size());
    const std::uint32_t op = r.below(3);
    if (op == 0 && c) {
        s.calibration[r.below(c)] = std::int16_t(std::int32_t(r.below(65536)) - 32768);
    } else if (op == 1 && c < 512) {
        const std::uint32_t at = r.below(c + 1);
        ops::insert_at(s.calibration, at, std::int16_t(std::int32_t(r.below(65536)) - 32768));
    } else if (op == 2 && c > 1) {
        ops::erase_at(s.calibration, r.below(c));
    }
    sensor_stats(s);
}

// ---- KvConfig ------------------------------------------------------------------------

inline std::string kv_key(Rng& r) {
    static constexpr std::string_view kSections[] = {"net", "ui", "audio", "render", "sync", "store", "log", "i18n"};
    const std::uint32_t sec = r.below(sizeof kSections / sizeof kSections[0]);
    const std::uint32_t word = r.below(ops::kWordCount);
    const std::uint32_t n = r.below(10000);
    return std::string(kSections[sec]) + "." + std::string(ops::kWords[word]) + "-" + ops::dec(n, 4);
}

template <class Value>
void kv_value(Value& v, Rng& r) {
    v.kind = ValueKind(r.below(5));
    v.version += 1;
    v.i = 0;
    v.f = 0.0f;
    v.flag = false;
    ops::set(v.text, "");
    v.blob.clear();
    switch (v.kind) {
        case ValueKind::Int: v.i = std::int32_t(r.range(-1000000, 1000000)); break;
        case ValueKind::Real: v.f = float(r.range(-(1 << 23), 1 << 23)) / 256.0f; break;
        case ValueKind::Text: ops::set(v.text, ops::text(r, 0, 80)); break;
        case ValueKind::Blob: {
            std::vector<std::uint8_t> bytes(r.below(96));
            for (auto& b : bytes) b = std::uint8_t(r.below(256));
            ops::set_bytes(v.blob, bytes);
            break;
        }
        case ValueKind::Flag: v.flag = r.chance(1, 2); break;
    }
}

template <class Fam>
void build_kv(typename Fam::KvConfig& c, Rng& r, Scale sc) {
    const std::uint32_t entries = pick(sc, 14, 520, 8400);
    const std::uint32_t flags = pick(sc, 8, 100, 1000);
    const std::uint32_t levels = pick(sc, 3, 20, 60);
    c.config_id = r.next();
    for (std::uint32_t i = 0; i < entries; ++i) {
        const std::string key = kv_key(r);
        kv_value(ops::get(c.entries, key), r);
    }
    for (std::uint32_t i = 0; i < flags; ++i) ops::add(c.feature_flags, std::int32_t(r.range(-100000, 100000)));
    for (std::uint32_t i = 0; i < levels; ++i) ops::add(c.levels, std::int8_t(std::int32_t(r.below(256)) - 128));
}

template <class Fam>
void mutate_kv(typename Fam::KvConfig& c, Rng& r) {
    c.generation += 1;
    const std::uint32_t steps = 3 + r.below(3);
    for (std::uint32_t s = 0; s < steps; ++s) {
        const std::uint32_t op = r.below(7);
        const std::uint32_t n = std::uint32_t(c.entries.size());
        if (op <= 1 && n) {
            const std::string key = ops::key_of(ops::nth(c.entries, r.below(n))->first);
            kv_value(ops::get(c.entries, key), r);
        } else if (op == 2 || (op <= 1 && !n)) {
            const std::string key = kv_key(r);
            kv_value(ops::get(c.entries, key), r);
        } else if (op == 3 && n > 1) {
            const std::string key = ops::key_of(ops::nth(c.entries, r.below(n))->first);
            ops::erase_key(c.entries, key);
        } else if (op == 4) {
            ops::add(c.feature_flags, std::int32_t(r.range(-100000, 100000)));
        } else if (op == 5 && c.feature_flags.size()) {
            const std::int32_t f = *ops::nth(c.feature_flags, r.below(std::uint32_t(c.feature_flags.size())));
            ops::remove(c.feature_flags, f);
        } else {
            const std::int8_t l = std::int8_t(std::int32_t(r.below(256)) - 128);
            if (c.levels.size() && c.levels.find(l) != c.levels.end()) ops::remove(c.levels, l);
            else ops::add(c.levels, l);
        }
    }
}

// ---- SceneTree ------------------------------------------------------------------------

inline constexpr std::uint32_t kNoParent = 0xffffffffu;

template <class Node>
void node_transform(Node& n, Rng& r) {
    for (std::size_t i = 0; i < 16; ++i) n.local[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    for (std::size_t i = 12; i < 15; ++i) n.local[i] = float(r.range(-(1 << 16), 1 << 16)) / 64.0f;
}

// A new node as the last element, child of `parent`.
template <class Fam>
void scene_add(typename Fam::SceneTree& t, Rng& r, std::uint32_t parent) {
    const std::uint32_t index = std::uint32_t(t.nodes.size());
    const std::uint32_t id = t.next_id++;
    const std::string name = "node-" + ops::dec(id, 6);
    {
        auto& n = ops::push(t.nodes);
        n.id = id;
        n.parent = parent;
        node_transform(n, r);
        ops::set(n.name, name);
        n.kind = NodeKind(r.below(4));
        n.visible = !r.chance(1, 8);
        n.layer = std::uint16_t(r.below(16));
    }
    if (parent != kNoParent) t.nodes[parent].children.push_back(index);
    ops::get(t.by_name, name) = index;
}

template <class Fam>
void build_scene(typename Fam::SceneTree& t, Rng& r, Scale sc) {
    const std::uint32_t nodes = pick(sc, 10, 360, 6000);
    t.scene_id = r.next();
    t.root = 0;
    t.next_id = 1;
    t.nodes.reserve(nodes);
    scene_add<Fam>(t, r, kNoParent);
    for (std::uint32_t i = 1; i < nodes; ++i) {
        const std::uint32_t parent = r.below(i);
        scene_add<Fam>(t, r, parent);
    }
}

// Removes leaf `i` (not the root): every index above it moves down by one.
template <class Fam>
void scene_remove(typename Fam::SceneTree& t, std::uint32_t i) {
    const std::string name = ops::key_of(t.nodes[i].name);
    const std::uint32_t parent = t.nodes[i].parent;
    auto& siblings = t.nodes[parent].children;
    for (std::size_t k = 0; k < siblings.size(); ++k)
        if (siblings[k] == i) {
            ops::erase_at(siblings, k);
            break;
        }
    ops::erase_at(t.nodes, i);
    for (auto& n : t.nodes) {
        if (n.parent != kNoParent && n.parent > i) n.parent -= 1;
        for (auto& c : n.children)
            if (c > i) c -= 1;
    }
    ops::erase_key(t.by_name, name);
    for (auto& kv : t.by_name)
        if (kv.second > i) kv.second -= 1;
    if (t.root > i) t.root -= 1;
}

template <class Fam>
void mutate_scene(typename Fam::SceneTree& t, Rng& r) {
    const std::uint32_t steps = 2 + r.below(3);
    for (std::uint32_t s = 0; s < steps; ++s) {
        const std::uint32_t op = r.below(6);
        const std::uint32_t n = std::uint32_t(t.nodes.size());
        if (op == 0 || n < 2) {
            const std::uint32_t parent = r.below(n);
            scene_add<Fam>(t, r, n ? parent : kNoParent);
        } else if (op <= 2) {                      // remove a leaf
            const std::uint32_t start = 1 + r.below(n - 1);
            for (std::uint32_t k = 0; k < n - 1; ++k) {
                const std::uint32_t i = 1 + (start - 1 + k) % (n - 1);
                if (t.nodes[i].children.size() == 0) {
                    scene_remove<Fam>(t, i);
                    break;
                }
            }
        } else if (op == 3) {                      // rename: the name index follows
            const std::uint32_t i = r.below(n);
            const std::string old = ops::key_of(t.nodes[i].name);
            const std::string name = old.substr(0, old.find('~')) + "~" + std::to_string(r.below(100000));
            if (t.by_name.find(name) == t.by_name.end()) {
                ops::erase_key(t.by_name, old);
                ops::set(t.nodes[i].name, name);
                ops::get(t.by_name, name) = i;
            }
        } else if (op == 4) {
            node_transform(t.nodes[r.below(n)], r);
        } else {
            const std::uint32_t i = r.below(n);
            t.nodes[i].visible = !t.nodes[i].visible;
            t.nodes[i].layer = std::uint16_t(r.below(16));
        }
    }
}

// ---- ChatRoom ----------------------------------------------------------------------------

template <class Fam>
void chat_member(typename Fam::ChatRoom& c, Rng& r, std::uint64_t id, Role role) {
    auto& m = ops::get(c.members, id);
    ops::set(m.nick, ops::text(r, 2, 18));
    m.role = role;
    m.muted = r.chance(1, 10);
    m.joined_ms = 1767225600000 + std::int64_t(r.below(1000000000));
}

template <class Message>
void chat_attachment(Message& msg, Rng& r) {
    static constexpr std::string_view kMimes[] = {"image/png", "image/jpeg", "application/pdf", "text/plain",
                                                  "video/mp4"};
    auto& a = ops::push(msg.attachments);
    ops::set(a.name, "file-" + ops::dec(r.below(100000), 5) + ".bin");
    ops::set(a.mime, kMimes[r.below(5)]);
    a.size = r.next() >> 40;
    for (auto& b : a.sha256) b = std::uint8_t(r.below(256));
}

template <class Fam>
void chat_post(typename Fam::ChatRoom& c, Rng& r) {
    const std::uint64_t id = c.messages.size() ? c.messages.back().id + 1 : 1;
    const std::int64_t last = c.messages.size() ? c.messages.back().time_ms : 1767225600000;
    std::uint64_t from = 0;
    if (c.members.size()) from = ops::nth(c.members, r.below(std::uint32_t(c.members.size())))->first;
    auto& m = ops::push(c.messages);
    m.id = id;
    m.from = from;
    m.time_ms = last + std::int64_t(r.below(600000));
    ops::set(m.text, ops::text(r, 0, 280));
    if (r.chance(1, 10)) {
        const std::uint32_t k = 1 + r.below(2);
        for (std::uint32_t i = 0; i < k; ++i) chat_attachment(m, r);
    }
    if (r.chance(1, 10)) {
        ops::add(m.visible_to, Role::Moderator);
        ops::add(m.visible_to, Role::Owner);
    }
}

template <class Fam>
void build_chat(typename Fam::ChatRoom& c, Rng& r, Scale sc) {
    const std::uint32_t members = pick(sc, 4, 60, 800);
    const std::uint32_t messages = pick(sc, 8, 260, 4200);
    c.room_id = r.next();
    ops::set(c.topic, ops::text(r, 4, 60));
    for (std::uint32_t i = 0; i < members; ++i) {
        const Role role = i == 0 ? Role::Owner : (i % 17 == 1 ? Role::Moderator : (i % 23 == 2 ? Role::Bot : Role::Member));
        chat_member<Fam>(c, r, 1000 + std::uint64_t(i) * 7, role);
    }
    c.messages.reserve(messages);
    for (std::uint32_t i = 0; i < messages; ++i) chat_post<Fam>(c, r);
    for (std::uint32_t i = 0; i < members; i += 2) {
        const std::uint64_t who = 1000 + std::uint64_t(i) * 7;
        ops::get(c.read_upto, who) = 1 + r.below(messages);
    }
    const std::uint32_t pins = pick(sc, 1, 5, 20);
    for (std::uint32_t i = 0; i < pins; ++i) ops::add(c.pinned, std::uint64_t(1 + r.below(messages)));
}

template <class Fam>
void mutate_chat(typename Fam::ChatRoom& c, Rng& r) {
    const std::uint32_t steps = 3 + r.below(3);
    for (std::uint32_t s = 0; s < steps; ++s) {
        const std::uint32_t op = r.below(10);
        const std::uint32_t n = std::uint32_t(c.messages.size());
        if (op <= 1 || !n) {
            chat_post<Fam>(c, r);
        } else if (op == 3) {
            const std::uint32_t i = r.below(n);
            ops::set(c.messages[i].text, ops::text(r, 0, 280));
            c.messages[i].edits += 1;
        } else if ((op == 2 || op == 4) && n > 1) {
            const std::uint32_t i = r.below(n);
            const std::uint64_t id = c.messages[i].id;
            ops::erase_at(c.messages, i);
            ops::remove(c.pinned, id);
        } else if (op == 5) {
            const std::uint32_t i = r.below(n);
            if (c.messages[i].attachments.size() && r.chance(1, 2)) ops::erase_at(c.messages[i].attachments, 0);
            else chat_attachment(c.messages[i], r);
        } else if (op == 6) {                       // join, or leave
            if (c.members.size() > 2 && r.chance(1, 2)) {
                const std::uint64_t who = ops::nth(c.members, 1 + r.below(std::uint32_t(c.members.size()) - 1))->first;
                ops::erase_key(c.members, who);
                ops::erase_key(c.read_upto, who);
            } else {
                const std::uint64_t who = 100000 + (r.next() >> 44);
                chat_member<Fam>(c, r, who, Role::Member);
            }
        } else if (op == 7 && c.members.size()) {
            const std::uint64_t who = ops::nth(c.members, r.below(std::uint32_t(c.members.size())))->first;
            auto& m = ops::get(c.members, who);
            m.muted = !m.muted;
            ops::get(c.read_upto, who) = c.messages.back().id;
        } else if (op == 8) {
            const std::uint64_t id = c.messages[r.below(n)].id;
            if (c.pinned.find(id) != c.pinned.end()) ops::remove(c.pinned, id);
            else ops::add(c.pinned, id);
        } else {
            ops::set(c.topic, ops::text(r, 4, 60));
        }
    }
}

} // namespace xmesh::gen
