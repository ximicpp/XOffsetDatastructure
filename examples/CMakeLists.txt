# Hello World Demo
add_executable(helloworld helloworld.cpp)
target_include_directories(helloworld PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${BOOST_INCLUDE_DIRS}
)
set_target_properties(helloworld
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Full Demo
add_executable(demo demo.cpp)
target_include_directories(demo PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${BOOST_INCLUDE_DIRS}
)
set_target_properties(demo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# macOS specific: disable RPATH and use system libraries
if(APPLE)
    set_target_properties(helloworld PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH FALSE
        INSTALL_RPATH ""
    )
    set_target_properties(demo PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH FALSE
        INSTALL_RPATH ""
    )
    target_link_options(helloworld PRIVATE
        -Wl,-rpath,/usr/lib
        -Wl,-rpath,/usr/local/lib
    )
    target_link_options(demo PRIVATE
        -Wl,-rpath,/usr/lib
        -Wl,-rpath,/usr/local/lib
    )
endif()
