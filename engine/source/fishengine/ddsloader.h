#ifndef DDSLOADER_H
#define DDSLOADER_H

#include <stdlib.h>
#include <stdint.h>

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t mipmaps;
} TextureDesc;

#ifdef __cplusplus
extern "C" {
#endif

uint8_t *loadDDS(const char *path, TextureDesc *desc, uint32_t *byteLength);

#ifdef __cplusplus
}
#endif

#endif /* DDSLOADER_H */
