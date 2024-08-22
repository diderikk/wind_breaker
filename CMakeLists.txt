cmake_minimum_required(VERSION 3.22)

project(CServer VERSION 0.0)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

add_executable(main src/main.c)

target_compile_options(main PRIVATE -Wall -Wextra -Werror)


