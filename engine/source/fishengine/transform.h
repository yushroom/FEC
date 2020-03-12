#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <stdbool.h>

#include "component.h"
#include "ecs.h"
#include "simd_math.h"

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if __clang__
struct __attribute((aligned(16))) Transform {
#else
struct __declspec(align(16)) Transform {
#endif
    quat localRotation;
    float3 localPosition;
    float3 localScale;
    uint32_t parent;
    uint32_t firstChild;
    uint32_t nextSibling;
};
typedef struct Transform Transform;

static inline void TransformInit(Transform *t) {
    memset(t, 0, sizeof(*t));
    t->localRotation.w = 1.f;
    t->localScale.x = t->localScale.y = t->localScale.z = 1.f;
}

float4x4 float4x4_compose(float3 position, quat quaternion, float3 scale);

static inline float4x4 TransformTRS(const Transform *t) {
    return float4x4_compose(t->localPosition, t->localRotation, t->localScale);
}

struct TransformNode {
    //uint32_t parent;
    //uint32_t firstChild;
    //uint32_t nextSibling;
    uint32_t modified;
    bool localNotDirty;
};
typedef struct TransformNode TransformNode;

struct SingletonTransformManager {
    float4x4 LocalToWorld[MaxComponent];
    TransformNode H[MaxComponent];
    float3 LocalEulerAnglesHints[MaxComponent];
    uint32_t count;
    uint32_t modified;
};
typedef struct SingletonTransformManager SingletonTransformManager;

static inline void SingletonTransformManagerInit(
    SingletonTransformManager *tm) {
    memset(tm, 0, sizeof(SingletonTransformManager));
    for (int i = 0; i < countof(tm->LocalToWorld); ++i) {
        tm->LocalToWorld[i] = float4x4_identity();
    }
}

static inline Transform *TransformGet(World *w, uint32_t idx) {
    if (idx == 0) return NULL;
    return (Transform *)WorldGetComponentAt(w, TransformID, idx);
}

static inline uint32_t TransformGetIndex(World *w, Transform *t) {
    return WorldGetComponentIndex(w, t, TransformID);
}

static inline Transform *TransformGetParent(World *w, Transform *t) {
    if (t == NULL) return NULL;
    return TransformGet(w, t->parent);
}

void TransformSetDirty(World *w, Transform *t);

void TransformSetParent2(World *w, Transform *t, uint32_t newParent);

static inline void TransformSetParent(World *w, Transform *t,
                                      Transform *parent) {
    if (t == NULL) return;
    TransformSetParent2(w, t, TransformGetIndex(w, parent));
}


void TransformUpdateLocalToWorldMatrix(World *w, Transform *t);

float4x4 TransformGetLocalToWorldMatrix(World *w, Transform *t);

static inline float4x4 TransformGetWorldToLocalMatrix(World *w, Transform *t) {
    return float4x4_inverse(TransformGetLocalToWorldMatrix(w, t));
}

void TransformSetLocalToWorldMatrix(World *w, Transform *t, float4x4 *l2w);

void TransformLookAt(World *w, Transform *t, float3 target);

float3 TransformGetPosition(World *w, Transform *t);
float3 TransformGetForward(World *w, Transform *t);
float3 TransformGetUp(World *w, Transform *t);
float3 TransformGetRight(World *w, Transform *t);

static inline 
void TransformSetLocalPosition(World *w, Transform *t,
                                             float3 pos) {
    t->localPosition = pos;
    TransformSetDirty(w, t);
}
static inline 
void TransformSetLocalRotation(World *w, Transform *t,
                                             quat rot) {
    t->localRotation = rot;
    TransformSetDirty(w, t);
}
void TransformSetPosition(World *w, Transform *t, float3 worldPosition);

typedef enum Space {
    SpaceWorld,
    SpaceSelf,
} Space;
void TransformTranslate(World *w, Transform *t, float3 translation,
                        Space relativeTo /*= SpaceSelf*/);
void TransformRotateAround(World *w, Transform *t, float3 point, float3 axis, float angle);

//void TransformPrintHierarchy(SingletonTransformManager *t);

#ifdef __cplusplus
}
#endif

#endif /* TRANSFORM_H */
