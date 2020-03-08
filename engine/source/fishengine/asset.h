#ifndef ASSET_H
#define ASSET_H

#include <stdint.h>
#include <stdlib.h>

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
    _AssetTypeCount
};
typedef enum AssetType AssetType;

AssetID AssetAdd(AssetType type, void *asset);
void *AssetGet(AssetID aid);
void AssetTypeCount(AssetType type);
void *AssetGet2(AssetType type, uint32_t idx);
// void *AssetNew(AssetType type, AssetID *aid);
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
