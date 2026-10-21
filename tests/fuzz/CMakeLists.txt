# G5: libFuzzer targets. Configure with -DXOFFSET_BUILD_FUZZERS=ON and a clang
# whose resource directory has the fuzzer runtime; add ASan/UBSan through
# CMAKE_CXX_FLAGS. CTest runs a short smoke over the seed corpus; CI runs the
# long sessions (.github/workflows/fuzz.yml).
add_executable(fuzz_v2_wire fuzz_v2_wire.cpp)
configure_xoffset_target(fuzz_v2_wire)
xoffset_enable_test_assertions(fuzz_v2_wire)
target_compile_options(fuzz_v2_wire PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_v2_wire PRIVATE -fsanitize=fuzzer)

set(XOFFSET_FUZZ_SEEDS "${CMAKE_CURRENT_BINARY_DIR}/seeds/v2_wire")
add_test(NAME fuzz_v2_wire_seeds
    COMMAND $<TARGET_FILE:test_v2_fuzz> --write-corpus "${XOFFSET_FUZZ_SEEDS}")
set_tests_properties(fuzz_v2_wire_seeds PROPERTIES
    LABELS "fuzz" FIXTURES_SETUP fuzz_v2_wire_seeds)
# Fixed seed and run count, so the smoke is reproducible. A failure leaves
# crash-* in the test working directory.
add_test(NAME fuzz_v2_wire_smoke
    COMMAND $<TARGET_FILE:fuzz_v2_wire> -seed=1 -runs=20000 -max_len=262144
            -rss_limit_mb=2048 -timeout=30 "${XOFFSET_FUZZ_SEEDS}")
set_tests_properties(fuzz_v2_wire_smoke PROPERTIES
    LABELS "fuzz" FIXTURES_REQUIRED fuzz_v2_wire_seeds)

# Three more schemas (fixed arrays/enums/blobs, nested maps, nested vectors);
# the first input byte picks the schema.
add_executable(fuzz_wire_schemas fuzz_wire_schemas.cpp)
configure_xoffset_target(fuzz_wire_schemas)
xoffset_enable_test_assertions(fuzz_wire_schemas)
target_compile_options(fuzz_wire_schemas PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_wire_schemas PRIVATE -fsanitize=fuzzer)

add_executable(fuzz_wire_schemas_seeds fuzz_wire_schemas.cpp)
configure_xoffset_target(fuzz_wire_schemas_seeds)
xoffset_enable_test_assertions(fuzz_wire_schemas_seeds)
target_compile_definitions(fuzz_wire_schemas_seeds PRIVATE XOFFSET_FUZZ_SEED_MAIN)

set(XOFFSET_FUZZ_SCHEMA_SEEDS "${CMAKE_CURRENT_BINARY_DIR}/seeds/wire_schemas")
add_test(NAME fuzz_wire_schemas_seeds
    COMMAND $<TARGET_FILE:fuzz_wire_schemas_seeds> "${XOFFSET_FUZZ_SCHEMA_SEEDS}")
set_tests_properties(fuzz_wire_schemas_seeds PROPERTIES
    LABELS "fuzz" FIXTURES_SETUP fuzz_wire_schemas_seeds)
add_test(NAME fuzz_wire_schemas_smoke
    COMMAND $<TARGET_FILE:fuzz_wire_schemas> -seed=1 -runs=20000 -max_len=262144
            -rss_limit_mb=2048 -timeout=30 "${XOFFSET_FUZZ_SCHEMA_SEEDS}")
set_tests_properties(fuzz_wire_schemas_smoke PROPERTIES
    LABELS "fuzz" FIXTURES_REQUIRED fuzz_wire_schemas_seeds)
