function(add_xoffset_test test_name test_label)
    add_executable(${test_name} ${test_name}.cpp)
    configure_xoffset_target(${test_name})
    xoffset_enable_test_assertions(${test_name})
    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
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
    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
    set_tests_properties(${test_name} PROPERTIES LABELS "storage")
endforeach()

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
    test_resize_growth
    test_reflection_core
    test_reflection_advanced
    test_type_signatures
    test_field_limit_fix
    test_type_safety
    test_wire_admission
    test_enum_support
    test_xstring_direct_assign
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
    test_free_table_policy
    test_stable_storage
    test_vm_lifecycle
    test_vm_transport
    test_owner_identity
    test_owner_registry
    test_tail_expansion
    test_storage_policies
    test_allocator_model
    test_allocator_failures
    test_v2_golden
    test_cross_compiler_wire
    test_schema_pin
    test_stability_contract
    test_wire_hole_zeroing
    test_implicit_padding
    test_v2_fuzz
    test_reload_reset
    test_clone
)

foreach(test_name ${REFLECTION_TESTS})
    add_xoffset_test(${test_name} reflection)
endforeach()

find_package(Threads REQUIRED)
target_link_libraries(test_owner_identity PRIVATE Threads::Threads)

# G5/F4: the two libFuzzer harnesses linked with fuzz/replay_main.cpp instead of
# libFuzzer, so every compiler builds them (independent of XOFFSET_BUILD_FUZZERS).
#   fuzz_<target>_replay          replays fuzz/regressions/<target>/: crash
#                                 inputs kept by fix PRs (see its README.md)
#   fuzz_<target>_replay_corpus   replays the harness's own seed corpus, written
#                                 by fuzz_<target>_replay_seeds
# Names start with the target name so tools/validate_vm.py --targets selects all three.
foreach(fuzz_target v2_wire wire_schemas)
    set(replay fuzz_${fuzz_target}_replay)
    add_executable(${replay} fuzz/fuzz_${fuzz_target}.cpp fuzz/replay_main.cpp)
    configure_xoffset_target(${replay})
    xoffset_enable_test_assertions(${replay})
    if(XOFFSET_FUZZ_INJECT_CRASH)
        target_compile_definitions(${replay} PRIVATE XOFFSET_FUZZ_INJECT_CRASH)
    endif()
    add_test(NAME ${replay}
        COMMAND $<TARGET_FILE:${replay}> "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/regressions/${fuzz_target}")
    set(replay_seeds "${CMAKE_CURRENT_BINARY_DIR}/replay-seeds/${fuzz_target}")
    add_test(NAME ${replay}_seeds COMMAND $<TARGET_FILE:${replay}> --write-seeds "${replay_seeds}")
    add_test(NAME ${replay}_corpus COMMAND $<TARGET_FILE:${replay}> "${replay_seeds}")
    set_tests_properties(${replay} ${replay}_seeds ${replay}_corpus PROPERTIES LABELS "reflection;replay")
    set_tests_properties(${replay}_seeds PROPERTIES FIXTURES_SETUP ${replay}_seeds)
    set_tests_properties(${replay}_corpus PROPERTIES FIXTURES_REQUIRED ${replay}_seeds)
endforeach()

# test_v2_fuzz --replay reruns one case alone; it must match the full run.
add_test(NAME test_v2_fuzz_replay COMMAND $<TARGET_FILE:test_v2_fuzz> --replay 20261021 17)
add_test(NAME test_v2_fuzz_replay_boundary COMMAND $<TARGET_FILE:test_v2_fuzz> --replay 20261021 b302)
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
        COMMAND $<TARGET_FILE:test_vm_lifecycle> --exit-${exit_mode})
    set_tests_properties(test_vm_lifecycle_${exit_mode} PROPERTIES LABELS "reflection;lifecycle")
endforeach()

# Real host allocation failures inside unordered_map, independently of the
# pre-registration fault hook. Rehash needs a fresh process/bucket inventory.
set(REGISTRY_FAILURE_MODES
    node
    rehash
)
foreach(failure_mode ${REGISTRY_FAILURE_MODES})
    add_test(NAME test_owner_registry_fail_${failure_mode}
        COMMAND $<TARGET_FILE:test_owner_registry> --fail-${failure_mode})
    set_tests_properties(test_owner_registry_fail_${failure_mode} PROPERTIES LABELS "reflection;registry")
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

# Explicit new common programs; no source glob or handwritten total.
include("${PROJECT_SOURCE_DIR}/cmake/XOffsetFourPathTests.cmake")
# These are mandatory configure-time checks, not runtime test programs.
xoffset_check_polymorphic_rejection()
xoffset_write_test_inventory()

