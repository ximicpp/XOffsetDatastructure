# Tests that ask P2996 reflection or TypeLayout themselves (std::meta in the
# test, TypeLayout signatures, member counts or safety levels, zero-boilerplate
# records), or whose records all need it (user constructors with custom
# lifetimes): built only with the C++26 reflection schema backend. The other
# builds (C++20/C++23, and C++26 with XOFFSET_SCHEMA_BACKEND=describe) list them
# in test-inventory.json as cxx26_only_not_built.
if(XOFFSET_CXX_STANDARD STREQUAL "26" AND NOT XOFFSET_SCHEMA_BACKEND STREQUAL "describe")
    set(XOFFSET_TESTS_REFLECTION ON)
else()
    set(XOFFSET_TESTS_REFLECTION OFF)
endif()
set(XOFFSET_CXX26_ONLY_TESTS
    test_admission_typelayout
    test_canonical_cold_no_callback
    test_canonical_zero_target
    test_custom_future_lifetime
    test_enum_support
    test_field_limit_fix
    test_reflection_advanced
    test_reflection_core
    test_schema_evolution
    test_schema_lineage
    test_subtree_copy
    test_type_safety
    test_type_signatures
    test_zero_boilerplate
)

function(xoffset_cxx26_only name)
    set_property(GLOBAL APPEND PROPERTY XOFFSET_CXX26_ONLY_NOT_BUILT ${name})
endfunction()

# Tests name their target as COMMAND (not $<TARGET_FILE:...>): only then does CTest
# prefix CMAKE_CROSSCOMPILING_EMULATOR (qemu-aarch64, wine) in cross builds.
function(add_xoffset_test test_name test_label)
    if(NOT XOFFSET_TESTS_REFLECTION AND test_name IN_LIST XOFFSET_CXX26_ONLY_TESTS)
        xoffset_cxx26_only(${test_name})
        return()
    endif()
    add_executable(${test_name} ${test_name}.cpp)
    configure_xoffset_target(${test_name})
    xoffset_enable_test_assertions(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS "${test_label}")
endfunction()

# These tests include only the OS header, with no full-library dependencies.
set(STORAGE_TESTS
    test_virtual_memory_storage
    test_vm_pool
)
foreach(test_name ${STORAGE_TESTS})
    add_executable(${test_name} ${test_name}.cpp)
    set_target_properties(${test_name} PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/$<CONFIG>)
    target_link_libraries(${test_name} PRIVATE XOffset::storage)
    xoffset_enable_test_assertions(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS "storage")
endforeach()

# SC-8: the OS layer without exceptions (C++17, every platform).
add_executable(test_vm_storage_noexc test_vm_storage_noexc.cpp test_vm_storage_noexc_tu2.cpp)
set_target_properties(test_vm_storage_noexc PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/$<CONFIG>)
target_link_libraries(test_vm_storage_noexc PRIVATE XOffset::storage)
xoffset_enable_test_assertions(test_vm_storage_noexc)
xoffset_disable_exceptions(test_vm_storage_noexc)
add_test(NAME test_vm_storage_noexc COMMAND test_vm_storage_noexc)
set_tests_properties(test_vm_storage_noexc PROPERTIES LABELS "storage;no-exceptions")

if(XOFFSET_STORAGE_ONLY)
    xoffset_write_test_inventory()
    return()
endif()

# Basic library behavior (still requires the full P2996 header/toolchain).
set(BASIC_TESTS
    test_basic_types
    test_vector
    test_map_set
    test_nested
    test_compaction
    test_modify
    test_xbuffer_api
)

foreach(test_name ${BASIC_TESTS})
    add_xoffset_test(${test_name} basic)
endforeach()

# Reflection tests (require P2996 Clang)
set(REFLECTION_TESTS
    test_assign_range
    test_scalar_resize
    test_validation_granules
    test_string_replace
    test_hot_path_constants
    test_resize_growth
    test_reflection_core
    test_reflection_advanced
    test_type_signatures
    test_field_limit_fix
    test_type_safety
    test_admission_typelayout
    test_wire_admission
    test_enum_support
    test_xstring_direct_assign
    test_string_append
    test_xhandle
    test_schema_envelope
    test_fixed_layout_containers
    test_error_paths
    test_memory_efficiency
    test_zero_boilerplate
    test_complex_nesting
    test_inheritance
    test_adaptive_reservation
    test_main_containers
    test_runtime_regressions
    test_mutation_failures
    test_wire_validation
    test_same_arena_move
    test_shift_relocate
    test_append_grow
    test_subtree_copy
    test_free_table_policy
    test_lower_bound_index
    test_stable_storage
    test_slot_storage
    test_ordinary_records
    test_schema_describe
    test_vm_lifecycle
    test_vm_transport
    test_owner_identity
    test_owner_registry
    test_owner_map
    test_tail_expansion
    test_regrow_used_range
    test_storage_policies
    test_allocator_model
    test_allocator_failures
    test_v2_golden
    test_cross_compiler_wire
    test_schema_guard_slg_values
    test_schema_pin
    test_stability_contract
    test_wire_hole_zeroing
    test_implicit_padding
    test_v2_fuzz
    test_save_compact
    test_save_composed
    test_free_run_emission
    test_wire_populate
    test_reload_reset
    test_reset_used_range
    test_clone
)

