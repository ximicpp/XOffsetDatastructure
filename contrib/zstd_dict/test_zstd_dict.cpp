// xoffset_zstd_dict unit test: dictionary file, lean frames, batches and
// every refusal. Synthetic samples shaped like small XOffset wires (40-byte
// 2.1 header, a record, a name), so it needs no XOffset build.
#include "xoffset_zstd_dict.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace zd = xoffset_zstd_dict;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}
void expect(const zd::Status& s, zd::Errc want, const char* what) {
    if (s.code != want) {
        std::printf("FAIL %s: got %s (%s), want %s\n", what, zd::errc_name(s.code), s.message.c_str(), zd::errc_name(want));
        ++failures;
    }
}

std::uint64_t rng_state = 0x9e3779b97f4a7c15ull;
std::uint32_t rnd() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return static_cast<std::uint32_t>(rng_state);
}

const zd::Binding kSchemaA = zd::make_binding(0x1111222233334444ull, 0xaaaabbbbccccddddull);
const zd::Binding kSchemaB = zd::make_binding(0x1111222233334444ull, 0xaaaabbbbccccdddeull);

std::string sample(std::uint32_t i, const zd::Binding& binding = kSchemaA) {
    std::string w(40, '\0');
    w[0] = 'X'; w[1] = 'O'; w[2] = 2; w[3] = 1;
    const std::uint32_t used = 120 + (i % 7) * 8;
    std::memcpy(&w[8], &used, 4);
    std::memcpy(&w[24], binding.data(), 16);
    char rec[96];
    const int n = std::snprintf(rec, sizeof rec, "unit=%05u hp=%03u behavior=%u name=U%05u-route-marker-%02u tick=%06u",
                                i * 100, 50 + rnd() % 51, rnd() % 3, i * 100, i % 100, (i / 7) * 20);
    w.append(rec, std::size_t(n));
    for (std::uint32_t k = 0; k < 4 + i % 3; ++k) {
        const std::uint32_t xy[2] = {rnd() % 4096, rnd() % 4096};
        w.append(reinterpret_cast<const char*>(xy), sizeof xy);
    }
    return w;
}

}   // namespace

