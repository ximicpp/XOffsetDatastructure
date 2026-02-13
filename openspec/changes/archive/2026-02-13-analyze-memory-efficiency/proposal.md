# Analysis: XOffset Memory Efficiency — Realistic Game Character Data

## Summary

Measured XBuffer memory overhead using three realistic RPG character profiles.

| Profile | Nested Objects | Logical Data | XBuffer Size | Efficiency | vs Protobuf |
|---------|---------------|-------------|-------------|-----------|-------------|
| New Player | 0 | 65B | 304B | 21.4% | 4.1x |
| Mid-Game | 78 | 1,728B | 4,464B | 38.7% | 2.3x |
| End-Game Veteran | 592+ | 17,140B | 37,904B | 45.2% | 1.9x |

## Key Findings

1. **Fixed overhead**: segment_manager = 112B, root index = ~48B (negligible for >1KB data)
2. **Dominant cost**: per-XString overhead = ~48B (32B header + 16B block_ctrl alloc header)
3. **Efficiency ceiling**: XString-heavy structs converge to ~45-50% efficiency
4. **Pure scalar arrays**: 93-99% efficiency (overhead is negligible)
5. **Trade-off**: ~2x protobuf wire size in exchange for zero encode/decode + in-place mutation

## sizeof Game Types (Linux x86_64)

| Type | Size | Notes |
|------|------|-------|
| offset_ptr<T> | 8B | Same as raw pointer |
| XString | 32B | std::string = 24B (+8B) |
| XVector<T> | 32B | std::vector = 24B (+8B) |
| InventoryItem | 48B | 3×i32 + XString |
| Skill | 48B | 2×i32 + float + XString |
| QuestProgress | 80B | 3×i32 + XString + XVector |
| CharacterMinimal | 120B | scalars + 2 strings |
| CharacterMidGame | 352B | scalars + 3 strings + 5 vectors |
| CharacterEndGame | 584B | scalars + 4 strings + 10 vectors |

## Affected Files

- `tests/test_memory_efficiency.cpp` — new test with 3 game character profiles
- `tests/CMakeLists.txt` — added test_memory_efficiency to build

## Verification

- 25/25 tests passed (including new test)
