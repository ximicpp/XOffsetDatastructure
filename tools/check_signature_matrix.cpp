// Verifies that tools/sigs/ matches the exact repository target matrix.

#include "signature_target_matrix.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::vector<std::string> expected_filenames() {
    std::vector<std::string> names;
    names.reserve(xoffset::signature::target_matrix.size());
    for (const auto& target : xoffset::signature::target_matrix) {
        names.emplace_back(std::string(target.platform_name) + ".sig.hpp");
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> actual_filenames(const std::filesystem::path& dir) {
    std::vector<std::string> names;
    if (!std::filesystem::exists(dir)) {
        return names;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".hpp") continue;
        names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

template <typename Range>
void print_list(const char* header, const Range& values) {
    std::cerr << header << '\n';
    for (const auto& value : values) {
        std::cerr << "  - " << value << '\n';
    }
}

} // namespace

int main(int argc, char* argv[]) {
    const std::filesystem::path dir =
        argc >= 2 ? std::filesystem::path(argv[1]) : std::filesystem::path("tools/sigs");

    const auto expected = expected_filenames();
    const auto actual = actual_filenames(dir);

    std::vector<std::string> missing;
    std::vector<std::string> unexpected;
    std::set_difference(
        expected.begin(), expected.end(),
        actual.begin(), actual.end(),
        std::back_inserter(missing));
    std::set_difference(
        actual.begin(), actual.end(),
        expected.begin(), expected.end(),
        std::back_inserter(unexpected));

    if (!missing.empty() || !unexpected.empty()) {
        std::cerr << "Signature target matrix mismatch under " << dir << '\n';
        if (!missing.empty()) {
            print_list("Missing baselines:", missing);
        }
        if (!unexpected.empty()) {
            print_list("Unexpected baselines:", unexpected);
        }
        return 1;
    }

    std::cout << "Signature target matrix OK (" << expected.size() << " baselines)\n";
    return 0;
}