foreach(test_name ${REFLECTION_TESTS})
    add_xoffset_test(${test_name} reflection)
endforeach()

find_package(Threads REQUIRED)
target_link_libraries(test_owner_identity PRIVATE Threads::Threads)

# Schema handshake (xoffset_handshake.hpp) and schema evolution
# (xoffset_evolution.hpp, server side).
set(EVOLUTION_TESTS
    test_schema_handshake
    test_schema_snapshot   # a frozen snapshot used by a client (every build)
    test_schema_evolution
    test_schema_lineage
)
foreach(test_name ${EVOLUTION_TESTS})
    add_xoffset_test(${test_name} "reflection;evolution")
endforeach()

# Server side (xoffset_evolution.hpp, xoffset_schema_freeze.hpp): reflection
# backend only.
if(XOFFSET_TESTS_REFLECTION)
    # tools/schema_freeze: snapshots and lock file against the checked-in copies.
    add_executable(test_schema_freeze test_schema_freeze.cpp)
    configure_xoffset_target(test_schema_freeze)
    xoffset_enable_test_assertions(test_schema_freeze)
    add_test(NAME test_schema_freeze COMMAND test_schema_freeze "${CMAKE_CURRENT_SOURCE_DIR}/schema_freeze")
    set_tests_properties(test_schema_freeze PROPERTIES LABELS "reflection;evolution")

    # Schema evolution: conversions the default rules do not cover, hooks that
    # name a field neither version has, and lineages whose versions share a
    # schema_hash must fail the build, naming the field or the versions.
    foreach(variant type_change container_kind unknown_handle lineage_same_hash)
        add_executable(schema_evolution_${variant}_fail EXCLUDE_FROM_ALL schema_evolution/${variant}_fail.cpp)
        configure_xoffset_target(schema_evolution_${variant}_fail)
        add_test(NAME schema_evolution_${variant}_fail
            COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} --target schema_evolution_${variant}_fail --config $<CONFIG>)
    endforeach()
    set_tests_properties(schema_evolution_type_change_fail PROPERTIES
        LABELS "reflection;evolution"
        PASS_REGULAR_EXPRESSION "field 'gold' changed type from int to long")
    set_tests_properties(schema_evolution_container_kind_fail PROPERTIES
        LABELS "reflection;evolution"
        PASS_REGULAR_EXPRESSION "field 'items' changed type from [A-Za-z:]*XVector<int> to [A-Za-z:]*XSet<int>")
    set_tests_properties(schema_evolution_unknown_handle_fail PROPERTIES
        LABELS "reflection;evolution"
        PASS_REGULAR_EXPRESSION "handles names 'glod', which neither version has")
    set_tests_properties(schema_evolution_lineage_same_hash_fail PROPERTIES
        LABELS "reflection;evolution"
        PASS_REGULAR_EXPRESSION "v1::Stats and v2::Stats have the same schema_hash")
else()
    xoffset_cxx26_only(test_schema_freeze)
    foreach(variant type_change container_kind unknown_handle lineage_same_hash)
        xoffset_cxx26_only(schema_evolution_${variant}_fail)
    endforeach()
endif()

