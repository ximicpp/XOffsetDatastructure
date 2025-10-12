# Add executables
add_executable(xoffsetdatastructure2_demo xoffsetdatastructure2_demo.cpp)

# Add include directories for all targets
foreach(target xoffsetdatastructure2_demo)
    target_include_directories(${target} PRIVATE 
        ${CMAKE_SOURCE_DIR}
        ${BOOST_INCLUDE_DIRS}
    )
endforeach()

# Add specific include directories
target_include_directories(xoffsetdatastructure2_demo PRIVATE ${CMAKE_SOURCE_DIR})

# Set output directory for all executables
set_target_properties(xoffsetdatastructure2_demo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)