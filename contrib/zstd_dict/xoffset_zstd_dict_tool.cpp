// xoffset_zstd_dict: train a dictionary from server samples, print a
// dictionary file, or measure sizes and times on test samples.
//
//   xoffset_zstd_dict train --id N --out FILE [--size BYTES] [--level L]
//                           [--binding auto|none|HEX32] INPUT...
//   xoffset_zstd_dict info FILE
//   xoffset_zstd_dict measure --dict FILE [--batch N,N,...] [--level L] [--time] INPUT...
//
// INPUT: a file (one message), a directory (every regular file in it, by
// name, one message each), or records:FILE (records of [u64 little-endian
// length][bytes], as slg_bench's SLG_DUMP_DIR hook writes them).
// --binding auto (default): every sample must be an XOffset 2.1 wire of one
// schema, and the dictionary is bound to it. none: bound to nothing.
// Exit 0 on success, 1 when a round trip differs, 2 on usage or input errors.
#include "xoffset_zstd_dict.hpp"

#include <zstd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace zd = xoffset_zstd_dict;
namespace fs = std::filesystem;

namespace {

int usage() {
    std::fputs("usage:\n"
               "  xoffset_zstd_dict train --id N --out FILE [--size BYTES] [--level L] [--binding auto|none|HEX32] INPUT...\n"
               "  xoffset_zstd_dict info FILE\n"
               "  xoffset_zstd_dict measure --dict FILE [--batch N,N,...] [--level L] [--time] INPUT...\n"
               "INPUT: FILE | DIR | records:FILE\n",
               stderr);
    return 2;
}

bool read_file(const std::string& path, std::string& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    out.clear();
    char buf[65536];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    return ok;
}

bool write_file(const std::string& path, const std::string& data) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    return std::fclose(f) == 0 && ok;
}

bool load_inputs(const std::vector<std::string>& inputs, std::vector<std::string>& out) {
    for (const std::string& in : inputs) {
        if (in.rfind("records:", 0) == 0) {
            std::string data;
            if (!read_file(in.substr(8), data)) {
                std::fprintf(stderr, "error: cannot read %s\n", in.c_str() + 8);
                return false;
            }
            for (std::size_t i = 0; i < data.size();) {
                std::uint64_t n = 0;
                if (data.size() - i < 8) {
                    std::fprintf(stderr, "error: %s: truncated record header\n", in.c_str());
                    return false;
                }
                for (int b = 0; b < 8; ++b) n |= std::uint64_t(static_cast<unsigned char>(data[i + b])) << (8 * b);
                i += 8;
                if (n > data.size() - i) {
                    std::fprintf(stderr, "error: %s: truncated record\n", in.c_str());
                    return false;
                }
                out.push_back(data.substr(i, n));
                i += n;
            }
            continue;
        }
        std::error_code ec;
        if (fs::is_directory(in, ec)) {
            std::vector<std::string> names;
            for (const auto& e : fs::directory_iterator(in, ec))
                if (e.is_regular_file()) names.push_back(e.path().string());
            std::sort(names.begin(), names.end());
            for (const std::string& n : names) {
                out.emplace_back();
                if (!read_file(n, out.back())) {
                    std::fprintf(stderr, "error: cannot read %s\n", n.c_str());
                    return false;
                }
            }
            continue;
        }
        out.emplace_back();
        if (!read_file(in, out.back())) {
            std::fprintf(stderr, "error: cannot read %s\n", in.c_str());
            return false;
        }
    }
    if (out.empty()) {
        std::fputs("error: no samples\n", stderr);
        return false;
    }
    return true;
}

bool parse_hex_binding(const std::string& s, zd::Binding& b) {
    if (s.size() != 32) return false;
    for (std::size_t i = 0; i < 16; ++i) {
        char* end = nullptr;
        const std::string byte = s.substr(2 * i, 2);
        const unsigned long v = std::strtoul(byte.c_str(), &end, 16);
        if (end != byte.c_str() + 2) return false;
        b[i] = static_cast<std::uint8_t>(v);
    }
    return true;
}

std::vector<std::string_view> views(const std::vector<std::string>& v) {
    return std::vector<std::string_view>(v.begin(), v.end());
}

