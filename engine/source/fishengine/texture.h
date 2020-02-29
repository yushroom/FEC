#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdint.h>

typedef uint32_t TextureID;

enum FilterMode {
    FilterModePoint = 0,
    FilterModeBilinear,
    FilterModeTrilinear,
    _FilterModeCount,
};

enum TextureWrapMode {
    TextureWrapModeRepeat = 0,
    TextureWrapModeClamp,
    TextureWrapModeMirror,
    TextureWrapModeMirrorOnce,
    _TextureWrapModeCount,
};

enum TextureDimension {
    TextureDimensionUnknown = -1,
    TextureDimensionNone,
    TextureDimensionAny,
    TextureDimensionTex2D,
    TextureDimensionTex3D,
    TextureDimensionCube,
    TextureDimensionCubeArray,
};

enum TextureFormat {
    TextureFormatInvalid,
    TextureFormatAlpha8,
    TextureFormatBGRA8Unorm,
    TextureFormatBGRA8Unorm_sRGB,
    TextureFormatRGBA8Unorm,
    TextureFormatRGBA8Unorm_sRGB,
    // Depth32Float,
    TextureFormatR32Float,

    // DXT1 format compresses textures to 4 bits per pixel
    // It is a good format to compress most of RGB textures.For RGBA(with alpha)
    // textures, use DXT5.
    TextureFormatDXT1,

    // Compressed color with alpha channel texture format.
    // DXT5 format compresses textures to 8 bits per pixel, and is widely
    // supported on PC, console and Windows Phone platforms. It is a good format
    // to compress most of RGBA textures.For RGB(without alpha) textures, DXT1
    // is better.When targeting DX11 - class hardware(modern PC, PS4, XboxOne),
    // using BC7 might be useful, since compression quality is often better.
    TextureFormatDXT5,
};

enum RenderTextureFormat {
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
};

enum RenderTextureReadWrite {
    RenderTextureReadWriteDefault,
    RenderTextureReadWriteLinear,
    RenderTextureReadWritesRGB,
};

struct Texture {
    TextureID handle;
    uint32_t width;
    uint32_t height;
    uint32_t mipmaps;
    enum TextureDimension dimension;
    enum FilterMode filterMode;
    enum TextureWrapMode wrapModeU;
    enum TextureWrapMode wrapModeV;
    enum TextureWrapMode wrapModeW;
    uint32_t anisoLevel;
};
typedef struct Texture Texture;

#ifdef __cplusplus
extern "C" {
#endif

void *TextureNew();
void TextureFree(void *);
void TextureSetWrapMode(Texture *t, enum TextureWrapMode mode);
Texture *TextureFromDDSFile(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* TEXTURE_H */
