#ifndef SHADER_H
#define SHADER_H

#include <stdint.h>

#include "rhi.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ShaderPropertyType {
    // ShaderPropertyTypeUnknown,
    ShaderPropertyTypeColor,
    ShaderPropertyTypeVector,
    ShaderPropertyTypeFloat,
    ShaderPropertyTypeRange,
    ShaderPropertyTypeTexture,
};

enum ShaderType {
    ShaderTypeVertex = 0,
    ShaderTypePixel,
    ShaderTypeCompute,
    ShaderTypeGeometry,
    ShaderTypeHull,
    ShaderTypeDomain,
};

//struct ShaderVariant {
//    ShaderHandle vs;
//    ShaderHandle ps;
//};

typedef struct Shader {
    char name[32];
    uint32_t passCount;
    void *impl;
} Shader;

struct ShaderDesc {
    Memory bytes;
    Memory reflectJson;
};
typedef struct ShaderDesc ShaderDesc;

Shader *ShaderNew();
void ShaderFree(void *s);
Shader *ShaderFromFile(const char *path);

// static method
int ShaderPropertyToID(const char *name);
Shader *ShaderFind(const char *name);


//struct ComputeShader {
//    ShaderHandle handle;
//    char name[32];
//    void *reflect;
//};

#ifdef __cplusplus
}
#endif

#endif /* SHADER_H */
