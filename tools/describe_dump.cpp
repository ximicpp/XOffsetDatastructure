// describe_dump: prints the XOFFSET_RECORD lines for some root types (C++26).
//
// Build with the header that defines the roots and the root list, e.g.
//   -DXOFFSET_DESCRIBE_DUMP_INCLUDE='"slg/model_xoffset.hpp"'
//   -DXOFFSET_DESCRIBE_DUMP_ROOTS='slg::xm::PlayerState, slg::xm::BattleMsg, slg::xm::ShardState'
//
//   describe_dump              print the lines
//   describe_dump --check F    exit 1 unless the XOFFSET_RECORD lines of file F
//                              are exactly the printed XOFFSET_RECORD lines, in
//                              order (comment lines are not compared)

#include "describe_dump.hpp"
#include XOFFSET_DESCRIBE_DUMP_INCLUDE

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const std::vector<std::string> lines = xoffset_describe_dump::lines<XOFFSET_DESCRIBE_DUMP_ROOTS>();
    if (argc == 3 && std::strcmp(argv[1], "--check") == 0) {
        std::vector<std::string> records;
        for (const auto& line : lines)
            if (line.rfind("XOFFSET_RECORD", 0) == 0) records.push_back(line);
        std::ifstream in(argv[2]);
        if (!in) {
            std::cerr << "describe_dump: cannot read " << argv[2] << "\n";
            return 2;
        }
        std::vector<std::string> committed;
        for (std::string line; std::getline(in, line);)
            if (line.rfind("XOFFSET_RECORD", 0) == 0) committed.push_back(line);
        if (committed == records) {
            std::cout << "describe_dump: " << records.size() << " lines match " << argv[2] << "\n";
            return 0;
        }
        std::cout << "describe_dump: " << argv[2] << " differs\n--- committed\n";
        for (const auto& line : committed) std::cout << line << "\n";
        std::cout << "--- generated\n";
        for (const auto& line : lines) std::cout << line << "\n";
        return 1;
    }
    if (argc != 1) {
        std::cerr << "usage: describe_dump [--check FILE]\n";
        return 2;
    }
    for (const auto& line : lines) std::cout << line << "\n";
    return 0;
}
