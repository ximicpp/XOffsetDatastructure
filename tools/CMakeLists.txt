# Tools CMakeLists.txt

# Signature Export Tool (requires P2996 Clang)
add_executable(export_signatures export_signatures.cpp)
target_include_directories(export_signatures PRIVATE
    ${TYPELAYOUT_INCLUDE_DIRS}
    ${BOOST_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
)
set_target_properties(export_signatures PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Compatibility Check Tool (C++17 — no P2996 required)
add_executable(check_compat check_compat.cpp)
target_include_directories(check_compat PRIVATE
    ${TYPELAYOUT_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/tools
)
set_target_properties(check_compat PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
