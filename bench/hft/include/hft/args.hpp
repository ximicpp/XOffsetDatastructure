// HFT demo: command line, as `--key value` pairs.
#pragma once
#include <cstdlib>
#include <map>
#include <string>

namespace hft {

struct Args {
    std::map<std::string, std::string> kv;
    std::string get(const std::string& k, const std::string& def) const {
        auto it = kv.find(k);
        return it == kv.end() ? def : it->second;
    }
    double num(const std::string& k, double def) const {
        auto it = kv.find(k);
        return it == kv.end() ? def : std::atof(it->second.c_str());
    }
};

}  // namespace hft
