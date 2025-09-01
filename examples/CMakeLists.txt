# Add executables
add_executable(xdemo_v1 xdemo_v1.cpp)
add_executable(xdemo_v2 xdemo_v2.cpp)

# Add include directories for v1
target_include_directories(xdemo_v1 PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(xdemo_v1 PRIVATE ${CMAKE_SOURCE_DIR}/xoffsetdatastructure)

# Add include directories for v2
target_include_directories(xdemo_v2 PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(xdemo_v2 PRIVATE ${CMAKE_SOURCE_DIR}/xoffsetdatastructure2)

# Link against Boost
target_link_libraries(xdemo_v1 PRIVATE Boost::headers Boost::interprocess)
target_link_libraries(xdemo_v2 PRIVATE Boost::headers Boost::interprocess)

# Set output directory for both executables
set_target_properties(xdemo_v1 xdemo_v2
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)