# Changelog - XOffsetDatastructure2

## [Unreleased] - 2025-01-19

### Fixed 🐛
1. **Critical**: Added missing `OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR` macro definition
   - File: `xoffsetdatastructure2.hpp`
   - Default value: `1` (custom growth factor enabled)
   - Can be overridden by defining before including the header
   - Properly documented with clear comments

2. **Medium**: Fixed uninitialized `char` members in generated code
   - File: `tools/xds_generator.py`
   - Now properly handles `char` type default values from YAML schemas
   - Generated code: `char mChar{\0};` instead of `char mChar;`
   - Affects: `generated/basic_types.hpp`, `generated/alignment_test.hpp`
   - See detailed report: `docs/FIX_CHAR_DEFAULT.md`

### Added 📝
- Configuration section at the top of `xoffsetdatastructure2.hpp`
- Comprehensive code review findings document (`docs/CODE_REVIEW_FINDINGS.md`)
- Detailed fix report for char default values (`docs/FIX_CHAR_DEFAULT.md`)
- Detailed documentation for macro configurations
- Support for `char` type default values in code generator

### Changed 🔄
- Improved header organization with clear section separators
- Enhanced documentation for container growth factor options

---

## Configuration Macro Reference

### `OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR`

Controls the growth strategy for dynamic containers (XVector, XSet, XMap).

**Values**:
- `0`: Use default Boost container growth factor (typically 2x)
  - Standard STL-like behavior
  - Faster growth, more memory overhead
  
- `1`: Use custom growth factor (default)
  - Defined by `growth_factor_custom` struct
  - Currently set to 1.1x (10% growth)
  - Better memory efficiency

**Default**: `1` (custom growth factor)

**How to Override**:
```cpp
// Before including the header:
#define OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR 0
#include "xoffsetdatastructure2.hpp"
```

**Or via CMake**:
```cmake
add_compile_definitions(OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR=0)
```

---

## Growth Factor Options

When `OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR` is set to `1`, the growth factor is defined by:

```cpp
struct growth_factor_custom
    : boost::container::dtl::grow_factor_ratio<0, 11, 10> // 1.1x (10% growth)
{
};
```

**Common Options**:
| Ratio | Factor | Growth | Use Case |
|-------|--------|--------|----------|
| 11/10 | 1.1 | 10% | Memory-constrained (current default) |
| 12/10 | 1.2 | 20% | Balanced |
| 15/10 | 1.5 | 50% | Performance-focused |
| 20/10 | 2.0 | 100% | STL default behavior |

**To Change**:
Edit line ~203 in `xoffsetdatastructure2.hpp`:
```cpp
struct growth_factor_custom
    : boost::container::dtl::grow_factor_ratio<0, 15, 10> // 50% growth
{
};
```

---

## Testing Status

All tests pass with the new configuration:
- ✅ Basic types test
- ✅ Vector operations test
- ✅ Map/Set operations test
- ✅ Nested structures test
- ✅ Memory compaction test
- ✅ Serialization test
- ✅ Alignment test
- ✅ Modification test
- ✅ Comprehensive operations test

---

## Fixed Issues ✅

1. ✅ Undefined macro `OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR`
2. ✅ Uninitialized `char` members in generated code
3. ✅ (Not an issue) YAML schemas already had correct char defaults

## Known Issues

See `docs/CODE_REVIEW_FINDINGS.md` for the complete list of issues and recommendations.

### Medium Priority
None currently blocking

### Low Priority
1. Commented-out alternative implementations
2. Magic numbers in growth factor definition
3. Incomplete error handling in code generator

---

## Future Roadmap

### Short Term
- [x] Add char default value support in code generator ✅
- [ ] Add validation for reserved keywords in schemas
- [ ] Clean up commented-out code
- [ ] Add negative test cases

### Long Term
- [ ] C++26 reflection support for automatic compaction
- [ ] Cross-platform endianness support
- [ ] Schema versioning and migration tools
- [ ] Performance benchmarking suite

---

## Migration Guide

### From Previous Version

No breaking changes in this update. The macro fix is backward compatible:
- Previous code relied on undefined behavior (undefined macro defaulting to 0)
- New code explicitly defines the macro with value 1
- Both behaviors are supported via the macro

If you want the previous behavior (default Boost growth factor):
```cpp
#define OFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR 0
#include "xoffsetdatastructure2.hpp"
```

---

## Contributors

- Code Review: 2025-01-19
- Fix Applied: 2025-01-19

---

## Acknowledgments

Special thanks to the Boost.Interprocess and Boost.Container libraries for providing the foundation for this project.
