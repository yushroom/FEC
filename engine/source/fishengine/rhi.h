#ifndef RHI_H
#define RHI_H

#include "transform.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Memory {
    void *buffer;
    size_t byteLength;
};
typedef struct Memory Memory;

static inline Memory MemoryMake(void *buffer, size_t byteLength) {
    Memory m = {.buffer = buffer, .byteLength = byteLength};
    return m;
}

enum GPUResourceUsage {
    GPUResourceUsageNone = 0,
    GPUResourceUsageVertexBuffer = GPUResourceUsageNone,
    GPUResourceUsageIndexBuffer = GPUResourceUsageNone,
    GPUResourceUsageShaderResource = 0x1,
    GPUResourceUsageRenderTarget = 0x2,
    GPUResourceUsageDepthStencil = 0x4,
    GPUResourceUsageUnorderedAccess = 0x8 | GPUResourceUsageShaderResource,
};

typedef int GPUResourceUsageFlags;

typedef uint32_t BufferHandle;
typedef uint32_t ShaderHandle;
typedef uint32_t TextureHandle;

// set memory.buffer = NULL if you want to create an empty buffer
BufferHandle CreateBuffer(Memory memory, GPUResourceUsageFlags usage);
void UpdateBuffer(BufferHandle handle, Memory memory);
uint32_t CreateTexture(uint32_t width, uint32_t height, uint32_t mipmaps,
                       Memory memory);
void DeleteBuffer(BufferHandle handle);
void DeleteTexture(uint32_t textureID);

ShaderHandle CreateShader(const char *path, const char *vs_name,
                      const char *ps_name);
//ShaderHandle CreateShaderFromCompiledFile(const char *path);
void DeleteShader(uint32_t shaderID);

struct Renderable;
// struct Transform;
int SimpleDraw(Transform *t, struct Renderable *r);
int GPUSkinning(struct Renderable *r);
void BeginPass();

void BeginRenderEvent(const char *label);
void EndRenderEvent();

#ifdef __cplusplus
}
#endif

#endif /* RHI_H */
