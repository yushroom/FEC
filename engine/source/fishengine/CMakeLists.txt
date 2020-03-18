cmake_minimum_required(VERSION 3.11.0)

add_library(FishEngine
    simd_math.h simd_math.c
    array.h array.c
    fs.hpp fs.cpp
    statistics.h statistics.c
    jsbinding.h jsbinding.c jsbinding.cpp jsbinding.hpp jsbinding.gen.cpp
    debug.h debug.c
    app.h app.c app.cpp
    input.h input.c keycode.h
    singleton_time.h singleton_time.c
    singleton_selection.h
    free_camera.h free_camera.c
    
    component.h ecs.h ecs.c
    transform.h transform.c
    camera.h
    renderable.h renderable.c
    light.h light.c
    
    asset.h asset.cpp
    script.h script.c
    shader.h shader.cpp shader_internal.hpp shader_util.h
    mesh.h mesh.c vertexdecl.h
    texture_format.h
    texture.h texture.c
    material.h material.cpp material_internal.hpp
    rhi.h
    
    animation.h animation.c
    ddsloader.h ddsloader.c
)
target_compile_features(FishEngine PUBLIC cxx_std_17)
target_compile_features(FishEngine PUBLIC c_std_11)
target_link_libraries(FishEngine PUBLIC imgui glfw quickjs mikktspace fmt microtar)
target_compile_options(FishEngine PUBLIC "-march=native")
target_include_directories(FishEngine PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../thirdparty/rapidjson/include")
target_include_directories(FishEngine PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
if (APPLE)
    target_link_libraries(FishEngine PUBLIC fsevents)
endif ()

if (APPLE)
    add_library(FishEngine_macos
        render_metal.h render_metal.mm
    )
    target_link_libraries(FishEngine_macos PUBLIC FishEngine)
    target_compile_options(FishEngine_macos PUBLIC "-fobjc-arc")
    target_link_libraries(FishEngine_macos PUBLIC "-framework Metal -framework MetalKit -framework QuartzCore")
endif ()

if (WIN32)
    add_library(FishEngine_d3d12
        render_d3d12.hpp render_d3d12.cpp
        vertexdecl_d3d12.cpp
    )
    target_link_libraries(FishEngine_d3d12 PUBLIC FishEngine DirectXTK12 DirectXTex)
    target_link_libraries(FishEngine_d3d12 PUBLIC d3d12 dxgi)
    # https://github.com/microsoft/DirectX-Graphics-Samples/issues/567
    target_link_libraries(FishEngine_d3d12 PUBLIC dxguid)
endif ()

if (APPLE)
    set(METAL_FILES shader.metal ../../shaders/Internal-Skinning.metal)
    add_executable(glTFViewer
        MACOSX_BUNDLE
        main.mm
        ${METAL_FILES}
    )
    set_source_files_properties(${METAL_FILES} PROPERTIES LANGUAGE METAL)
else ()
    add_executable(glTFViewer main.cpp)
endif ()

target_link_libraries(glTFViewer PUBLIC FishEditor)

if (APPLE)
    target_link_libraries(glTFViewer PUBLIC FishEngine_macos imgui_macos)
endif ()
if (WIN32)
    target_link_libraries(glTFViewer PUBLIC glfw FishEngine_d3d12 imgui_win d3d12 dxgi)
endif ()

SET( INFO_PLIST ${CMAKE_CURRENT_LIST_DIR}/MacOSXBundleInfo.plist.in)
IF (NOT EXISTS ${INFO_PLIST})
	MESSAGE(FATAL_ERROR "${INFO_PLIST} not found!")
ENDIF()
set_target_properties(glTFViewer PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${INFO_PLIST}" )
# add_dependencies(glTFViewer stage_dxc)
