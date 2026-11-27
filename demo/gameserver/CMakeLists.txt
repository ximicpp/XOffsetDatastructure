# Game server demo (demo/gameserver/README.md). Built only with
# -DXOFFSET_BUILD_GAMESERVER_DEMO=ON. POSIX sockets: epoll on Linux, poll() elsewhere.
#
#   gs_logic    a logic node: IO thread, shard threads, write-behind saves to Redis
#   gs_gateway  client connections, handshake, sessions, routing
#   gs_bot      load generator and end-to-end checks
#
# The schema is examples/game_server/game_schema.hpp.

if(WIN32)
    message(STATUS "gameserver demo: POSIX only, skipped on Windows")
    return()
endif()

add_library(gs_common STATIC src/net.cpp src/redis.cpp)
target_include_directories(gs_common PUBLIC include "${PROJECT_SOURCE_DIR}/examples")
target_link_libraries(gs_common PUBLIC xoffset_common)

foreach(program logic gateway bot)
    add_executable(gs_${program} src/${program}.cpp)
    target_link_libraries(gs_${program} PRIVATE gs_common)
    set_target_properties(gs_${program} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/$<CONFIG>)
endforeach()

# An end-to-end run under CTest when redis-server is installed: Redis, two
# logic nodes, a gateway and 100 bots for 5 seconds, with session resumes,
# migrations and the hostile-client checks.
find_program(GS_REDIS_SERVER redis-server)
if(XOFFSET_BUILD_TESTS AND GS_REDIS_SERVER)
    add_test(NAME gameserver_demo
             COMMAND ${CMAKE_COMMAND} -E env REDIS_SERVER=${GS_REDIS_SERVER} REDIS_PORT=6395 GATEWAY_PORT=7195
                     NODE_PORT=7295 RECONNECTS=5 MIGRATIONS=5
                     bash ${CMAKE_CURRENT_SOURCE_DIR}/tools/run_demo.sh $<TARGET_FILE_DIR:gs_bot> 100 5 10 2 2)
    set_tests_properties(gameserver_demo PROPERTIES LABELS "demo" TIMEOUT 120)
elseif(XOFFSET_BUILD_TESTS)
    message(STATUS "gameserver demo: redis-server not found, the end-to-end test is not registered")
endif()
