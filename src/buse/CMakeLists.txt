cmake_minimum_required(VERSION 3.10)
project(BUSE)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb3 -pedantic -Wall -Wextra")

add_library(buse STATIC buse.c)
target_include_directories(buse PUBLIC .)