int cmd_train(int argc, char** argv) {
    zd::TrainOptions opt;
    std::string out_path, binding = "auto";
    std::vector<std::string> inputs;
    bool have_id = false;
    for (int i = 0; i < argc; ++i) {
        const std::string a = argv[i];
        const bool more = i + 1 < argc;
        if (a == "--id" && more) { opt.id = std::uint32_t(std::strtoul(argv[++i], nullptr, 10)); have_id = true; }
        else if (a == "--out" && more) out_path = argv[++i];
        else if (a == "--size" && more) opt.dict_bytes = std::size_t(std::strtoull(argv[++i], nullptr, 10));
        else if (a == "--level" && more) opt.level = std::atoi(argv[++i]);
        else if (a == "--binding" && more) binding = argv[++i];
        else if (a.rfind("--", 0) == 0) return usage();
        else inputs.push_back(a);
    }
    if (!have_id || out_path.empty() || inputs.empty()) return usage();
    std::vector<std::string> samples;
    if (!load_inputs(inputs, samples)) return 2;
    if (binding == "auto") {
        if (!zd::wire_binding(samples[0], opt.binding)) {
            std::fputs("error: sample 0 is not an XOffset 2.1 wire (use --binding none or HEX32)\n", stderr);
            return 2;
        }
        for (std::size_t i = 1; i < samples.size(); ++i) {
            zd::Binding b{};
            if (!zd::wire_binding(samples[i], b) || b != opt.binding) {
                std::fprintf(stderr, "error: sample %zu is not a wire of the same schema as sample 0\n", i);
                return 2;
            }
        }
    } else if (binding == "none") {
        opt.binding = zd::kUnbound;
    } else if (!parse_hex_binding(binding, opt.binding)) {
        std::fputs("error: --binding takes auto, none or 32 hex digits\n", stderr);
        return 2;
    }
    zd::Dictionary dict;
    if (zd::Status s = zd::train(views(samples), opt, dict); !s) {
        std::fprintf(stderr, "error: %s\n", s.message.c_str());
        return 2;
    }
    if (!write_file(out_path, dict.file_bytes())) {
        std::fprintf(stderr, "error: cannot write %s\n", out_path.c_str());
        return 2;
    }
    std::printf("dictionary %u: %zu bytes from %zu samples, binding %s -> %s\n", dict.id(), dict.content().size(),
                samples.size(), zd::binding_hex(dict.binding()).c_str(), out_path.c_str());
    return 0;
}

bool load_dict(const std::string& path, zd::Dictionary& dict) {
    std::string file;
    if (!read_file(path, file)) {
        std::fprintf(stderr, "error: cannot read %s\n", path.c_str());
        return false;
    }
    if (zd::Status s = zd::Dictionary::parse(file, dict); !s) {
        std::fprintf(stderr, "error: %s: %s\n", path.c_str(), s.message.c_str());
        return false;
    }
    return true;
}

int cmd_info(int argc, char** argv) {
    if (argc != 1) return usage();
    zd::Dictionary dict;
    if (!load_dict(argv[0], dict)) return 2;
    std::printf("id %u\ndictionary_bytes %zu\nbinding %s\n", dict.id(), dict.content().size(),
                zd::binding_hex(dict.binding()).c_str());
    return 0;
}

// Best of 5 rounds of at least 20 ms, in microseconds per call.
template <class F>
double time_us(std::size_t calls, F&& f) {
    using clock = std::chrono::steady_clock;
    double best = 1e300;
    for (int round = 0; round < 5; ++round) {
        std::size_t reps = 0;
        const auto t0 = clock::now();
        double elapsed = 0;
        do {
            f();
            ++reps;
            elapsed = std::chrono::duration<double, std::micro>(clock::now() - t0).count();
        } while (elapsed < 20000.0);
        best = std::min(best, elapsed / double(reps * calls));
    }
    return best;
}

