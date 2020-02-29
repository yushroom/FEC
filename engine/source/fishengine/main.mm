// dear imgui: standalone example application for GLFW + Metal, using
// programmable pipeline If you are new to dear imgui, see examples/README.txt
// and documentation at the top of imgui.cpp. (GLFW is a cross-platform general
// purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics
// context creation, etc.)

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_metal.h>
#include <stdio.h>
#include "app.h"
#include "render_metal.h"

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

static void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, 1);
    if (key == GLFW_KEY_R && action == GLFW_PRESS) app_reload();
}

int main(int, char **) {
    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad
    // Controls

    // Setup style
    ImGui::StyleColorsLight();
    // ImGui::StyleColorsClassic();
    ImGuiStyle *style = &ImGui::GetStyle();
    style->WindowRounding = 0;
    style->FrameBorderSize = 1.f;

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can
    // also load multiple fonts and use ImGui::PushFont()/PopFont() to select
    // them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you
    // need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please
    // handle those errors in your application (e.g. use an assertion, or
    // display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and
    // stored into a texture when calling
    // ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame
    // below will call.
    // - Read 'docs/FONTS.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string
    // literal you need to write a double backslash \\ !
//    io.Fonts->AddFontDefault();
#define misc_path "/Users/yushroom/program/cengine/engine/source/thirdparty/imgui/misc/fonts/"
    ImFont *font = io.Fonts->AddFontFromFileTTF(misc_path "DroidSans.ttf", 16.0f);
    //    io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //    io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //    io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //    ImFont* font =
    //    io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f,
    //    NULL, io.Fonts->GetGlyphRangesJapanese());
    IM_ASSERT(font != NULL);

    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    // Create window with graphics context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(800, 600, "Dear ImGui GLFW+Metal example", NULL, NULL);
    if (window == NULL) return 1;

    glfwSetKeyCallback(window, glfw_key_callback);

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> commandQueue = [device newCommandQueue];

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplMetal_Init(device);

    NSWindow *nswin = glfwGetCocoaWindow(window);
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    nswin.contentView.layer = layer;
    nswin.contentView.wantsLayer = YES;

    MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor new];

    RenderInit(layer);
    app_init();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        @autoreleasepool {
            // Poll and handle events (inputs, window resize, etc.)
            // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard
            // flags to tell if dear imgui wants to use your inputs.
            // - When io.WantCaptureMouse is true, do not dispatch mouse input
            // data to your main application.
            // - When io.WantCaptureKeyboard is true, do not dispatch keyboard
            // input data to your main application. Generally you may always
            // pass all inputs to dear imgui, and hide them from your
            // application based on those two flags.
            glfwPollEvents();

            // https://stackoverflow.com/questions/23550423/high-cpu-usage-when-glfw-opengl-window-isnt-visible
            if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) == 1) {
                [NSThread sleepForTimeInterval:1.f];
            } else if (glfwGetWindowAttrib(window, GLFW_VISIBLE) == 0) {
                [NSThread sleepForTimeInterval:0.5f];
            } else if (glfwGetWindowAttrib(window, GLFW_FOCUSED) == 0) {
                [NSThread sleepForTimeInterval:0.2f];
            }

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            layer.drawableSize = CGSizeMake(width, height);
            id<CAMetalDrawable> drawable = [layer nextDrawable];
            id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];

            FrameBegin(commandBuffer, drawable);
            app_update();
            FrameEnd();

            //            renderPassDescriptor.colorAttachments[0].clearColor =
            //            MTLClearColorMake(clear_color[0], clear_color[1],
            //            clear_color[2], clear_color[3]);
            renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
            renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
            renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
            id<MTLRenderCommandEncoder> renderEncoder =
                [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
            [renderEncoder pushDebugGroup:@"ImGui"];

            // Start the Dear ImGui frame
            ImGui_ImplMetal_NewFrame(renderPassDescriptor);
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::ShowDemoWindow();

            app_render_ui();

            // Rendering
            ImGui::Render();
            ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);

            [renderEncoder popDebugGroup];
            [renderEncoder endEncoding];

            [commandBuffer presentDrawable:drawable];
            [commandBuffer commit];
        }
    }

    // Cleanup
    ImGui_ImplMetal_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