int main() {
    std::vector<std::string> train_set, test_set;
    for (std::uint32_t i = 0; i < 2000; ++i) train_set.push_back(sample(i));
    for (std::uint32_t i = 5000; i < 5200; ++i) test_set.push_back(sample(i));
    std::vector<std::string_view> train_views(train_set.begin(), train_set.end());

    // ---- training and the file format ----
    zd::Dictionary d1, d2;
    zd::TrainOptions opt;
    opt.id = 1;
    opt.binding = kSchemaA;
    opt.dict_bytes = 4096;
    expect(zd::train(train_views, opt, d1), zd::Errc::ok, "train id 1");
    opt.id = 2;
    expect(zd::train(train_views, opt, d2), zd::Errc::ok, "train id 2");
    check(d1.id() == 1 && d2.id() == 2, "ids");
    check(!d1.content().empty() && d1.content().size() <= 4096, "dictionary size");
    {
        zd::Dictionary again;
        opt.id = 1;
        expect(zd::train(train_views, opt, again), zd::Errc::ok, "train again");
        check(again.file_bytes() == d1.file_bytes(), "training is deterministic");
    }
    opt.id = 0;
    zd::Dictionary none;
    expect(zd::train(train_views, opt, none), zd::Errc::invalid_argument, "train id 0");
    expect(zd::train({}, zd::TrainOptions{}, none), zd::Errc::invalid_argument, "train without samples");

    const std::string file = d1.file_bytes();
    {
        zd::Dictionary p;
        expect(zd::Dictionary::parse(file, p), zd::Errc::ok, "parse");
        check(p.id() == 1 && p.binding() == kSchemaA && p.content() == d1.content(), "parse round trip");
        expect(p.check_binding(kSchemaA), zd::Errc::ok, "binding A");
        expect(p.check_binding(kSchemaB), zd::Errc::binding_mismatch, "binding B refused");
        expect(p.check_binding(zd::kUnbound), zd::Errc::binding_mismatch, "unbound refused");
        zd::Binding b{};
        check(zd::wire_binding(test_set[0], b) && b == kSchemaA, "wire_binding");
        check(!zd::wire_binding("XO\x02\x02" + test_set[0].substr(4), b), "wire_binding refuses another wire version");
    }
    {
        // Every corruption of the file is refused.
        auto refused = [&](std::string f, const char* what) {
            zd::Dictionary p;
            expect(zd::Dictionary::parse(f, p), zd::Errc::bad_dict_file, what);
        };
        refused(file.substr(0, 47), "file shorter than the header");
        refused(file.substr(0, file.size() - 1), "file truncated");
        refused(file + "x", "file with a trailing byte");
        std::string f = file; f[0] ^= 1; refused(f, "file magic");
        f = file; f[4] = 2; refused(f, "file format");
        f = file; f[6] = 40; refused(f, "file header size");
        f = file; f[16] ^= 1; refused(f, "file binding changed (checksum)");
        f = file; f[100] ^= 1; refused(f, "file dictionary byte changed (checksum)");
        f = file; f[40] = 1; refused(f, "file reserved bytes");
        // Id rewritten in the header only (checksum fixed up): the zstd
        // dictionary's own id disagrees.
        zd::Dictionary relabel;
        expect(zd::Dictionary::from_zstd(3, kSchemaA, d1.content(), relabel), zd::Errc::bad_dict_file,
               "from_zstd with a different id");
        expect(zd::Dictionary::from_zstd(1, kSchemaA, "not a dictionary", relabel), zd::Errc::bad_dict_file,
               "from_zstd with garbage");
    }

    // ---- single messages ----
    zd::Encoder enc1, enc2;
    zd::Decoder dec1, dec2;
    expect(enc1.set_dictionary(d1), zd::Errc::ok, "encoder 1");
    expect(enc2.set_dictionary(d2), zd::Errc::ok, "encoder 2");
    expect(dec1.set_dictionary(d1), zd::Errc::ok, "decoder 1");
    expect(dec2.set_dictionary(d2), zd::Errc::ok, "decoder 2");
    std::size_t raw = 0, packed = 0;
    std::string frame, back;
    for (const std::string& m : test_set) {
        expect(enc1.encode(m, frame), zd::Errc::ok, "encode");
        raw += m.size();
        packed += frame.size();
        zd::FrameInfo info;
        expect(zd::peek_frame(frame, info), zd::Errc::ok, "peek");
        check(info.dict_id == 1 && info.content_size == m.size() - zd::kElidedBytes && info.frame_bytes == frame.size() &&
                  info.header_bytes == 3,
              "frame header: id 1, elided size, 3 header bytes");
        expect(dec1.decode(frame, m.size(), back), zd::Errc::ok, "decode");
        check(back == m, "round trip");
    }
    std::printf("single: %zu messages, %.1f -> %.1f bytes\n", test_set.size(), double(raw) / test_set.size(),
                double(packed) / test_set.size());
    check(packed * 2 < raw, "dictionary halves the samples");

    const std::string& m0 = test_set[0];
    expect(enc1.encode(m0, frame), zd::Errc::ok, "encode m0");
    expect(dec2.decode(frame, 1 << 20, back), zd::Errc::dict_id_mismatch, "version mismatch refused");
    check(back.empty(), "nothing produced on mismatch");
    expect(dec1.decode(frame, m0.size() - 1, back), zd::Errc::too_large, "over the cap refused");
    expect(dec1.decode(frame, m0.size(), back), zd::Errc::ok, "exactly the cap");
    check(back == m0, "exactly the cap round trip");
    expect(enc1.encode(sample(1, kSchemaB), back), zd::Errc::binding_mismatch, "encoder refuses another schema");
    expect(enc1.encode("plain bytes, not an XOffset wire, long enough to have 40 bytes", back), zd::Errc::binding_mismatch,
           "encoder refuses a non-wire");
    {
        std::string w = m0;
        w[5] = 1;   // flags
        expect(enc1.encode(w, back), zd::Errc::binding_mismatch, "encoder refuses nonzero flags");
        opt.id = 9;
        opt.binding = kSchemaB;
        zd::Dictionary wrong;
        expect(zd::train(train_views, opt, wrong), zd::Errc::binding_mismatch, "training refuses samples of another schema");
        opt.binding = kSchemaA;
    }
    {
        // An unbound dictionary carries any bytes unchanged.
        std::vector<std::string> text;
        for (std::uint32_t i = 0; i < 1000; ++i) text.push_back("event " + std::to_string(i % 37) + " player lord_" +
                                                                 std::to_string(i * 7 % 1000) + " gold " + std::to_string(i * 13));
        std::vector<std::string_view> tv(text.begin(), text.end());
        zd::TrainOptions u;
        u.id = 5;
        u.dict_bytes = 2048;
        zd::Dictionary du;
        expect(zd::train(tv, u, du), zd::Errc::ok, "train unbound");
        zd::Encoder eu;
        zd::Decoder dd;
        expect(eu.set_dictionary(du), zd::Errc::ok, "unbound encoder");
        expect(dd.set_dictionary(du), zd::Errc::ok, "unbound decoder");
        std::string f2, b2;
        expect(eu.encode(text[3], f2), zd::Errc::ok, "unbound encode");
        expect(dd.decode(f2, 4096, b2), zd::Errc::ok, "unbound decode");
        check(b2 == text[3], "unbound round trip");
        std::vector<std::string_view> grp(tv.begin(), tv.begin() + 10), got;
        std::string st;
        expect(eu.encode_batch(grp, f2), zd::Errc::ok, "unbound batch");
        expect(dd.decode_batch(f2, 4096, 10, 4096, st, got), zd::Errc::ok, "unbound batch decode");
        check(got == grp, "unbound batch round trip");
        expect(dec1.decode(f2, 4096, b2), zd::Errc::dict_id_mismatch, "dictionary 5 frame refused by dictionary 1");
    }
    expect(dec1.decode(frame.substr(0, frame.size() - 1), 1 << 20, back), zd::Errc::bad_frame, "truncated frame");
    expect(dec1.decode(frame.substr(0, 2), 1 << 20, back), zd::Errc::bad_frame, "truncated header");
    expect(dec1.decode(frame + '\0', 1 << 20, back), zd::Errc::bad_frame, "trailing byte");
    expect(dec1.decode(frame + frame, 1 << 20, back), zd::Errc::bad_frame, "two frames");
    expect(dec1.decode({}, 1 << 20, back), zd::Errc::bad_frame, "empty input");
    {
        // A frame header without the dictionary id: flags byte bits 0-1 = 0.
        std::string f = frame;
        const unsigned char fhd = static_cast<unsigned char>(f[0]);
        check((fhd & 3) == 1, "frame has a 1-byte id field");
        f[0] = static_cast<char>(fhd & ~3u);
        f.erase(1, 1);
        expect(dec1.decode(f, 1 << 20, back), zd::Errc::bad_frame, "frame without id");
        // Without the content size: single-segment off, size flag 0, so a
        // window byte takes its place.
        f = frame;
        check((fhd & 0xe0) == 0x20, "frame is single segment with a 1-byte size");
        f[0] = static_cast<char>(fhd & ~0xe0u);
        f.erase(2, 1);           // the size byte
        f.insert(1, 1, '\0');    // a window descriptor (1 KiB) before the id
        expect(dec1.decode(f, 1 << 20, back), zd::Errc::bad_frame, "frame without content size");
        // Checksum flag set.
        f = frame;
        f[0] = static_cast<char>(fhd | 0x04);
        expect(dec1.decode(f, 1 << 20, back), zd::Errc::bad_frame, "frame with checksum flag");
        // A standard frame (with magic) is not a lean frame.
        std::string std_frame = std::string("\x28\xb5\x2f\xfd", 4) + frame;
        expect(dec1.decode(std_frame, 1 << 20, back), zd::Errc::bad_frame, "standard frame with magic");
        // Reserved block type.
        f = frame;
        f[3] = static_cast<char>(static_cast<unsigned char>(f[3]) | 0x06);
        expect(dec1.decode(f, 1 << 20, back), zd::Errc::bad_frame, "reserved block type");
        // Declared size larger than the blocks produce.
        f = frame;
        f[2] = static_cast<char>(static_cast<unsigned char>(f[2]) + 1);
        const zd::Status s = dec1.decode(f, 1 << 20, back);
        check(!s.ok() && back.empty(), "declared size above the content is refused");
        // Every single-byte change: refused, or decodes to some bytes the
        // receiver verifies; never a crash, never more than the cap.
        for (std::size_t i = 0; i < frame.size(); ++i) {
            for (int bit = 0; bit < 8; ++bit) {
                f = frame;
                f[i] = static_cast<char>(static_cast<unsigned char>(f[i]) ^ (1u << bit));
                const zd::Status r = dec1.decode(f, 4096, back);
                if (r.ok()) check(back.size() <= 4096, "bit flip stays within the cap");
                else check(back.empty(), "refused bit flip produces nothing");
            }
        }
    }
    {
        zd::Decoder empty;
        expect(empty.decode(frame, 1 << 20, back), zd::Errc::invalid_argument, "decoder without dictionary");
        zd::Encoder empty_enc;
        expect(empty_enc.encode(m0, frame), zd::Errc::invalid_argument, "encoder without dictionary");
    }

    // ---- batches ----
    std::vector<std::string_view> group(test_set.begin(), test_set.begin() + 32);
    std::string storage;
    std::vector<std::string_view> msgs;
    expect(enc1.encode_batch(group, frame), zd::Errc::ok, "encode batch");
    std::size_t group_raw = 0;
    for (auto g : group) group_raw += g.size();
    std::printf("batch 32: %.1f bytes per message\n", double(frame.size()) / 32);
    expect(dec1.decode_batch(frame, 1 << 20, 64, 4096, storage, msgs), zd::Errc::ok, "decode batch");
    check(msgs == group, "batch round trip");
    expect(dec2.decode_batch(frame, 1 << 20, 64, 4096, storage, msgs), zd::Errc::dict_id_mismatch, "batch version mismatch");
    expect(dec1.decode_batch(frame, 1 << 20, 31, 4096, storage, msgs), zd::Errc::bad_batch, "batch over max count");
    expect(dec1.decode_batch(frame, 1 << 20, 64, 100, storage, msgs), zd::Errc::bad_batch, "batch message over cap_each");
    expect(dec1.decode_batch(frame, group_raw - 32 * zd::kElidedBytes, 64, 4096, storage, msgs), zd::Errc::too_large,
           "batch over cap_total");
    {
        std::vector<std::string_view> mixed(group);
        const std::string other = sample(3, kSchemaB);
        mixed[7] = other;
        expect(enc1.encode_batch(mixed, frame), zd::Errc::binding_mismatch, "batch with another schema refused");
        expect(enc1.encode_batch({}, frame), zd::Errc::invalid_argument, "empty batch refused");
    }
    {
        std::string content;
        zd::pack_batch(group, content);
        expect(zd::unpack_batch(content, 64, 4096, msgs), zd::Errc::ok, "unpack");
        check(msgs == group, "pack round trip");
        expect(zd::unpack_batch(content + "x", 64, 4096, msgs), zd::Errc::bad_batch, "unpack trailing byte");
        expect(zd::unpack_batch(content.substr(0, content.size() - 1), 64, 4096, msgs), zd::Errc::bad_batch,
               "unpack truncated");
        expect(zd::unpack_batch(std::string(1, '\0'), 64, 4096, msgs), zd::Errc::bad_batch, "unpack count 0");
        expect(zd::unpack_batch(std::string("\x80\x00", 2), 64, 4096, msgs), zd::Errc::bad_batch, "unpack non-minimal count");
        expect(zd::unpack_batch(std::string("\x01\x00", 2), 64, 4096, msgs), zd::Errc::bad_batch, "unpack zero length");
        expect(zd::unpack_batch(std::string("\x01\xff\xff\xff\xff\x7f", 6), 64, ~std::size_t(0), msgs), zd::Errc::bad_batch,
               "unpack length above 2^32");
        expect(zd::unpack_batch({}, 64, 4096, msgs), zd::Errc::bad_batch, "unpack empty");
        // Forged batch content inside a valid frame (an unbound encoder of
        // the same dictionary id writes it): count 2, a length past the bytes.
        zd::Dictionary raw1;
        expect(zd::Dictionary::from_zstd(1, zd::kUnbound, d1.content(), raw1), zd::Errc::ok, "unbound copy of 1");
        zd::Encoder forger;
        expect(forger.set_dictionary(raw1), zd::Errc::ok, "forger");
        std::string ff;
        expect(forger.encode(std::string("\x02\x05" "abcd", 6), ff), zd::Errc::ok, "encode forged batch");
        expect(dec1.decode_batch(ff, 1 << 20, 64, 4096, storage, msgs), zd::Errc::bad_batch, "forged batch lengths");
        // A message shorter than the kept header bytes.
        expect(forger.encode(std::string("\x01\x04" "abcd", 6), ff), zd::Errc::ok, "encode short batch");
        expect(dec1.decode_batch(ff, 1 << 20, 64, 4096, storage, msgs), zd::Errc::bad_batch, "batch message too short");
        expect(forger.encode(std::string("abcd", 4), ff), zd::Errc::ok, "encode short message");
        expect(dec1.decode(ff, 4096, back), zd::Errc::bad_frame, "message too short for a header");
    }
    {
        std::uint32_t v = 0;
        std::size_t pos = 0;
        check(zd::get_varint32(std::string("\xff\xff\xff\xff\x0f", 5), pos, v) && v == 0xffffffffu, "varint max");
        pos = 0;
        check(!zd::get_varint32(std::string("\xff\xff\xff\xff\x1f", 5), pos, v), "varint above 32 bits");
        pos = 0;
        check(!zd::get_varint32(std::string("\x80\x80\x80\x80\x80\x01", 6), pos, v), "varint too long");
        std::string s;
        zd::put_varint(300, s);
        pos = 0;
        check(zd::get_varint32(s, pos, v) && v == 300 && pos == 2, "varint 300");
    }

    if (failures == 0) std::printf("test_zstd_dict: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
