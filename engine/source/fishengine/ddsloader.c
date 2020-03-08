#include "ddsloader.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint32_t DWORD;

// https://docs.microsoft.com/en-us/windows/desktop/direct3ddds/dds-pixelformat
typedef struct {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwFourCC;
    DWORD dwRGBBitCount;
    DWORD dwRBitMask;
    DWORD dwGBitMask;
    DWORD dwBBitMask;
    DWORD dwABitMask;
} DDS_PIXELFORMAT;

// https://docs.microsoft.com/en-us/windows/desktop/direct3ddds/dds-header
typedef struct {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwHeight;
    DWORD dwWidth;
    DWORD dwPitchOrLinearSize;
    DWORD dwDepth;
    DWORD dwMipMapCount;
    DWORD dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    DWORD dwCaps;
    DWORD dwCaps2;
    DWORD dwCaps3;
    DWORD dwCaps4;
    DWORD dwReserved2;
} DDS_HEADER;

// https://docs.microsoft.com/en-us/windows/desktop/direct3ddds/dx-graphics-dds-pguide
struct FullHeader {
    uint32_t magic;
    DDS_HEADER header;
};

// static uint32_t MakeFourCC(int a, int b, int c, int d)
//{
//	return (((((d << 8) | c) << 8) | b) << 8) | a;
//}
#define MakeFourCC(a, b, c, d) (((((((d) << 8) | c) << 8) | b) << 8) | a)

uint8_t *loadDDS(const char *path, TextureDesc *desc, uint32_t *byteLength) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

#if WIN32
    uint8_t *bytes = malloc(file_size);
    fread(bytes, file_size, 1, f);
    fclose(f);
    *byteLength = file_size;
    return bytes;
#else
    struct FullHeader header;
    fread(&header, sizeof(header), 1, f);

    const uint32_t dds_magic = 0x20534444;  // DDS
    assert(dds_magic == header.magic);
    assert(124 == header.header.dwSize);

    switch (header.header.ddspf.dwFourCC) {
        case MakeFourCC('D', 'X', 'T', '1'):
            //			puts("DXT1");
            break;
        case MakeFourCC('D', 'X', 'T', '2'):
        case MakeFourCC('D', 'X', 'T', '3'):
        case MakeFourCC('D', 'X', 'T', '4'):
        case MakeFourCC('D', 'X', 'T', '5'):
        case MakeFourCC('D', 'X', '1', '0'):
        default:
            puts("[dds loader] only support DXT1");
            abort();
    }

    size_t size = file_size - sizeof(header);
    uint8_t *bytes = malloc(size);
    fread(bytes, size, 1, f);
    fclose(f);

    desc->width = header.header.dwWidth;
    desc->height = header.header.dwHeight;
    desc->mipmaps = header.header.dwMipMapCount;

    *byteLength = size;
    return bytes;
#endif
}
