#include "renderable.h"

#include "transform.h"

void RenderableUpdateBones(Renderable *r, World *w) {
    if (r->skin == NULL) return;
    int boneCount = r->skin->inverseBindMatrices.size;
    assert(boneCount <= 64);
    if (r->skin->boneMats.size != boneCount) {
        r->skin->boneMats.stride = sizeof(float4x4);
        array_resize(&r->skin->boneMats, boneCount);
    }
    SingletonTransformManager *tm =
        WorldGetSingletonComponent(w, SingletonTransformManagerID);
    Entity e = ComponentGetEntity(w, r, RenderableID);
    float4x4 world2Object = TransformGetLocalToWorldMatrix(tm, w, e);
    world2Object = float4x4_inverse(world2Object);
    assert(r->skin->joints.size == r->skin->inverseBindMatrices.size);
    Entity *joints = r->skin->joints.ptr;
    float4x4 *inverseBindMatrices = r->skin->inverseBindMatrices.ptr;
    float4x4 *boneT = r->skin->boneMats.ptr;
    for (int i = 0; i < boneCount; ++i) {
        Entity joint = joints[i];
        float4x4 bindpose = inverseBindMatrices[i];
        boneT[i] = float4x4_mul(
            float4x4_mul(world2Object,
                         TransformGetLocalToWorldMatrix(tm, w, joint)),
            bindpose);
    }
}
