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
    t->assetID = AssetAdd(AssetTypeTexture, t);
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

#if APPLE

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

#else
#include "ddsloader.h"

Texture *TextureFromDDSFile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t byteLength = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *bytes = malloc(byteLength);
    fread(bytes, byteLength, 1, f);
    fclose(f);

    if (!bytes) {
        return NULL;
    }

    Texture *t = NULL;
    TextureDesc desc;
    Memory m = MemoryMake(bytes, byteLength);
    uint32_t handle = CreateTexture(m, &desc);
    if (handle == 0) {
        goto fail;
    }
    t = TextureNew();
    t->width = desc.width;
    t->height = desc.height;
    t->mipmaps = desc.mipmaps;
    t->handle = handle;

    Asset *a = AssetGet(t->assetID);
    a->fromFile = true;
    strncat(a->filePath, path, min(countof(a->filePath), strlen(path)));

fail:
    free(bytes);
    return t;
}

#endif