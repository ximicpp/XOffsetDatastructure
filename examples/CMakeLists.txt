# Add executable
add_executable(xoffsetdatastructure2_demo xoffsetdatastructure2_demo.cpp)

# Add include directories
target_include_directories(xoffsetdatastructure2_demo PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${BOOST_INCLUDE_DIRS}
)

# Set output directory
set_target_properties(xoffsetdatastructure2_demo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