# SC-8: the library without exceptions. test_no_exceptions checks the
# non-throwing receive entries against the throwing ones; it and every test
# program that needs no exception of its own are built a second time with
# exceptions off as <name>_noexc (test_v2_fuzz then runs typed load, reload
# and direct reload through the non-throwing entries; raw and verified
# receive have none yet).
add_xoffset_test(test_no_exceptions reflection)
set(NO_EXCEPTIONS_TESTS
    test_no_exceptions
    test_enum_support
    test_free_run_emission
    test_hot_path_constants
    test_implicit_padding
    test_inheritance
    test_lower_bound_index
    test_memory_efficiency
    test_ordinary_records
    test_reflection_advanced
    test_reflection_core
    test_regrow_used_range
    test_save_compact
    test_schema_pin
    test_shift_relocate
    test_stability_contract
    test_string_append
    test_type_safety
    test_type_signatures
    test_v2_fuzz
    test_wire_populate
    test_xhandle
    test_xstring_direct_assign
    test_schema_handshake
)
foreach(test_name ${NO_EXCEPTIONS_TESTS})
    if(NOT XOFFSET_TESTS_REFLECTION AND test_name IN_LIST XOFFSET_CXX26_ONLY_TESTS)
        xoffset_cxx26_only(${test_name}_noexc)
        continue()
    endif()
    add_executable(${test_name}_noexc ${test_name}.cpp)
    configure_xoffset_target(${test_name}_noexc)
    xoffset_enable_test_assertions(${test_name}_noexc)
    xoffset_disable_exceptions(${test_name}_noexc)
    add_test(NAME ${test_name}_noexc COMMAND ${test_name}_noexc)
    set_tests_properties(${test_name}_noexc PROPERTIES LABELS "reflection;no-exceptions")
endforeach()
# A failure without a caller to return it to goes to the fatal handler.
add_test(NAME test_no_exceptions_fatal COMMAND test_no_exceptions --fatal)
add_test(NAME test_no_exceptions_noexc_fatal COMMAND test_no_exceptions_noexc --fatal)
set_tests_properties(test_no_exceptions_fatal test_no_exceptions_noexc_fatal PROPERTIES
    LABELS "reflection;no-exceptions"
    PASS_REGULAR_EXPRESSION "fatal no_root: root<T>\\(\\): typed root access requires")

# G5/F4: the libFuzzer harnesses linked with fuzz/replay_main.cpp instead of
# libFuzzer, so every compiler builds them (independent of XOFFSET_BUILD_FUZZERS).
#   fuzz_<target>_replay          replays fuzz/regressions/<target>/: crash
#                                 inputs kept by fix PRs (see its README.md)
#   fuzz_<target>_replay_corpus   replays the harness's own seed corpus, written
#                                 by fuzz_<target>_replay_seeds
# Names start with the target name so tools/validate_vm.py --targets selects all three.
set(XOFFSET_REPLAY_TARGETS v2_wire wire_schemas)
if(XOFFSET_TESTS_REFLECTION)
    list(APPEND XOFFSET_REPLAY_TARGETS schema_evolution)  # server side, reflection only
else()
    xoffset_cxx26_only(fuzz_schema_evolution_replay)
endif()
foreach(fuzz_target ${XOFFSET_REPLAY_TARGETS})
    set(replay fuzz_${fuzz_target}_replay)
    add_executable(${replay} fuzz/fuzz_${fuzz_target}.cpp fuzz/replay_main.cpp)
    configure_xoffset_target(${replay})
    xoffset_enable_test_assertions(${replay})
    if(XOFFSET_FUZZ_INJECT_CRASH)
        target_compile_definitions(${replay} PRIVATE XOFFSET_FUZZ_INJECT_CRASH)
    endif()
    add_test(NAME ${replay}
        COMMAND ${replay} "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/regressions/${fuzz_target}")
    set(replay_seeds "${CMAKE_CURRENT_BINARY_DIR}/replay-seeds/${fuzz_target}")
    add_test(NAME ${replay}_seeds COMMAND ${replay} --write-seeds "${replay_seeds}")
    add_test(NAME ${replay}_corpus COMMAND ${replay} "${replay_seeds}")
    set_tests_properties(${replay} ${replay}_seeds ${replay}_corpus PROPERTIES LABELS "reflection;replay")
    set_tests_properties(${replay}_seeds PROPERTIES FIXTURES_SETUP ${replay}_seeds)
    set_tests_properties(${replay}_corpus PROPERTIES FIXTURES_REQUIRED ${replay}_seeds)
endforeach()

# test_v2_fuzz --replay reruns one case alone; it must match the full run.
add_test(NAME test_v2_fuzz_replay COMMAND test_v2_fuzz --replay 20261021 17)
add_test(NAME test_v2_fuzz_replay_boundary COMMAND test_v2_fuzz --replay 20261021 b302)
set_tests_properties(test_v2_fuzz_replay test_v2_fuzz_replay_boundary PROPERTIES LABELS "reflection")

