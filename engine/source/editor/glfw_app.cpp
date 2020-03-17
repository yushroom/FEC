#include "glfw_app.hpp"

#include <imgui.h>
#include <imgui_impl_dx12.h>
//#include <tchar.h>

#include <imgui_impl_glfw.h>
#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <stdio.h>
#include <string>
#include <filesystem>

#include "ecs.h"
#include "app.h"
#include "render_d3d12.hpp"
#include "input.h"
#include "animation.h"
#include "free_camera.h"

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

static void glfw_key_callback(GLFWwindow* window, int key, int scancode,
    int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);
    if (key == GLFW_KEY_R && action == GLFW_PRESS) app_reload2();

    World* world = (World*)glfwGetWindowUserPointer(window);
    SingletonInput* si = (SingletonInput*)WorldGetSingletonComponent(world, SingletonInputID);
    KeyEvent e;
    if (action != GLFW_PRESS && action != GLFW_RELEASE) return;
    e.action = action == GLFW_PRESS ? KeyActionPressed : KeyActionReleased;
    e.key = KeyCodeFromGLFWKey(key);
    SingletonInputPostKeyEvent(si, e);
}

static void glfw_mouse_button_callback(GLFWwindow* window, int button,
    int action, int mods) {
    KeyEvent e;
    if (action == GLFW_PRESS) {
        e.action = KeyActionPressed;
    }
    else if (action == GLFW_RELEASE) {
        e.action = KeyActionReleased;
    }
    else {
        return;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        e.key = KeyCodeMouseLeftButton;
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        e.key = KeyCodeMouseRightButton;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        e.key = KeyCodeMouseMiddleButton;
    else
        return;
    World* world = (World *)glfwGetWindowUserPointer(window);
    SingletonInput* si = (SingletonInput*)WorldGetSingletonComponent(
        world, SingletonInputID);
    SingletonInputPostKeyEvent(si, e);
}

void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    World* world = (World*)glfwGetWindowUserPointer(window);
    SingletonInput* si = (SingletonInput*)WorldGetSingletonComponent(
        world, SingletonInputID);
    SingletonInputUpdateAxis(si, AxisMouseScrollWheel, yoffset);
}

static void glfw_resize_callback(GLFWwindow* window, int width, int height) {
    WaitForLastSubmittedFrame();
    ImGui_ImplDX12_InvalidateDeviceObjects();
    CleanupRenderTarget();
    HWND hWnd = glfwGetWin32Window(window);
    ResizeSwapChain(hWnd, width, height);
    //CreateRenderTarget();
    ImGui_ImplDX12_CreateDeviceObjects();
}


#define COMP(T)                                                        \
    {                                                                  \
        .type = T##ID, .name = #T, .size = sizeof(T), .ctor = T##Init, \
        .dtor = NULL,                                                  \
    }
#define COMP2(T)                                                    \
    {                                                               \
        .type = T##ID, .name = #T, .size = sizeof(T), .ctor = NULL, \
        .dtor = NULL,                                               \
    }

#include <transform.h>
#include <renderable.h>
#include <camera.h>
#include <light.h>
#include <animation.h>
#include <free_camera.h>
#include <input.h>
#include <singleton_time.h>
#include <singleton_selection.h>

static ComponentDef g_componentDef[] = { COMP(Transform), COMP(Renderable),
                                        COMP(Camera),    COMP(Light),
                                        COMP(Animation), COMP(FreeCamera) };

static ComponentDef g_singleComponentDef[] = {
    COMP(SingletonTransformManager), COMP(SingletonInput),
    COMP2(SingletonTime), COMP(SingletonSelection) };


void RenderSystem(World* w) {
    ComponentArray* a = w->componentArrays + RenderableID;
    uint32_t size = a->m.size;
    Renderable* r = (Renderable*)a->m.ptr;
    for (int i = 0; i < size; ++i, ++r) {
        if (r->mesh && !MeshIsUploaded(r->mesh)) {
            MeshUploadMeshData(r->mesh);
        }
    }
    r = (Renderable*)a->m.ptr;
    for (int i = 0; i < size; ++i, ++r) {
        if (r->skin) {
            RenderableUpdateBones(r, w);
            GPUSkinning(r);
        }
    }
    r = (Renderable*)a->m.ptr;
    BeginPass();
    for (int i = 0; i < size; ++i, ++r) {
        Transform* t =
            (Transform *)ComponentGetSiblingComponent(w, r, RenderableID, TransformID);
        SimpleDraw(t, r);
    }
}


int GLFWApp::Init()
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    // Create window with graphics context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_Window =
        glfwCreateWindow(1280, 800, "FishEngine Editor", NULL, NULL);
    if (m_Window == NULL) return 1;

    glfwSetKeyCallback(m_Window, glfw_key_callback);
    glfwSetWindowSizeCallback(m_Window, glfw_resize_callback);
    glfwSetMouseButtonCallback(m_Window, glfw_mouse_button_callback);
    glfwSetScrollCallback(m_Window, glfw_scroll_callback);

    HWND hwnd = glfwGetWin32Window(m_Window);
    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    //ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic);
    ImGui::StyleColorsLight();
    ImGuiStyle* style = &ImGui::GetStyle();
    style->WindowRounding = 0;
    style->FrameBorderSize = 1.f;

    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    // Setup Platform/Renderer bindings
    auto srvHeap = GetSRVDescriptorHeap();
    ImGui_ImplDX12_Init(GetDevice(), NUM_FRAMES_IN_FLIGHT,
        DXGI_FORMAT_R8G8B8A8_UNORM, srvHeap,
        srvHeap->GetCPUDescriptorHandleForHeapStart(),
        srvHeap->GetGPUDescriptorHandleForHeapStart());

    std::string font_path = ApplicationFilePath();
    font_path += "/../DroidSans.ttf";
    if (std::filesystem::exists(font_path)) {
        ImFont* font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f);
        IM_ASSERT(font != NULL);
    }

    WorldDef def;
    def.componentDefs = g_componentDef;
    def.componentDefCount = countof(g_componentDef);
    def.singletonComponentDefs = g_singleComponentDef;
    def.singletonComponentDefCount = countof(g_singleComponentDef);

    m_EditorWorld = WorldCreate(&def);
    WorldAddSystem(m_EditorWorld, AnimationSystem);
    WorldAddSystem(m_EditorWorld, FreeCameraSystem);
    WorldAddSystem(m_EditorWorld, RenderSystem);
    WorldPrintStats(m_EditorWorld);

    app_init(m_EditorWorld);

    glfwSetWindowUserPointer(m_Window, m_EditorWorld);

    return 0;
}

void GLFWApp::Run()
{
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    while (!glfwWindowShouldClose(m_Window))
    {
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(m_Window, &width, &height);

        double xpos, ypos;
        glfwGetCursorPos(m_Window, &xpos, &ypos);

        SingletonInput* si = (SingletonInput*)WorldGetSingletonComponent(
            m_EditorWorld, SingletonInputID);
        SingletonInputSetMousePosition(si, xpos / width, 1 - ypos / height);

        FrameBegin();

        app_update();

        // Start the Dear ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        OnEditorUI();
        app_render_ui();

        // Rendering
        ImGui::Render();

        BeginRenderEvent("ImGui");
        auto cmdList = GetCurrentGraphicsCommandList();
        auto srvHeap = GetSRVDescriptorHeap();
        cmdList->SetDescriptorHeaps(1, &srvHeap);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
        EndRenderEvent();

        FrameEnd();
        app_frame_end();
    }

    WaitForLastSubmittedFrame();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplGlfw_NewFrame();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}