int cmd_measure(int argc, char** argv) {
    std::string dict_path;
    std::vector<std::size_t> batches{1, 8, 32};
    int level = 3;
    bool timing = false;
    std::vector<std::string> inputs;
    for (int i = 0; i < argc; ++i) {
        const std::string a = argv[i];
        const bool more = i + 1 < argc;
        if (a == "--dict" && more) dict_path = argv[++i];
        else if (a == "--level" && more) level = std::atoi(argv[++i]);
        else if (a == "--time") timing = true;
        else if (a == "--batch" && more) {
            batches.clear();
            std::string list = argv[++i];
            for (std::size_t p = 0; p < list.size();) {
                const std::size_t q = std::min(list.find(',', p), list.size());
                const std::size_t n = std::size_t(std::strtoull(list.substr(p, q - p).c_str(), nullptr, 10));
                if (n == 0) return usage();
                batches.push_back(n);
                p = q + 1;
            }
        } else if (a.rfind("--", 0) == 0) return usage();
        else inputs.push_back(a);
    }
    if (dict_path.empty() || inputs.empty()) return usage();
    zd::Dictionary dict;
    if (!load_dict(dict_path, dict)) return 2;
    std::vector<std::string> msgs;
    if (!load_inputs(inputs, msgs)) return 2;
    zd::Encoder enc;
    zd::Decoder dec;
    if (zd::Status s = enc.set_dictionary(dict, level); !s) { std::fprintf(stderr, "error: %s\n", s.message.c_str()); return 2; }
    if (zd::Status s = dec.set_dictionary(dict); !s) { std::fprintf(stderr, "error: %s\n", s.message.c_str()); return 2; }

    std::size_t raw = 0, plain = 0, lean = 0, max_size = 0;
    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    std::string frame, back;
    int bad = 0;
    for (const std::string& m : msgs) {
        raw += m.size();
        max_size = std::max(max_size, m.size());
        frame.resize(ZSTD_compressBound(m.size()));
        plain += ZSTD_compressCCtx(cctx, frame.data(), frame.size(), m.data(), m.size(), level);
        if (zd::Status s = enc.encode(m, frame); !s) { std::fprintf(stderr, "error: %s\n", s.message.c_str()); return 2; }
        lean += frame.size();
        if (zd::Status s = dec.decode(frame, m.size(), back); !s || back != m) ++bad;
    }
    const double n = double(msgs.size());
    std::printf("messages %zu  dictionary %u (%zu B)  level %d\n", msgs.size(), dict.id(), dict.content().size(), level);
    std::printf("%-28s %10s\n", "encoding", "bytes/msg");
    std::printf("%-28s %10.1f\n", "raw", double(raw) / n);
    std::printf("%-28s %10.1f\n", "zstd, no dictionary", double(plain) / n);
    std::printf("%-28s %10.1f\n", "dictionary, lean frame", double(lean) / n);
    std::string storage;
    std::vector<std::string_view> out;
    for (std::size_t b : batches) {
        if (b == 1) continue;
        std::size_t total = 0, total_plain = 0, groups = 0;
        std::string packed;
        for (std::size_t i = 0; i + b <= msgs.size(); i += b) {
            std::vector<std::string_view> group(msgs.begin() + std::ptrdiff_t(i), msgs.begin() + std::ptrdiff_t(i + b));
            if (zd::Status s = enc.encode_batch(group, frame); !s) { std::fprintf(stderr, "error: %s\n", s.message.c_str()); return 2; }
            total += frame.size();
            zd::pack_batch(group, packed);
            std::string pf(ZSTD_compressBound(packed.size()), '\0');
            total_plain += ZSTD_compressCCtx(cctx, pf.data(), pf.size(), packed.data(), packed.size(), level);
            if (zd::Status s = dec.decode_batch(frame, packed.size(), b, max_size, storage, out); !s || out != group) ++bad;
            ++groups;
        }
        if (groups == 0) continue;
        const double per = double(groups * b);
        std::printf("%-28s %10.1f\n", ("batch " + std::to_string(b) + ", no dictionary").c_str(), double(total_plain) / per);
        std::printf("%-28s %10.1f\n", ("batch " + std::to_string(b) + ", dictionary").c_str(), double(total) / per);
    }
    if (timing) {
        std::vector<std::string> frames(msgs.size());
        for (std::size_t i = 0; i < msgs.size(); ++i) (void)enc.encode(msgs[i], frames[i]);
        const double e = time_us(msgs.size(), [&] { for (const auto& m : msgs) (void)enc.encode(m, frame); });
        const double d = time_us(msgs.size(), [&] { for (const auto& f : frames) (void)dec.decode(f, max_size, back); });
        std::printf("time us/msg: dictionary encode %.2f decode %.2f\n", e, d);
    }
    ZSTD_freeCCtx(cctx);
    if (bad != 0) {
        std::fprintf(stderr, "error: %d round trips differ\n", bad);
        return 1;
    }
    return 0;
}

}   // namespace

int main(int argc, char** argv) {
    if (argc < 2) return usage();
    const std::string cmd = argv[1];
    if (cmd == "train") return cmd_train(argc - 2, argv + 2);
    if (cmd == "info") return cmd_info(argc - 2, argv + 2);
    if (cmd == "measure") return cmd_measure(argc - 2, argv + 2);
    return usage();
}
