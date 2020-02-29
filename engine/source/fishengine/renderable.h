#ifndef RENDERABLE_H
#define RENDERABLE_H

#include "material.h"
#include "mesh.h"

struct Renderable {
    Mesh *mesh;
    Material *material;
    Skin *skin;

    //    array bones;  // vector<float4x4>;
};
typedef struct Renderable Renderable;

#ifdef __cplusplus
extern "C" {
#endif

static inline void RenderableInit(Renderable *r) {
    memset(r, 0, sizeof(*r));
    //    r->bones.stride = sizeof(float4x4);
}

static inline void RenderableSetMesh(Renderable *r, Mesh *mesh) {
    if (r->mesh) {
        //		MeshRelease(r->mesh);
        r->mesh--;
    }
    if (mesh) mesh->refcount++;
    r->mesh = mesh;
}

void RenderableUpdateBones(Renderable *r, World *w);

#ifdef __cplusplus
}
#endif

#endif /* RENDERABLE_H */
