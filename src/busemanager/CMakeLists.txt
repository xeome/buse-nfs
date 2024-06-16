cmake_minimum_required(VERSION 3.10)
project(busemanager)

add_library(busemanager STATIC busemanager.cpp busemanager.hpp)
target_link_libraries(busemanager loguru::loguru buse)
target_include_directories(busemanager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})