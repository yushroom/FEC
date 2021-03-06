#include "jsbinding.h"

#include <cutils.h>
#include <list.h>
#include <quickjs-libc.h>

#include "animation.h"
#include "app.h"
#include "camera.h"
#include "ddsloader.h"
#include "light.h"
#include "mesh.h"
#include "renderable.h"
#include "rhi.h"
#include "texture.h"
#include "transform.h"
#include "free_camera.h"
#include "input.h"

World *defaultWorld = NULL;

void SetDefaultWorld(World *w) { defaultWorld = w; }

extern JSClassID js_fe_Transform_class_id;
extern JSClassID js_fe_Animation_class_id;
extern JSClassID js_fe_Texture_class_id;
extern JSClassID js_fe_Light_class_id;
extern JSClassID js_fe_Camera_class_id;
extern JSClassID js_fe_Renderable_class_id;
extern JSClassID js_fe_Mesh_class_id;
extern JSClassID js_fe_SingletonInput_class_id;

JSClassID js_fe_world_class_id = 0;
static JSClassDef js_fe_world_class = {
    "World",
    .finalizer = NULL,
};

JSClassID js_fe_entity_class_id;
static JSClassDef js_fe_entity_class = {
    "Entity",
    .finalizer = NULL,
};

static JSValue js_wrap_class(JSContext *ctx, void *s, JSClassID classID) {
    if (s == NULL) return JS_NULL;
    JSValue obj = JS_NewObjectClass(ctx, classID);
    if (JS_IsException(obj)) return obj;
    JS_SetOpaque(obj, s);
    return obj;
}

#define FUNC(fn)                                                       \
    static JSValue js_fe_##fn(JSContext *ctx, JSValueConst this_value, \
                              int argc, JSValueConst *argv)

FUNC(GetDefaultWorld) {
    return js_wrap_class(ctx, defaultWorld, js_fe_world_class_id);
}

#define PFUNC(T, fn)                                                         \
    static JSValue js_fe_##T##_##fn(JSContext *ctx, JSValueConst this_value, \
                                    int argc, JSValueConst *argv)

PFUNC(World, CreateEntity) {
    World *w = JS_GetOpaque2(ctx, this_value, js_fe_world_class_id);
    if (!w) return JS_EXCEPTION;
    Entity e = WorldCreateEntity(w);
    //	return JS_NewInt32(ctx, e);
    return js_wrap_class(ctx, (void *)e, js_fe_entity_class_id);
}

PFUNC(World, CreateEntities) {
    if (argc != 1) return JS_EXCEPTION;
    World *w = JS_GetOpaque2(ctx, this_value, js_fe_world_class_id);
    if (!w) return JS_EXCEPTION;
    int count = 0;
    if (JS_ToInt32(ctx, &count, argv[0])) return JS_EXCEPTION;
    for (int i = 0; i < count; ++i) WorldCreateEntity(w);
    return JS_UNDEFINED;
}

// TODO: delete
typedef struct {
    struct list_head link;
    JSContext *ctx;
    JSValue js_world;
    JSValue func;
} JSSystemFunction;

struct list_head js_world_systems = LIST_HEAD_INIT(js_world_systems);

static void unlink_system(JSRuntime *rt, JSSystemFunction *th) {
    if (th->link.prev) {
        list_del(&th->link);
        th->link.prev = th->link.next = NULL;
        JS_FreeValueRT(rt, th->js_world);
    }
}

static void free_system(JSRuntime *rt, JSSystemFunction *th) {
    JS_FreeValueRT(rt, th->func);
    js_free_rt(rt, th);
}

extern void fe_js_dump_error(JSContext *ctx);

static void js_system_wrapper(World *w) {
    struct list_head *el;
    list_for_each(el, &js_world_systems) {
        JSSystemFunction *f = list_entry(el, JSSystemFunction, link);
        JSValue ret = JS_Call(f->ctx, f->func, JS_UNDEFINED, 1, &f->js_world);
        if (JS_IsException(ret)) {
            fe_js_dump_error(f->ctx);
        }
        JS_FreeValue(f->ctx, ret);
    }
}

