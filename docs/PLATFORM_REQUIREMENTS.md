# Platform Requirements and Testing

## Overview

XOffsetDatastructure2 is designed specifically for **64-bit, little-endian** platforms. This document explains the requirements, testing strategy, and rationale.

---

## Platform Requirements

### 1. **64-bit Architecture** (Required)
- **Requirement**: `sizeof(void*) == 8`
- **Reason**: Memory layout and type signatures are calculated for 64-bit pointers
- **Impact**: Using on 32-bit systems will cause incorrect memory layouts and data corruption

### 2. **Little-Endian Byte Order** (Required)
- **Requirement**: LSB-first byte order (x86, x86-64, ARM-LE)
- **Reason**: Binary data is stored in little-endian format
- **Impact**: Using on big-endian systems will cause data corruption when serializing/deserializing

---

## Supported Platforms

### ✅ Fully Supported
| Platform | Architecture | Endianness | Status |
|----------|-------------|------------|--------|
| x86-64 Linux | 64-bit | Little-endian | ✅ Supported |
| x86-64 macOS | 64-bit | Little-endian | ✅ Supported |
| x86-64 Windows | 64-bit | Little-endian | ✅ Supported |
| ARM64 Linux (aarch64) | 64-bit | Little-endian | ✅ Supported |
| ARM64 macOS (Apple Silicon) | 64-bit | Little-endian | ✅ Supported |

### ❌ Not Supported
| Platform | Architecture | Endianness | Status | Reason |
|----------|-------------|------------|--------|---------|
| x86 (32-bit) | 32-bit | Little-endian | ❌ Not supported | Requires 64-bit |
| PowerPC | 64-bit | Big-endian | ❌ Not supported | Requires little-endian |
| SPARC | 64-bit | Big-endian | ❌ Not supported | Requires little-endian |
| MIPS (BE) | 64-bit | Big-endian | ❌ Not supported | Requires little-endian |

---

## Compile-Time Platform Checks

The library includes automatic compile-time checks in `xoffsetdatastructure2.hpp`:

```cpp
// Platform detection
#if !XOFFSET_ARCH_64BIT
    #error "XOffsetDatastructure2 requires 64-bit architecture"
#endif

#if !XOFFSET_LITTLE_ENDIAN
    #error "XOffsetDatastructure2 requires little-endian architecture"
#endif
```

### Disabling Checks (Not Recommended)

If you need to compile on an unsupported platform for testing purposes **only**:

```cpp
#define XOFFSET_DISABLE_PLATFORM_CHECKS
#include "xoffsetdatastructure2.hpp"
```

**⚠️ WARNING**: This will likely cause data corruption. Only use for academic/research purposes.

---

## Runtime Platform Testing

### Test Suite: `test_platform.cpp`

The platform test suite verifies:

1. **Architecture Detection**
   - Verifies 64-bit pointer size
   - Checks `sizeof(void*) == 8`

2. **Endianness Detection**
   - Tests byte order of integers
   - Verifies IEEE 754 double representation

3. **Type Size Verification**
   - `sizeof(char) == 1`
   - `sizeof(int) == 4`
   - `sizeof(float) == 4`
   - `sizeof(double) == 8`
   - `sizeof(long long) == 8`
   - `sizeof(XString) == 32`

4. **Alignment Verification**
   - `alignof(int) == 4`
   - `alignof(double) == 8`
   - `alignof(void*) == 8`

5. **Binary Compatibility Test**
   - Writes known values in little-endian format
   - Verifies byte order in serialized buffer
   - Tests: `0x12345678` → `78 56 34 12` (bytes)

6. **Cross-Buffer Compatibility**
   - Serializes data to binary
   - Deserializes in new buffer
   - Verifies data integrity

### Running Platform Tests

```bash
# Build and run
./build.sh

# Or run manually
cd build
./bin/test_platform
```

### Expected Output (Supported Platform)

```
[TEST] Platform Requirements Verification
--------------------------------------------------
Platform Information:
  Architecture: 64-bit
  Endianness:   Little-Endian
  Pointer size: 8 bytes

Checking Requirements:
  64-bit architecture: ✓ YES
  Little-endian:       ✓ YES

Running Verification Tests:
  Verifying type sizes...
    ✓ All type sizes correct
  Verifying endianness...
    ✓ Endianness is little-endian
  Verifying struct alignment...
    ✓ Alignment requirements met
  Testing binary data compatibility...
    ✓ Binary compatibility verified
  Testing cross-buffer data transfer...
    ✓ Cross-buffer compatibility verified

[PASS] ✓ Platform meets all requirements!
       Your system is compatible with XOffsetDatastructure2.
```

### Expected Output (Unsupported Platform)

#### Example: 32-bit System
```
Platform Information:
  Architecture: 32-bit
  Endianness:   Little-Endian
  Pointer size: 4 bytes

Checking Requirements:
  64-bit architecture: ✗ NO
  Little-endian:       ✓ YES

[FAIL] ❌ XOffsetDatastructure2 requires 64-bit architecture
       Current platform is 32-bit
       This library is designed for 64-bit systems only.
```

