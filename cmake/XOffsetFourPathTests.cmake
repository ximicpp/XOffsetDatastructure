set(XOFFSET_FOUR_PATH_TEST_PROGRAMS
    test_verified_paths
    test_stable_erase_if
    test_locator_soundness
    test_snapshot_index_header_only
)
foreach(program IN LISTS XOFFSET_FOUR_PATH_TEST_PROGRAMS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${program}.cpp" OR TARGET ${program})
        message(FATAL_ERROR "Missing/duplicate explicit four-path test: ${program}")
    endif()
    add_xoffset_test(${program} "reflection;four-paths")
endforeach()

add_xoffset_test(test_core_lifetime reflection)

add_xoffset_test(test_custom_future_lifetime reflection)

add_xoffset_test(test_receive_raw reflection)

add_xoffset_test(test_verified_receive reflection)

add_xoffset_test(test_canonical_registry_v2 reflection)

# Canonical cold recovery must not introduce callback boundaries.
add_xoffset_test(test_canonical_cold_no_callback reflection)
add_xoffset_test(test_canonical_zero_target reflection)
# Exit 77: built without per-thread caches (XOFFSET_THREAD_CACHES=0, GCC on MinGW).
set_tests_properties(test_canonical_cold_no_callback test_canonical_zero_target PROPERTIES SKIP_RETURN_CODE 77)
