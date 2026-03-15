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

# Compatibility Check Tool — DISABLED
# Requires compat_auto.hpp and definition_match() which were removed
# in the TypeLayout main-branch cleanup.  Will be re-enabled once the
# tool is updated to use the new compat_check.hpp API.
#
# add_executable(check_compat check_compat.cpp)
# target_include_directories(check_compat PRIVATE
#     ${TYPELAYOUT_INCLUDE_DIRS}
#     ${CMAKE_SOURCE_DIR}/tools
# )
# set_target_properties(check_compat PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
# )
