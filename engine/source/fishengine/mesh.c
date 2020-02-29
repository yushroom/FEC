#include "mesh.h"

#include <stddef.h>

#include "asset.h"
#include "rhi.h"
#include "statistics.h"

Skin *SkinNew() {
    Skin *s = (Skin *)malloc(sizeof(Skin));
    memset(s, 0, sizeof(*s));
    s->inverseBindMatrices.stride = sizeof(float4x4);
    s->joints.stride = sizeof(Entity);
    s->boneMats.stride = sizeof(float4x4);
    AssetAdd(AssetTypeSkin, s);
    return s;
}

void SkinFree(void *s) {
    if (s == NULL) return;
    Skin *skin = s;
    array_free(&skin->inverseBindMatrices);
    array_free(&skin->joints);
    array_free(&skin->boneMats);
    free(skin);
}

static void MeshInit(Mesh *m) {
    memset(m, 0, sizeof(Mesh));
    m->vertices.stride = sizeof(Vertex);
    m->triangles.stride = sizeof(uint16_t);
    m->boneWeights.stride = sizeof(BoneWeight);
}

void *MeshNew() {
    Mesh *mesh;
    mesh = malloc(sizeof(*mesh));
    MeshInit(mesh);
    AssetAdd(AssetTypeMesh, mesh);
    return mesh;
}

void MeshFree(void *m) {
    if (!m) return;
    Mesh *mesh = m;
    g_statistics.cpu.vertexBufferSize -= array_get_bytelength(&mesh->vertices);
    g_statistics.cpu.vertexBufferSize -=
        array_get_bytelength(&mesh->boneWeights);
    // TODO: g_statistics
    // g_statistics.cpu.indexBufferSize -=
    // array_get_bytelength(&mesh->triangles);
    array_free(&mesh->vertices);
    array_free(&mesh->triangles);
    array_free(&mesh->boneWeights);
    DeleteBuffer(mesh->vb);
    DeleteBuffer(mesh->ib);
    DeleteBuffer(mesh->sb);
    DeleteBuffer(mesh->skinnedvb);

    // TODO: g_meshes
    free(mesh);
}

static void MeshResizeVertices(Mesh *mesh, uint32_t size) {
    if (size != mesh->vertices.size) {
        g_statistics.cpu.vertexBufferSize -=
            array_get_bytelength(&mesh->vertices);
        array_resize(&mesh->vertices, size);
        g_statistics.cpu.vertexBufferSize +=
            array_get_bytelength(&mesh->vertices);
    }
}

void MeshSetTriangles(Mesh *mesh, void *buffer, uint32_t byteLength,
                      uint32_t stride) {
    g_statistics.cpu.indexBufferSize -= array_get_bytelength(&mesh->triangles);
    mesh->triangles.stride = stride;
    array_resize(&mesh->triangles, byteLength / stride);
    assert(array_get_bytelength(&mesh->triangles) == byteLength);
    memcpy(mesh->triangles.ptr, buffer, byteLength);
    g_statistics.cpu.indexBufferSize += byteLength;
}

void _MeshSetVertices(Mesh *m, enum VertexAttr attr, void *buffer, int count,
                      int stride) {
    if (m->vertices.capacity == 0) {
        MeshResizeVertices(m, count);
    } else {
        assert(m->vertices.size == count);
    }
    void *p = m->vertices.ptr;
    size_t offset = 0;
    size_t len = sizeof(float4);
    if (attr == VertexAttrNormal)
        offset = offsetof(Vertex, normal);
    else if (attr == VertexAttrTangent)
        offset = offsetof(Vertex, tangent);
    else if (attr == VertexAttrUV0) {
        offset = offsetof(Vertex, uv0);
        len = sizeof(float2);
    } else if (attr == VertexAttrUV1) {
        offset = offsetof(Vertex, uv1);
        len = sizeof(float2);
    }
    p += offset;
    void *q = buffer;
    len = len < stride ? len : stride;
    for (int i = 0; i < count; ++i) {
        memcpy(p, q, len);
        p += sizeof(Vertex);
        q += stride;
    }
    m->attributes |= (1 << attr);
}

