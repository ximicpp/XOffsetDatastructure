// xoffset_zstd_dict: dictionary training (needs zstd's lib/dictBuilder).
#define ZDICT_STATIC_LINKING_ONLY   // ZDICT_optimizeTrainFromBuffer_fastCover
#include "xoffset_zstd_dict.hpp"

#include <zdict.h>

namespace xoffset_zstd_dict {

Status train(const std::vector<std::string_view>& samples, const TrainOptions& options, Dictionary& out) {
    if (options.id == 0 || options.id > kMaxDictId)
        return Status{Errc::invalid_argument, "train: id " + std::to_string(options.id) + " outside 1..2^31-1"};
    if (options.dict_bytes < 256 || options.dict_bytes > kMaxDictBytes)
        return Status{Errc::invalid_argument, "train: dictionary size " + std::to_string(options.dict_bytes) +
                                                  " outside 256..1 MiB"};
    if (samples.empty()) return Status{Errc::invalid_argument, "train: no samples"};
    std::string flat;
    std::vector<std::size_t> sizes;
    sizes.reserve(samples.size());
    // A bound dictionary is trained on what its frames carry: the wires
    // without the header bytes the binding fixes.
    const bool bound = options.binding != kUnbound;
    std::string elided;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        std::string_view s = samples[i];
        if (bound) {
            if (Status e = elide_wire(s, options.binding, elided); !e)
                return Status{e.code, "train: sample " + std::to_string(i) + ": " + e.message};
            s = elided;
        }
        flat.append(s.data(), s.size());
        sizes.push_back(s.size());
    }
    // ZDICT_trainFromBuffer's parameters (fastCover d=8, f=20, steps=4,
    // accel=1, one thread), plus the id and the target level.
    ZDICT_fastCover_params_t p{};
    p.d = 8;
    p.f = 20;
    p.steps = 4;
    p.accel = 1;
    p.nbThreads = 1;
    p.zParams.compressionLevel = options.level;
    p.zParams.dictID = options.id;
    p.zParams.notificationLevel = 0;
    std::string dict(options.dict_bytes, '\0');
    const std::size_t n = ZDICT_optimizeTrainFromBuffer_fastCover(dict.data(), dict.size(), flat.data(), sizes.data(),
                                                                  static_cast<unsigned>(sizes.size()), &p);
    if (ZDICT_isError(n))
        return Status{Errc::zstd_error, std::string("train: ") + ZDICT_getErrorName(n) + " (" +
                                            std::to_string(samples.size()) + " samples)"};
    dict.resize(n);
    return Dictionary::from_zstd(options.id, options.binding, dict, out);
}

} // namespace xoffset_zstd_dict
