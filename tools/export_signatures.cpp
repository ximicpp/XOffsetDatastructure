// Exports the signature baselines for the sample root/data types.

#include "signature_catalog.hpp"

#include <boost/typelayout/tools/sig_export.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

constexpr const char kStableGeneratedLine[] = "// Generated: committed baseline";

int normalize_signature_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        std::cerr << "Error: cannot reopen " << path << " for normalization\n";
        return 1;
    }

    std::string contents((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
    const std::string marker = "// Generated: ";
    const std::size_t begin = contents.find(marker);
    if (begin == std::string::npos) {
        return 0;
    }

    const std::size_t end = contents.find('\n', begin);
    contents.replace(begin, end - begin, kStableGeneratedLine);

    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        std::cerr << "Error: cannot rewrite " << path << " after normalization\n";
        return 1;
    }

    output << contents;
    return output.good() ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[]) {
    ::boost::typelayout::SigExporter ex;
    xoffset::signature::add_exported_types(ex);

    if (argc >= 2) {
        std::string dir = argv[1];
        std::filesystem::create_directories(dir);
        std::string path = dir;
        if (path.back() != '/') path += '/';
        path += ex.platform_name() + ".sig.hpp";
        const int write_status = ex.write(path);
        if (write_status != 0) {
            return write_status;
        }
        return normalize_signature_file(path);
    }
    ex.write_stdout();
    return 0;
}
