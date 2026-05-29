cmake_minimum_required(VERSION 3.10)

project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gsm gsm.cpp)

target_compile_options(my_app PRIVATE
    -Wall
    -Wextra
)