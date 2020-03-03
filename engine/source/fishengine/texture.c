#include "texture.h"

#include <stdlib.h>
#include <string.h>

#include "asset.h"
#include "rhi.h"

void *TextureNew() {
    Texture *t = malloc(sizeof(Texture));
    memset(t, 0, sizeof(*t));
    t->dimension = TextureDimensionUnknown;
    t->filterMode = FilterModeBilinear;
    t->wrapModeU = t->wrapModeV = t->wrapModeW = TextureWrapModeRepeat;
    t->anisoLevel = 1;
    AssetAdd(AssetTypeTexture, t);
    return t;
}

void TextureFree(void *t) {
    if (!t) return;
    Texture *texture = t;
    DeleteTexture(texture->handle);
    free(t);
}

void TextureSetWrapMode(Texture *t, TextureWrapMode mode) {
    t->wrapModeU = t->wrapModeV = t->wrapModeW = mode;
}

#include "ddsloader.h"

Texture *TextureFromDDSFile(const char *path) {
    TextureDesc desc;
    Texture *t = NULL;
    uint32_t byteLength;
    uint8_t *bytes = loadDDS(path, &desc, &byteLength);
    if (!bytes) {
        return NULL;
    }
    Memory m = MemoryMake(bytes, byteLength);
    uint32_t handle = CreateTexture(desc.width, desc.height, desc.mipmaps, m);
    if (handle == 0) {
        goto fail;
    }
    t = TextureNew();
    t->width = desc.width;
    t->height = desc.height;
    t->mipmaps = desc.mipmaps;
    t->handle = handle;
fail:
    free(bytes);
    return t;
}
