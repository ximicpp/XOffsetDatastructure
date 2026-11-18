set(XOFFSET_FOUR_PATH_TEST_PROGRAMS
    test_verified_paths
    test_stable_erase_if
    test_locator_soundness
)
foreach(program IN LISTS XOFFSET_FOUR_PATH_TEST_PROGRAMS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${program}.cpp" OR TARGET ${program})
        message(FATAL_ERROR "Missing/duplicate explicit four-path test: ${program}")
    endif()
    add_xoffset_test(${program} "reflection;four-paths")
endforeach()

add_xoffset_test(test_core_lifetime reflection)
