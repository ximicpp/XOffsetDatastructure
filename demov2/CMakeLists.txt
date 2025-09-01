# Add executable target
add_executable(xdemov2 xdemov2.cpp)

# Add include directories relative to root
target_include_directories(xdemov2 PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(xdemov2 PRIVATE ${CMAKE_SOURCE_DIR}/xoffsetdatastructure2)

# Link against Boost
target_link_libraries(xdemov2 PRIVATE Boost::headers Boost::interprocess)

# Set output directory
set_target_properties(xdemov2
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)