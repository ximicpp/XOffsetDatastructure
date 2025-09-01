# Add executable target
add_executable(xdemo xdemo.cpp)

# Add include directories relative to root
target_include_directories(xdemo PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(xdemo PRIVATE ${CMAKE_SOURCE_DIR}/xoffsetdatastructure)

# Link against Boost
target_link_libraries(xdemo PRIVATE Boost::headers Boost::interprocess)

# Set output directory
set_target_properties(xdemo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)