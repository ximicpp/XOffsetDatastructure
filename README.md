# XOffsetDatastructure2

[![CI Status](https://github.com/ximicpp/XOffsetDatastructure/workflows/Cross-Platform%20CI/badge.svg)](https://github.com/ximicpp/XOffsetDatastructure/actions)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20Windows%20%7C%20iOS%20%7C%20Android-lightgrey.svg)](#supported-platforms)

### Introduction
XOffsetDatastructure is a serialization library designed to reduce or even eliminate the performance consumption of serialization and deserialization by utilizing zero-encoding and zero-decoding. It is also a collection of high-performance data structures designed for efficient read and in-place/non-in-place write, with performance comparable to STL.

### Supported Platforms

XOffsetDatastructure2 supports multiple platforms with comprehensive CI testing:

| Platform | Compiler | Status | Notes |
|----------|----------|--------|-------|
| **macOS** | Clang (Apple) | ✅ Tested | Reference platform |
| **Linux** | GCC 11, Clang 14 | ✅ Tested | Full compatibility |
| **Windows** | MSVC 2019+ | ✅ Tested | Size/alignment validation |
| **iOS** | Clang (Xcode) | ✅ Tested | Simulator + Device |
| **Android** | Clang (NDK r25c) | ✅ Tested | All major ABIs |

See [GitHub Actions CI Guide](docs/GITHUB_ACTIONS_CI.md) for detailed testing information.

### Quick Start

```bash
# Clone with submodules
git clone --recursive https://github.com/ximicpp/XOffsetDatastructure.git
cd XOffsetDatastructure

# Build (macOS/Linux)
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)

# Run tests
ctest -V

# Run demo
./bin/demo
```

For Windows (MSVC), see [Quick Start Guide](docs/QUICK_START.md).

### Key Features

- ✅ **Zero-copy serialization**: No encoding/decoding overhead
- ✅ **Type-safe**: Compile-time type signature validation
- ✅ **Cross-platform**: Binary compatibility across platforms (with same endianness)
- ✅ **YAML schema**: Code generation from declarative schemas
- ✅ **STL-like API**: Familiar interface (XVector, XMap, XSet, XString)
- ✅ **Memory efficient**: Offset-based pointers, relocatable buffers
- ✅ **MSVC compatible**: Works on all major compilers

### Documentation

- 📘 [Quick Start Guide](docs/QUICK_START.md) - Get started in 5 minutes
- 🔧 [MSVC Compatibility](docs/MSVC_COMPATIBILITY.md) - MSVC-specific notes
- 🧪 [GitHub Actions CI](docs/GITHUB_ACTIONS_CI.md) - Cross-platform testing
- 📝 [Type Signature System](docs/MSVC_TYPE_SIGNATURE_VALIDATION.md) - Advanced validation

### Schema Example

```yaml
# schemas/game_data.xds.yaml
types:
  - name: GameData
    fields:
      - name: player_id
        type: int
      - name: player_name
        type: XString
      - name: items
        type: XVector<Item>
```

Generate C++ code:
```bash
python3 tools/xds_generator.py schemas/game_data.xds.yaml -o generated/
```

### Usage Example

```cpp
#include "xoffsetdatastructure2.hpp"
#include "generated/game_data.hpp"

XBufferExt xbuf(4096);
auto* game = xbuf.make<GameData>("player_save");

game->player_name = XString("Hero", xbuf.allocator<XString>());
game->player_id = 12345;

// Serialize
std::string data = xbuf.save_to_string();

// Deserialize (zero-copy!)
XBufferExt loaded = XBufferExt::load_from_string(data);
auto* game2 = loaded.find<GameData>("player_save").first;
```

### Compiler Support

| Compiler | Version | Type Signature Validation | Notes |
|----------|---------|---------------------------|-------|
| Clang | 14+ | ✅ Full | Recommended |
| GCC | 11+ | ✅ Full | Fully supported |
| MSVC | 2019+ | ⚠️ Size/Alignment only | See [MSVC docs](docs/MSVC_COMPATIBILITY.md) |
| Apple Clang | Xcode 14+ | ✅ Full | macOS/iOS |

### Contributing

Contributions are welcome! Please see our testing requirements:

1. Run all tests locally: `ctest -V`
2. Ensure MSVC compatibility test passes: `./test_msvc_compat`
3. Verify on multiple platforms via GitHub Actions CI
4. Update documentation as needed

### License

MIT License - see [LICENSE](LICENSE) for details.

### References

**CppCon 2024**  
[Using Modern C++ to Build XOffsetDatastructure: A Zero-Encoding and Zero-Decoding High-Performance Serialization Library](https://github.com/CppCon/CppCon2024/blob/main/Presentations/Using_Modern_Cpp_to_Build_XOffsetDatastructure.pdf)

**CppCon 2025**  
[Cross-platform XOffsetDatastructure: Ensuring Zero-encoding/Zero-decoding Serialization Compatibility Through Compile-time Type Signatures](https://github.com/ximicpp/XOffsetDatastructure/blob/main/docs/Compile-timeTypeSignatures.pdf)

**Benchmarks**  
Benchmark code and results: see the tag ["v1.0.0: CppCon 2024 Milestone Release"](https://github.com/ximicpp/XOffsetDatastructure/releases/tag/v1.0.0)

---

**Made with ❤️ for high-performance C++ applications**