// typelayout_tool.cpp
// Signature generation and comparison tool
//
// Usage:
//   typelayout-tool generate [-o FILE]
//   typelayout-tool compare FILE1 FILE2 [...]
//
// Build:
//   clang++ -std=c++26 -freflection -I../include typelayout_tool.cpp -o typelayout-tool

#include <typelayout.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <array>
#include <tuple>

using namespace typelayout;

// ============================================================================
// User-defined types (modify this section)
// ============================================================================

namespace shared {

struct Point {
    int32_t x;
    int32_t y;
};

struct Header {
    uint32_t magic;
    uint32_t version;
    uint64_t timestamp;
    uint32_t payload_size;
    uint32_t checksum;
};

struct Message {
    Header header;
    int32_t type;
    char data[64];
};

struct Config {
    uint32_t flags;
    double timeout;
    int32_t max_connections;
    char name[32];
};

} // namespace shared

// ============================================================================
// Signature Export (C++26 Reflection)
// ============================================================================

struct SignatureEntry {
    const char* type_name;
    const char* signature;
};

// Get type name using reflection (P2996 Bloomberg fork API)
template<typename T>
consteval auto get_type_name() {
    return display_string_of(^^T);
}

// Build signatures array from tuple of types
template<typename Tuple, std::size_t... Is>
constexpr auto make_signatures_impl(std::index_sequence<Is...>) {
    return std::array<SignatureEntry, sizeof...(Is)>{{
        SignatureEntry{
            get_type_name<std::tuple_element_t<Is, Tuple>>().data(),
            get_layout_signature_cstr<std::tuple_element_t<Is, Tuple>>()
        }...
    }};
}

template<typename Tuple>
constexpr auto make_signatures() {
    return make_signatures_impl<Tuple>(
        std::make_index_sequence<std::tuple_size_v<Tuple>>{}
    );
}

// ============================================================================
// Register types here (just list them in a tuple)
// ============================================================================

using ExportTypes = std::tuple<
    shared::Point,
    shared::Header,
    shared::Message,
    shared::Config
>;

constexpr auto g_signatures = make_signatures<ExportTypes>();
constexpr size_t g_signature_count = std::tuple_size_v<ExportTypes>;

// ============================================================================
// Generate Command
// ============================================================================

int cmd_generate(const char* output_file) {
    std::ofstream file;
    std::ostream* out = &std::cout;
    
    if (output_file) {
        file.open(output_file);
        if (!file) {
            std::cerr << "Error: cannot open " << output_file << "\n";
            return 1;
        }
        out = &file;
    }
    
    for (const auto& e : g_signatures) {
        *out << e.type_name << " " << e.signature << "\n";
    }
    
    std::cerr << "Generated " << g_signature_count << " signatures\n";
    return 0;
}

// ============================================================================
// Compare Command
// ============================================================================

std::vector<std::pair<std::string, std::string>> parse_file(const std::string& filename) {
    std::vector<std::pair<std::string, std::string>> result;
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error: cannot open " << filename << "\n";
        return result;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t space = line.find(' ');
        if (space != std::string::npos) {
            result.emplace_back(line.substr(0, space), line.substr(space + 1));
        }
    }
    return result;
}

int cmd_compare(int file_count, char* files[]) {
    if (file_count < 2) {
        std::cerr << "Error: need at least 2 files\n";
        return 1;
    }
    
    std::map<std::string, std::map<std::string, std::string>> all_sigs;
    
    for (int i = 0; i < file_count; ++i) {
        for (const auto& [type, sig] : parse_file(files[i])) {
            all_sigs[type][files[i]] = sig;
        }
    }
    
    if (all_sigs.empty()) {
        std::cerr << "Error: no signatures found\n";
        return 1;
    }
    
    int mismatches = 0;
    for (const auto& [type, file_sigs] : all_sigs) {
        std::string first;
        bool mismatch = false;
        for (const auto& [f, s] : file_sigs) {
            if (first.empty()) first = s;
            else if (s != first) mismatch = true;
        }
        if (mismatch) {
            if (mismatches == 0) std::cout << "=== Mismatches ===\n";
            std::cout << type << ":\n";
            for (const auto& [f, s] : file_sigs) {
                std::cout << "  " << f << ": " << s << "\n";
            }
            ++mismatches;
        }
    }
    
    if (mismatches == 0) {
        std::cout << "OK: " << all_sigs.size() << " types match\n";
        return 0;
    }
    std::cout << "FAILED: " << mismatches << " mismatches\n";
    return 1;
}

// ============================================================================
// Main
// ============================================================================

void print_usage(const char* prog) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << prog << " generate [-o FILE]\n";
    std::cerr << "  " << prog << " compare FILE1 FILE2 [...]\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string cmd = argv[1];
    
    if (cmd == "generate") {
        const char* output = nullptr;
        if (argc >= 4 && std::string(argv[2]) == "-o") {
            output = argv[3];
        }
        return cmd_generate(output);
    }
    
    if (cmd == "compare") {
        return cmd_compare(argc - 2, argv + 2);
    }
    
    print_usage(argv[0]);
    return 1;
}