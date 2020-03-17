#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdint.h>

struct gpu_statistics {
    uint32_t drawCall;
    uint32_t bufferCount;
    uint32_t bufferSize;
    uint32_t textureCount;
    uint32_t textureSize;
    uint32_t renderTextureCount;
    uint32_t renderTextureSize;
};

struct asset_statistics {
    uint32_t animationClipCount;
    uint32_t animationClipSize;
};

struct cpu_statistics {
    uint32_t vertexBufferSize;
    uint32_t indexBufferSize;
    uint32_t textureSize;
    uint32_t ecsSize;
    uint32_t uploadHeapSize;
};

struct statistics {
    struct cpu_statistics cpu;
    struct gpu_statistics gpu;
};

#ifdef __cplusplus
extern "C" {
#endif

void init_global_statistics();
struct statistics *get_global_statistics();

#ifdef __cplusplus
}
#endif

#define g_statistics (*get_global_statistics())

#endif /* STATISTICS_H */
