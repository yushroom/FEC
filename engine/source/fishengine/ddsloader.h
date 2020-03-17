#ifndef DDSLOADER_H
#define DDSLOADER_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "texture_format.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t mipmaps;
    TextureFormat format;
} TextureDesc;

#ifdef __cplusplus
extern "C" {
#endif

bool ConvertToDDS(const char *path);
uint8_t *loadDDS(const char *path, TextureDesc *desc, uint32_t *byteLength);

#ifdef __cplusplus
}
#endif

#endif /* DDSLOADER_H */
