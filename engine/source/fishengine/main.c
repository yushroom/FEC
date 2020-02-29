#include <assert.h>
#include <time.h>

#include "ecs.h"
#include "transform.h"

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

struct Mesh {};
typedef struct Mesh Mesh;

struct Material {};
typedef struct Material Material;

struct Renderable {
    Mesh *mesh;
    Material *materil;
};
typedef struct Renderable Renderable;

struct Camera {
    float fov;
};
typedef struct Camera Camera;

typedef struct {
    float speed;
} Rotator;

void RotatorSystemImpl(Transform *t, Rotator *r) {
    assert(t && r);
    t->rotation.y += r->speed;
}
System2(RotatorSystem, RotatorSystemImpl, TransformID, RotatorID)

    void RenderSystem(World *w) {
    ComponentArray *a = w->componentArrays + RenderableID;
    ForeachComponent(a, Renderable, r) {}
}

#define COMP(T) \
    { .type = T##ID, .name = #T, .size = sizeof(T) }
static ComponentDef g_componentDef[] = {
    COMP(Transform),
    COMP(Renderable),
    COMP(Camera),
    COMP(Rotator),
};

static ComponentDef g_singleComponentDef[] = {
    COMP(TransformManager),
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
        js_std_dump_error(ctx);
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

int main() {
    static_assert(sizeof(float4) == 16, "");
    static_assert(sizeof(float4x4) == 64, "");

    clock_t start, end;
    double duration;
    start = clock();

    const char *path = "/Users/yushroom/program/cengine/load.js";

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    js_std_add_helpers(ctx, 0, NULL);
    js_init_module_os(ctx, "os");
    js_init_module_std(ctx, "std");

    JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);

    WorldDef def;
    def.componentDefs = g_componentDef;
    def.componentDefCount = countof(g_componentDef);
    def.singletonComponentDefs = g_singleComponentDef;
    def.singletonComponentDefCount = countof(g_singleComponentDef);

    World *w = WorldCreate(&def);
    {
        Entity root = WorldCreateEntity(w);
        Transform *t = EntityAddComponent(root, w, TransformID);
        TransformInit(t);
    }
    WorldAddSystem(w, TransformSystem);
    WorldAddSystem(w, RenderSystem);
    TransformManager *t;
    {
        t = aligned_alloc(16, sizeof(TransformManager));
        memset(t, 0, sizeof(TransformManager));
        for (int i = 0; i < countof(t->LocalToWorld); ++i) {
            float4x4 *m = &t->LocalToWorld[i];
            m->m00 = m->m11 = m->m22 = m->m33 = 1.f;
        }
        WorldAddSingletonComponent(w, t, TransformManagerID);
    }

    SetDefaultWorld(w);
    js_init_module_fishengine(ctx, "FishEngine");

    int flags = JS_EVAL_TYPE_MODULE;
    eval_file(ctx, path, flags);

    TransformPrintHierarchy(t);
    WorldPrintStats(w);

    WorldFree(w);

    end = clock();
    duration = (double)(end - start) / CLOCKS_PER_SEC;
    printf("time: %lfs\n", duration);
    return 0;
}

#endif
