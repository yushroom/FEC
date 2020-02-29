#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

#include <imgui.h>
#include <imgui_impl_metal.h>
#include <imgui_impl_osx.h>

@interface Renderer : NSObject <MTKViewDelegate>
- (nonnull instancetype)initWithView:(nonnull MTKView *)view;
@end

@interface Renderer ()
@property(nonatomic, strong) id<MTLDevice> device;
@property(nonatomic, strong) id<MTLCommandQueue> commandQueue;

//@property TestEditor* gameApp;
@end

@implementation Renderer

- (nonnull instancetype)initWithView:(nonnull MTKView *)view;
{
    self = [super init];

    if (self) {
        _device = view.device;
        _commandQueue = [_device newCommandQueue];
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplMetal_Init(_device);

    return self;
}

- (void)drawInMTKView:(nonnull MTKView *)view {
    // NSLog(@"drawInMTKView");
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize.x = view.bounds.size.width;
    io.DisplaySize.y = view.bounds.size.height;

#if TARGET_OS_OSX
    CGFloat framebufferScale =
        view.window.screen.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor;
#else
    CGFloat framebufferScale = view.window.screen.scale ?: UIScreen.mainScreen.scale;
#endif
    io.DisplayFramebufferScale = ImVec2(framebufferScale, framebufferScale);

    io.DeltaTime = 1 / float(view.preferredFramesPerSecond ?: 60);

    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];

    static bool show_demo_window = true;
    static bool show_another_window = false;
    static float clear_color[4] = {0.28f, 0.36f, 0.5f, 1.0f};

    MTLRenderPassDescriptor *renderPassDescriptor = view.currentRenderPassDescriptor;
    if (renderPassDescriptor != nil) {
        renderPassDescriptor.colorAttachments[0].clearColor =
            MTLClearColorMake(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);

        // Here, you could do additional rendering work, including other passes as necessary.

        id<MTLRenderCommandEncoder> renderEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder pushDebugGroup:@"ImGui demo"];

        // Start the Dear ImGui frame
        ImGui_ImplMetal_NewFrame(renderPassDescriptor);
#if TARGET_OS_OSX
        ImGui_ImplOSX_NewFrame(view);
#endif
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You
        // can browse its code to learn more about Dear ImGui!).
        if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a
        // named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin(
                "Hello, world!");  // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");  // Display some text (you can use a format
                                                       // strings too)
            ImGui::Checkbox("Demo Window",
                            &show_demo_window);  // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f,
                               1.0f);  // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color",
                              (float *)&clear_color);  // Edit 3 floats representing a color

            if (ImGui::Button("Button"))  // Buttons return true when clicked (most widgets return
                                          // true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                        1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window) {
            ImGui::Begin(
                "Another Window",
                &show_another_window);  // Pass a pointer to our bool variable (the window will have
                                        // a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me")) show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        ImDrawData *drawData = ImGui::GetDrawData();
        ImGui_ImplMetal_RenderDrawData(drawData, commandBuffer, renderEncoder);

        [renderEncoder popDebugGroup];
        [renderEncoder endEncoding];

        [commandBuffer presentDrawable:view.currentDrawable];
    }

    [commandBuffer commit];
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
    //	CGFloat w = size.width;
    //	CGFloat h = size.height;
    //	NSLog(@"drawableSizeWillChange");
}

@end

@interface ViewController : NSViewController
@property(nonatomic, readonly) MTKView *mtkView;
@property(nonatomic, strong) Renderer *renderer;
@end

@implementation ViewController
- (MTKView *)mtkView {
    return (MTKView *)self.view;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    self.renderer = [[Renderer alloc] initWithView:self.mtkView];
    self.mtkView.delegate = self.renderer;

    [self.mtkView setPreferredFramesPerSecond:60];

    //#if TARGET_OS_OSX
    // Add a tracking area in order to receive mouse events whenever the mouse is within the bounds
    // of our view
    NSTrackingArea *trackingArea = [[NSTrackingArea alloc]
        initWithRect:NSZeroRect
             options:NSTrackingMouseMoved | NSTrackingInVisibleRect | NSTrackingActiveAlways
               owner:self
            userInfo:nil];
    [self.view addTrackingArea:trackingArea];

    // If we want to receive key events, we either need to be in the responder chain of the key
    // view, or else we can install a local monitor. The consequence of this heavy-handed approach
    // is that we receive events for all controls, not just Dear ImGui widgets. If we had native
    // controls in our window, we'd want to be much more careful than just ingesting the complete
    // event stream, though we do make an effort to be good citizens by passing along events when
    // Dear ImGui doesn't want to capture.
    NSEventMask eventMask =
        NSEventMaskKeyDown | NSEventMaskKeyUp | NSEventMaskFlagsChanged | NSEventTypeScrollWheel;
    [NSEvent addLocalMonitorForEventsMatchingMask:eventMask
                                          handler:^NSEvent *_Nullable(NSEvent *event) {
                                            BOOL wantsCapture =
                                                ImGui_ImplOSX_HandleEvent(event, self.view);
                                            if (event.type == NSEventTypeKeyDown && wantsCapture) {
                                                return nil;
                                            } else {
                                                return event;
                                            }
                                          }];

    ImGui_ImplOSX_Init();
    //#endif
}

