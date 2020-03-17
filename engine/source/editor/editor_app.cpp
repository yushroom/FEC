#include "editor_app.hpp"

#include <iostream>
#include <regex>
#include <fmt/format.h>
#include <animation.h>
#include "imgui_extra.hpp"
#include <transform.h>
#include <ecs.h>
#include <singleton_selection.h>
#include <mesh.h>
#include <material.h>
#include <material_internal.hpp>
#include <shader_internal.hpp>
#include <quickjs.h>
#include <debug.h>
#include <statistics.h>
#include <camera.h>
#include <renderable.h>
#include <app.h>

uint32_t selectedEntity = 0;
static World* world;

void hierarchy_impl(uint32_t id) {
    if (id == 0) return;
    Transform* t = TransformGet(world, id);
    ImGui::PushID(id);
    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth;
    if (selectedEntity == id) flags |= ImGuiTreeNodeFlags_Selected;
    bool isLeaf = t->firstChild == 0;
    if (isLeaf) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    bool open = false;
    const char* name = world->entities[id].name;
    if (strlen(name) == 0)
        open = ImGui::TreeNodeEx("", flags, "Entity%u", id);
    else
        open = ImGui::TreeNodeEx("", flags, "%s##%u", name, id);
    bool clicked = ImGui::IsItemClicked(0) || ImGui::IsItemFocused();
    if (open) {
        if (!isLeaf) {
            hierarchy_impl(t->firstChild);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();

    hierarchy_impl(t->nextSibling);

    if (clicked) selectedEntity = id;
}

struct FEImGuiStyle {
    float toolbarHeight = 40;
    float hierarchyWidth = 200;
    float inspectorWidth = 200;
    float assetHeight = 200;
};
struct FEImGuiStyle g_style;

void HierarchyWindow(World* w) {
    world = w;
    SingletonSelection* selection =
        (SingletonSelection*)WorldGetSingletonComponent(w,
            SingletonSelectionID);
    selectedEntity = selection->selectedEntity;
    auto size = ImGui::GetIO().DisplaySize;
    float height = size.y - g_style.toolbarHeight - g_style.assetHeight;
    ImGui::SetNextWindowSize(ImVec2(g_style.hierarchyWidth, height));
    ImGui::SetNextWindowPos(ImVec2(0, g_style.toolbarHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 8);
    ImGui::Begin("Hierarchy");
    for (int i = 1; i < w->componentArrays[TransformID].m.size; ++i) {
        if (TransformGet(w, i)->parent == 0) hierarchy_impl(i);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    selection->selectedEntity = selectedEntity;
}

void Toolbar(World* world) {
    auto size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(size.x, g_style.toolbarHeight));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse;
    ImGui::Begin("ToolBar", NULL, flags);

    {
        float sw = 48;
        float sh = 24;
        ImGui::BeginSegmentedButtons(4);
        ImVec2 sz(sw, sh);
        ImGui::SegmentedButton("hand", sz);
        ImGui::SegmentedButton("move", sz);
        ImGui::SegmentedButton("rotate", sz);
        ImGui::SegmentedButton("scale", sz);
        ImGui::EndSegmentedButtons();
    }
    {
        float sw = 48;
        float sh = 24;
        float posx = (ImGui::GetWindowWidth() - sw * 3) / 2;
        float posy = (ImGui::GetWindowHeight() - sh) / 2;
        ImGui::SetCursorPosX(posx);
        ImGui::SetCursorPosY(posy);
        ImGui::BeginSegmentedButtons(3);
        ImVec2 sz(sw, sh);
        if (ImGui::SegmentedButton("play", sz, app_is_playing())) {
            if (app_is_playing())
                app_stop();
            else
                app_play();
        }
        if (ImGui::SegmentedButton("pause", sz, app_is_paused())) {
            app_pause();
        }
        ImGui::SegmentedButton("step", sz);
        ImGui::EndSegmentedButtons();
        ImGui::End();
    }
}

static bool inspector_debug_mode = false;

#include "render_d3d12.hpp"
#include "shader_util.h"

void ImGuiTexture(Texture* t, ImVec2 size) {
    if (t) {
        auto handle = GetGPUHandle(t);
        ImTextureID id = *(ImTextureID*)&handle;
        ImGui::Image(id, size);
    }
}

void AssetInspectorWindow(World* w, AssetID assetID) {
    Asset* asset = AssetGet(assetID);
    if (asset == NULL) return;

    ImGui::TextWrapped("path: %s", asset->filePath);
    if (asset->type == AssetTypeTexture) {
        Texture* t = (Texture*)asset->ptr;
        ImGui::Text("Width: %u", t->width);
        ImGui::Text("Height: %u", t->height);
        float w = ImGui::GetContentRegionAvailWidth();
        ImGuiTexture(t, { w, w });
    }
    else if (asset->type == AssetTypeMesh) {
        Mesh* mesh = (Mesh*)asset->ptr;
        ImGui::Text("Vertex Count: %u", MeshGetVertexCount(mesh));
        ImGui::Text("Triangle Count: %u", MeshGetIndexCount(mesh) / 3);
    }
}

void InspectorWindow(World* w) {
    Toolbar(w);
    //    if (selectedEntity >= w->entityCount) return;
    auto size = ImGui::GetIO().DisplaySize;
    float height = size.y - g_style.toolbarHeight;
    ImGui::SetNextWindowSize(ImVec2(g_style.inspectorWidth, height));
    ImGui::SetNextWindowPos(
        ImVec2(size.x - g_style.inspectorWidth, g_style.toolbarHeight));
    ImGui::Begin("Inspector");

    SingletonSelection* selection =
        (SingletonSelection*)WorldGetSingletonComponent(w,
            SingletonSelectionID);
    if (selection->selectedEntity != 0) {
        ImGui::Checkbox("debug", &inspector_debug_mode);

        if (selectedEntity != 0 && selectedEntity < w->entityCount) {
            ImGui::Text("Transform");
            SingletonTransformManager* tm =
                (SingletonTransformManager*)WorldGetSingletonComponent(
                    w, SingletonTransformManagerID);
            Transform* t = TransformGet(w, selectedEntity);
            uint32_t idx = WorldGetComponentIndex(w, t, TransformID);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 2));
            if (ImGui::InputFloat3("position", &t->localPosition.x)) {
                TransformSetDirty(w, t);
            }
            auto eulerHint = tm->LocalEulerAnglesHints[idx];
            float3 euler;
            bool useHint = !float3_is_zero(eulerHint);
            if (useHint)
                euler = eulerHint;
            else
                euler = quat_to_euler(t->localRotation);
            if (ImGui::InputFloat3("rotation", &euler.x)) {
                t->localRotation = euler_to_quat(euler);
                tm->LocalEulerAnglesHints[idx] = euler;
                TransformSetDirty(w, t);
            }
            if (ImGui::InputFloat3("scale", &t->localScale.x)) {
                TransformSetDirty(w, t);
            }
            if (inspector_debug_mode) {
                ImGui::Text("parent: %u", t->parent);
                ImGui::Text("firstChild: %u", t->firstChild);
                ImGui::Text("nextSibling: %u", t->nextSibling);

                float3 position = TransformGetPosition(w, t);
                ImGui::InputFloat3("wposition", &position.x);

                float4 q = t->localRotation;
                if (ImGui::InputFloat4("qrotation", (float*)&q)) {
                    t->localRotation = q;
                    TransformSetDirty(w, t);
                }

                ImGui::Text("tm.mod: %u", tm->modified);
                ImGui::Text("mod: %u", tm->H[idx].modified);
            }
            ImGui::PopStyleVar();

            Camera* camera =
                (Camera*)EntityGetComponent(selectedEntity, w, CameraID);
            if (camera) {
                ImGui::Separator();
                ImGui::TextUnformatted("Camera");
            }

            Renderable* renderable = (Renderable*)EntityGetComponent(
                selectedEntity, w, RenderableID);
            if (renderable) {
                ImGui::Separator();
                ImGui::TextUnformatted("Renderable");
                if (renderable->material) {
                    Material* m = renderable->material;
                    if (m->shader) {
                        Shader* s = m->shader;
                        int count = ShaderUtilGetPropertyCount(s);
                        for (int i = 0; i < count; ++i) {
                            auto type = ShaderUtilGetPropertyType(s, i);
                            const char* name = ShaderUtilGetPropertyName(s, i);
                            int nameID = ShaderPropertyToID(name);
                            if (type == ShaderPropertyTypeFloat) {
                                float v = MaterialGetFloat(m, nameID);
                                if (ImGui::SliderFloat(name, &v, 0, 1)) {
                                    MaterialSetFloat(m, nameID, v);
                                }
                            }
                            else if (type == ShaderPropertyTypeVector ||
                                type == ShaderPropertyTypeColor) {
                                float4 v = MaterialGetVector(m, nameID);
                                float c[4] = { v.x, v.y, v.z, v.w };
                                if (ImGui::ColorEdit4(name, c)) {
                                    MaterialSetVector(
                                        m, nameID,
                                        float4_make(c[0], c[1], c[2], c[3]));
                                }
                            }
                            else if (type == ShaderPropertyTypeTexture) {
                                Texture* t = MaterialGetTexture(m, nameID);
                                ImGui::TextUnformatted(name);
                                if (t) {
                                    auto handle = GetGPUHandle(t);
                                    ImTextureID id = *(ImTextureID*)&handle;
                                    if (ImGui::ImageButton(id, { 64, 64 })) {
                                        // ???
                                        SingletonSelectionSetSelectedAsset(
                                            selection, t->assetID);
                                    }
                                    if (ImGui::IsItemClicked()) {
                                        SingletonSelectionSetSelectedAsset(
                                            selection, t->assetID);
                                    }
                                }
                            }
                        }
                        ImGui::TextUnformatted("Keywords");
                        for (auto& kw : ShaderGetKeywords(s)) {
                            bool enabled =
                                MaterialIsKeywordEnabled(m, kw.c_str());
                            if (ImGui::Checkbox(kw.c_str(), &enabled))
                                MaterialSetKeyword(m, kw.c_str(), enabled);
                        }
                        ImGui::TextUnformatted("Shader Variant");
                        for (int i = 0; i < s->passCount; ++i) {
                            int var = MaterialGetVariantIndex(m, i);
                            ImGui::Text("Pass %d: %d", i, var);
                        }
                    }
                }
            }

            Animation* animation =
                (Animation*)EntityGetComponent(selectedEntity, w, AnimationID);
            if (animation) {
                ImGui::Separator();
                ImGui::TextUnformatted("Animation");
                ImGui::LabelText("localTimer", "%f", animation->localTime);
                ImGui::Checkbox("playing", &animation->playing);
            }
        }
    }
    else if (selection->selectedAsset != 0) {
        AssetInspectorWindow(w, selection->selectedAsset);
    }
    ImGui::End();
}

void AssetWindow(World* w) {
    auto size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(
        ImVec2(size.x - g_style.inspectorWidth, g_style.assetHeight));
    ImGui::SetNextWindowPos(ImVec2(0, size.y - g_style.assetHeight));
    ImGui::Begin("Assets");
    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;

    float window_visible_x2 =
        ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

    SingletonSelection* selection =
        (SingletonSelection*)WorldGetSingletonComponent(w,
            SingletonSelectionID);

    if (ImGui::BeginTabBar("Assets", tab_bar_flags)) {
        if (ImGui::BeginTabItem("Texture")) {
            auto count = AssetTypeCount(AssetTypeTexture);
            for (int i = 0; i < count; ++i) {
                ImGuiStyle& style = ImGui::GetStyle();
                ImVec2 button_sz(64, 64);
                Asset* asset = AssetGet2(AssetTypeTexture, i);
                AssetID aid = asset->id;
                auto handle = GetGPUHandle((Texture*)asset->ptr);
                ImTextureID id = *(ImTextureID*)&handle;
                if (selection->selectedAsset == aid) {
                    button_sz.x -= 2;
                    button_sz.y -= 2;
                    ImGui::Image(id, button_sz, { 0, 0 }, { 1, 1 }, { 1, 1, 1, 1 },
                        { 0, 0.5, 1, 1 });
                }
                else {
                    ImGui::Image(id, button_sz);
                }
                if (ImGui::IsItemClicked()) {
                    SingletonSelectionSetSelectedAsset(selection, aid);
                }
                float last_button_x2 = ImGui::GetItemRectMax().x;
                float next_button_x2 =
                    last_button_x2 + style.ItemSpacing.x +
                    button_sz.x;  // Expected position if next button was on
                                  // same line
                if (next_button_x2 < window_visible_x2) {
                    ImGui::SameLine();
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Mesh")) {
            auto count = AssetTypeCount(AssetTypeMesh);
            for (int i = 0; i < count; ++i) {
                ImGuiStyle& style = ImGui::GetStyle();
                ImVec2 button_sz(64, 64);
                Asset* asset = AssetGet2(AssetTypeMesh, i);
                AssetID aid = asset->id;
                std::string name = "Mesh";
                name += std::to_string(i);
                ImGui::Button(name.c_str(), button_sz);
                // auto handle = GetGPUHandle((Texture *)asset->ptr);
                // ImTextureID id = *(ImTextureID *)&handle;
                // if (selection->selectedAsset == aid) {
                //    button_sz.x -= 2;
                //    button_sz.y -= 2;
                //    ImGui::Image(id, button_sz, {0, 0}, {1, 1}, {1, 1, 1, 1},
                //                 {0, 0.5, 1, 1});
                //} else {
                //    ImGui::Image(id, button_sz);
                //}
                if (ImGui::IsItemClicked()) {
                    SingletonSelectionSetSelectedAsset(selection, aid);
                }
                float last_button_x2 = ImGui::GetItemRectMax().x;
                float next_button_x2 =
                    last_button_x2 + style.ItemSpacing.x +
                    button_sz.x;  // Expected position if next button was on
                                  // same line
                if (next_button_x2 < window_visible_x2) {
                    ImGui::SameLine();
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("AnimationClip")) {
            ImGui::Text("This is the Cucumber tab!\nblah blah blah blah blah");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("All")) {
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

static inline float MB(uint32_t s) { return 1.0f * s / (1024 * 1024); }

void StatisticsWindow(World* w, JSRuntime* rt) {
    ImGui::Begin("Statistics");

    ImGui::Text("%s", "GPU");
    uint32_t total = 0;
    ImGui::Text("    draw call: %u", g_statistics.gpu.drawCall);
    ImGui::Text("    buffer count: %u", g_statistics.gpu.bufferCount);
    ImGui::Text("    buffer size: %.2f MB", MB(g_statistics.gpu.bufferSize));
    total += g_statistics.gpu.bufferSize;
    ImGui::Text("    texture count: %u", g_statistics.gpu.textureCount);
    ImGui::Text("    texture size: %.2f MB", MB(g_statistics.gpu.textureSize));
    total += g_statistics.gpu.textureSize;
    ImGui::Text("    rt count: %u", g_statistics.gpu.renderTextureCount);
    ImGui::Text("    rt size: %.2f MB", MB(g_statistics.gpu.renderTextureSize));
    total += g_statistics.gpu.renderTextureSize;
    ImGui::Text("    total: %.2f MB", MB(total));
    ImGui::Separator();

    uint32_t total2 = 0;
    ImGui::Text("%s", "CPU");
    ImGui::Text("    vb: %.2f MB", MB(g_statistics.cpu.vertexBufferSize));
    total2 += g_statistics.cpu.vertexBufferSize;
    ImGui::Text("    ib: %.2f MB", MB(g_statistics.cpu.indexBufferSize));
    total2 += g_statistics.cpu.indexBufferSize;
    g_statistics.cpu.ecsSize = WorldGetMemoryUsage(w);
    ImGui::Text("    ecs: %.2f MB", MB(g_statistics.cpu.ecsSize));
    total2 += g_statistics.cpu.ecsSize;
    ImGui::Text("    upload heap: %.2f MB",
        MB(g_statistics.cpu.uploadHeapSize));
    total2 += g_statistics.cpu.uploadHeapSize;
    ImGui::Text("    total: %.2f MB", MB(total2));
    ImGui::Separator();

    static JSMemoryUsage usage;
    static uint64_t frame = 0;
    if (frame % 60 == 0) {
        JS_ComputeMemoryUsage(rt, &usage);
    }
    frame++;
    ImGui::Text("JS runtime:");
    ImGui::Text("    malloc count: %lld", usage.malloc_count);
    ImGui::Text("    malloc size: %.2f MB", MB(usage.malloc_size));
    ImGui::Text("    memory used count: %lld", usage.memory_used_count);
    ImGui::Text("    memory used size: %.2f MB", MB(usage.memory_used_size));
    ImGui::Separator();

    ImGui::Text("Total: %.2f MB", MB(total + total2 + usage.malloc_size));

    ImGui::End();
}


void open_file_at_line(const char* file, uint32_t line) {
    const char* vsc_path =
        R"(C:\Users\yushroom\AppData\Local\Programs\Microsoft VS Code\Code.exe)";
    const auto command = fmt::format(R"(""{}" -g "{}":{}")", vsc_path, file, line);
    printf("command: %s\n", command.c_str());
    system(command.c_str());
}

void open_file_by_callstack(const char* callstack) {
    std::regex rgx("at.+\\((.+)\\:(\\d+)\\)");
    std::regex rgx2("at\\s(.+)\\:(\\d+)");
    std::smatch matches;

    std::string s = callstack;
    if (std::regex_search(s, matches, rgx) ||
        std::regex_search(s, matches, rgx2)) {
        if (matches.size() >= 3) {
            open_file_at_line(matches[1].str().c_str(),
                std::atoi(matches[2].str().c_str()));
        }
    }
}

void ConsoleWindow() {
    if (!debug_has_error()) return;

    ImGui::Begin("Console");

    if (ImGui::Button("Clear")) {
        debug_clear_all();
    }

    for (int i = 0; i < debug_get_error_message_count(); ++i) {
        const char* msg = debug_get_error_message_at(i);
        char* callstack = (char*)strchr(msg, '\n');
        if (callstack != NULL) *callstack = '\0';

        ImVec4 red(1, 0, 0, 1);
        ImGui::TextColored(red, "[%d]%s", i, msg);

        if (callstack != NULL) *callstack = '\n';
        if (ImGui::IsItemClicked(0)) {
#ifdef APPLE
            char command[strlen(msg) + 32];
            sprintf(command, "echo \"%s\" | pbcopy", msg);
            printf("copy to clipbord: %s\n", command);
            system(command);
            puts("copyed!");
#endif
            open_file_by_callstack(msg);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::Text("%s", callstack + 1);
        }
    }
    ImGui::End();
}

void EditorApp::OnEditorUI() {
    HierarchyWindow(m_EditorWorld);
    InspectorWindow(m_EditorWorld);
    AssetWindow(m_EditorWorld);
    //StatisticsWindow(m_EditorWorld, rt);
    ConsoleWindow();
}