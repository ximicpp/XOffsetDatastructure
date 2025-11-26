# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Helper function for macOS RPATH settings
function(configure_macos_target target_name)
    if(APPLE)
        set_target_properties(${target_name} PROPERTIES
            SKIP_BUILD_RPATH TRUE
            BUILD_WITH_INSTALL_RPATH FALSE
            INSTALL_RPATH ""
        )
        target_link_options(${target_name} PRIVATE
            -Wl,-rpath,/usr/lib
            -Wl,-rpath,/usr/local/lib
        )
    endif()
    # Set iOS bundle identifier if building for iOS
    set_ios_bundle_id(${target_name})
endfunction()

# Test executables
add_executable(test_basic_types test_basic_types.cpp)
target_include_directories(test_basic_types PRIVATE 
    ${BOOST_INCLUDE_DIRS} 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/generated
)
set_target_properties(test_basic_types PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_basic_types)
if(Python3_FOUND)
    add_dependencies(test_basic_types generate_schemas)
endif()

add_executable(test_vector test_vector.cpp)
target_include_directories(test_vector PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_vector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_vector)

add_executable(test_map_set test_map_set.cpp)
target_include_directories(test_map_set PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_map_set PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_map_set)

add_executable(test_nested test_nested.cpp)
target_include_directories(test_nested PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_nested PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_nested)

add_executable(test_compaction test_compaction.cpp)
target_include_directories(test_compaction PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_compaction PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_compaction)

add_executable(test_modify test_modify.cpp)
target_include_directories(test_modify PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_modify PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_modify)

add_executable(test_comprehensive test_comprehensive.cpp)
target_include_directories(test_comprehensive PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_comprehensive PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_comprehensive)

add_executable(test_serialization test_serialization.cpp)
target_include_directories(test_serialization PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_serialization PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_serialization)

add_executable(test_alignment test_alignment.cpp)
target_include_directories(test_alignment PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_alignment PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_alignment)

# Type signature verification test
add_executable(test_type_signature test_type_signature.cpp)
target_include_directories(test_type_signature PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_type_signature PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_type_signature)

# Platform requirements test (64-bit, little-endian)
add_executable(test_platform test_platform.cpp)
target_include_directories(test_platform PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_platform PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_platform)

# MSVC compatibility test (comprehensive test suite for MSVC-specific issues)
add_executable(test_msvc_compat test_msvc_compat.cpp)
target_include_directories(test_msvc_compat PRIVATE 
    ${BOOST_INCLUDE_DIRS} 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/generated
)
set_target_properties(test_msvc_compat PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_msvc_compat)
if(Python3_FOUND)
    add_dependencies(test_msvc_compat generate_schemas)
endif()

# Add CTest tests
add_test(NAME BasicTypes COMMAND test_basic_types)
add_test(NAME VectorOps COMMAND test_vector)
add_test(NAME MapSetOps COMMAND test_map_set)
add_test(NAME NestedStructures COMMAND test_nested)
add_test(NAME MemoryStats COMMAND test_compaction)
add_test(NAME DataModification COMMAND test_modify)
add_test(NAME Comprehensive COMMAND test_comprehensive)
add_test(NAME Serialization COMMAND test_serialization)
add_test(NAME Alignment COMMAND test_alignment)
add_test(NAME TypeSignature COMMAND test_type_signature)
add_test(NAME Platform COMMAND test_platform)
add_test(NAME MSVCCompat COMMAND test_msvc_compat)

# const T support test
add_executable(test_const_support test_const_support.cpp)
target_include_directories(test_const_support PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_const_support PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
configure_macos_target(test_const_support)
add_test(NAME ConstSupport COMMAND test_const_support)

