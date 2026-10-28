include_guard(GLOBAL)

# Test host for phones (tools/test_host): an iPhone runs no command-line programs, so
# each test, the demo client and the benchmarks are built a second time as a shared
# library libxoh_<program> (same sources and settings, plus hosted_shim.cpp) and one
# app loads one of them per launch. On Linux and macOS the same libraries run under
# the command-line host, which checks the whole flow without a phone.
option(XOFFSET_TEST_HOST "Also build every program as a library for the test host (tools/test_host)" OFF)
# The programs of the phone measurements (xo_mem_probe, xo_slg_bench), for Android
# without the host; the test host builds them anyway.
option(XOFFSET_PHONE_PROGRAMS "Build the phone measurement programs (tools/test_host)" ${XOFFSET_TEST_HOST})
set(XOFFSET_TEST_HOST_DIR "${CMAKE_CURRENT_LIST_DIR}/../tools/test_host")
get_filename_component(XOFFSET_TEST_HOST_DIR "${XOFFSET_TEST_HOST_DIR}" ABSOLUTE)

# The library for one executable target: libxoh_<target> in <build>/hosted.
function(xoffset_test_host_module target)
    set(module xoh_${target})
    get_target_property(sources ${target} SOURCES)
    get_target_property(source_dir ${target} SOURCE_DIR)
    set(absolute_sources "")
    foreach(source IN LISTS sources)
        if(IS_ABSOLUTE "${source}" OR source MATCHES "^\\$<")
            list(APPEND absolute_sources "${source}")
        else()
            list(APPEND absolute_sources "${source_dir}/${source}")
        endif()
    endforeach()
    add_library(${module} SHARED ${absolute_sources} "${XOFFSET_TEST_HOST_DIR}/hosted_shim.cpp")
    set_source_files_properties("${XOFFSET_TEST_HOST_DIR}/hosted_shim.cpp" TARGET_DIRECTORY ${module}
        PROPERTIES COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU>:-Wno-attributes>")
    foreach(property COMPILE_DEFINITIONS COMPILE_OPTIONS COMPILE_FEATURES INCLUDE_DIRECTORIES
                     LINK_LIBRARIES LINK_OPTIONS CXX_STANDARD CXX_STANDARD_REQUIRED CXX_EXTENSIONS)
        get_target_property(value ${target} ${property})
        if(NOT value STREQUAL "value-NOTFOUND")
            set_property(TARGET ${module} PROPERTY ${property} "${value}")
        endif()
    endforeach()
    target_include_directories(${module} PRIVATE "${XOFFSET_TEST_HOST_DIR}")
    if(NOT APPLE)
        # ELF: the program's calls to exit and _Exit bind to the shim's definitions
        # (GCC drops the hidden attribute on these builtins). Mach-O binds them
        # inside the library anyway.
        target_link_options(${module} PRIVATE -Wl,-Bsymbolic-functions)
    endif()
    set_target_properties(${module} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/hosted"
        PREFIX "lib" OUTPUT_NAME ${module})
    set_property(GLOBAL APPEND PROPERTY XOFFSET_TEST_HOST_MODULES ${module})
endfunction()

# Call at the end of a directory's CMakeLists.txt: every executable of the directory
# that is part of the default build gets its library.
function(xoffset_test_host_modules)
    if(NOT XOFFSET_TEST_HOST)
        return()
    endif()
    get_property(targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        get_target_property(excluded ${target} EXCLUDE_FROM_ALL)
        if(type STREQUAL "EXECUTABLE" AND NOT excluded)
            xoffset_test_host_module(${target})
        endif()
    endforeach()
endfunction()
