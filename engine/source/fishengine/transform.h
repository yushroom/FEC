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
    //    bool localNotDirty;
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
    uint32_t parent;
    uint32_t firstChild;
    uint32_t nextSibling;
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

static inline void TransformSystem(World *w) {
    //	ComponentArray *a = w->componentArrays + TransformID;
    //	TransformManager *tm = WorldGetSingletonComponent(w,
    // TransformManagerID); 	ForeachComponent(a, Transform, t) {
    // float4x4 *m = tm->LocalToWorld + i; 		*m = TransformTRS(t);
    //	}
}

static inline void TransformManagerInit(SingletonTransformManager *tm) {
    memset(tm, 0, sizeof(SingletonTransformManager));
    for (int i = 0; i < countof(tm->LocalToWorld); ++i) {
        tm->LocalToWorld[i] = float4x4_identity();
    }
}

static inline Transform *TransformGet(World *w, uint32_t idx) {
    return (Transform *)WorldGetComponentAt(w, TransformID, idx);
}

static inline void TransformSetDirty(SingletonTransformManager *tm,
                                     uint32_t idx) {
    tm->modified++;
    tm->H[idx].modified = tm->modified;
    tm->H[idx].localNotDirty = false;
}

// static inline void TransformSetDirty(Transform *t) { t->localNotDirty = 0; }

static inline uint32_t TransformGetParent(SingletonTransformManager *tm,
                                          uint32_t child) {
    return tm->H[child].parent;
}

static inline void TransformSetParent(SingletonTransformManager *tm,
                                      uint32_t child, uint32_t newParent) {
    uint32_t parent = tm->H[child].parent;
    if (parent == newParent) return;

    // detach from the old parent
    uint32_t p = tm->H[parent].firstChild;
    if (p == child)  // is the first child
    {
        tm->H[parent].firstChild = tm->H[child].nextSibling;
    } else if (parent != 0) {
        while (tm->H[p].nextSibling != child) {
            p = tm->H[p].nextSibling;
        }
        tm->H[p].nextSibling = tm->H[child].nextSibling;
    }

    tm->H[child].nextSibling = tm->H[newParent].firstChild;
    tm->H[newParent].firstChild = child;
    tm->H[child].parent = newParent;
    TransformSetDirty(tm, child);
}

static inline void TransformUpdateLocalToWorldMatrix(
    SingletonTransformManager *tm, World *w, uint32_t idx) {
    float4x4 *l2w = &tm->LocalToWorld[idx];
    Transform *t = TransformGet(w, idx);
    uint32_t parent = TransformGetParent(tm, idx);
    if (parent != 0) {
        TransformUpdateLocalToWorldMatrix(tm, w,
                                          parent);  // update parent's l2w
        if (tm->H[idx].modified <
                tm->H[parent].modified ||  // parent is modified after child
            !tm->H[idx]
                 .localNotDirty)  // child self is modified, so update its l2w
        {
            *l2w = TransformTRS(t);
            float4x4 *parentL2W = &tm->LocalToWorld[parent];
            *l2w = float4x4_mul(*parentL2W, *l2w);
            if (tm->H[idx].modified < tm->H[parent].modified)
                tm->H[idx].modified = tm->H[parent].modified;
        }
    } else {  // has no parent
        if (!tm->H[idx].localNotDirty) {
            *l2w = TransformTRS(t);
        }
    }
    tm->H[idx].localNotDirty = true;
}

static inline float4x4 TransformGetLocalToWorldMatrix(
    SingletonTransformManager *tm, World *w, uint32_t idx) {
    TransformUpdateLocalToWorldMatrix(tm, w, idx);
    return tm->LocalToWorld[idx];
}
static inline float4x4 TransformGetWorldToLocalMatrix(
    SingletonTransformManager *tm, World *w, uint32_t idx) {
    return float4x4_inverse(TransformGetLocalToWorldMatrix(tm, w, idx));
}

void TransformSetLocalToWorldMatrix(SingletonTransformManager *tm, World *w,
                                    uint32_t idx, float4x4 *l2w);
void TransformLookAt(SingletonTransformManager *tm, World *w, uint32_t idx,
                     float3 target);

float3 TransformGetPosition(SingletonTransformManager *tm, World *w,
                            uint32_t idx);
float3 TransformGetForward(SingletonTransformManager *tm, World *w,
                           uint32_t idx);
float3 TransformGetUp(SingletonTransformManager *tm, World *w, uint32_t idx);

void TransformPrintHierarchy(SingletonTransformManager *t);

#ifdef __cplusplus
}
#endif

#endif /* TRANSFORM_H */
