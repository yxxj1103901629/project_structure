cmake_minimum_required(VERSION 3.16)
project(network_tests)

# ==================== Basic Configuration ====================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================== Paths ====================
set(BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bin)
set(NETWORK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
set(NETWORK_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bin/$<IF:$<CONFIG:Debug>,debug,release>)

# ==================== Test Helper ====================
function(add_network_test target_name source_file)
    add_executable(${target_name} ${source_file})

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}/debug
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}/debug
    )

    target_include_directories(${target_name} PRIVATE ${NETWORK_INCLUDE_DIR})
    target_link_directories(${target_name} PRIVATE ${NETWORK_LIB_DIR})
    target_link_libraries(${target_name} PRIVATE network)

    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

# ==================== Tests ====================
add_network_test(${PROJECT_NAME} test_tcp_server.cpp)
add_network_test(network_client_tests test_tcp_client.cpp)
add_network_test(mqtt_client_tests test_mqtt_client.cpp)
