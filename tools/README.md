# XOffsetDatastructure2 Tools

## Signature Export Tool (`export_signatures`)

Exports type signatures for key data types (Player, Item, GameData) to portable `.sig.hpp` header files.

### Requirements
- **Compile**: P2996 Clang (same as main project)
- **Run**: Any platform (generates platform-specific signatures)
- **Compare**: Any C++17 compiler (`.sig.hpp` files are pure constexpr data)

### Usage

```bash
# Build (via CMake, included in main build)
cd build
cmake --build . --target export_signatures

# Export to stdout
./bin/export_signatures

# Export to directory (creates <platform>.sig.hpp)
./bin/export_signatures tools/sigs/
```

### Output

The tool generates a `.sig.hpp` file containing:
- Platform metadata (arch, pointer size, endianness)
- Layout signatures (for byte-layout comparison)
- Definition signatures (for full type identity)
- Type registry for `CompatReporter`

### Example

```
$ ./bin/export_signatures tools/sigs/
Exported 3 type(s) to tools/sigs/x86_64_linux_clang.sig.hpp [x86_64_linux_clang]
```

## Cross-Platform Compatibility Check (`check_compat`)

*Coming soon* — Compares exported `.sig.hpp` files across platforms using `CompatReporter`.