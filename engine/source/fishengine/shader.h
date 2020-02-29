#ifndef SHADER_H
#define SHADER_H

#include <stdint.h>

#include "rhi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t ShaderID;

enum ShaderPropertyType {
    // ShaderPropertyTypeUnknown,
    ShaderPropertyTypeColor,
    ShaderPropertyTypeVector,
    ShaderPropertyTypeFloat,
    ShaderPropertyTypeRange,
    ShaderPropertyTypeTexture,
};

struct Shader {
    ShaderID handle;
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
