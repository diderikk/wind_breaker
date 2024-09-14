cmake_minimum_required(VERSION 3.22)

project(CServer VERSION 0.0)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

find_package(Threads REQUIRED)

add_library(socket_lib STATIC src/socket.c)
add_library(listener_lib STATIC src/listener.c)
add_library(worker_lib STATIC src/worker.c)
add_library(worker_queue_lib STATIC src/worker_queue.c)
add_library(http_request_lib STATIC src/http/request.c)
add_library(http_response_lib STATIC src/http/response.c)

add_executable(main src/main.c)
target_compile_options(main PRIVATE -Wall -Wextra -Werror)
target_link_libraries(main PRIVATE listener_lib http_request_lib http_response_lib socket_lib worker_lib worker_queue_lib Threads::Threads)

add_executable(client src/client.c)
target_link_libraries(client PRIVATE socket_lib)
target_compile_options(client PRIVATE -Wall -Wextra -Werror)