PFUNC(World, AddSystem) {
    if (argc != 1) return JS_EXCEPTION;
    World *w = JS_GetOpaque2(ctx, this_value, js_fe_world_class_id);
    if (!w) return JS_EXCEPTION;
    JSValue f = argv[0];
    if (!JS_IsFunction(ctx, f)) {
        return JS_EXCEPTION;
    }
    JS_DupValue(ctx, f);
    JSSystemFunction *s = js_malloc(ctx, sizeof(JSSystemFunction));
    list_add_tail(&s->link, &js_world_systems);
    s->ctx = ctx;
    s->func = f;
    s->js_world = this_value;
    JS_DupValue(ctx, this_value);
    WorldAddSystem(w, js_system_wrapper);
    return JS_UNDEFINED;
}

PFUNC(World, GetSingletonComponent) {
    if (argc != 1) return JS_EXCEPTION;
    World *w = JS_GetOpaque2(ctx, this_value, js_fe_world_class_id);
    if (!w) return JS_EXCEPTION;
    uint32_t typeID = 0;
    if (JS_ToUint32(ctx, &typeID, argv[0])) return JS_EXCEPTION;
    void * scomp = WorldGetSingletonComponent(w, typeID);
    if (scomp == NULL) {
        return JS_NULL;
    } else if (typeID == SingletonInputID) {
        return js_wrap_class(ctx, scomp, js_fe_SingletonInput_class_id);
    }
    return JS_NULL;
}

PFUNC(World, Clear) {
    World *w = JS_GetOpaque2(ctx, this_value, js_fe_world_class_id);
    if (!w) return JS_EXCEPTION;
    WorldClear(w);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_fe_world_proto_funcs[] = {
    JS_CFUNC_DEF("CreateEntity", 0, js_fe_World_CreateEntity),
    JS_CFUNC_DEF("CreateEntities", 1, js_fe_World_CreateEntities),
    JS_CFUNC_DEF("AddSystem", 1, js_fe_World_AddSystem),
    JS_CFUNC_DEF("GetSingletonComponent", 1, js_fe_World_GetSingletonComponent),
    JS_CFUNC_DEF("Clear", 1, js_fe_World_Clear),
};

static JSValue js_fe_Entity_tranform_getter(JSContext *ctx,
                                            JSValueConst this_val) {
    void *p = JS_GetOpaque2(ctx, this_val, js_fe_entity_class_id);
    if (!p) return JS_EXCEPTION;
    Entity e = (Entity)p;
    void *comp = EntityGetComponent(e, defaultWorld, TransformID);
    return js_wrap_class(ctx, comp, js_fe_Transform_class_id);
}

// string name getter
static JSValue js_fe_Entity_name_getter(JSContext *ctx, JSValueConst this_val) {
    void *p = JS_GetOpaque2(ctx, this_val, js_fe_entity_class_id);
    if (!p) return JS_EXCEPTION;
    Entity e = (Entity)p;
    const char *value = defaultWorld->entities[e].name;
    return JS_NewString(ctx, value);
}
// string name setter
static JSValue js_fe_Entity_name_setter(JSContext *ctx, JSValueConst this_val,
                                        JSValue val) {
    void *p = JS_GetOpaque2(ctx, this_val, js_fe_entity_class_id);
    if (!p) return JS_EXCEPTION;
    Entity e = (Entity)p;
    size_t len;
    const char *str = JS_ToCStringLen(ctx, &len, val);
    if (len >= EntityNameLenth) len = EntityNameLenth - 1;
    char *name = defaultWorld->entities[e].name;
    memcpy(name, str, len);
    name[len] = '\0';
    JS_FreeCString(ctx, str);
    return JS_UNDEFINED;
}

PFUNC(Entity, GetID) {
    void *p = JS_GetOpaque2(ctx, this_value, js_fe_entity_class_id);
    if (!p) return JS_EXCEPTION;
    Entity e = (Entity)p;
    return JS_NewInt32(ctx, e);
}

PFUNC(Entity, AddComponent) {
    if (argc != 1) return JS_EXCEPTION;
    void *p = JS_GetOpaque2(ctx, this_value, js_fe_entity_class_id);
    if (!p) return JS_EXCEPTION;
    Entity e = (Entity)p;
    uint32_t type;
    if (JS_IsUndefined(argv[0]) || JS_IsNull(argv[0])) return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &type, argv[0])) return JS_EXCEPTION;
    void *comp = EntityAddComponent(e, defaultWorld, type);
    if (type == TransformID) {
        return js_wrap_class(ctx, comp, js_fe_Transform_class_id);
    } else if (type == CameraID) {
        return js_wrap_class(ctx, comp, js_fe_Camera_class_id);
    } else if (type == RenderableID) {
        return js_wrap_class(ctx, comp, js_fe_Renderable_class_id);
    } else if (type == AnimationID) {
        return js_wrap_class(ctx, comp, js_fe_Animation_class_id);
    } else if (type == LightID) {
        return js_wrap_class(ctx, comp, js_fe_Light_class_id);
    }
    return JS_UNDEFINED;
}

