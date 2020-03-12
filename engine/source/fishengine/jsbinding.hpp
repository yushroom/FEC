#ifndef JSBINDING_HPP
#define JSBINDING_HPP

#include <assert.h>
#include <cutils.h>
#include <quickjs.h>

#include <cstdint>
#include <type_traits>

#include "ecs.h"
#include "simd_math.h"
#include "transform.h"

typedef const char *string;

typedef struct {
    int32_t size;
    JSValue obj;
    JSContext *ctx;
    JSValue operator[](int idx) {
        assert(idx >= 0 && idx < size);
        return JS_GetPropertyUint32(ctx, obj, idx);
    }
} Array;

typedef struct {
    uint8_t *buffer;
    size_t byteLength;
} ArrayBuffer;

typedef struct {
    uint8_t *buffer;
    size_t byteLength;
    size_t bytesPerElement;
} TypedArray;

template <typename T>
JSClassID JSGetClassID();

extern "C" {
extern JSClassID js_fe_world_class_id;
}

template <>
inline JSClassID JSGetClassID<World>() {
    return js_fe_world_class_id;
}

template <typename T>
int JSValueTo(JSContext *ctx, T *v, JSValue val);

template <typename T>
inline int JSValueTo(JSContext *ctx, std::enable_if_t<std::is_enum_v<T>, T> *v,
                     JSValue val) {
    int64_t _v;
    int ret = JS_ToInt64(ctx, &_v, val);
    if (ret == 0) *v = (T)_v;
    return ret;
}

template <typename T>
int JSValueTo(JSContext *ctx, std::enable_if_t<std::is_pointer_v<T>, T> *v,
              JSValue val) {
    assert(v);
    if (JS_IsNull(val) || JS_IsUndefined(val)) {
        *v = NULL;
        return 0;
    }
    *v = (T)JS_GetOpaque2(ctx, val, JSGetClassID<std::remove_pointer_t<T>>());
    return *v == NULL;
}

template <>
inline int JSValueTo<int32_t>(JSContext *ctx, int32_t *v, JSValue val) {
    return JS_ToInt32(ctx, v, val);
}

template <>
inline int JSValueTo<float>(JSContext *ctx, float *v, JSValue val) {
    double f;
    int ret = JS_ToFloat64(ctx, &f, val);
    if (ret == 0) *v = (float)f;
    return ret;
}

template <>
inline int JSValueTo<uint32_t>(JSContext *ctx, uint32_t *v, JSValue val) {
    return JS_ToUint32(ctx, v, val);
}

// TODO: free string
template <>
inline int JSValueTo<string>(JSContext *ctx, string *v, JSValue val) {
    const char *str = JS_ToCString(ctx, val);
    *v = str;
    return str == NULL;
}

template <>
inline int JSValueTo<float3>(JSContext *ctx, float3 *v, JSValue val) {
    int ret;
    if (!JS_IsArray(ctx, val)) return 1;
    ret = JSValueTo<float>(ctx, &v->x, JS_GetPropertyUint32(ctx, val, 0));
    ret =
        ret || JSValueTo<float>(ctx, &v->y, JS_GetPropertyUint32(ctx, val, 1));
    ret =
        ret || JSValueTo<float>(ctx, &v->z, JS_GetPropertyUint32(ctx, val, 2));
    return ret;
}

template <>
inline int JSValueTo<float4>(JSContext *ctx, float4 *v, JSValue val) {
    int ret;
    if (!JS_IsArray(ctx, val)) return 1;
    float x = 0, y = 0, z = 0, w = 0;
    ret = JSValueTo<float>(ctx, &x, JS_GetPropertyUint32(ctx, val, 0));
    ret = ret || JSValueTo<float>(ctx, &y, JS_GetPropertyUint32(ctx, val, 1));
    ret = ret || JSValueTo<float>(ctx, &z, JS_GetPropertyUint32(ctx, val, 2));
    ret = ret || JSValueTo<float>(ctx, &w, JS_GetPropertyUint32(ctx, val, 3));
    if (!ret) *v = float4_make(x, y, z, w);
    return ret;
}

template <>
inline int JSValueTo<float4x4>(JSContext *ctx, float4x4 *v, JSValue val) {
    int ret = 0;
    if (!JS_IsArray(ctx, val)) return 1;
    for (int i = 0; i < 16; ++i)
        ret = ret || JSValueTo<float>(ctx, &v->a[i],
                                      JS_GetPropertyUint32(ctx, val, i));
    return ret;
}

template <>
inline int JSValueTo<Array>(JSContext *ctx, Array *v, JSValue val) {
    assert(v != NULL);
    if (!JS_IsArray(ctx, val)) return 1;
    JSValue len_val = JS_GetPropertyStr(ctx, val, "length");
    if (JS_IsException(len_val)) {
        JS_FreeValue(ctx, len_val);
        return -1;
    }
    int ret = JS_ToInt32(ctx, &v->size, len_val);
    JS_FreeValue(ctx, len_val);
    if (ret == 0) {
        v->obj = val;
        v->ctx = ctx;
    }
    return ret;
}

