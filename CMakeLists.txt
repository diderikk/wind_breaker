cmake_minimum_required(VERSION 3.22)

project(CServer VERSION 0.0)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Debug)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(socket_lib STATIC src/socket.c)
add_library(listener_lib STATIC src/listener.c)
add_library(worker_lib STATIC src/worker.c)
add_library(worker_queue_lib STATIC src/data_structures/worker_queue.c)
add_library(poll_array_lib STATIC src/data_structures/poll_array.c)
add_library(http_request_lib STATIC src/http/request.c)
add_library(http_response_lib STATIC src/http/response.c)
add_library(file_utils_lib STATIC src/utils/static_file.c)
add_library(compression_utils_lib STATIC src/utils/compression.c)
add_library(hash_utils_lib STATIC src/utils/hash.c)
add_library(logger_utils_lib STATIC src/utils/logger.c)
add_library(assert_utils_lib STATIC src/utils/assert2.c)
add_library(regex_utils_lib STATIC src/utils/regex2.c)
add_library(session_lib STATIC src/data_structures/session.c)
add_library(simulator_cases_lib STATIC src/simulator/cases.c)
add_library(simulator_connection_lib STATIC src/simulator/connection_cases.c)
add_library(simulator_http_lib STATIC src/simulator/http_cases.c)

add_executable(main src/main.c)
target_compile_options(main PRIVATE -Wall -Wextra -Werror -g -O3)
target_link_libraries(main PRIVATE listener_lib http_request_lib http_response_lib socket_lib worker_lib worker_queue_lib poll_array_lib file_utils_lib compression_utils_lib regex_utils_lib hash_utils_lib assert_utils_lib logger_utils_lib session_lib OpenSSL::SSL OpenSSL::Crypto Threads::Threads ZLIB::ZLIB)

add_executable(client src/simulator/client.c)
target_link_libraries(client PRIVATE simulator_cases_lib simulator_http_lib simulator_connection_lib socket_lib hash_utils_lib assert_utils_lib logger_utils_lib session_lib OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
target_compile_options(client PRIVATE -Wall -Wextra -Werror)

# Add custom target to copy the static folder
add_custom_target(copy_static ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/static ${CMAKE_BINARY_DIR}/static
)

# Ensure the main executable depends on the copy_static target
add_dependencies(main copy_static)
