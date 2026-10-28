# Device mesh demo (demo/mesh/INTERFACES.md). Built only with
# -DXOFFSET_BUILD_MESH_DEMO=ON.
# OWNER: network thread. The types thread owns the xmesh_core source list
# (src/types/, src/hop.cpp) and the self-test lines; other changes go
# through the design thread.
#
# Programs: xo_hub (the hub, Linux), xo_peer (the device program, every
# device), xo_peer_noexc (the same without exceptions, as engine clients
# build), xmesh_selftest (types, hops and evil cases in memory; writes and
# checks the cross-device corpus) and xmesh_schema_freeze (schema.lock).
#
# XMESH_PEER_ONLY=ON builds xo_peer, xo_peer_noexc and xmesh_selftest
# alone: the device targets (MinGW, MSVC, clang-cl, Android NDK, iOS
# Simulator, macOS) build them with XOFFSET_CXX_STANDARD=20 and the
# XOFFSET_RECORD field lists; the hub runs on Linux in either backend.

option(XMESH_PEER_ONLY "Build only the device programs (xo_peer, xo_peer_noexc, xmesh_selftest)" OFF)
option(XMESH_NOEXC_PEER "Also build xo_peer_noexc, the device program without exceptions" ON)

# Types, hops (in memory) and the network layer, shared by every program.
function(xmesh_core_library name)
    add_library(${name} STATIC
        src/types/names.cpp
        src/types/objects.cpp
        src/hop.cpp
        src/net.cpp)
    target_include_directories(${name} PUBLIC include)
    target_link_libraries(${name} PUBLIC xoffset_common)
    if(WIN32)
        target_link_libraries(${name} PUBLIC ws2_32)
        target_compile_definitions(${name} PUBLIC _CRT_SECURE_NO_WARNINGS)
    endif()
    # Bitwise-identical floats on every device and in the hub's replay
    # (types.hpp). MSVC and clang-cl do not contract by default.
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
        target_compile_options(${name} PUBLIC -ffp-contract=off)
    endif()
endfunction()

function(xmesh_executable name core)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE ${core})
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/$<CONFIG>)
endfunction()

xmesh_core_library(xmesh_core)
xmesh_executable(xo_peer xmesh_core src/peer_main.cpp)
xmesh_executable(xmesh_selftest xmesh_core src/selftest.cpp)

if(XMESH_NOEXC_PEER)
    xmesh_core_library(xmesh_core_noexc)
    xoffset_disable_exceptions(xmesh_core_noexc)
    xmesh_executable(xo_peer_noexc xmesh_core_noexc src/peer_main.cpp)
    xoffset_disable_exceptions(xo_peer_noexc)
    message(STATUS "xmesh: xo_peer_noexc builds without exceptions")
endif()

if(NOT XMESH_PEER_ONLY)
    xmesh_executable(xo_hub xmesh_core src/hub_main.cpp)
    xmesh_executable(xmesh_schema_freeze xmesh_core src/schema_freeze.cpp)
endif()

xoffset_test_host_modules()