#### Example: Big-Endian System
```
Platform Information:
  Architecture: 64-bit
  Endianness:   Big-Endian
  Pointer size: 8 bytes

Checking Requirements:
  64-bit architecture: ✓ YES
  Little-endian:       ✗ NO

[FAIL] ❌ XOffsetDatastructure2 requires little-endian architecture
       Current platform is big-endian
       This library stores data in little-endian format.
       Using it on big-endian systems will cause data corruption.
```

---

## Design Rationale

### Why 64-bit Only?

1. **Type Signature Stability**
   - Type signatures include pointer sizes and offsets
   - Calculated at compile-time with `sizeof(void*)` assumptions
   - Changing to 32-bit would invalidate all signatures

2. **Modern Systems**
   - 64-bit is the standard for servers, desktops, and mobile devices
   - Supporting both 32/64-bit would double the testing matrix

3. **Memory Limits**
   - 64-bit allows addressing >4GB memory
   - Suitable for large-scale data processing

### Why Little-Endian Only?

1. **Binary Portability**
   - Data is serialized to binary format
   - Little-endian is the dominant architecture (x86, ARM)
   - Supporting both would require byte-swapping overhead

2. **Performance**
   - No runtime byte-swapping needed
   - Direct memory mapping possible

3. **Simplicity**
   - Single, well-defined binary format
   - Easier to debug and reason about

### Future Considerations

**Could we support big-endian?**
- Technically yes, but requires:
  - Runtime byte-swapping for all I/O
  - Separate type signatures for BE/LE
  - Complexity in serialization code
  - Performance penalty

**Could we support 32-bit?**
- Technically yes, but requires:
  - Separate type signature calculations
  - Different memory layouts
  - Incompatible data formats with 64-bit
  - Increased testing complexity

**Current Decision**: Focus on 64-bit little-endian for simplicity, performance, and market coverage.

---

## Migration and Portability

### Cross-Platform Compatibility

**Within Supported Platforms**: ✅ **Full Binary Compatibility**

Data serialized on one supported platform can be deserialized on another:
- Linux x86-64 ↔ macOS x86-64 ✅
- macOS ARM64 ↔ Linux x86-64 ✅
- Windows x86-64 ↔ Any other 64-bit LE ✅

**To Unsupported Platforms**: ❌ **Not Supported**

No automatic migration. Manual options:
1. **JSON Export/Import** (future feature)
2. **Custom Migration Tool** (user-implemented)
3. **Run on Supported Platform** (recommended)

---

## Testing in CI/CD

### Recommended CI Matrix

```yaml
matrix:
  platform:
    - ubuntu-latest (x86-64)
    - macos-latest (ARM64)
    - windows-latest (x86-64)
  
  # Don't test on:
  # - 32-bit systems
  # - Big-endian systems
```

### Test Strategy

1. **Every Commit**: Run `test_platform` first
2. **Platform Test Must Pass**: Before running other tests
3. **Cross-Platform**: Test serialization between different OSes

---

## FAQ

### Q: Can I use this on Raspberry Pi 3 (ARM 32-bit)?
**A**: No, requires 64-bit. Use Raspberry Pi 4/5 with 64-bit OS.

### Q: Can I use this on older x86 (32-bit) systems?
**A**: No, requires 64-bit x86-64.

### Q: What about WebAssembly?
**A**: Depends on WASM configuration:
- WASM64 with little-endian: ✅ Supported
- WASM32: ❌ Not supported

### Q: Can I port data from 64-bit to 32-bit?
**A**: Not automatically. Need custom migration tool to convert:
- Pointer sizes (8 → 4 bytes)
- Memory layouts
- Type signatures

### Q: Does this work on Apple Silicon (M1/M2/M3)?
**A**: ✅ Yes! ARM64 with little-endian is fully supported.

### Q: What about Android?
**A**: ✅ Yes, if using ARM64 or x86-64 Android devices.

---

## Summary

| Requirement | Status | Check Method |
|------------|--------|--------------|
| 64-bit architecture | ✅ Required | Compile-time + runtime test |
| Little-endian | ✅ Required | Compile-time + runtime test |
| Type sizes | ✅ Verified | Runtime test |
| Alignment | ✅ Verified | Runtime test |
| Binary compatibility | ✅ Verified | Runtime test |

**Recommended Practice**:
1. Run `test_platform` before deploying to a new system
2. Include platform checks in your CI/CD pipeline
3. Document the platform requirements in your project README

---

## Related Files

- **Header**: `xoffsetdatastructure2.hpp` - Platform detection and compile-time checks
- **Test**: `tests/test_platform.cpp` - Comprehensive platform testing
- **CMake**: `tests/CMakeLists.txt` - Test configuration
- **Docs**: This file - Platform requirements documentation

---

**Last Updated**: 2025-01-19  
**Version**: 1.0
