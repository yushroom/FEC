#ifndef ASSET_H
#define ASSET_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t AssetID;

enum AssetType {
    AssetTypeUnknown = 0,
    AssetTypeTexture,
    AssetTypeShader,
    AssetTypeMaterial,
    AssetTypeMesh,
    AssetTypeSkin,
    AssetTypeAnimationClip,
    AssetScript,
    _AssetTypeCount
};
typedef enum AssetType AssetType;

typedef struct Asset {
    AssetType type;
    AssetID id;
    bool fromFile;
    char filePath[128];
    void *ptr;
} Asset;

AssetID AssetAdd(AssetType type, void *asset);
Asset *AssetGet(AssetID aid);
uint32_t AssetTypeCount(AssetType type);
Asset *AssetGet2(AssetType type, uint32_t idx);
void AssetDelete(AssetID aid);
void AssetDeleteAll();

typedef struct {
    const char name[32];
    void *impl;
} AssetBundle;

AssetBundle *AssetBundleNew(const char *path);
void AssetbundleFree(AssetBundle *ab);

#ifdef __cplusplus
}
#endif

#endif /* ASSET_H */
