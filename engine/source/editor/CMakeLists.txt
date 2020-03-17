cmake_minimum_required(VERSION 3.11.0)

add_executable(FishEditor
	editor_app.hpp editor_app.cpp
    imgui_extra.hpp imgui_extra.cpp
	glfw_app.hpp glfw_app.cpp
	main.cpp
)

target_link_libraries(FishEditor PUBLIC FishEngine)

if (APPLE)
    target_link_libraries(FishEditor PUBLIC FishEngine_macos imgui_macos)
endif ()
if (WIN32)
    target_link_libraries(FishEditor PUBLIC glfw FishEngine_d3d12 imgui_win d3d12 dxgi)
endif ()