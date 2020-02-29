#include "rhi.h"

uint32_t CreateBuffer(Memory memory) { return 0; }
uint32_t CreateTexture(uint32_t width, uint32_t height, uint32_t mipmaps,
                       Memory memory) {
    return 1;
}
void DeleteBuffer(uint32_t bufferID) {}
void DeleteTexture(uint32_t textureID) {}

uint32_t CreateShader(const char* path, const char* vs_name,
                      const char* ps_name) {
    return 0;
}
void DeleteShader(uint32_t shaderID) {}

struct Renderable;
// struct Transform;
int SimpleDraw(Transform* t, struct Renderable* r) { return 1; }
int GPUSkinning(struct Renderable* r) { return 1; }
void BeginPass() {}