#ifndef MATERIAL_H
#define MATERIAL_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "shader.h"
#include "simd_math.h"
#include "texture.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Material {
    Shader *shader;
    Texture *mainTexture;
    float4 color;
    void *impl;
};
typedef struct Material Material;

void *MaterialNew();
void MaterialFree(void *material);
void MaterialSetShader(Material *mat, Shader *shader);
void MaterialSetFloat(Material *mat, int nameID, float value);
float MaterialGetFloat(Material *mat, int nameID);
void MaterialSetVector(Material *mat, int nameID, float4 value);
float4 MaterialGetVector(Material *mat, int nameID);
void MaterialSetTexture(Material *mat, int nameID, Texture *texture);
Texture *MaterialGetTexture(Material *mat, int nameID);

bool MaterialIsKeywordEnabled(Material *mat, const char *keyword);
void MaterialSetKeyword(Material *mat, const char *keyword, bool enabled);
void MaterialEnableKeyword(Material *mat, const char *keyword);
void MaterialDisableKeyword(Material *mat, const char *keyword);

#ifdef __cplusplus
}
#endif

#endif /* MATERIAL_H */
