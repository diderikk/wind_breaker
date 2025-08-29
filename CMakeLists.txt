cmake_minimum_required(VERSION 3.22)

project(WindBreaker VERSION 1.1.1 LANGUAGES C)


configure_file(src/version.h.in ../src/version.h)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXE_LINKER_FLAGS "-rdynamic")

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)

function(enable_compile_options target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -g -O3 -rdynamic)
endfunction()

function(enable_address_sanitizer target)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
endfunction()

add_library(static_lib STATIC src/static.c)
add_library(socket_lib STATIC src/socket.c)
add_library(listener_lib STATIC src/listener.c)
add_library(worker_lib STATIC src/worker.c)
add_library(properties_lib STATIC src/properties.c)
add_library(poll_array_lib STATIC src/data_structures/poll_array.c)
add_library(marked_fds_lib STATIC src/data_structures/marked_fds.c)
add_library(http_request_lib STATIC src/http/request.c)
add_library(http_response_lib STATIC src/http/response.c)
add_library(file_utils_lib STATIC src/utils/static_file.c)
add_library(compression_utils_lib STATIC src/utils/compression.c)
add_library(hash_utils_lib STATIC src/utils/hash.c)
add_library(logger_utils_lib STATIC src/utils/logger.c)
add_library(assert_utils_lib STATIC src/utils/assert2.c)
add_library(regex_utils_lib STATIC src/utils/regex2.c)
add_library(ssl_utils_lib STATIC src/utils/ssl.c)
add_library(db_migration_utils_lib STATIC src/utils/db_migration.c)
add_library(signal_wrapper_lib STATIC src/shutdown/signal2.c)
add_library(stop_lib STATIC src/shutdown/stop.c)
add_library(session_lib STATIC src/data_structures/session.c)


# Handlers libraries
add_library(builder_lib STATIC src/handlers/builder.c)
add_library(loader_lib STATIC src/handlers/loader.c)
add_library(parser_lib STATIC src/handlers/parser.c)
add_library(sender_lib STATIC src/handlers/sender.c)


# Simulator libraries
add_library(simulator_cases_lib STATIC test/simulator/cases.c)
add_library(simulator_connection_lib STATIC test/simulator/connection_cases.c)
add_library(simulator_http_lib STATIC test/simulator/http_cases.c)

#Unit test libraries


add_executable(main src/main.c ${HEADER_FILES})
target_link_libraries(main PRIVATE listener_lib worker_lib builder_lib loader_lib parser_lib sender_lib http_request_lib http_response_lib socket_lib signal_wrapper_lib poll_array_lib db_migration_utils_lib file_utils_lib compression_utils_lib regex_utils_lib hash_utils_lib ssl_utils_lib assert_utils_lib logger_utils_lib session_lib marked_fds_lib stop_lib properties_lib static_lib OpenSSL::SSL OpenSSL::Crypto Threads::Threads ZLIB::ZLIB SQLite::SQLite3)
enable_compile_options(main)
#enable_address_sanitizer(main)

add_executable(client test/simulator/client.c)
target_link_libraries(client PRIVATE simulator_cases_lib simulator_http_lib simulator_connection_lib socket_lib compression_utils_lib hash_utils_lib assert_utils_lib logger_utils_lib session_lib stop_lib marked_fds_lib properties_lib static_lib OpenSSL::SSL OpenSSL::Crypto Threads::Threads ZLIB::ZLIB CURL::libcurl SQLite::SQLite3)
enable_compile_options(client)
# enable_address_sanitizer(client)


add_executable(unit_test test/unit_tests/all_tests.c)
target_link_libraries(unit_test PRIVATE http_request_lib http_response_lib file_utils_lib regex_utils_lib poll_array_lib compression_utils_lib hash_utils_lib properties_lib assert_utils_lib logger_utils_lib session_lib stop_lib marked_fds_lib properties_lib static_lib OpenSSL::SSL OpenSSL::Crypto Threads::Threads ZLIB::ZLIB)
enable_compile_options(unit_test)
#enable_address_sanitizer(unit_test)
# Add custom target to copy the static folder
add_custom_target(copy_static ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/static ${CMAKE_BINARY_DIR}/static
)

add_custom_target(copy_migrations ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/migrations ${CMAKE_BINARY_DIR}/migrations
)

# Ensure the main executable depends on the copy_static target
add_dependencies(main copy_static)
add_dependencies(main copy_migrations)