void _MeshSetVertices2(Mesh *m, enum VertexAttr attr, void *buffer, int count,
                       int stride) {
    if (m->boneWeights.capacity == 0) {
        //        MeshResizeVertices(m, count);
        array_resize(&m->boneWeights, count);
    } else {
        assert(m->boneWeights.size == count);
    }
    void *p = m->boneWeights.ptr;
    size_t offset = 0;
    size_t len = sizeof(float4);
    if (attr == VertexAttrWeights) offset = offsetof(BoneWeight, weights);
    p += offset;
    void *q = buffer;
    len = len < stride ? len : stride;
    for (int i = 0; i < count; ++i) {
        if (attr == VertexAttrBoneIndex && stride == sizeof(uint16_t) * 4) {
            uint16_t indices[4];
            memcpy(indices, q, stride);
            for (int i = 0; i < 4; ++i) ((uint32_t *)p)[i] = indices[i];
        } else {
            memcpy(p, q, len);
        }
        if (attr == VertexAttrWeights) {
            float4 *v = p;
            float sum = v->x + v->y + v->z + v->w;
            if (fabsf(sum - 1) > 0.01f) {
                printf("sum not one: %f\n", sum);
            }
            //			if (fabsf(sum) > 0.1f)
            //				*v = float4_mul(*v, 1.f/sum);
        }
        p += sizeof(BoneWeight);
        q += stride;
    }
    m->attributes |= (1 << attr);
}

void MeshSetVertices(Mesh *m, enum VertexAttr attr, void *buffer, int count,
                     int stride) {
    if (attr >= VertexAttrBoneIndex)
        _MeshSetVertices2(m, attr, buffer, count, stride);
    else
        _MeshSetVertices(m, attr, buffer, count, stride);
}

void MeshUploadMeshData(Mesh *m) {
    if (m->triangles.size != 0) {
        Memory memory = {.buffer = m->triangles.ptr,
                         .byteLength = array_get_bytelength(&m->triangles)};
        m->ib = CreateBuffer(memory);
    }
    if (m->vertices.size != 0) {
        if (!(m->attributes & (1 << VertexAttrNormal))) {
            MeshRecalculateNormals(m);
        }
        //		if (!(m->attributes & (1 << VertexAttrTangent))) {
        //			MeshRecalculateTangents(m);
        //		}
        Memory memory = {.buffer = m->vertices.ptr,
                         .byteLength = array_get_bytelength(&m->vertices)};
        m->vb = CreateBuffer(memory);
    }
    if (m->boneWeights.size != 0) {
        Memory memory = {.buffer = m->boneWeights.ptr,
                         .byteLength = array_get_bytelength(&m->boneWeights)};
        m->sb = CreateBuffer(memory);
    }
}

Mesh *MeshCombine(Mesh **meshes, uint32_t count) {
    if (count <= 1) return NULL;
    Mesh *combined = MeshNew();
    uint32_t vertex_count = 0, index_count = 0;
    for (int i = 0; i < count; ++i) {
        vertex_count += meshes[i]->vertices.size;
        index_count += meshes[i]->triangles.size;
    }

    array_resize(&combined->vertices, vertex_count);
    combined->triangles.stride = 4;
    array_resize(&combined->triangles, index_count);

    Vertex *pv = combined->vertices.ptr;
    uint32_t *pi = combined->triangles.ptr;
    uint32_t vertex_offset = 0;
    for (int i = 0; i < count; ++i) {
        Mesh *m = meshes[i];
        memcpy(pv, m->vertices.ptr, array_get_bytelength(&m->vertices));
        pv += m->vertices.size;
        if (m->triangles.stride == 2) {
            uint16_t *ppi = m->triangles.ptr;
            for (int j = 0; j < m->triangles.size; ++j) {
                *pi = *ppi + vertex_offset;
                pi++;
                ppi++;
            }
        } else if (m->triangles.stride == 4) {
            uint32_t *ppi = m->triangles.ptr;
            for (int j = 0; j < m->triangles.size; ++j) {
                *pi = *ppi + vertex_offset;
                pi++;
                ppi++;
            }
        }
        vertex_offset += m->vertices.size;
    }

    g_statistics.cpu.vertexBufferSize +=
        array_get_bytelength(&combined->vertices);
    g_statistics.cpu.indexBufferSize +=
        array_get_bytelength(&combined->triangles);

    return combined;
}

static uint32_t MeshGetTriangleCount(Mesh *mesh) {
    if (mesh->triangles.size != 0) {
        assert(mesh->triangles.size % 3 == 0);
        return mesh->triangles.size / 3;
    }
    assert(mesh->vertices.size % 3 == 0);
    return mesh->vertices.size / 3;
}

static uint32_t MeshGetIndexAt(Mesh *mesh, int idx) {
    if (mesh->triangles.size == 0) return idx;
    assert(idx < mesh->triangles.size);
    if (mesh->triangles.stride == sizeof(uint16_t)) {
        uint16_t *p = mesh->triangles.ptr;
        return p[idx];
    } else {
        uint32_t *p = mesh->triangles.ptr;
        return p[idx];
    }
}

static uint32_t MeshGetIndexAt2(Mesh *mesh, int faceIdx, int vertexIdx) {
    assert(faceIdx < MeshGetTriangleCount(mesh));
    return MeshGetIndexAt(mesh, faceIdx * 3 + vertexIdx);
}

