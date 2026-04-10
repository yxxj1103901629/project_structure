cmake_minimum_required(VERSION 3.16)
project(network_tests)


# ==================== 基础配置 ====================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================== 路径配置 ====================
set(BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bin)

# ==================== 编译器配置 ====================
add_executable(${PROJECT_NAME} test_tcp_server.cpp)
add_executable(network_client_tests test_tcp_client.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}/debug
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}/debug
)

set_target_properties(network_client_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}/debug
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}/debug
)



target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/debug
)
target_link_libraries(${PROJECT_NAME} 
    PRIVATE
    network
)

target_include_directories(network_client_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_directories(network_client_tests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin/debug
)
target_link_libraries(network_client_tests 
    PRIVATE
    network
)



add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
add_test(NAME network_client_tests COMMAND network_client_tests)