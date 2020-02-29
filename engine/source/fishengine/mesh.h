#ifndef MESH_H
#define MESH_H

#include <stdbool.h>

#include "array.h"
#include "ecs.h"
#include "simd_math.h"

#ifdef __cplusplus
extern "C" {
#endif

enum VertexAttr {
    VertexAttrPosition = 0,
    VertexAttrNormal,
    VertexAttrTangent,
    VertexAttrUV0,
    VertexAttrUV1,
    VertexAttrBoneIndex,
    VertexAttrWeights,
};
// typedef enum VertexAttr VertexAttr;

struct Vertex {
    float4 position;
    float4 normal;
    float4 tangent;
    float2 uv0;
    float2 uv1;
};
typedef struct Vertex Vertex;

struct BoneWeight {
    uint32_t boneIndex[4];
    float weights[4];
};
typedef struct BoneWeight BoneWeight;

struct Skin {
    Entity root;
    array inverseBindMatrices;
    array joints;

    array boneMats;  // vector<float4x4>;
};
typedef struct Skin Skin;

Skin *SkinNew();
void SkinFree(void *skin);

struct Mesh {
    // public:
    array vertices;
    array triangles;
    array boneWeights;

    // private:
    uint32_t vb;
    uint32_t ib;
    uint32_t sb;
    uint32_t skinnedvb;  // TODO: move to Renderable
    int refcount;
    uint32_t attributes;
};
typedef struct Mesh Mesh;

void *MeshNew();
void MeshFree(void *mesh);

static inline uint32_t MeshGetIndexCount(Mesh *mesh) {
    return mesh->triangles.size;
}

static inline void MeshClear(Mesh *m) {
    m->vertices.size = 0;
    m->triangles.size = 0;
}

static inline uint32_t MeshGetVertexCount(Mesh *mesh) {
    return mesh->vertices.size;
}
void MeshSetTriangles(Mesh *mesh, void *buffer, uint32_t byteLength,
                      uint32_t stride);
void MeshSetVertices(Mesh *m, enum VertexAttr attr, void *buffer, int count,
                     int stride);
void MeshUploadMeshData(Mesh *m);
Mesh *MeshCombine(Mesh **meshes, uint32_t count);
void MeshRecalculateNormals(Mesh *mesh);
void MeshRecalculateTangents(Mesh *mesh);

static inline void MeshRelease(Mesh *m) { m->refcount--; }

static inline bool MeshIsUploaded(Mesh *m) { return m->vb != 0; }

#ifdef __cplusplus
}
#endif

#endif
