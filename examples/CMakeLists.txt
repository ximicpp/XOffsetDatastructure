# Add executables
add_executable(xoffsetdatastructure_demo xoffsetdatastructure_demo.cpp)
add_executable(xoffsetdatastructure2_demo xoffsetdatastructure2_demo.cpp)
add_executable(xtypesignature_demo xtypesignature_demo.cpp)

# Add include directories for all targets
foreach(target xoffsetdatastructure_demo xoffsetdatastructure2_demo xtypesignature_demo)
    target_include_directories(${target} PRIVATE 
        ${CMAKE_SOURCE_DIR}
        ${BOOST_INCLUDE_DIR}
    )
endforeach()

# Add specific include directories
target_include_directories(xoffsetdatastructure_demo PRIVATE ${CMAKE_SOURCE_DIR}/xoffsetdatastructure)
target_include_directories(xoffsetdatastructure2_demo PRIVATE ${CMAKE_SOURCE_DIR}/xoffsetdatastructure2)
target_include_directories(xtypesignature_demo PRIVATE ${CMAKE_SOURCE_DIR}/xtypesignature)

# Set output directory for all executables
set_target_properties(xoffsetdatastructure_demo xoffsetdatastructure2_demo xtypesignature_demo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)