- (void)mouseMoved:(NSEvent *)event {
    ImGui_ImplOSX_HandleEvent(event, self.view);
}

- (void)mouseDown:(NSEvent *)event {
    ImGui_ImplOSX_HandleEvent(event, self.view);
}

- (void)mouseUp:(NSEvent *)event {
    ImGui_ImplOSX_HandleEvent(event, self.view);
}

- (void)mouseDragged:(NSEvent *)event {
    ImGui_ImplOSX_HandleEvent(event, self.view);
}

- (void)scrollWheel:(NSEvent *)event {
    ImGui_ImplOSX_HandleEvent(event, self.view);
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate> {
    NSWindow *window;
    ViewController *viewController;
}
@end

@implementation AppDelegate : NSObject
- (id)init {
    if (self = [super init]) {
        window = [NSWindow.alloc
            initWithContentRect:NSMakeRect(0, 0, 800, 600)
                      styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                NSWindowStyleMaskResizable
                        backing:NSBackingStoreBuffered
                          defer:NO];

        viewController = [[ViewController alloc] init];
        NSView *content = window.contentView;
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            NSLog(@"Metal is not supported");
            abort();
        }
        MTKView *view = [[MTKView alloc] initWithFrame:content.frame device:device];
        view.translatesAutoresizingMaskIntoConstraints = false;
        [content addSubview:view];
        NSLayoutConstraint *c1 = [NSLayoutConstraint constraintWithItem:view
                                                              attribute:NSLayoutAttributeLeft
                                                              relatedBy:NSLayoutRelationEqual
                                                                 toItem:content
                                                              attribute:NSLayoutAttributeLeft
                                                             multiplier:1.0
                                                               constant:0];
        NSLayoutConstraint *c2 = [NSLayoutConstraint constraintWithItem:view
                                                              attribute:NSLayoutAttributeRight
                                                              relatedBy:NSLayoutRelationEqual
                                                                 toItem:content
                                                              attribute:NSLayoutAttributeRight
                                                             multiplier:1.0
                                                               constant:0];
        NSLayoutConstraint *c3 = [NSLayoutConstraint constraintWithItem:view
                                                              attribute:NSLayoutAttributeTop
                                                              relatedBy:NSLayoutRelationEqual
                                                                 toItem:content
                                                              attribute:NSLayoutAttributeTop
                                                             multiplier:1.0
                                                               constant:0];
        NSLayoutConstraint *c4 = [NSLayoutConstraint constraintWithItem:view
                                                              attribute:NSLayoutAttributeBottom
                                                              relatedBy:NSLayoutRelationEqual
                                                                 toItem:content
                                                              attribute:NSLayoutAttributeBottom
                                                             multiplier:1.0
                                                               constant:0];
        [content addConstraint:c1];
        [content addConstraint:c2];
        [content addConstraint:c3];
        [content addConstraint:c4];

        viewController.view = view;
        [viewController viewDidLoad];
    }
    return self;
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification {
    window.title = NSProcessInfo.processInfo.processName;
    [window cascadeTopLeftFromPoint:NSMakePoint(20, 20)];
    [window makeKeyAndOrderFront:self];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

@end

int main(void) {
    NSApplication *app = NSApplication.sharedApplication;
    app.ActivationPolicy = NSApplicationActivationPolicyRegular;
    NSMenuItem *item = [NSMenuItem new];
    NSApp.mainMenu = [NSMenu new];
    item.submenu = [NSMenu new];
    [app.mainMenu addItem:item];
    [item.submenu
        addItem:[[NSMenuItem alloc]
                    initWithTitle:[@"Quit "
                                      stringByAppendingString:NSProcessInfo.processInfo.processName]
                           action:@selector(terminate:)
                    keyEquivalent:@"q"]];
    AppDelegate *appDelegate =
        [AppDelegate new];  // cannot collapse this and next line because .dlegate is weak
    app.delegate = appDelegate;
    [app run];

    return 0;
}
