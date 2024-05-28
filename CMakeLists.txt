cmake_minimum_required(VERSION 3.5)
project(buse_nfs)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 17)

# set debug flags
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -O0 -Wall -Wextra -Wpedantic")

find_program(LLD lld)
# If lld is found, set the linker flags
if(LLD)
    message(STATUS "lld linker found: ${LLD}")
    add_link_options("-fuse-ld=lld")
else()
    message(STATUS "lld linker not found")
endif()

# Use ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
    message(STATUS "ccache found: ${CCACHE_PROGRAM}")
endif()

add_executable(buse_nfs src/main.cpp)

# Use precompiled headers
target_precompile_headers(buse_nfs PRIVATE src/pch.h)

add_subdirectory(src/loguru)
add_subdirectory(src/cxxopts)

target_link_libraries(buse_nfs PRIVATE loguru::loguru cxxopts::cxxopts)