// Replays saved inputs through a libFuzzer harness without libFuzzer: linked
// with fuzz_v2_wire.cpp or fuzz_wire_schemas.cpp, it builds with any compiler
// that builds the library (fuzz_v2_wire_replay, fuzz_wire_schemas_replay). CTest
// runs it over tests/fuzz/regressions/<target>/ in every CI job; the nightly
// fuzz run uses it to replay the libFuzzer corpus on GCC 16.
//
// usage: <target>_replay <file|dir>...   each file, and each regular file
//                                        directly in a dir (sorted, dot files
//                                        skipped), goes through the harness once
//        <target>_replay --write-seeds <dir>
// A harness failure aborts, after the line naming the input. A path that does
// not exist is an error; a directory without inputs is not.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);
extern "C" int XOffsetFuzzWriteSeeds(const char* dir);

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    if (argc == 3 && std::string_view(argv[1]) == "--write-seeds") return XOffsetFuzzWriteSeeds(argv[2]);
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <file|dir>...\n       " << argv[0] << " --write-seeds <dir>\n";
        return 2;
    }
    std::vector<fs::path> inputs;
    for (int i = 1; i < argc; ++i) {
        const fs::path arg = argv[i];
        std::error_code ec;
        if (fs::is_regular_file(arg, ec)) { inputs.push_back(arg); continue; }
        if (!fs::is_directory(arg, ec)) { std::cerr << "replay: no such file or directory: " << arg.string() << "\n"; return 2; }
        std::vector<fs::path> found;
        for (const auto& entry : fs::directory_iterator(arg))
            if (entry.is_regular_file() && entry.path().filename().string().front() != '.') found.push_back(entry.path());
        std::sort(found.begin(), found.end());
        inputs.insert(inputs.end(), found.begin(), found.end());
    }
    std::size_t bytes = 0;
    for (const auto& path : inputs) {
        std::ifstream in(path, std::ios::binary);
        const std::string data{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        if (!in.good() && !in.eof()) { std::cerr << "replay: cannot read " << path.string() << "\n"; return 2; }
        // Flushed before the call: if the harness aborts, the log names the input.
        std::cout << "replay: " << path.string() << " (" << data.size() << " bytes)" << std::endl;
        LLVMFuzzerTestOneInput(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
        bytes += data.size();
    }
    std::cout << "replay: " << inputs.size() << " inputs, " << bytes << " bytes, all passed\n";
    return 0;
}
