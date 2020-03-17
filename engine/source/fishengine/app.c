#include "app.h"

#include <assert.h>
#ifdef APPLE
#include <rawfsevents.h>
#endif
#include <stdlib.h>
#include <time.h>

#include "animation.h"
#include "asset.h"
#include "camera.h"
#include "ddsloader.h"
#include "debug.h"
#include "ecs.h"
#include "input.h"
#include "jsbinding.h"
#include "light.h"
#include "mesh.h"
#include "renderable.h"
#include "rhi.h"
#include "singleton_time.h"
#include "singleton_selection.h"
#include "statistics.h"
#include "transform.h"
#include "free_camera.h"

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

#if WIN32
#ifndef aligned_alloc
#define aligned_alloc(alignment, size) _aligned_malloc(size, alignment)
#endif
#ifndef aligned_free
#define aligned_free _aligned_free
#endif
#else
#define aligned_free free
#endif


#include <cutils.h>
#include <quickjs-libc.h>

#include "jsbinding.h"

static char string_buffer[2048];
static int offset = 0;
static void safe_puts(const char *s) {
    char *p = string_buffer + offset;
    offset += snprintf(p, 2048 - offset, "%s", s);
}

static JSValue fe_js_print(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv) {
    int i;
    const char *str;

    for (i = 0; i < argc; i++) {
        if (i != 0)
            //            putchar(' ');
            //			sprintf(string_buffer, " ");
            safe_puts(" ");
        str = JS_ToCString(ctx, argv[i]);
        if (!str) return JS_EXCEPTION;
        //        fputs(str, stdout);
        safe_puts(str);
        JS_FreeCString(ctx, str);
    }
    //    putchar('\n');
    safe_puts("\n");
    return JS_UNDEFINED;
}

static void fe_js_std_dump_error1(JSContext *ctx, JSValueConst exception_val,
                                  BOOL is_throw) {
    JSValue val;
    const char *stack;
    BOOL is_error;

    is_error = JS_IsError(ctx, exception_val);
    if (is_throw && !is_error)
        //        printf("Throw: ");
        //		sprintf(string_buffer, "Throw: ");
        safe_puts("Throw: ");
    fe_js_print(ctx, JS_NULL, 1, (JSValueConst *)&exception_val);
    if (is_error) {
        val = JS_GetPropertyStr(ctx, exception_val, "stack");
        if (!JS_IsUndefined(val)) {
            stack = JS_ToCString(ctx, val);
            //            printf("%s\n", stack);
            //			sprintf(string_buffer, "%s\n", stack);
            safe_puts(stack);
            safe_puts("\n");
            JS_FreeCString(ctx, stack);
        }
        JS_FreeValue(ctx, val);
    }
}

void fe_js_dump_error(JSContext *ctx) {
    JSValue exception_val;

    offset = 0;
    exception_val = JS_GetException(ctx);
    fe_js_std_dump_error1(ctx, exception_val, TRUE);
    JS_FreeValue(ctx, exception_val);
    log_error(string_buffer);
}

static int eval_buf(JSContext *ctx, const void *buf, int buf_len,
                    const char *filename, int eval_flags) {
    JSValue val;
    int ret;

    if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
        /* for the modules, we compile then run to be able to set
           import.meta */
        val = JS_Eval(ctx, buf, buf_len, filename,
                      eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
        if (!JS_IsException(val)) {
            js_module_set_import_meta(ctx, val, TRUE, TRUE);
            val = JS_EvalFunction(ctx, val);
        }
    } else {
        val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
    }
    if (JS_IsException(val)) {
        fe_js_dump_error(ctx);
        ret = -1;
    } else {
        ret = 0;
    }
    JS_FreeValue(ctx, val);
    return ret;
}

static int eval_file(JSContext *ctx, const char *filename, int module) {
    uint8_t *buf;
    int ret, eval_flags;
    size_t buf_len;

    buf = js_load_file(ctx, &buf_len, filename);
    if (!buf) {
        perror(filename);
        exit(1);
    }

    if (module < 0) {
        module = (has_suffix(filename, ".mjs") ||
                  JS_DetectModule((const char *)buf, buf_len));
    }
    if (module)
        eval_flags = JS_EVAL_TYPE_MODULE;
    else
        eval_flags = JS_EVAL_TYPE_GLOBAL;
    ret = eval_buf(ctx, buf, buf_len, filename, eval_flags);
    js_free(ctx, buf);
    return ret;
}

static World *w = NULL;
static const char *path =
    "E:\\workspace\\cengine\\samples\\gltfviewer\\assets\\index.js";
static JSRuntime *rt = NULL;
static JSContext *ctx = NULL;

static bool g_reload = false;
#ifdef APPLE
#include <constants.h>
static fse_watcher_t g_watcher;

static void OnFileChanged(void *context, size_t numevents,
                          fse_event_t *events) {
    puts(__FUNCTION__);
    for (int i = 0; i < numevents; ++i) {
        fse_event_t *e = &events[i];
        if (e->flags & kFSEventStreamEventFlagItemModified) {
            g_reload = true;
        }
    }
}
#endif

struct Application {
    bool is_playing;
    bool is_paused;
    char current_project_dir[512];
};

struct Application g_app = {.is_playing = false};