void MeshRecalculateNormals(Mesh *mesh) {
    uint32_t icount = MeshGetTriangleCount(mesh);
    uint32_t vcount = MeshGetVertexCount(mesh);
    Vertex *vertices = mesh->vertices.ptr;
    float4 zero = {0, 0, 0, 0};
    for (int vid = 0; vid < vcount; ++vid) {
        vertices[vid].normal = zero;
    }

    for (int faceIdx = 0; faceIdx < icount; ++faceIdx) {
        uint32_t i0 = MeshGetIndexAt2(mesh, faceIdx, 0);
        uint32_t i1 = MeshGetIndexAt2(mesh, faceIdx, 1);
        uint32_t i2 = MeshGetIndexAt2(mesh, faceIdx, 2);
        assert(i0 < vcount && i1 < vcount && i2 < vcount);
        // TODO: use float4
        float3 v0 = float4_to_float3(vertices[i0].position);
        float3 v1 = float4_to_float3(vertices[i1].position);
        float3 v2 = float4_to_float3(vertices[i2].position);
        float3 v01 = float3_normalize(float3_subtract(v1, v0));
        float3 v12 = float3_normalize(float3_subtract(v2, v1));
        float3 faceNormal = float3_cross(v01, v12);
        float4 n = {faceNormal.x, faceNormal.y, faceNormal.z, 0};
        n = float4_normalize(n);
        vertices[i0].normal += n;
        vertices[i1].normal += n;
        vertices[i2].normal += n;
    }

    for (int vid = 0; vid < vcount; ++vid) {
        vertices[vid].normal = float4_normalize(vertices[vid].normal);
    }

    mesh->attributes &= (1 << VertexAttrNormal);
}

#include <mikktspace.h>

static int getNumFaces(const SMikkTSpaceContext *pContext) {
    Mesh *mesh = (Mesh *)pContext->m_pUserData;
    return MeshGetTriangleCount(mesh);
}

static int getNumVerticesOfFace(const SMikkTSpaceContext *pContext,
                                const int iFace) {
    return 3;
}

static void getPosition(const SMikkTSpaceContext *pContext, float fvPosOut[],
                        const int iFace, const int iVert) {
    Mesh *mesh = (Mesh *)pContext->m_pUserData;
    uint32_t idx = MeshGetIndexAt(mesh, iFace * 3 + iVert);
    Vertex *p = mesh->vertices.ptr;
    float4 pos = p[idx].position;
    fvPosOut[0] = pos.x;
    fvPosOut[1] = pos.y;
    fvPosOut[2] = pos.z;
}

static void getNormal(const SMikkTSpaceContext *pContext, float fvNormOut[],
                      const int iFace, const int iVert) {
    Mesh *mesh = (Mesh *)pContext->m_pUserData;
    uint32_t idx = MeshGetIndexAt(mesh, iFace * 3 + iVert);
    Vertex *p = mesh->vertices.ptr;
    float4 normal = p[idx].normal;
    fvNormOut[0] = normal.x;
    fvNormOut[1] = normal.y;
    fvNormOut[2] = normal.z;
}

static void getTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[],
                        const int iFace, const int iVert) {
    Mesh *mesh = (Mesh *)pContext->m_pUserData;
    uint32_t idx = MeshGetIndexAt(mesh, iFace * 3 + iVert);
    Vertex *p = mesh->vertices.ptr;
    float2 uv = p[idx].uv0;
    fvTexcOut[0] = uv.x;
    fvTexcOut[1] = uv.y;
}

static void setTSpaceBasic(const SMikkTSpaceContext *pContext,
                           const float fvTangent[], const float fSign,
                           const int iFace, const int iVert) {
    Mesh *mesh = (Mesh *)pContext->m_pUserData;
    uint32_t idx = MeshGetIndexAt(mesh, iFace * 3 + iVert);
    Vertex *p = mesh->vertices.ptr;
    float4 tangent;
    tangent.x = fvTangent[0];
    tangent.y = fvTangent[1];
    tangent.z = fvTangent[2];
    tangent.w = fSign;
    p[idx].tangent = tangent;
}

void MeshRecalculateTangents(Mesh *mesh) {
    SMikkTSpaceInterface interface;
    interface.m_getNumFaces = getNumFaces;
    interface.m_getNumVerticesOfFace = getNumVerticesOfFace;
    interface.m_getPosition = getPosition;
    interface.m_getNormal = getNormal;
    interface.m_getTexCoord = getTexCoord;
    interface.m_setTSpaceBasic = setTSpaceBasic;
    interface.m_setTSpace = NULL;

    SMikkTSpaceContext ctx;
    ctx.m_pInterface = &interface;
    ctx.m_pUserData = (void *)mesh;

    int ret = genTangSpaceDefault(&ctx);
    assert(ret == 1);
    mesh->attributes &= (1 << VertexAttrTangent);
}
