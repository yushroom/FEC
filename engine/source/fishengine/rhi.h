#ifndef RHI_H
#define RHI_H

#include "transform.h"

struct Memory {
    void *buffer;
    size_t byteLength;
};
typedef struct Memory Memory;

#ifdef __cplusplus
extern "C" {
#endif

static inline Memory MemoryMake(void *buffer, size_t byteLength) {
    Memory m = {.buffer = buffer, .byteLength = byteLength};
    return m;
}

uint32_t CreateBuffer(Memory memory);
uint32_t CreateTexture(uint32_t width, uint32_t height, uint32_t mipmaps,
                       Memory memory);
void DeleteBuffer(uint32_t bufferID);
void DeleteTexture(uint32_t textureID);

uint32_t CreateShader(const char *path, const char *vs_name,
                      const char *ps_name);
void DeleteShader(uint32_t shaderID);

struct Renderable;
// struct Transform;
int SimpleDraw(Transform *t, struct Renderable *r);
int GPUSkinning(struct Renderable *r);
void BeginPass();

#ifdef __cplusplus
}
#endif

#endif /* RHI_H */
