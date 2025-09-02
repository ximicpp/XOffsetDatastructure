# Add executables
add_executable(xoffsetdatastructure_demo xoffsetdatastructure_demo.cpp)
add_executable(xoffsetdatastructure2_demo xoffsetdatastructure2_demo.cpp)

# Add include directories for v1
target_include_directories(xoffsetdatastructure_demo PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(xoffsetdatastructure_demo PRIVATE ${CMAKE_SOURCE_DIR}/xoffsetdatastructure)

# Add include directories for v2
target_include_directories(xoffsetdatastructure2_demo PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(xoffsetdatastructure2_demo PRIVATE ${CMAKE_SOURCE_DIR}/xoffsetdatastructure2)

# Link against Boost
target_link_libraries(xoffsetdatastructure_demo PRIVATE Boost::headers Boost::interprocess)
target_link_libraries(xoffsetdatastructure2_demo PRIVATE Boost::headers Boost::interprocess)

# Set output directory for both executables
set_target_properties(xoffsetdatastructure_demo xoffsetdatastructure2_demo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)