PFUNC(Entity, GetComponent) {
    if (argc != 1) return JS_EXCEPTION;
    void *p = JS_GetOpaque2(ctx, this_value, js_fe_entity_class_id);
    if (!p) return JS_EXCEPTION;
    Entity e = (Entity)p;
    uint32_t type;
    if (JS_ToUint32(ctx, &type, argv[0])) return JS_EXCEPTION;
    void *comp = EntityGetComponent(e, defaultWorld, type);
    if (type == TransformID)
        return js_wrap_class(ctx, comp, js_fe_Transform_class_id);
    else if (type == CameraID)
        return js_wrap_class(ctx, comp, js_fe_Camera_class_id);
    else if (type == AnimationID)
        return js_wrap_class(ctx, comp, js_fe_Animation_class_id);
    else if (type == RenderableID)
        return js_wrap_class(ctx, comp, js_fe_Renderable_class_id);
    return JS_NULL;
}

static const JSCFunctionListEntry js_fe_entity_proto_funcs[] = {
    JS_CGETSET_DEF("transform", js_fe_Entity_tranform_getter, NULL),
    JS_CFUNC_DEF("GetID", 0, js_fe_Entity_GetID),
    JS_CGETSET_DEF("name", js_fe_Entity_name_getter, js_fe_Entity_name_setter),
    JS_CFUNC_DEF("AddComponent", 1, js_fe_Entity_AddComponent),
    JS_CFUNC_DEF("GetComponent", 1, js_fe_Entity_GetComponent),
};

static JSValue js_fe_render_ConvertTexture(JSContext *ctx,
                                           JSValueConst this_value, int argc,
                                           JSValueConst *argv) {
    if (argc < 1) return JS_EXCEPTION;
    const char *path = JS_ToCString(ctx, argv[0]);
    bool ret = ConvertToDDS(path);
    JS_FreeCString(ctx, path);
    return JS_NewBool(ctx, ret);
}

static JSValue js_fe_reload(JSContext *ctx, JSValueConst this_value, int argc,
                            JSValueConst *argv) {
    js_fishengine_free_handlers(JS_GetRuntime(ctx));
    app_reload();
    return JS_UNDEFINED;
}

static JSValue js_fe_system(JSContext *ctx, JSValueConst this_value, int argc,
                            JSValueConst *argv) {
    const char *str = NULL;
    if (argc < 1) return JS_UNDEFINED;
    str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_EXCEPTION;
    int ret = system(str);
    return JS_NewInt32(ctx, ret);
}

