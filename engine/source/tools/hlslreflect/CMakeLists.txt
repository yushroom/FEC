cmake_minimum_required(VERSION 3.11.0)

add_executable(hlslreflect main.cpp)
target_link_libraries(hlslreflect FishEngine_d3d12 d3dcompiler)
