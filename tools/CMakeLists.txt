set(XOFFSET_SIG_BASELINES
    sigs/arm64_android_clang.sig.hpp
    sigs/arm64_ios_clang.sig.hpp
    sigs/x86_64_linux_clang.sig.hpp
    sigs/x86_64_windows_clang.sig.hpp)

file(GLOB XOFFSET_SIG_BASELINES_ACTUAL
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "sigs/*.sig.hpp")
list(SORT XOFFSET_SIG_BASELINES_ACTUAL)
list(SORT XOFFSET_SIG_BASELINES)

foreach(sig_header IN LISTS XOFFSET_SIG_BASELINES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${sig_header}")
        message(FATAL_ERROR "Missing required signature baseline: tools/${sig_header}")
    endif()
endforeach()

foreach(sig_header IN LISTS XOFFSET_SIG_BASELINES_ACTUAL)
    if(NOT sig_header IN_LIST XOFFSET_SIG_BASELINES)
        message(FATAL_ERROR
            "Unexpected signature baseline outside the target matrix: tools/${sig_header}")
    endif()
endforeach()

set(XOFFSET_CHECK_COMPAT_INCLUDES "")
set(XOFFSET_CHECK_COMPAT_ADD_PLATFORMS "")
set(XOFFSET_CHECK_COMPAT_PLATFORM_NAMES "")

foreach(sig_header IN LISTS XOFFSET_SIG_BASELINES)
    get_filename_component(sig_filename "${sig_header}" NAME)
    string(REGEX REPLACE "\\.sig\\.hpp$" "" sig_namespace "${sig_filename}")

    string(APPEND XOFFSET_CHECK_COMPAT_INCLUDES
        "#include \"tools/${sig_header}\"\n")
    string(APPEND XOFFSET_CHECK_COMPAT_ADD_PLATFORMS
        "    reporter.add_platform(::boost::typelayout::platform::${sig_namespace}::get_platform_info());\n")
    string(APPEND XOFFSET_CHECK_COMPAT_PLATFORM_NAMES
        "        \"${sig_namespace}\",\n")
endforeach()

list(LENGTH XOFFSET_SIG_BASELINES XOFFSET_CHECK_COMPAT_PLATFORM_COUNT)

configure_file(
    check_compat_baselines.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/check_compat_baselines.hpp"
    @ONLY)

add_executable(export_signatures export_signatures.cpp)
configure_xoffset_target(export_signatures)

add_executable(bench_compare bench_compare.cpp)
configure_xoffset_target(bench_compare)

add_test(
    NAME check_signature_regeneration
    COMMAND ${CMAKE_COMMAND}
        -DEXPORTER=$<TARGET_FILE:export_signatures>
        -DSOURCE_SIG_DIR=${CMAKE_SOURCE_DIR}/tools/sigs
        -DGENERATED_SIG_DIR=${CMAKE_BINARY_DIR}/generated_sigs
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_signature_regeneration.cmake)
set_tests_properties(check_signature_regeneration PROPERTIES LABELS "tooling")

# Exact target-matrix inventory verifier.
add_executable(check_signature_matrix check_signature_matrix.cpp)
target_compile_features(check_signature_matrix PRIVATE cxx_std_17)
target_include_directories(check_signature_matrix PRIVATE
    "${CMAKE_SOURCE_DIR}")
set_target_properties(check_signature_matrix PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
add_test(
    NAME check_signature_matrix
    COMMAND $<TARGET_FILE:check_signature_matrix> "${CMAKE_SOURCE_DIR}/tools/sigs")
set_tests_properties(check_signature_matrix PROPERTIES LABELS "tooling")

# Lightweight baseline verifier: consumes committed .sig.hpp files only.
add_executable(check_compat check_compat.cpp)
target_compile_features(check_compat PRIVATE cxx_std_17)
target_include_directories(check_compat PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}")
target_include_directories(check_compat SYSTEM PRIVATE
    ${TYPELAYOUT_INCLUDE_DIRS})
set_target_properties(check_compat PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
add_test(NAME check_compat COMMAND $<TARGET_FILE:check_compat>)
set_tests_properties(check_compat PROPERTIES LABELS "tooling")
