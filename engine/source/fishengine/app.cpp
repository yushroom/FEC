#include "app.h"

#include <imgui.h>
#include <quickjs.h>

#include "animation.h"
#include "asset.h"
#include "camera.h"
#include "debug.h"
#include "ecs.h"
#include "mesh.h"
#include "renderable.h"
#include "statistics.h"
#include "transform.h"
#include "ddsloader.h"
#include "shader_internal.hpp"
#include "material_internal.hpp"
#include "singleton_selection.h"

extern "C" {
void HierarchyWindow(World *w);
void InspectorWindow(World *w);
void AssetWindow();
void StatisticsWindow(World *w, JSRuntime *rt);
void ConsoleWindow();
void open_file_by_callstack(const uint8_t *callstack);
const char *ApplicationFilePath();
void LoadTAR();
}

uint32_t selectedEntity = 0;
static World *world;

void hierarchy_impl(uint32_t id) {
    if (id == 0) return;
    Transform *t = TransformGet(world, id);
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
    const char *name = world->entities[id].name;
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

constexpr float width = 200;
constexpr float toolbarHeight = 40;

void HierarchyWindow(World *w) {
    world = w;
    SingletonSelection *selection =
        (SingletonSelection *)WorldGetSingletonComponent(w,
                                                         SingletonSelectionID);
    selectedEntity = selection->selectedEntity;
    auto size = ImGui::GetIO().DisplaySize;
    float height = (size.y - toolbarHeight) / 2;
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::SetNextWindowPos(ImVec2(size.x - width, toolbarHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 8);
    ImGui::Begin("Hierarchy");
    for (int i = 1; i < w->componentArrays[TransformID].m.size; ++i) {
        if (TransformGet(w, i)->parent == 0) hierarchy_impl(i);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    selection->selectedEntity = selectedEntity;
}

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

namespace ImGui {
// Render a rectangle shaped with optional rounding and borders
void RenderFrame2(ImVec2 p_min, ImVec2 p_max, ImU32 fill_col, bool border,
                  float rounding, ImDrawCornerFlags corner_flags) {
    ImGuiContext &g = *GImGui;
    ImGuiWindow *window = g.CurrentWindow;
    window->DrawList->AddRectFilled(p_min, p_max, fill_col, rounding,
                                    corner_flags);
    //		const float border_size = g.Style.FrameBorderSize;
    const float border_size = 1.0f;
    if (border && border_size > 0.0f) {
        window->DrawList->AddRect(p_min + ImVec2(1, 1), p_max + ImVec2(1, 1),
                                  GetColorU32(ImGuiCol_BorderShadow), rounding,
                                  corner_flags, border_size);
        window->DrawList->AddRect(p_min, p_max, GetColorU32(ImGuiCol_Border),
                                  rounding, corner_flags, border_size);
    }
}

bool ButtonEx2(const char *label, const ImVec2 &size_arg,
               ImGuiButtonFlags flags, ImDrawCornerFlags corner_flags,
               bool active) {
    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext &g = *GImGui;
    const ImGuiStyle &style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    if ((flags & ImGuiButtonFlags_AlignTextBaseLine) &&
        style.FramePadding.y <
            window->DC.CurrLineTextBaseOffset)  // Try to vertically align
                                                // buttons that are smaller/have
                                                // no padding so that text
                                                // baseline matches (bit hacky,
                                                // since it shouldn't be a flag)
        pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
    ImVec2 size =
        CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f,
                     label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    if (window->DC.ItemFlags & ImGuiItemFlags_ButtonRepeat)
        flags |= ImGuiButtonFlags_Repeat;
    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

    // Render
    const ImU32 col =
        GetColorU32((active || (held && hovered))
                        ? ImGuiCol_ButtonActive
                        : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    RenderNavHighlight(bb, id);
    RenderFrame2(bb.Min, bb.Max, col, true, 4.0f, corner_flags);
    RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding,
                      label, NULL, &label_size, style.ButtonTextAlign, &bb);

    // Automatically close popups
    // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) &&
    // (window->Flags & ImGuiWindowFlags_Popup))
    //    CloseCurrentPopup();

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
    return pressed;
}

bool Button2(const char *label, const ImVec2 &size_arg,
             ImDrawCornerFlags corner_flags, bool active = false) {
    return ButtonEx2(label, size_arg, 0, corner_flags, active);
}
}  // namespace ImGui

struct FEImGuiStyle {
    float toolBarHeight;
    float inspectorWidth;
};

struct FEImGuiStyle g_style;

void Toolbar(World *world) {
    auto size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(size.x, toolbarHeight));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse;
    ImGui::Begin("ToolBar", NULL, flags);

    {
        float sw = 48;
        float sh = 24;
        //		ImGui::SetCursorPosX(posx);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 2));
        ImGui::Button2("hand", ImVec2(sw, sh), ImDrawCornerFlags_Left);
        ImGui::SameLine(0);
        ImGui::Button2("move", ImVec2(sw, sh), ImDrawCornerFlags_None);
        ImGui::SameLine(0);
        ImGui::Button2("rotate", ImVec2(sw, sh), ImDrawCornerFlags_None);
        ImGui::SameLine(0);
        ImGui::Button2("scale", ImVec2(sw, sh), ImDrawCornerFlags_Right);
        ImGui::PopStyleVar();
    }
    {
        float sw = 48;
        float sh = 24;
        float posx = (ImGui::GetWindowWidth() - sw * 3) / 2;
        float posy = (ImGui::GetWindowHeight() - sh) / 2;
        ImGui::SetCursorPosX(posx);
        ImGui::SetCursorPosY(posy);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 2));
        if (ImGui::Button2("play", ImVec2(sw, sh), ImDrawCornerFlags_Left,
                           app_is_playing())) {
            if (app_is_playing())
                app_stop();
            else
                app_play();
        }
        ImGui::SameLine(0);
        if (ImGui::Button2("pause", ImVec2(sw, sh), ImDrawCornerFlags_None,
                           app_is_paused())) {
            app_pause();
        }
        ImGui::SameLine(0);
        ImGui::Button2("step", ImVec2(sw, sh), ImDrawCornerFlags_Right);
        ImGui::PopStyleVar();
        ImGui::End();
    }
}