#define FE_COMP(name) \
    JS_PROP_INT32_DEF(#name "ID", name##ID, JS_PROP_CONFIGURABLE)
#define FE_FLAG(name) JS_PROP_INT32_DEF(#name, name, JS_PROP_CONFIGURABLE)

static const JSCFunctionListEntry js_fe_funcs[] = {
    JS_CFUNC_DEF("GetDefaultWorld", 0, js_fe_GetDefaultWorld),
    JS_CFUNC_DEF("ConvertTexture", 1, js_fe_render_ConvertTexture),
    JS_CFUNC_DEF("reload", 0, js_fe_reload),
    JS_CFUNC_DEF("system", 1, js_fe_system),
    FE_COMP(Transform),
    FE_COMP(Renderable),
    FE_COMP(Camera),
    FE_COMP(Animation),
    FE_COMP(Light),
    FE_COMP(FreeCamera),
    FE_FLAG(SingletonInputID),
    FE_FLAG(SingletonSelectionID),
    FE_FLAG(VertexAttrPosition),
    FE_FLAG(VertexAttrNormal),
    FE_FLAG(VertexAttrTangent),
    FE_FLAG(VertexAttrUV0),
    FE_FLAG(VertexAttrUV1),
    FE_FLAG(VertexAttrBoneIndex),
    FE_FLAG(VertexAttrWeights),
    FE_FLAG(FilterModePoint),
    FE_FLAG(FilterModeBilinear),
    FE_FLAG(FilterModeTrilinear),
    FE_FLAG(TextureWrapModeRepeat),
    FE_FLAG(TextureWrapModeClamp),
    FE_FLAG(TextureWrapModeMirror),
    FE_FLAG(TextureWrapModeMirrorOnce),
    FE_FLAG(KeyCodeLeftAlt),
    FE_FLAG(KeyCodeRightAlt),
    FE_FLAG(KeyCodeLeftControl),
    FE_FLAG(KeyCodeRightControl),
    FE_FLAG(KeyCodeLeftCommand),
    FE_FLAG(KeyCodeRightCommand),
    FE_FLAG(KeyCodeMouse0),
    FE_FLAG(KeyCodeMouse1),
    FE_FLAG(KeyCodeMouse2),
    FE_FLAG(AxisMouseScrollWheel),
    FE_FLAG(AxisMouseX),
    FE_FLAG(AxisMouseY),
    FE_FLAG(KeyCodeF),
};

int js_init_module_fishengine_extra(JSContext *ctx, JSModuleDef *m);
int js_fe_init_extra(JSContext *ctx, JSModuleDef *m);

JSClassID js_def_class(JSContext *ctx, JSClassDef *class_def,
                       const JSCFunctionListEntry proto_funcs[],
                       int proto_func_count) {
    JSValue proto;
    JSClassID class_id = 0;
    JS_NewClassID(&class_id);
    JS_NewClass(JS_GetRuntime(ctx), class_id, class_def);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, proto_funcs, proto_func_count);
    JS_SetClassProto(ctx, class_id, proto);
    return class_id;
}

JSClassID js_def_class2(JSContext *ctx, JSClassDef *class_def,
                        const JSCFunctionListEntry proto_funcs[],
                        int proto_func_count, JSModuleDef *m,
                        const char *class_name, JSCFunction *ctor) {
    JSValue proto, _class;
    JSClassID class_id = 0;
    JS_NewClassID(&class_id);
    JS_NewClass(JS_GetRuntime(ctx), class_id, class_def);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, proto_funcs, proto_func_count);
    JS_SetClassProto(ctx, class_id, proto);

    _class =
        JS_NewCFunction2(ctx, ctor, class_name, 0, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, _class, proto);

    int ret = JS_SetModuleExport(ctx, m, class_name, _class);
    assert(ret == 0);
    return class_id;
}

#define CreateClass(name)                                                    \
    js_fe_##name##_class_id =                                                \
        js_def_class(ctx, &js_fe_##name##_class, js_fe_##name##_proto_funcs, \
                     countof(js_fe_##name##_proto_funcs))

#define CreateClass2(name)                                      \
    js_fe_##name##_class_id = js_def_class2(                    \
        ctx, &js_fe_##name##_class, js_fe_##name##_proto_funcs, \
        countof(js_fe_##name##_proto_funcs), m, #name, js_fe_##name##_ctor)

static int js_fe_init(JSContext *ctx, JSModuleDef *m) {
    CreateClass(world);
    CreateClass(entity);
    //CreateClass(transform);
    js_fe_init_extra(ctx, m);
    JS_SetModuleExportList(ctx, m, js_fe_funcs, countof(js_fe_funcs));
    return 0;
}

JSModuleDef *js_init_module_fishengine(JSContext *ctx,
                                       const char *module_name) {
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, js_fe_init);
    if (!m) return NULL;
    JS_AddModuleExport(ctx, m, js_fe_world_class.class_name);
    //JS_AddModuleExport(ctx, m, js_fe_transform_class.class_name);
    js_init_module_fishengine_extra(ctx, m);
    JS_AddModuleExportList(ctx, m, js_fe_funcs, countof(js_fe_funcs));
    return m;
}

void js_fishengine_free_handlers(JSRuntime *rt) {
    struct list_head *el, *el1;
    list_for_each_safe(el, el1, &js_world_systems) {
        JSSystemFunction *th = list_entry(el, JSSystemFunction, link);
        unlink_system(rt, th);
        free_system(rt, th);
    }
}
