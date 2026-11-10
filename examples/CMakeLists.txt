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
