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

# macOS specific: disable RPATH and use system libraries
if(APPLE)
    set_target_properties(xoffsetdatastructure2_demo PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH FALSE
        INSTALL_RPATH ""
    )
    target_link_options(xoffsetdatastructure2_demo PRIVATE
        -Wl,-rpath,/usr/lib
        -Wl,-rpath,/usr/local/lib
    )
endif()
