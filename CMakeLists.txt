cmake_minimum_required(VERSION 3.10)
project(XOffsetDatastructure)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS and iOS specific settings
if(APPLE)
    # Use the system C++ library
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    
    # Set RPATH settings globally
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "@executable_path;@loader_path")
    
    # iOS specific settings
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # Set code signing identity (use automatic for CI)
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
        
        # Set development team (empty for unsigned builds)
        set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "")
        
        message(STATUS "Configuring for iOS platform")
    else()
        # Ensure we use the correct deployment target for macOS
        if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
            set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version" FORCE)
        endif()
    endif()
endif()

# Helper function to set iOS bundle identifier
function(set_ios_bundle_id target)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set_target_properties(${target} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME ${target}
            MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.xoffsetdatastructure.${target}"
        )
    endif()
endfunction()

# Enable testing
enable_testing()

# Find Python for code generation
# On Windows, prefer the Python from environment if set
if(WIN32 AND DEFINED ENV{Python3_EXECUTABLE})
    set(Python3_EXECUTABLE $ENV{Python3_EXECUTABLE} CACHE FILEPATH "Path to Python3 executable")
    message(STATUS "Using Python from environment: ${Python3_EXECUTABLE}")
elseif(WIN32 AND DEFINED ENV{pythonLocation})
    # GitHub Actions sets pythonLocation
    set(Python3_EXECUTABLE "$ENV{pythonLocation}/python.exe" CACHE FILEPATH "Path to Python3 executable")
    message(STATUS "Using Python from GitHub Actions: ${Python3_EXECUTABLE}")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
message(STATUS "Python3 executable: ${Python3_EXECUTABLE}")
message(STATUS "Python3 version: ${Python3_VERSION}")

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
        
        # Check if PyYAML is available by testing import
        execute_process(
            COMMAND ${Python3_EXECUTABLE} -c "import yaml"
            RESULT_VARIABLE PYYAML_CHECK
            OUTPUT_QUIET
            ERROR_QUIET
        )
        
        if(NOT PYYAML_CHECK EQUAL 0)
            message(WARNING "PyYAML not found for Python ${Python3_EXECUTABLE}. Code generation may fail.")
        endif()
        
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