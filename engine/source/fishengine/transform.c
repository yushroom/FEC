#include "transform.h"

void TransformSetDirty(World *w, Transform *t) {
    if (t == NULL) return;
    SingletonTransformManager *tm =
        WorldGetSingletonComponent(w, SingletonTransformManagerID);
    uint32_t idx = WorldGetComponentIndex(w, t, TransformID);
    tm->modified++;
    tm->H[idx].modified = tm->modified;
    tm->H[idx].localNotDirty = false;
}

void TransformSetParent2(World *w, Transform *t, uint32_t newParent) {
    uint32_t child = WorldGetComponentIndex(w, t, TransformID);
    uint32_t parent = t->parent;
    if (parent == newParent) return;

    // detach from the old parent
    if (parent != 0) {
        Transform *parentT = TransformGet(w, parent);
        uint32_t p = parentT->firstChild;
        if (p == child) {  // is the first child
            parentT->firstChild = t->nextSibling;
        } else {
            while (TransformGet(w, p)->nextSibling != child) {
                p = TransformGet(w, p)->nextSibling;
            }
            TransformGet(w, p)->nextSibling = t->nextSibling;
        }
    }

    Transform *newParentT = TransformGet(w, newParent);
    t->nextSibling = newParentT->firstChild;
    newParentT->firstChild = child;
    t->parent = newParent;
    TransformSetDirty(w, t);
}

//static void TransformPrintHierarchyNode(SingletonTransformManager *t, int n,
//                                        int depth) {
//    for (int i = 0; i < depth; ++i) {
//        putchar(' ');
//    }
//    printf("%d\n", n);
//    if (t->H[n].firstChild != 0)
//        TransformPrintHierarchyNode(t, t->H[n].firstChild, depth + 1);
//    if (t->H[n].nextSibling != 0)
//        TransformPrintHierarchyNode(t, t->H[n].nextSibling, depth);
//}
//
//void TransformPrintHierarchy(SingletonTransformManager *t) {
//    puts("Transform [");
//    for (int i = 1; i < 100; ++i) {
//        if (t->H[i].parent == 0 && t->H[i].firstChild != 0) {
//            TransformPrintHierarchyNode(t, i, 1);
//        }
//    }
//    puts("]");
//}

void TransformUpdateLocalToWorldMatrix2(World* w, uint32_t idx) {
    SingletonTransformManager *tm =
        WorldGetSingletonComponent(w, SingletonTransformManagerID);
    float4x4 *l2w = &tm->LocalToWorld[idx];
    Transform *t = TransformGet(w, idx);
    uint32_t parent = t->parent;
    if (parent != 0) {
        TransformUpdateLocalToWorldMatrix2(w, parent);  // update parent's l2w
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

void TransformUpdateLocalToWorldMatrix(World *w, Transform *t) {
    uint32_t idx = TransformGetIndex(w, t);
    TransformUpdateLocalToWorldMatrix2(w, idx);
}

float4x4 TransformGetLocalToWorldMatrix(World *w, Transform *t) {
    TransformUpdateLocalToWorldMatrix(w, t);
    SingletonTransformManager *tm =
        WorldGetSingletonComponent(w, SingletonTransformManagerID);
    uint32_t idx = TransformGetIndex(w, t);
    return tm->LocalToWorld[idx];
}

void TransformSetLocalToWorldMatrix(World *w, Transform *t, float4x4 *l2w) {
    uint32_t parent = t->parent;
    if (parent == 0) {
        float4x4_decompose(l2w, &t->localPosition, &t->localRotation,
                           &t->localScale);
    } else {
        // l2w = l2w_parent * localMat => localMat = w2l_parent * l2w
        float4x4 local =
            float4x4_mul(TransformGetWorldToLocalMatrix(w, TransformGet(w, parent)), *l2w);
        float4x4_decompose(&local, &t->localPosition, &t->localRotation,
                           &t->localScale);
    }
    TransformSetDirty(w, t);
}
void TransformLookAt(World *w, Transform *t, float3 target) {
    float3 worldPosition = TransformGetPosition(w, t);
    float4x4 w2l = float4x4_look_at(worldPosition, target, float3_up);
    float4x4 l2w = float4x4_inverse(w2l);
    TransformSetLocalToWorldMatrix(w, t, &l2w);
}

float3 TransformGetPosition(World *w, Transform *t) {
    float3 pos = t->localPosition;
    uint32_t p = t->parent;
    if (p != 0) {
        float4x4 l2w = TransformGetLocalToWorldMatrix(w, t);
        pos = float4x4_mul_point(l2w, float3_zero);
    }
    return pos;
}

float3 TransformGetForward(World *w, Transform *t) {
    float3 forward = float3_forward;
    float4x4 l2w = TransformGetLocalToWorldMatrix(w, t);
    forward = float4x4_mul_vector(l2w, forward);
    return float3_normalize(forward);
}

float3 TransformGetUp(World *w, Transform *t) {
    float3 up = float3_up;
    float4x4 l2w = TransformGetLocalToWorldMatrix(w, t);
    up = float4x4_mul_vector(l2w, up);
    return float3_normalize(up);
}

float3 TransformGetRight(World *w, Transform *t) {
    float3 right = float3_right;
    float4x4 l2w = TransformGetLocalToWorldMatrix(w, t);
    right = float4x4_mul_vector(l2w, right);
    return float3_normalize(right);
}

void TransformSetPosition(World *w, Transform *t, float3 worldPosition) {
    uint32_t parent = t->parent;
    if (parent == 0) {
        t->localPosition = worldPosition;
    } else {
        t->localPosition = float4x4_mul_point(
            TransformGetWorldToLocalMatrix(w, TransformGet(w, parent)),
            worldPosition);
    }
}

void TransformTranslate(World *w, Transform *t, float3 translation, Space relativeTo /*= SpaceSelf*/) {
    if (relativeTo == SpaceWorld) {
        float3 pos = float3_add(TransformGetPosition(w, t), translation);
        TransformSetPosition(w, t, pos);
    } else {
        float3 pos = float3_add(t->localPosition, translation);
        TransformSetLocalPosition(w, t, pos);
    }
}

void TransformRotateAround(World* w, Transform* t, float3 point, float3 axis,
    float angle) {
    // step1: update position
    float3 vector = TransformGetPosition(w, t);
    quat rotation = quat_angle_axis(angle, axis);
    float3 vector2 = float3_subtract(vector, point);
    vector2 = quat_mul_point(rotation, vector2);
    vector = float3_add(point, vector2);
    TransformSetPosition(w, t, vector);

    // step2: update rotation
    TransformSetLocalRotation(w, t, quat_mul_quat(rotation, t->localRotation));
}