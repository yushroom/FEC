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

struct Shader {
    ShaderHandle handle;
    char name[32];
    void *reflect;
};
typedef struct Shader Shader;

Shader *ShaderNew();
void ShaderFree(void *s);
Shader *ShaderFromFile(const char *path);
int ShaderPropertyToID(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* SHADER_H */