# Observe process exit after main() returns: these exercise global/static
# destructor order, which an ordinary in-process assertion cannot cover.
set(VM_EXIT_MODES
    empty-globals
    global-assigned
    static-before-registry
    static-after-registry
    shutdown-transfers
)
foreach(exit_mode ${VM_EXIT_MODES})
    add_test(NAME test_vm_lifecycle_${exit_mode}
        COMMAND test_vm_lifecycle --exit-${exit_mode})
    set_tests_properties(test_vm_lifecycle_${exit_mode} PROPERTIES LABELS "reflection;lifecycle")
endforeach()

# A SHIFT rebase that would overflow a rel32 delta ends the program. Fresh processes.
set(SHIFT_OVERFLOW_MODES
    up
    down
    mid
)
foreach(overflow_mode ${SHIFT_OVERFLOW_MODES})
    add_test(NAME test_shift_relocate_overflow-${overflow_mode}
        COMMAND test_shift_relocate --overflow-${overflow_mode})
    set_tests_properties(test_shift_relocate_overflow-${overflow_mode} PROPERTIES LABELS "reflection")
endforeach()

# Real host allocation failure of an ordinary owner's identity record,
# independently of the pre-registration fault hook, and the check that an
# ordinary owner allocates nothing else (it is not indexed). Fresh processes.
set(REGISTRY_FAILURE_MODES
    fail-record
    record-only
)
foreach(failure_mode ${REGISTRY_FAILURE_MODES})
    add_test(NAME test_owner_registry_${failure_mode}
        COMMAND test_owner_registry --${failure_mode})
    set_tests_properties(test_owner_registry_${failure_mode} PROPERTIES LABELS "reflection;registry")
endforeach()

# K7: a schema pin that disagrees with the compiled layout must fail the build.
add_executable(schema_pin_mismatch EXCLUDE_FROM_ALL schema_pin/pin_mismatch.cpp)
configure_xoffset_target(schema_pin_mismatch)
add_test(NAME schema_pin_mismatch
    COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} --target schema_pin_mismatch --config $<CONFIG>)
set_tests_properties(schema_pin_mismatch PROPERTIES
    LABELS "reflection"
    PASS_REGULAR_EXPRESSION "XOffset schema pin mismatch \\(K7\\)")

# G5: coverage-guided fuzzers, in their own directory (not test programs).
if(XOFFSET_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()
# M6: XOFFSET_REQUIRE_NO_PADDING on a padded type must fail the build.
add_executable(require_no_padding_fail EXCLUDE_FROM_ALL padding/require_no_padding_fail.cpp)
configure_xoffset_target(require_no_padding_fail)
add_test(NAME require_no_padding_fail
    COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} --target require_no_padding_fail --config $<CONFIG>)
set_tests_properties(require_no_padding_fail PROPERTIES
    LABELS "reflection"
    PASS_REGULAR_EXPRESSION "has implicit padding bytes")

# XOFFSET_ORDINARY_RECORDS on a record that is not ordinary must fail the build.
# The macro is the C++26 zero-boilerplate declaration; XOFFSET_RECORD carries
# the same promises in the C++20/C++23 builds.
if(XOFFSET_TESTS_REFLECTION)
    foreach(variant overlap base bit_field)
        add_executable(ordinary_records_${variant}_fail EXCLUDE_FROM_ALL ordinary_records/${variant}_fail.cpp)
        configure_xoffset_target(ordinary_records_${variant}_fail)
        add_test(NAME ordinary_records_${variant}_fail
            COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} --target ordinary_records_${variant}_fail --config $<CONFIG>)
    endforeach()
    set_tests_properties(ordinary_records_overlap_fail PROPERTIES
        LABELS "reflection" PASS_REGULAR_EXPRESSION "has overlapping members")
    set_tests_properties(ordinary_records_base_fail PROPERTIES
        LABELS "reflection" PASS_REGULAR_EXPRESSION "must not have base classes")
    set_tests_properties(ordinary_records_bit_field_fail PROPERTIES
        LABELS "reflection" PASS_REGULAR_EXPRESSION "must not have bit-fields")
else()
    foreach(variant overlap base bit_field)
        xoffset_cxx26_only(ordinary_records_${variant}_fail)
    endforeach()
endif()

# XOFFSET_RECORD lists that are wrong, and uses the field-list backend cannot
# serve, must fail the build with these diagnostics. promise_fail needs
# reflection to see the destructor, so a forced field-list build skips it.
set(XOFFSET_SCHEMA_DESCRIBE_FAIL_TESTS
    "omitted|the list does not name every non-static data member"
    "order|list every member once, in declaration order"
    "duplicate|list every member once, in declaration order"
    "array|the list does not name every non-static data member"
    "padding|the list does not name every non-static data member"
    "enum|the enumerators of an enum without a fixed underlying type are unknown"
    "undescribed|this record has no field list")
