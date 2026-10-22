# Map state demo (demo/mapstate/INTERFACES.md). Built only with -DXOFFSET_BUILD_DEMOS=ON.
# OWNER: integration thread. Other threads ask before changing target lists.

add_library(mapdemo_common STATIC src/net.cpp src/metrics.cpp src/checks.cpp)
target_include_directories(mapdemo_common PUBLIC include)
target_link_libraries(mapdemo_common PUBLIC xoffset_common)

# Bitwise-identical floats between B (real state) and A's reference model.
target_compile_options(mapdemo_common PUBLIC -ffp-contract=off)

function(mapdemo_executable name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE mapdemo_common)
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/$<CONFIG>)
endfunction()

mapdemo_executable(mapdemo_b src/server_b.cpp)
mapdemo_executable(mapdemo_a src/server_a.cpp)
mapdemo_executable(mapdemo_selftest src/selftest.cpp)
mapdemo_executable(mapdemo_client src/client_main.cpp src/client_core.cpp)

# Window client: raylib 5.5 (commit c1ab645ca298a2801097931d1079b10ff7eb9df8),
# prebuilt as a C library and found through CMAKE_PREFIX_PATH.
find_package(raylib 5.5 CONFIG QUIET)
if(raylib_FOUND)
    target_sources(mapdemo_client PRIVATE src/client_gui.cpp)
    target_link_libraries(mapdemo_client PRIVATE raylib)
    target_compile_definitions(mapdemo_client PRIVATE MAPDEMO_WITH_GUI=1)
    if(APPLE)
        target_link_libraries(mapdemo_client PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" "-framework OpenGL")
    endif()
    message(STATUS "mapdemo: window client enabled (raylib ${raylib_VERSION})")
else()
    message(STATUS "mapdemo: raylib not found; mapdemo_client is --headless only")
endif()
