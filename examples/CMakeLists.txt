# Hello World Demo
add_executable(helloworld helloworld.cpp)
target_include_directories(helloworld PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${BOOST_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/generated
)
set_target_properties(helloworld
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
if(Python3_FOUND)
    add_dependencies(helloworld generate_schemas)
endif()

# Full Demo
add_executable(demo demo.cpp)
target_include_directories(demo PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${BOOST_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/generated
)
set_target_properties(demo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
if(Python3_FOUND)
    add_dependencies(demo generate_schemas)
endif()

# Signature Demo
add_executable(signature_demo signature_demo.cpp)
target_include_directories(signature_demo PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${BOOST_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/generated
)
set_target_properties(signature_demo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
if(Python3_FOUND)
    add_dependencies(signature_demo generate_schemas)
endif()

# Signature Calculation Demo
add_executable(signature_calc_demo signature_calculation_demo.cpp)
target_include_directories(signature_calc_demo PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${BOOST_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/generated
)
set_target_properties(signature_calc_demo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
if(Python3_FOUND)
    add_dependencies(signature_calc_demo generate_schemas)
endif()

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
    set_target_properties(signature_demo PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH FALSE
        INSTALL_RPATH ""
    )
    target_link_options(signature_demo PRIVATE
        -Wl,-rpath,/usr/lib
        -Wl,-rpath,/usr/local/lib
    )
    set_target_properties(signature_calc_demo PROPERTIES
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH FALSE
        INSTALL_RPATH ""
    )
    target_link_options(signature_calc_demo PRIVATE
        -Wl,-rpath,/usr/lib
        -Wl,-rpath,/usr/local/lib
    )
endif()
