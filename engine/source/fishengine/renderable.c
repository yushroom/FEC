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
    Entity e = ComponentGetEntity(w, r, RenderableID);
    Transform *t = EntityGetComponent(e, w, TransformID);
    float4x4 world2Object = TransformGetLocalToWorldMatrix(w, t);
    world2Object = float4x4_inverse(world2Object);
    assert(r->skin->joints.size == r->skin->inverseBindMatrices.size);
    uint32_t *joints = r->skin->joints.ptr;
    float4x4 *inverseBindMatrices = r->skin->inverseBindMatrices.ptr;
    float4x4 *boneT = r->skin->boneMats.ptr;
    for (int i = 0; i < boneCount; ++i) {
        uint32_t bone = joints[i] - r->skin->minJoint;
        Entity joint = r->boneToEntity[bone];
        float4x4 bindpose = inverseBindMatrices[i];
        boneT[i] = float4x4_mul(
            float4x4_mul(world2Object,
                         TransformGetLocalToWorldMatrix(w, TransformGet(w, joint))),
            bindpose);
    }
}
