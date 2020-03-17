#ifndef TEXTURE_FORMAT_H
#define TEXTURE_FORMAT_H

typedef enum {
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
} TextureFormat;

#endif /* TEXTURE_FORMAT_H */