#if WIN32
#define _SEP '\\'
static char *js_module_normalize_name(JSContext *ctx, const char *base_name,
                                      const char *name, void *opaque) {
    char *filename, *p;
    const char *r;
    int len;

    if (name[0] != '.') {
        /* if no initial dot, the module name is not modified */
        return js_strdup(ctx, name);
    }

    p = strrchr(base_name, _SEP);
    if (p)
        len = p - base_name;
    else
        len = 0;

    filename = js_malloc(ctx, len + strlen(name) + 1 + 1);
    if (!filename) return NULL;
    memcpy(filename, base_name, len);
    filename[len] = '\0';

    /* we only normalize the leading '..' or '.' */
    r = name;
    // for (;;) {
    //    if (r[0] == '.' && (r[1] == _SEP || r[1] == '/')) {
    //        r += 2;
    //    } else if (r[0] == '.' && r[1] == '.' &&
    //               (r[1] == _SEP || r[1] == '/')) {
    //        /* remove the last path element of filename, except if "."
    //           or ".." */
    //        if (filename[0] == '\0') break;
    //        p = strrchr(filename, _SEP);
    //        if (!p)
    //            p = filename;
    //        else
    //            p++;
    //        if (!strcmp(p, ".") || !strcmp(p, "..")) break;
    //        if (p > filename) p--;
    //        *p = '\0';
    //        r += 3;
    //    } else {
    //        break;
    //    }
    //}
    if (filename[0] != '\0') strcat(filename, "\\");
    strcat(filename, r);
    //    printf("normalize: %s %s -> %s\n", base_name, name, filename);
    return filename;
}
#endif

const char *ApplicationFilePath();

int app_init(World *initWorld) {
    debug_init();

    clock_t start, end;
    double duration;
    start = clock();

    rt = JS_NewRuntime();
    ctx = JS_NewContext(rt);
    js_std_add_helpers(ctx, 0, NULL);
    js_init_module_os(ctx, "os");
    js_init_module_std(ctx, "std");

#if WIN32
    JS_SetModuleLoaderFunc(rt, js_module_normalize_name, js_module_loader,
                           NULL);
#else
    JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);
#endif

    w = initWorld;
    SetDefaultWorld(w);
    js_init_module_fishengine(ctx, "FishEngine");
    js_init_module_imgui(ctx, "imgui");

    int flags = JS_EVAL_TYPE_MODULE;
    eval_file(ctx, path, flags);

#ifdef APPLE
    fse_init();
    g_watcher = fse_alloc();
    fse_watch(path, OnFileChanged, NULL, NULL, NULL, g_watcher);
#endif

    end = clock();
    duration = (double)(end - start) / CLOCKS_PER_SEC;
    printf("time: %lfs\n", duration);
    return 0;
}

int app_reload() {
    //	debug_clear_all();
    WorldClear(w);
    // AssetDeleteAll();

    SetDefaultWorld(w);

    //	int flags = JS_EVAL_TYPE_MODULE;
    //	eval_file(ctx, path, flags);

    return 0;
}

int app_reload2() {
    debug_clear_all();
    js_fishengine_free_handlers(rt);
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    rt = JS_NewRuntime();
    ctx = JS_NewContext(rt);
    js_std_add_helpers(ctx, 0, NULL);
    js_init_module_os(ctx, "os");
    js_init_module_std(ctx, "std");

    JSModuleNormalizeFunc *module_normalize = NULL;
#if WIN32
    module_normalize = js_module_normalize_name;
#endif
    JS_SetModuleLoaderFunc(rt, module_normalize, js_module_loader, NULL);

    js_init_module_fishengine(ctx, "FishEngine");
    js_init_module_imgui(ctx, "imgui");

    int flags = JS_EVAL_TYPE_MODULE;
    eval_file(ctx, path, flags);
    app_reload();
    return 0;
}

int app_update() {
    if (g_reload) {
        debug_clear_all();
        js_fishengine_free_handlers(rt);
        int flags = JS_EVAL_TYPE_MODULE;
        eval_file(ctx, path, flags);
        app_reload();
        g_reload = false;
    }
    WorldTick(w);
    return 0;
}

int app_frame_end() {
    // SingletonInput *si = WorldGetSingletonComponent(w, SingletonInputID);
    InputSystemPostUpdate(w);
}

int app_render_ui() {

    if (1) {
        JSValue global_obj = JS_GetGlobalObject(ctx);
        JSValue renderUI = JS_GetPropertyStr(ctx, global_obj, "renderUI");
        if (JS_IsFunction(ctx, renderUI)) {
            JSValue ret = JS_Call(ctx, renderUI, JS_UNDEFINED, 0, NULL);
            if (JS_IsException(ret)) {
                fe_js_dump_error(ctx);
            }
        }
        JS_FreeValue(ctx, renderUI);
        JS_FreeValue(ctx, global_obj);
    }

    return 0;
}

void app_play() {
    g_app.is_playing = true;
    g_app.is_paused = false;
}
void app_stop() {
    g_app.is_playing = false;
    g_app.is_paused = false;
}
void app_pause() {
    if (app_is_playing()) g_app.is_paused = true;
}
void app_step();
bool app_is_playing() { return g_app.is_playing && !g_app.is_paused; }
bool app_is_paused() { return g_app.is_playing && g_app.is_paused; }
