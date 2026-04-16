file(GLOB XOFFSET_SIG_BASELINES
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "sigs/*.sig.hpp")
list(SORT XOFFSET_SIG_BASELINES)

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
