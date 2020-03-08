#include "app.h"

#include <assert.h>
#include <libregexp.h>
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
#include "jsbinding.h"
#include "light.h"
#include "mesh.h"
#include "renderable.h"
#include "rhi.h"
#include "statistics.h"
#include "transform.h"

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

void RenderSystem(World *w) {
    ComponentArray *a = w->componentArrays + RenderableID;
    uint32_t size = a->m.size;
    Renderable *r = a->m.ptr;
    for (int i = 0; i < size; ++i, ++r) {
        if (r->mesh && !MeshIsUploaded(r->mesh)) {
            MeshUploadMeshData(r->mesh);
        }
    }
    r = a->m.ptr;
    for (int i = 0; i < size; ++i, ++r) {
        if (r->skin) {
            RenderableUpdateBones(r, w);
            GPUSkinning(r);
        }
    }
    r = a->m.ptr;
    BeginPass();
    for (int i = 0; i < size; ++i, ++r) {
        Transform *t =
            ComponentGetSiblingComponent(w, r, RenderableID, TransformID);
        SimpleDraw(t, r);
    }
}

#define COMP(T) \
    { .type = T##ID, .name = #T, .size = sizeof(T), .ctor=NULL, .dtor=NULL, }

static ComponentDef g_componentDef[] = {
    {.type = TransformID,
     .name = "Transform",
     .size = sizeof(Transform),
     .ctor = TransformInit,
     .dtor = NULL},
    COMP(Renderable),
    COMP(Camera),
    COMP(Light),
    {.type = AnimationID,
     .name = "Animation",
     .size = sizeof(Animation),
     .ctor = AnimationInit,
     .dtor = NULL},
};

static ComponentDef g_singleComponentDef[] = {
    COMP(SingletonTransformManager),
};

#if 0

int main()
{
	size_t count = 0;
	count += sizeof(World);
	count += (sizeof(Transform) + sizeof(Rotator)) * MaxEntity;
	count += sizeof(TransformManager);
	printf("%zu %zuMB\n", count, count/1024/1024);
	
	WorldDef def;
	def.componentDefs = g_componentDef;
	def.componentDefCount = countof(g_componentDef);
	def.singletonComponentDefs = g_singleComponentDef;
	def.singletonComponentDefCount = countof(g_singleComponentDef);
	
	World *w = WorldCreate(&def);
	WorldPrintStats(w);
	for (int i = 0; i < MaxEntity; ++i) {
		Entity e = WorldCreateEntity(w);
		Transform *t = EntityAddComponent(e, w, TransformID);
		TransformInit(t);
		Rotator *r = EntityAddComponent(e, w, RotatorID);
		r->speed = 0.1f * i;
	}
//	WorldAddSystem(w, RotatorSystem);
	WorldAddSystem(w, TransformSystem);
//	WorldAddSystem(w, RenderSystem);
	TransformManager *t;
	{
		t = aligned_alloc(16, sizeof(TransformManager));
		memset(t, 0, sizeof(TransformManager));
		WorldAddSingletonComponent(w, t, TransformManagerID);
	}
	WorldPrintStats(w);
	
	TransformSetParent(t, 1, 2);
	TransformPrintHierarchy(t);
	
	clock_t start, end;
	double duration;
	start = clock();
	const int frames = 1000;
	for (int i = 0; i < frames; ++i) {
		WorldTick(w);
	}
	end = clock();
	duration = (double)(end - start) / CLOCKS_PER_SEC;
	printf("time: %lfs %lfs per frame\n", duration, duration/frames);
	WorldFree(w);
	return 0;
}
#else

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

int app_init() {
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

    WorldDef def;
    def.componentDefs = g_componentDef;
    def.componentDefCount = countof(g_componentDef);
    def.singletonComponentDefs = g_singleComponentDef;
    def.singletonComponentDefCount = countof(g_singleComponentDef);

    w = WorldCreate(&def);
    WorldAddSystem(w, AnimationSystem);
    WorldAddSystem(w, TransformSystem);
    WorldAddSystem(w, RenderSystem);
    SingletonTransformManager *tm;
    {
        // tm = aligned_alloc(16, sizeof(TransformManager));
        tm = malloc(sizeof(SingletonTransformManager));
        TransformManagerInit(tm);
        WorldAddSingletonComponent(w, tm, SingletonTransformManagerID);
    }

    SetDefaultWorld(w);
    js_init_module_fishengine(ctx, "FishEngine");
    js_init_module_imgui(ctx, "imgui");

    int flags = JS_EVAL_TYPE_MODULE;
    eval_file(ctx, path, flags);

    //	TransformPrintHierarchy(t);
    WorldPrintStats(w);

    //	WorldFree(w);

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
    //AssetDeleteAll();

    WorldAddSystem(w, AnimationSystem);
    WorldAddSystem(w, TransformSystem);
    WorldAddSystem(w, RenderSystem);
    {
        SingletonTransformManager *tm =
            WorldGetSingletonComponent(w, SingletonTransformManagerID);
        if (!tm) tm = aligned_alloc(16, sizeof(SingletonTransformManager));
        TransformManagerInit(tm);
        WorldAddSingletonComponent(w, tm, SingletonTransformManagerID);
    }
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

void HierarchyWindow(World *w);
void InspectorWindow(World *w);
void AssetWindow();
void ConsoleWindow();
void StatisticsWindow(World *w, JSRuntime *rt);

int app_render_ui() {
    HierarchyWindow(w);
    InspectorWindow(w);
    AssetWindow();
    StatisticsWindow(w, rt);
    ConsoleWindow();

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

// TODO: libregex in quickjs need a JSContext
// TODO: utf8 support
void open_file_by_callstack(const uint8_t *callstack) {
    const char *re =
        "at.+\\((.+)\\:(\\d+)\\)";  // match "at
                                    // function_name(file_path:line_number)"
    const char *re2 = "at\\s(.+)\\:(\\d+)";  // match "at file_path:line_number"

    int re_bytecode_len;
    char error_msg[64];
    int flags = 0;

    uint8_t *bytecode =
        lre_compile(&re_bytecode_len, error_msg, countof(error_msg), re,
                    strlen(re), flags, ctx);
    const int capture_count = lre_get_capture_count(bytecode);
    assert(capture_count == 3);
    uint8_t *capture[capture_count * 2];
    int matched = lre_exec(&capture[0], bytecode, callstack, 0,
                           strlen((const char *)callstack), 0, ctx);
    if (matched != 1) {
        js_free(ctx, bytecode);
        bytecode = lre_compile(&re_bytecode_len, error_msg, countof(error_msg),
                               re2, strlen(re2), flags, ctx);
        assert(capture_count == 3);
        matched = lre_exec(&capture[0], bytecode, callstack, 0,
                           strlen((const char *)callstack), 0, ctx);
    }
    printf("%s\n", matched ? "matched" : "no match");
    if (matched == 1) {
        char command[1024] = {0};
#if WIN32
        // https://stackoverflow.com/questions/2642551/windows-c-system-call-with-spaces-in-command
        //strcat(command, "\"\"C:\\Program Files\\Sublime Text 3\\subl.exe\" \"");
        strcat(command, "\"\"C:\\Users\\yushroom\\AppData\\Local\\Programs\\Microsoft VS Code\\Code.exe\" -g \"");
        
#else
        strcat(command,
               "\"/Applications/Sublime "
               "Text.app/Contents/SharedSupport/bin/subl\" \"");
#endif
        strncat(command, (const char *)capture[2], capture[3] - capture[2]);
        strcat(command, "\":");
        strncat(command, (const char *)capture[4], capture[5] - capture[4]);
#if WIN32
        strcat(command, "\"");
#endif
        printf("command: %s\n", command);
        system(command);
    }
    js_free(ctx, bytecode);
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

#endif
