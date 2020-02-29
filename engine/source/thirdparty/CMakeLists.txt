cmake_minimum_required(VERSION 3.11.0)

set(IMGUI_DIR "imgui")
add_library(imgui
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_internal.h
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
target_compile_features(imgui PUBLIC cxx_std_17)
set_target_properties(imgui PROPERTIES FOLDER "ThirdParty")

add_subdirectory("glfw")
set_target_properties(glfw PROPERTIES FOLDER "ThirdParty")

if (APPLE)
add_library(imgui_macos
    ${IMGUI_DIR}/examples/imgui_impl_glfw.h
    ${IMGUI_DIR}/examples/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/examples/imgui_impl_metal.h
    ${IMGUI_DIR}/examples/imgui_impl_metal.mm
    ${IMGUI_DIR}/examples/imgui_impl_osx.h
    ${IMGUI_DIR}/examples/imgui_impl_osx.mm
)
target_include_directories(imgui_macos PUBLIC "${IMGUI_DIR}/examples")
target_link_libraries(imgui_macos PUBLIC glfw imgui)
target_compile_options(imgui_macos PUBLIC "-fobjc-arc")
set_target_properties(imgui_macos PROPERTIES FOLDER "ThirdParty")
endif ()

if (WIN32)
add_library(imgui_win
    ${IMGUI_DIR}/examples/imgui_impl_glfw.h
    ${IMGUI_DIR}/examples/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/examples/imgui_impl_win32.h
    ${IMGUI_DIR}/examples/imgui_impl_win32.cpp
    ${IMGUI_DIR}/examples/imgui_impl_dx12.h
    ${IMGUI_DIR}/examples/imgui_impl_dx12.cpp
)
target_include_directories(imgui_win PUBLIC "${IMGUI_DIR}/examples")
target_link_libraries(imgui_win PUBLIC glfw imgui)
set_target_properties(imgui_win PROPERTIES FOLDER "ThirdParty")
endif ()

if (APPLE)
set(FSEVENTS_ROOT_DIR "fsevents/src")
add_library(fsevents
    ${FSEVENTS_ROOT_DIR}/constants.h
    ${FSEVENTS_ROOT_DIR}/rawfsevents.h
    ${FSEVENTS_ROOT_DIR}/rawfsevents.c
)
target_include_directories(fsevents PUBLIC ${FSEVENTS_ROOT_DIR})
target_link_libraries(fsevents "-framework CoreFoundation -framework CoreServices")
set_target_properties(fsevents PROPERTIES FOLDER "ThirdParty")
endif ()

#add_subdirectory("./nvidia-texture-tools")

set(MIKKTSPACE_ROOT_DIR "mikktspace")
add_library(mikktspace
    ${MIKKTSPACE_ROOT_DIR}/mikktspace.h
    ${MIKKTSPACE_ROOT_DIR}/mikktspace.c
)
target_include_directories(mikktspace PUBLIC ${MIKKTSPACE_ROOT_DIR})
set_target_properties(mikktspace PROPERTIES FOLDER "ThirdParty")

set(QUICKJS_ROOT_DIR "QuickJS")
add_library(quickjs
    ${QUICKJS_ROOT_DIR}/cutils.c
    ${QUICKJS_ROOT_DIR}/cutils.h
    #${QUICKJS_ROOT_DIR}/libbf.c
    #${QUICKJS_ROOT_DIR}/libbf.h
    ${QUICKJS_ROOT_DIR}/libregexp.c
    ${QUICKJS_ROOT_DIR}/libregexp.h
    ${QUICKJS_ROOT_DIR}/libunicode.c
    ${QUICKJS_ROOT_DIR}/libunicode.h
    ${QUICKJS_ROOT_DIR}/list.h
    ${QUICKJS_ROOT_DIR}/quickjs-atom.h
    ${QUICKJS_ROOT_DIR}/quickjs-libc.c
    ${QUICKJS_ROOT_DIR}/quickjs-libc.h
    ${QUICKJS_ROOT_DIR}/quickjs-opcode.h
    ${QUICKJS_ROOT_DIR}/quickjs.c
    ${QUICKJS_ROOT_DIR}/quickjs.h
)

target_include_directories(quickjs PUBLIC ${QUICKJS_ROOT_DIR})
target_compile_definitions(quickjs PUBLIC CONFIG_VERSION="2020-01-19")
target_compile_definitions(quickjs PUBLIC _GNU_SOURCE)
target_compile_features(quickjs PUBLIC c_std_11)
set_target_properties(quickjs PROPERTIES FOLDER "ThirdParty")
if (WIN32)
target_compile_definitions(quickjs PUBLIC _CRT_SECURE_NO_WARNINGS EMSCRIPTEN)
else ()
target_compile_definitions(quickjs PUBLIC CONFIG_BIGNUM)
endif ()
