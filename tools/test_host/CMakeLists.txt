# Test host (cmake/XOffsetTestHost.cmake, host_core.hpp), with -DXOFFSET_TEST_HOST=ON;
# -DXOFFSET_PHONE_PROGRAMS=ON alone builds only the two programs below. iOS: the app xoffset_test_host.app with every hosted
# program library in its Frameworks/; elsewhere: the command-line host
# bin/<config>/xoffset_test_host and the libraries in <build>/hosted.
#
# Programs that exist only for phone measurements live here too:
#   xo_mem_probe   SC-12: address space and memory a process may use (mem_probe.cpp)
#   xo_slg_bench   SC-6: bench/slg/bench/slg_bench.cpp on the field-list backend, with
#                  FlatBuffers when -DXO_SLG_FLATBUFFERS_INCLUDE (flatbuffers headers) and
#                  -DXO_SLG_GENERATED (directory with slg_generated.h from flatc) are given

add_executable(xo_mem_probe mem_probe.cpp)
configure_xoffset_target(xo_mem_probe)

set(XO_SLG_FLATBUFFERS_INCLUDE "" CACHE PATH "flatbuffers include directory for xo_slg_bench")
set(XO_SLG_GENERATED "" CACHE PATH "directory with slg_generated.h (flatc --cpp bench/slg/schema/slg.fbs)")
set(XO_SLG_CEREAL_INCLUDE "" CACHE PATH "cereal include directory for xo_slg_bench (optional)")
set(slg "${PROJECT_SOURCE_DIR}/bench/slg")
add_executable(xo_slg_bench "${slg}/bench/slg_bench.cpp")
configure_xoffset_target(xo_slg_bench)
target_include_directories(xo_slg_bench PRIVATE "${slg}/include" "${slg}/adapters" "${slg}/bench")
target_compile_definitions(xo_slg_bench PRIVATE SLG_WITH_XOFFSET=1)
if(XO_SLG_FLATBUFFERS_INCLUDE AND XO_SLG_GENERATED)
    target_include_directories(xo_slg_bench SYSTEM PRIVATE "${XO_SLG_FLATBUFFERS_INCLUDE}" "${XO_SLG_GENERATED}")
    target_compile_definitions(xo_slg_bench PRIVATE SLG_WITH_FLATBUFFERS=1)
endif()
if(XO_SLG_CEREAL_INCLUDE)
    target_include_directories(xo_slg_bench SYSTEM PRIVATE "${XO_SLG_CEREAL_INCLUDE}")
    target_compile_definitions(xo_slg_bench PRIVATE SLG_WITH_CEREAL=1)
endif()
xoffset_test_host_modules()
if(NOT XOFFSET_TEST_HOST)
    return()
endif()

get_property(modules GLOBAL PROPERTY XOFFSET_TEST_HOST_MODULES)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
    set(XO_HOST_BUNDLE_ID "com.ximicpp.xoffset.testhost" CACHE STRING "Bundle identifier of the test host app")
    if(CMAKE_OSX_SYSROOT MATCHES "simulator")
        set(XO_HOST_SDK iphonesimulator)
        set(XO_HOST_PLATFORM iPhoneSimulator)
    else()
        set(XO_HOST_SDK iphoneos)
        set(XO_HOST_PLATFORM iPhoneOS)
    endif()
    execute_process(COMMAND xcrun --sdk ${XO_HOST_SDK} --show-sdk-version
        OUTPUT_VARIABLE XO_HOST_SDK_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(XO_HOST_MIN_OS "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    configure_file(Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)
    add_executable(xoffset_test_host MACOSX_BUNDLE host_ios.mm host_core.cpp)
    set_target_properties(xoffset_test_host PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/$<CONFIG>")
    target_compile_features(xoffset_test_host PRIVATE cxx_std_17)
    target_compile_options(xoffset_test_host PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    target_link_libraries(xoffset_test_host PRIVATE "-framework UIKit" "-framework CoreGraphics" "-framework Foundation")
    set(frameworks "$<TARGET_BUNDLE_DIR:xoffset_test_host>/Frameworks")
else()
    add_executable(xoffset_test_host host_cli.cpp host_core.cpp)
    set_target_properties(xoffset_test_host PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/$<CONFIG>")
    target_compile_features(xoffset_test_host PRIVATE cxx_std_17)
    find_package(Threads REQUIRED)
    target_link_libraries(xoffset_test_host PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    set(frameworks "$<TARGET_FILE_DIR:xoffset_test_host>/hosted")
endif()

# Copies the libraries next to the host on every build (a changed library does not
# relink the host). The app's code signature is the driver's business (xo_host_run.py).
set(files "")
foreach(module IN LISTS modules)
    list(APPEND files "$<TARGET_FILE:${module}>")
endforeach()
add_custom_target(xoffset_test_host_bundle ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${frameworks}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${files} "${frameworks}"
    COMMENT "test host: ${CMAKE_SYSTEM_NAME} host and its program libraries"
    VERBATIM)
add_dependencies(xoffset_test_host_bundle xoffset_test_host ${modules})
