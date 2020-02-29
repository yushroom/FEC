#include "transform.h"

static void TransformPrintHierarchyNode(SingletonTransformManager *t, int n,
                                        int depth) {
    for (int i = 0; i < depth; ++i) {
        putchar(' ');
    }
    printf("%d\n", n);
    if (t->H[n].firstChild != 0)
        TransformPrintHierarchyNode(t, t->H[n].firstChild, depth + 1);
    if (t->H[n].nextSibling != 0)
        TransformPrintHierarchyNode(t, t->H[n].nextSibling, depth);
}

void TransformPrintHierarchy(SingletonTransformManager *t) {
    puts("Transform [");
    for (int i = 1; i < 100; ++i) {
        if (t->H[i].parent == 0 && t->H[i].firstChild != 0) {
            TransformPrintHierarchyNode(t, i, 1);
        }
    }
    puts("]");
}

float3 TransformGetPosition(SingletonTransformManager *tm, World *w,
                            uint32_t idx) {
    Transform *t = WorldGetComponentAt(w, TransformID, idx);
    float3 pos = t->localPosition;
    uint32_t p = TransformGetParent(tm, idx);
    if (p != 0) {
        TransformUpdateLocalToWorldMatrix(tm, w, idx);
        float4x4 *l2w = &tm->LocalToWorld[idx];
        float3 zero = {0, 0, 0};
        pos = float4x4_mul_point(l2w, zero);
    }
    return pos;
}

float3 TransformGetForward(SingletonTransformManager *tm, World *w,
                           uint32_t idx) {
    float3 forward = float3_forward;
    float4x4 l2w = TransformGetLocalToWorldMatrix(tm, w, idx);
    forward = float4x4_mul_vector(&l2w, forward);
    return float3_normalize(forward);
}

float3 TransformGetUp(SingletonTransformManager *tm, World *w, uint32_t idx) {
    float3 up = float3_up;
    float4x4 l2w = TransformGetLocalToWorldMatrix(tm, w, idx);
    up = float4x4_mul_vector(&l2w, up);
    return float3_normalize(up);
}