template <>
inline int JSValueTo<ArrayBuffer>(JSContext *ctx, ArrayBuffer *v, JSValue val) {
    assert(v != NULL);
    size_t bufferSize;
    uint8_t *buffer = JS_GetArrayBuffer(ctx, &bufferSize, val);
    if (!buffer) {
        return 1;
    }
    v->buffer = buffer;
    v->byteLength = bufferSize;
    return 0;
}

template <>
inline int JSValueTo<TypedArray>(JSContext *ctx, TypedArray *v, JSValue val) {
    assert(v != NULL);
    size_t byteOffset, byteLength, bytesPerElement, bufferSize;
    JSValue arrayBuffer = JS_GetTypedArrayBuffer(ctx, val, &byteOffset,
                                                 &byteLength, &bytesPerElement);
    if (JS_IsException(arrayBuffer)) {
        // TODO: return exception
        JS_FreeValue(ctx, arrayBuffer);
        return 1;
    }
    uint8_t *buffer = JS_GetArrayBuffer(ctx, &bufferSize, arrayBuffer);
    JS_FreeValue(ctx, arrayBuffer);
    if (!buffer) {
        return 1;
    }
    assert(bufferSize >= byteLength + byteOffset);
    v->buffer = buffer + byteOffset;
    v->byteLength = byteLength;
    v->bytesPerElement = bytesPerElement;
    return 0;
}

template <typename T>
JSValue JSValueFrom(JSContext *ctx, T v);

template <>
inline JSValue JSValueFrom<float>(JSContext *ctx, float v) {
    return JS_NewFloat64(ctx, v);
}

template <>
inline JSValue JSValueFrom<int32_t>(JSContext *ctx, int32_t v) {
    return JS_NewInt32(ctx, v);
}

template <>
inline JSValue JSValueFrom<uint32_t>(JSContext *ctx, uint32_t v) {
    // TODO: JS_NewUint32 is not public
    return JS_NewInt32(ctx, v);
}

template <>
inline JSValue JSValueFrom<bool>(JSContext *ctx, bool v) {
    return JS_NewBool(ctx, v);
}

template <typename T>
inline JSValue JSValueFrom(JSContext *ctx,
                           std::enable_if_t<std::is_enum_v<T>, T> v) {
    return JS_NewInt64(ctx, v);
}

// template <typename T>
// inline JSValue JSValueFrom(JSContext *ctx,
// std::enable_if_t<std::is_pointer_v<T>, T> v)
//{
//	return JS_NULL;
//}
template <typename T>
inline JSValue JSValueFrom(JSContext *ctx,
                           std::enable_if_t<std::is_pointer_v<T>, T> v) {
    if (v == NULL) return JS_NULL;
    JSValue obj =
        JS_NewObjectClass(ctx, JSGetClassID<std::remove_pointer_t<T>>());
    if (JS_IsException(obj)) return obj;
    JS_SetOpaque(obj, v);
    return obj;
}

template <>
inline JSValue JSValueFrom(JSContext *ctx, float3 v) {
    JSValue e = JS_NewArray(ctx);
    JS_DefinePropertyValueUint32(ctx, e, 0, JS_NewFloat64(ctx, v.x),
                                 JS_PROP_C_W_E);
    JS_DefinePropertyValueUint32(ctx, e, 1, JS_NewFloat64(ctx, v.y),
                                 JS_PROP_C_W_E);
    JS_DefinePropertyValueUint32(ctx, e, 2, JS_NewFloat64(ctx, v.z),
                                 JS_PROP_C_W_E);
    return e;
}

template <>
inline JSValue JSValueFrom(JSContext *ctx, float4 v) {
    JSValue e = JS_NewArray(ctx);
    JS_DefinePropertyValueUint32(ctx, e, 0, JS_NewFloat64(ctx, v.x),
                                 JS_PROP_C_W_E);
    JS_DefinePropertyValueUint32(ctx, e, 1, JS_NewFloat64(ctx, v.y),
                                 JS_PROP_C_W_E);
    JS_DefinePropertyValueUint32(ctx, e, 2, JS_NewFloat64(ctx, v.z),
                                 JS_PROP_C_W_E);
    JS_DefinePropertyValueUint32(ctx, e, 3, JS_NewFloat64(ctx, v.w),
                                 JS_PROP_C_W_E);
    return e;
}

template <typename T>
void JSValueFree(JSContext *ctx, T v) {
    // do nothing
}

template <>
void JSValueFree<string>(JSContext *ctx, string v) {
    JS_FreeCString(ctx, v);
}

template <>
void JSValueFree<ArrayBuffer>(JSContext *ctx, ArrayBuffer v) {}

#include "array.h"

static inline void array_from_TypedArray(array *a, TypedArray *ta) {
    assert(a && ta);
    a->stride = ta->bytesPerElement;
    array_free(a);
    array_resize(a, ta->byteLength / ta->bytesPerElement);
    memcpy(a->ptr, ta->buffer, ta->byteLength);
}

#include "animation.h"
#include "asset.h"
#include "camera.h"
#include "light.h"
#include "mesh.h"
#include "renderable.h"
#include "texture.h"
#include "input.h"

#endif /* JSBINDING_HPP */
