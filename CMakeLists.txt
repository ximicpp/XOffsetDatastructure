cmake_minimum_required(VERSION 3.10)
project(XOffsetDatastructure)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS specific settings
if(APPLE)
    # Use the system C++ library
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    
    # Set RPATH settings globally
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "@executable_path;@loader_path")
    
    # Ensure we use the correct deployment target
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version" FORCE)
    endif()
endif()

# Enable testing
enable_testing()

# Find Python for code generation
find_package(Python3 COMPONENTS Interpreter)

# Set Boost include directories
set(BOOST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/external/boost
    ${CMAKE_SOURCE_DIR}/external/boost/libs/any/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/container/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/interprocess/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/type_index/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/core/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/config/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/throw_exception/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/assert/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/static_assert/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/container_hash/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/pfr/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/move/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/type_traits/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/winapi/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/intrusive/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/predef/include
)

# Schema code generation
if(Python3_FOUND)
    set(SCHEMA_FILES
        ${CMAKE_SOURCE_DIR}/schemas/test_types.xds.yaml
        ${CMAKE_SOURCE_DIR}/schemas/basic_types.xds.yaml
        ${CMAKE_SOURCE_DIR}/schemas/vector_test.xds.yaml
        ${CMAKE_SOURCE_DIR}/schemas/map_set_test.xds.yaml
        ${CMAKE_SOURCE_DIR}/schemas/nested_test.xds.yaml
        ${CMAKE_SOURCE_DIR}/schemas/modify_test.xds.yaml
        ${CMAKE_SOURCE_DIR}/schemas/compaction_test.xds.yaml
        ${CMAKE_SOURCE_DIR}/schemas/serialization_test.xds.yaml
        ${CMAKE_SOURCE_DIR}/schemas/alignment_test.xds.yaml
        ${CMAKE_SOURCE_DIR}/schemas/player.xds.yaml
        ${CMAKE_SOURCE_DIR}/schemas/game_data.xds.yaml
    )
    
    # Generate code from schemas
    set(GENERATED_HEADERS "")
    foreach(SCHEMA_FILE ${SCHEMA_FILES})
        get_filename_component(SCHEMA_NAME ${SCHEMA_FILE} NAME_WE)
        string(REPLACE ".xds" "" SCHEMA_NAME ${SCHEMA_NAME})
        set(OUTPUT_FILE "${CMAKE_SOURCE_DIR}/generated/${SCHEMA_NAME}.hpp")
        list(APPEND GENERATED_HEADERS ${OUTPUT_FILE})
        
        add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/generated
            COMMAND ${Python3_EXECUTABLE} 
                    ${CMAKE_SOURCE_DIR}/tools/xds_generator.py 
                    ${SCHEMA_FILE}
                    -o ${CMAKE_SOURCE_DIR}/generated/
            DEPENDS ${SCHEMA_FILE} ${CMAKE_SOURCE_DIR}/tools/xds_generator.py
            COMMENT "Generating ${SCHEMA_NAME}.hpp from schema"
            VERBATIM
        )
    endforeach()
    
    add_custom_target(generate_schemas ALL DEPENDS ${GENERATED_HEADERS})
    
    message(STATUS "Schema code generation enabled")
    message(STATUS "  Python: ${Python3_EXECUTABLE}")
    message(STATUS "  Schemas: ${SCHEMA_FILES}")
else()
    message(WARNING "Python3 not found - schema code generation disabled")
endif()

# Add examples subdirectory
add_subdirectory(examples)

# Add tests subdirectory
add_subdirectory(tests)