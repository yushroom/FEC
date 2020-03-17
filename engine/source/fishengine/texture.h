#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdint.h>
#include "asset.h"
#include "texture_format.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t TextureID;

typedef enum {
    FilterModePoint = 0,
    FilterModeBilinear,
    FilterModeTrilinear,
    _FilterModeCount,
} FilterMode;

typedef enum {
    TextureWrapModeRepeat = 0,
    TextureWrapModeClamp,
    TextureWrapModeMirror,
    TextureWrapModeMirrorOnce,
    _TextureWrapModeCount,
} TextureWrapMode;

typedef enum {
    TextureDimensionUnknown = -1,
    TextureDimensionNone,
    TextureDimensionAny,
    TextureDimensionTex2D,
    TextureDimensionTex2DArray,
    TextureDimensionTex3D,
    TextureDimensionCube,
    TextureDimensionCubeArray,
} TextureDimension;


typedef enum {
    RenderTextureFormatARGB32 = 0,
    RenderTextureFormatDepth,
    RenderTextureFormatARGBHalf,
    RenderTextureFormatShadowmap,
    RenderTextureFormatRGB565,
    RenderTextureFormatARGB444,
    RenderTextureFormatARGB1555,
    RenderTextureFormatDefault,  // Default color render texture format: will be
                                 // chosen accordingly to Frame Buffer format
                                 // and Platform.
    RenderTextureFormatARGB2101010,
    RenderTextureFormatDefaultHDR,
    RenderTextureFormatARGB64,
    RenderTextureFormatARGBFloat,
    RenderTextureFormatRGFloat,
    RenderTextureFormatRGHalf,
    RenderTextureFormatRFloat,
    RenderTextureFormatRHalf,
    RenderTextureFormatR8,
    RenderTextureFormatARGBInt,
    RenderTextureFormatRGInt,
    RenderTextureFormatRInt,
    RenderTextureFormatBGRA32,
    RenderTextureFormatRGB111110Float = 22,
    RenderTextureFormatRG32,
    RenderTextureFormatRGBAUShort,
    RenderTextureFormatRG16,
    RenderTextureFormatBGRA10101010_XR,
    RenderTextureFormatBGR101010_XR,
    RenderTextureFormatR16
} RenderTextureFormat;

enum RenderTextureReadWrite {
    RenderTextureReadWriteDefault,
    RenderTextureReadWriteLinear,
    RenderTextureReadWritesRGB,
};

struct Texture {
    AssetID assetID;
    TextureID handle;
    uint32_t width;
    uint32_t height;
    uint32_t mipmaps;
    TextureDimension dimension;
    FilterMode filterMode;
    TextureWrapMode wrapModeU;
    TextureWrapMode wrapModeV;
    TextureWrapMode wrapModeW;
    uint32_t anisoLevel;
};
typedef struct Texture Texture;

void *TextureNew();
void TextureFree(void *);
void TextureSetWrapMode(Texture *t, TextureWrapMode mode);
Texture *TextureFromDDSFile(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* TEXTURE_H */
