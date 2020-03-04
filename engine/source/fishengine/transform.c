#include "transform.h"

void TransformSetParent(SingletonTransformManager *tm, uint32_t child,
                        uint32_t newParent) {
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

void TransformUpdateLocalToWorldMatrix(SingletonTransformManager *tm, World *w,
                                       uint32_t idx) {
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

void TransformSetLocalToWorldMatrix(SingletonTransformManager *tm, World *w,
                                    uint32_t idx, float4x4 *l2w) {
    uint32_t parent = TransformGetParent(tm, idx);
    Transform *t = TransformGet(w, idx);
    if (parent == 0) {
        float4x4_decompose(l2w, &t->localPosition, &t->localRotation,
                           &t->localScale);
    } else {
        // l2w = l2w_parent * localMat => localMat = w2l_parent * l2w
        float4x4 local =
            float4x4_mul(TransformGetWorldToLocalMatrix(tm, w, parent), *l2w);
        float4x4_decompose(&local, &t->localPosition, &t->localRotation,
                           &t->localScale);
    }
    TransformSetDirty(tm, idx);
}
void TransformLookAt(SingletonTransformManager *tm, World *w, uint32_t idx,
                     float3 target) {
    float3 worldPosition = TransformGetPosition(tm, w, idx);
    float4x4 w2l = float4x4_look_at(worldPosition, target, float3_up);
    float4x4 l2w = float4x4_inverse(w2l);
    TransformSetLocalToWorldMatrix(tm, w, idx, &l2w);
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
