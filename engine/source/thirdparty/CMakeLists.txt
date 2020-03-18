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

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/fmt)
set_target_properties(fmt PROPERTIES FOLDER "ThirdParty")

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

if (WIN32)
set(NATIVE_FILE_DIALOG_ROOT_DIR "nativefiledialog/src")
add_library(nfd
    ${NATIVE_FILE_DIALOG_ROOT_DIR}/include/nfd.h
    ${NATIVE_FILE_DIALOG_ROOT_DIR}/common.h
    ${NATIVE_FILE_DIALOG_ROOT_DIR}/nfd_win.cpp
    ${NATIVE_FILE_DIALOG_ROOT_DIR}/nfd_common.c
    ${NATIVE_FILE_DIALOG_ROOT_DIR}/nfd_common.h
)
target_include_directories(nfd PUBLIC ${NATIVE_FILE_DIALOG_ROOT_DIR}/include)
set_target_properties(nfd PROPERTIES FOLDER "ThirdParty")
endif ()

#add_subdirectory("./nvidia-texture-tools")

set(MIKKTSPACE_ROOT_DIR "mikktspace")
add_library(mikktspace
    ${MIKKTSPACE_ROOT_DIR}/mikktspace.h
    ${MIKKTSPACE_ROOT_DIR}/mikktspace.c
)
target_include_directories(mikktspace PUBLIC ${MIKKTSPACE_ROOT_DIR})
set_target_properties(mikktspace PROPERTIES FOLDER "ThirdParty")

set(MICROTAR_ROOT_DIR "microtar/src")
add_library(microtar
	${MICROTAR_ROOT_DIR}/microtar.h
	${MICROTAR_ROOT_DIR}/microtar.c)
target_include_directories(microtar PUBLIC ${MICROTAR_ROOT_DIR})
set_target_properties(microtar PROPERTIES FOLDER "ThirdParty")
target_compile_definitions(microtar PRIVATE _CRT_SECURE_NO_WARNINGS)

set(QUICKJS_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/QuickJS")
add_library(quickjs
    ${QUICKJS_ROOT_DIR}/cutils.c
    ${QUICKJS_ROOT_DIR}/cutils.h
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
target_compile_definitions(quickjs PUBLIC CONFIG_VERSION="2020-03-16")
target_compile_definitions(quickjs PUBLIC _GNU_SOURCE)
target_compile_features(quickjs PUBLIC c_std_11)
set_target_properties(quickjs PROPERTIES FOLDER "ThirdParty")
if (WIN32)
    target_compile_definitions(quickjs PUBLIC _CRT_SECURE_NO_WARNINGS EMSCRIPTEN)
else ()
    target_sources(quickjs PRIVATE
        ${QUICKJS_ROOT_DIR}/libbf.c
        ${QUICKJS_ROOT_DIR}/libbf.h)
    target_compile_definitions(quickjs PUBLIC CONFIG_BIGNUM)
endif ()

if (WIN32)
    set(BUILD_XAUDIO_WIN10 OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/DirectXTK12)
    set_target_properties(DirectXTK12 PROPERTIES FOLDER "ThirdParty")

    set(BUILD_DX11 OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/DirectXTex)
    set_target_properties(DirectXTex PROPERTIES FOLDER "ThirdParty/DirectXTex")
    set_target_properties(texassemble PROPERTIES FOLDER "ThirdParty/DirectXTex")
    set_target_properties(texconv PROPERTIES FOLDER "ThirdParty/DirectXTex")
    set_target_properties(texdiag PROPERTIES FOLDER "ThirdParty/DirectXTex")
    set_target_properties(texconv PROPERTIES FOLDER "ThirdParty/DirectXTex")
endif (WIN32)