if(NOT XOFFSET_SCHEMA_BACKEND STREQUAL "describe" AND
   (NOT DEFINED XOFFSET_CXX_STANDARD OR XOFFSET_CXX_STANDARD STREQUAL "26"))
    list(APPEND XOFFSET_SCHEMA_DESCRIBE_FAIL_TESTS
        "promise|declares a destructor, copy/move constructor or assignment")
endif()
foreach(entry IN LISTS XOFFSET_SCHEMA_DESCRIBE_FAIL_TESTS)
    string(REGEX REPLACE "\\|.*$" "" variant "${entry}")
    string(REGEX REPLACE "^[^|]*\\|" "" diagnostic "${entry}")
    add_executable(schema_describe_${variant}_fail EXCLUDE_FROM_ALL schema_describe/${variant}_fail.cpp)
    configure_xoffset_target(schema_describe_${variant}_fail)
    add_test(NAME schema_describe_${variant}_fail
        COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} --target schema_describe_${variant}_fail --config $<CONFIG>)
    set_tests_properties(schema_describe_${variant}_fail PROPERTIES
        LABELS "schema" PASS_REGULAR_EXPRESSION "${diagnostic}")
endforeach()
# MSVC stops at the duplicate's redefinition of the generated field type,
# before the static assertion; that error names the duplicated member.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set_tests_properties(schema_describe_duplicate_fail PROPERTIES PASS_REGULAR_EXPRESSION
        "list every member once, in declaration order;fields<sdf::Neg>::field_a': 'struct' type redefinition")
endif()

# Wire schemas whose meaning differs between 64-bit little-endian platforms
# fail the build: a set or map key ordered by plain char, an enum without a
# fixed underlying type. The portable spellings must build.
foreach(entry
        "set_char|a set or map key orders by plain char"
        "map_char|a set or map key orders by plain char"
        "char_enum|a set or map key orders by plain char"
        "record_member|a set or map key orders by plain char"
        "array_member|a set or map key orders by plain char"
        "unfixed_enum|an enum without a fixed underlying type has a compiler-chosen representation"
        "portable|")
    string(REGEX REPLACE "\\|.*$" "" case "${entry}")
    string(REGEX REPLACE "^[^|]*\\|" "" diagnostic "${entry}")
    set(name portable_schema_${case}_fail)
    if(case STREQUAL "portable")
        set(name portable_schema_portable_build)
    endif()
    add_executable(${name} EXCLUDE_FROM_ALL portable_schema/key_fail.cpp)
    configure_xoffset_target(${name})
    target_compile_definitions(${name} PRIVATE XOFFSET_TEST_CASE_${case})
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} --target ${name} --config $<CONFIG>)
    if(diagnostic)
        set_tests_properties(${name} PROPERTIES LABELS "schema" PASS_REGULAR_EXPRESSION "${diagnostic}")
    else()
        set_tests_properties(${name} PROPERTIES LABELS "schema")
    endif()
endforeach()

# Without reflection, raw receive of an enum without a fixed underlying type
# must fail the build clearly (test_receive_raw runs these enums only in C++26).
if(NOT XOFFSET_TESTS_REFLECTION)
    foreach(enum Nonfixed Negative Zero EmptyEnum Wide UnsignedWide)
        add_executable(receive_raw_nonfixed_${enum}_fail EXCLUDE_FROM_ALL receive_raw/nonfixed_enum_fail.cpp)
        configure_xoffset_target(receive_raw_nonfixed_${enum}_fail)
        target_compile_definitions(receive_raw_nonfixed_${enum}_fail PRIVATE XOFFSET_TEST_ENUM=${enum})
        add_test(NAME receive_raw_nonfixed_${enum}_fail
            COMMAND ${CMAKE_COMMAND} --build ${PROJECT_BINARY_DIR} --target receive_raw_nonfixed_${enum}_fail --config $<CONFIG>)
        set_tests_properties(receive_raw_nonfixed_${enum}_fail PROPERTIES
            LABELS "schema" PASS_REGULAR_EXPRESSION
            "the enumerators of an enum without a fixed underlying type are unknown")
    endforeach()
endif()

# Explicit new common programs; no source glob or handwritten total.
include("${PROJECT_SOURCE_DIR}/cmake/XOffsetFourPathTests.cmake")
# These are mandatory configure-time checks, not runtime test programs.
xoffset_check_polymorphic_rejection()
xoffset_test_host_modules()
xoffset_write_test_inventory()

