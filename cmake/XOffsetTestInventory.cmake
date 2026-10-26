include_guard(GLOBAL)

function(xoffset_inventory_array output_variable)
    set(items "")
    foreach(item IN LISTS ARGN)
        if(NOT items STREQUAL "")
            string(APPEND items ", ")
        endif()
        string(APPEND items "\"${item}\"")
    endforeach()
    set(${output_variable} "[${items}]" PARENT_SCOPE)
endfunction()

# Called from tests/ after all add_test calls. Inventory comes from registered
# CMake tests/targets, rather than source-file globs or a handwritten total.
function(xoffset_write_test_inventory)
    get_property(test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    set(programs "")
    foreach(target_name IN LISTS test_targets)
        get_target_property(target_type ${target_name} TYPE)
        if(target_type STREQUAL "EXECUTABLE")
            list(APPEND programs "${target_name}")
        endif()
    endforeach()
    get_property(runtime_tests DIRECTORY PROPERTY TESTS)
    set(scenarios ${runtime_tests})
    foreach(program IN LISTS programs)
        if(NOT program IN_LIST runtime_tests)
            message(FATAL_ERROR "Test executable ${program} has no registered CTest entry")
        endif()
        list(REMOVE_ITEM scenarios "${program}")
    endforeach()
    if(XOFFSET_STORAGE_ONLY)
        set(mode storage-only)
        set(tooling "")
    else()
        set(mode full)
        set(tooling "")
        if(XOFFSET_BUILD_TOOLS)
            get_property(tooling DIRECTORY "${PROJECT_SOURCE_DIR}/tools" PROPERTY TESTS)
        endif()
    endif()
    get_property(configure_checks GLOBAL PROPERTY XOFFSET_CONFIGURE_CHECKS)
    get_property(cxx26_only GLOBAL PROPERTY XOFFSET_CXX26_ONLY_NOT_BUILT)
    foreach(category programs scenarios tooling configure_checks cxx26_only)
        list(SORT ${category})
        list(LENGTH ${category} ${category}_count)
        xoffset_inventory_array(${category}_json ${${category}})
    endforeach()
    math(EXPR ctest_count "${programs_count} + ${scenarios_count} + ${tooling_count}")
    file(WRITE "${PROJECT_BINARY_DIR}/test-inventory.json"
        "{\n  \"mode\": \"${mode}\",\n"
        "  \"cxx_standard\": ${XOFFSET_CXX_STANDARD},\n"
        "  \"runtime_test_program_count\": ${programs_count},\n"
        "  \"subprocess_scenario_count\": ${scenarios_count},\n"
        "  \"tooling_check_count\": ${tooling_count},\n"
        "  \"ctest_count\": ${ctest_count},\n"
        "  \"configure_check_count\": ${configure_checks_count},\n"
        "  \"runtime_test_programs\": ${programs_json},\n"
        "  \"subprocess_scenarios\": ${scenarios_json},\n"
        "  \"tooling_checks\": ${tooling_json},\n"
        "  \"configure_checks\": ${configure_checks_json},\n"
        "  \"cxx26_only_not_built\": ${cxx26_only_json}\n}\n")
    message(STATUS "CTest inventory: ${programs_count} test programs + ${scenarios_count} subprocess scenarios + ${tooling_count} tooling checks = ${ctest_count}; ${configure_checks_count} configure checks listed separately")
    if(cxx26_only_count GREATER 0)
        message(STATUS "C++${XOFFSET_CXX_STANDARD} build: ${cxx26_only_count} C++26-only tests not built (listed in test-inventory.json)")
    endif()
    message(STATUS "Test inventory: ${PROJECT_BINARY_DIR}/test-inventory.json")
endfunction()
