cmake_minimum_required(VERSION 3.11.0)

add_library(FishEditor
    imgui_extra.hpp imgui_extra.cpp
)

target_link_libraries(FishEditor PUBLIC FishEngine)