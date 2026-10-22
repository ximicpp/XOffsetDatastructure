cmake_minimum_required(VERSION 3.30)
project(XOffsetFormalHarness LANGUAGES CXX)

# XOffset part of the R3 formal acceptance harness (meta-r3 serialization_comparison).
# Same compile settings as the R3 tree: GNU++26 for every translation unit,
# -O3 -DNDEBUG for the driver; competitor adapters stay out of the repository.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Threads REQUIRED)
option(SC_ENABLE_SANITIZERS "Separate adapter/header ASan+UBSan correctness profile" OFF)
if(SC_ENABLE_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()
# Instruction-count build for the CI performance gate (SC_IR_PHASE, see README.md).
# OFF leaves the timed region unchanged.
option(SC_IR_COUNT "Toggle callgrind collection around one run phase" OFF)

# In the repository the library is two levels up (bench/formal -> root).
get_filename_component(_sc_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(SC_XOFFSET_SOURCE "${_sc_repo_root}" CACHE PATH "XOffset source directory")
if(NOT EXISTS "${SC_XOFFSET_SOURCE}/xoffsetdatastructure.hpp")
  message(FATAL_ERROR "SC_XOFFSET_SOURCE must name an XOffset source directory")
endif()

set(XOFFSET_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(XOFFSET_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(XOFFSET_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(XOFFSET_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${SC_XOFFSET_SOURCE}" "${CMAKE_BINARY_DIR}/xoffset-interface" EXCLUDE_FROM_ALL)
# The library declares strict c++26 in its interface. The harness deliberately
# selects GNU++26 for every runtime; keep reflection, expansion and runtime
# options, remove only the conflicting dialect spelling.
get_target_property(_sc_xoffset_options xoffset_common INTERFACE_COMPILE_OPTIONS)
if(_sc_xoffset_options)
  list(REMOVE_ITEM _sc_xoffset_options "-std=c++26")
  set_property(TARGET xoffset_common PROPERTY INTERFACE_COMPILE_OPTIONS "${_sc_xoffset_options}")
endif()

add_executable(sc_xoffset driver.cpp)
target_include_directories(sc_xoffset PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(sc_xoffset PRIVATE "SC_ADAPTER_HEADER=\"adapters/xoffset/adapter.hpp\"" "SC_ADAPTER_TYPE=sc::XOffsetAdapter")
target_compile_options(sc_xoffset PRIVATE -O3 -DNDEBUG -Wall -Wextra -Wpedantic)
target_link_libraries(sc_xoffset PRIVATE Threads::Threads XOffset::xoffset)
set_target_properties(sc_xoffset PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(SC_IR_COUNT)
  find_path(VALGRIND_INCLUDE valgrind/callgrind.h REQUIRED)
  target_include_directories(sc_xoffset SYSTEM PRIVATE ${VALGRIND_INCLUDE})
  target_compile_definitions(sc_xoffset PRIVATE SC_IR_COUNT=1)
endif()

enable_testing()
add_executable(sc_model_contract tests/contracts/model_contract.cpp)
target_include_directories(sc_model_contract PRIVATE include)
target_compile_options(sc_model_contract PRIVATE -O2 -Wall -Wextra -Wpedantic)
set_target_properties(sc_model_contract PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME model_contract COMMAND sc_model_contract)
add_executable(sc_view_contract tests/view_contract.cpp)
target_include_directories(sc_view_contract PRIVATE include)
target_compile_options(sc_view_contract PRIVATE -O2 -Wall -Wextra -Wpedantic)
set_target_properties(sc_view_contract PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME view_contract COMMAND sc_view_contract)

function(sc_driver_contract name source)
  add_executable(${name} ${source})
  foreach(prop IN ITEMS INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS LINK_LIBRARIES)
    get_target_property(settings sc_xoffset ${prop})
    if(settings)
      set_property(TARGET ${name} PROPERTY ${prop} "${settings}")
    endif()
  endforeach()
  set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  add_test(NAME ${name} COMMAND ${name})
endfunction()
sc_driver_contract(sc_xoffset_contract tests/contracts/adapter_contract.cpp)
sc_driver_contract(sc_xoffset_resource tests/contracts/resource_failure.cpp)