static bool inspector_debug_mode = false;

#include "shader_util.h"

void InspectorWindow(World *w) {
    Toolbar(w);
    //    if (selectedEntity >= w->entityCount) return;
    auto size = ImGui::GetIO().DisplaySize;
    float height = (size.y - toolbarHeight) / 2;
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::SetNextWindowPos(ImVec2(size.x - width, size.y - height));
    ImGui::Begin("Inspector");

    ImGui::Checkbox("debug", &inspector_debug_mode);

    if (selectedEntity != 0 && selectedEntity < w->entityCount) {
        ImGui::Text("Transform");
        SingletonTransformManager *tm =
            (SingletonTransformManager *)WorldGetSingletonComponent(
                w, SingletonTransformManagerID);
        Transform *t = TransformGet(w, selectedEntity);
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
            if (ImGui::InputFloat4("qrotation", (float *)&q)) {
                t->localRotation = q;
                TransformSetDirty(w, t);
            }

            ImGui::Text("tm.mod: %u", tm->modified);
            ImGui::Text("mod: %u", tm->H[idx].modified);
        }
        ImGui::PopStyleVar();

        Camera *camera =
            (Camera *)EntityGetComponent(selectedEntity, w, CameraID);
        if (camera) {
            ImGui::Separator();
            ImGui::TextUnformatted("Camera");
        }

        Renderable *renderable =
            (Renderable *)EntityGetComponent(selectedEntity, w, RenderableID);
        if (renderable) {
            ImGui::Separator();
            ImGui::TextUnformatted("Renderable");
            if (renderable->material) {
                Material *m = renderable->material;
                if (m->shader) {
                    Shader *s = m->shader;
                    int count = ShaderUtilGetPropertyCount(s);
                    for (int i = 0; i < count; ++i) {
                        auto type = ShaderUtilGetPropertyType(s, i);
                        const char *name = ShaderUtilGetPropertyName(s, i);
                        if (type == ShaderPropertyTypeFloat) {
                            int nameID = ShaderPropertyToID(name);
                            float v = MaterialGetFloat(m, nameID);
                            if (ImGui::SliderFloat(name, &v, 0, 1)) {
                                MaterialSetFloat(m, nameID, v);
                            }
                        } else if (type == ShaderPropertyTypeVector ||
                                   type == ShaderPropertyTypeColor) {
                            int nameID = ShaderPropertyToID(name);
                            float4 v = MaterialGetVector(m, nameID);
                            float c[4] = {v.x, v.y, v.z, v.w};
                            if (ImGui::ColorEdit4(name, c)) {
                                MaterialSetVector(
                                    m, nameID,
                                    float4_make(c[0], c[1], c[2], c[3]));
                            }
                        }
                    }
                    ImGui::TextUnformatted("Keywords");
                    for (auto &kw : ShaderGetKeywords(s)) {
                        bool enabled = MaterialIsKeywordEnabled(m, kw.c_str());
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

        Animation *animation =
            (Animation *)EntityGetComponent(selectedEntity, w, AnimationID);
        if (animation) {
            ImGui::Separator();
            ImGui::TextUnformatted("Animation");
            ImGui::LabelText("localTimer", "%f", animation->localTime);
            ImGui::Checkbox("playing", &animation->playing);
        }
    }
    ImGui::End();
}

void AssetWindow() {
    auto size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(size.x - width, 200));
    ImGui::SetNextWindowPos(ImVec2(0, size.y - 200));
    ImGui::Begin("Assets");
    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("Assets", tab_bar_flags)) {
        if (ImGui::BeginTabItem("All")) {
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Mesh")) {
            ImGui::Text("This is the Avocado tab!\nblah blah blah blah blah");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Texture")) {
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("AnimationClip")) {
            ImGui::Text("This is the Cucumber tab!\nblah blah blah blah blah");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

static inline float MB(uint32_t s) { return 1.0f * s / (1024 * 1024); }

void StatisticsWindow(World *w, JSRuntime *rt) {
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

void ConsoleWindow() {
    if (!debug_has_error()) return;

    ImGui::Begin("Console");

    if (ImGui::Button("Clear")) {
        debug_clear_all();
    }

    for (int i = 0; i < debug_get_error_message_count(); ++i) {
        const char *msg = debug_get_error_message_at(i);
        char *callstack = (char *)strchr(msg, '\n');
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
            open_file_by_callstack((uint8_t *)msg);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::Text("%s", callstack + 1);
        }
    }
    ImGui::End();
}


#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"

// from
// https://stackoverflow.com/questions/516200/relative-paths-not-working-in-xcode-c
// ----------------------------------------------------------------------------
// This makes relative paths work in C++ in Xcode by changing directory to the
// Resources folder inside the .app bundle
std::string ApplicationFilePath() {
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    char path[PATH_MAX];
    if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path,
                                          PATH_MAX)) {
        // error!
        abort();
    }
    CFRelease(resourcesURL);

    chdir(path);
    //	std::cout << "Current Path: " << path << std::endl;
    return std::string(path);
}
#endif
// ----------------------------------------------------------------------------

#if _WIN32
#include <Shlwapi.h>
#include <filesystem>

char g_exe_path[MAX_PATH];

const char *ApplicationFilePath() {
    auto hModule = GetModuleHandleW(NULL);
    char path2[MAX_PATH];
    GetModuleFileNameA(hModule, path2, MAX_PATH);
    std::filesystem::path p(path2);
    std::string p2 = p.parent_path().string();
    memcpy(g_exe_path, p2.c_str(), p2.length());
    return g_exe_path;
}
#endif

#if __linux__
// #include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>  // memset

#include <filesystem>

// https://stackoverflow.com/questions/4025370/can-an-executable-discover-its-own-path-linux
std::string ApplicationFilePath() {
    char dest[PATH_MAX];
    memset(dest, 0, sizeof(dest));
    if (readlink("/proc/self/exe", dest, PATH_MAX) == -1) {
        puts("[fatal error] readlink failed!");
        abort();
    }
    std::filesystem::path p(dest);
    return p.parent_path().string();
}
#endif

#include <fmt/format.h>

bool ConvertToDDS(const char *path) {
    std::filesystem::path p(path);
    p = p.lexically_normal();
    assert(std::filesystem::exists(p));
    const auto dir = p.parent_path();
    
    std::filesystem::path texconv = ApplicationFilePath();
    texconv = texconv / "texconv.exe";
    texconv = texconv.lexically_normal();
    assert(std::filesystem::exists(texconv));
    const auto cmd = fmt::format(R"({} -f BC1_UNORM -y -o "{}" "{}")",
                                 texconv.string(), dir.string(), p.string());
    puts(cmd.c_str());
    int ret = system(cmd.c_str());
    return ret == 0;
}