cmake_minimum_required(VERSION 3.11.0)

project(FishEngine)

set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_LIST_DIR}/../binaries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(thirdparty)
add_subdirectory(fishengine)
add_subdirectory(editor)
add_subdirectory(tools)