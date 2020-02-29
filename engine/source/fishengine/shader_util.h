#ifndef SHADER_UTIL_H
#define SHADER_UTIL_H
#include "shader.h"

#ifdef __cplusplus
extern "C" {
#endif

int ShaderUtilGetPropertyCount(Shader *s);
const char *ShaderUtilGetPropertyName(Shader *s, int propertyIdx);
enum ShaderPropertyType ShaderUtilGetPropertyType(Shader *s, int propertyIdx);

#ifdef __cplusplus
}
#endif

#endif /* SHADER_UTIL_H */
