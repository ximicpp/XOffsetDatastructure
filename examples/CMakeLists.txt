add_executable(helloworld helloworld.cpp)
configure_xoffset_target(helloworld)

add_executable(stable_storage stable_storage.cpp)
configure_xoffset_target(stable_storage)

add_executable(schema_handshake schema_handshake.cpp)
configure_xoffset_target(schema_handshake)

add_executable(delta_sync_example delta_sync.cpp)
configure_xoffset_target(delta_sync_example)

add_executable(shared_delta_example shared_delta.cpp)
configure_xoffset_target(shared_delta_example)

# The demo prints std::meta and TypeLayout signatures and has no field lists:
# C++26 with the reflection backend only.
if(XOFFSET_CXX_STANDARD STREQUAL "26" AND NOT XOFFSET_SCHEMA_BACKEND STREQUAL "describe")
    add_executable(xoffsetdatastructure_demo demo.cpp)
    configure_xoffset_target(xoffsetdatastructure_demo)
endif()

# The examples above run under CTest too, with assert() on, so an example that
# no longer matches the library fails the test run instead of going unnoticed.
if(XOFFSET_BUILD_TESTS)
    set(xoffset_example_programs helloworld stable_storage schema_handshake
        delta_sync_example shared_delta_example)
    if(TARGET xoffsetdatastructure_demo)
        list(APPEND xoffset_example_programs xoffsetdatastructure_demo)
    endif()
    foreach(example IN LISTS xoffset_example_programs)
        xoffset_enable_test_assertions(${example})
        add_test(NAME example_${example} COMMAND ${example})
        set_tests_properties(example_${example} PROPERTIES LABELS "examples")
    endforeach()
endif()

# Getting-started use cases (docs/GETTING_STARTED.md). Each is also built
# without exceptions (-fno-exceptions, MSVC /EHs-c-), as client engines are,
# and runs under CTest.
foreach(use_case server_to_client client_uplink server_to_server)
    add_executable(${use_case} ${use_case}.cpp)
    configure_xoffset_target(${use_case})
    add_executable(${use_case}_noexc ${use_case}.cpp)
    configure_xoffset_target(${use_case}_noexc)
    xoffset_disable_exceptions(${use_case}_noexc)
    if(XOFFSET_BUILD_TESTS)
        add_test(NAME example_${use_case} COMMAND ${use_case})
        add_test(NAME example_${use_case}_noexc COMMAND ${use_case}_noexc)
        set_tests_properties(example_${use_case} example_${use_case}_noexc PROPERTIES LABELS "examples")
    endif()
endforeach()
