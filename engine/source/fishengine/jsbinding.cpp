#include "jsbinding.h"

#include <imgui.h>

#include "animation.h"
#include "app.h"
#include "camera.h"
#include "ddsloader.h"
#include "mesh.h"
#include "renderable.h"
#include "rhi.h"
#include "texture.h"
#include "transform.h"

static JSValue js_imgui_begin(JSContext *ctx, JSValueConst this_value, int argc,
                              JSValueConst *argv) {
    if (argc != 1) return JS_EXCEPTION;
    const char *name = NULL;
    name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;
    ImGui::Begin(name);
    JS_FreeCString(ctx, name);
    return JS_UNDEFINED;
}

static JSValue js_imgui_end(JSContext *ctx, JSValueConst this_value, int argc,
                            JSValueConst *argv) {
    ImGui::End();
    return JS_UNDEFINED;
}

static JSValue js_imgui_IsItemFocused(JSContext *ctx, JSValueConst this_value,
                                      int argc, JSValueConst *argv) {
    return JS_NewBool(ctx, ImGui::IsItemFocused());
}

static JSValue js_imgui_selectable(JSContext *ctx, JSValueConst this_value,
                                   int argc, JSValueConst *argv) {
    const char *label = NULL;
    bool selected;
    if (argc != 2) return JS_EXCEPTION;
    label = JS_ToCString(ctx, argv[0]);
    if (!label) return JS_EXCEPTION;
    selected = JS_ToBool(ctx, argv[1]);
    bool pressed = ImGui::Selectable(label, selected);
    JS_FreeCString(ctx, label);
    return JS_NewBool(ctx, pressed);
}

static const JSCFunctionListEntry js_imgui_funcs[] = {
    JS_CFUNC_DEF("Begin", 1, js_imgui_begin),
    JS_CFUNC_DEF("End", 0, js_imgui_end),
    JS_CFUNC_DEF("Selectable", 2, js_imgui_selectable),
    JS_CFUNC_DEF("IsItemFocused", 0, js_imgui_IsItemFocused),
};

static int js_imgui_init(JSContext *ctx, JSModuleDef *m) {
    JS_SetModuleExportList(ctx, m, js_imgui_funcs, countof(js_imgui_funcs));
    return 0;
}

JSModuleDef *js_init_module_imgui(JSContext *ctx, const char *module_name) {
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, js_imgui_init);
    if (!m) return NULL;
    JS_AddModuleExportList(ctx, m, js_imgui_funcs, countof(js_imgui_funcs));
    return m;
}
