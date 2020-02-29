#include "asset.h"

#include <map>

#include "animation.h"
#include "material.h"
#include "mesh.h"
#include "texture.h"

typedef void *AssetNewFunc();
typedef void AssetFreeFunc(void *);

struct AssetClassDef {
    AssetNewFunc *newFunc;
    AssetFreeFunc *freeFunc;
};

static struct AssetClassDef defs[_AssetTypeCount];
//= {
//	{MeshNew, MeshFree},
//	{MaterialNew, MaterialFree},
//};

void AssetManagerInit() {}

struct AssetWrap {
    void *ptr = NULL;
    AssetType type = AssetTypeUnknown;
    AssetID id = 0;

    AssetWrap() = default;
    AssetWrap(void *ptr, AssetType type, AssetID id)
        : ptr(ptr), type(type), id(id) {}
};

struct AssetManagerImpl {
   public:
    static AssetManagerImpl &GetInstance() {
        static AssetManagerImpl inst;
        return inst;
    }

    AssetID Add(AssetType type, void *ptr) {
        AssetID id = nextAssetID++;
        auto &a = assets[id];
        a.ptr = ptr;
        a.type = type;
        a.id = id;
        return id;
    }

    void *Get(AssetID id) {
        auto it = assets.find(id);
        if (it != assets.end()) {
            return it->second.ptr;
        }
        return NULL;
    }

    uint32_t GetCount(AssetType type) { return 0; }

    std::pair<void *, AssetID> New(AssetType type) {
        void *asset = defs[type].newFunc();
        AssetID id = Add(type, asset);
        return std::make_pair(asset, id);
    }

    void Delete(AssetID id) {
        auto it = assets.find(id);
        if (it != assets.end()) {
            void *asset = it->second.ptr;
            assets.erase(it);
            defs[it->second.type].freeFunc(asset);
        }
    }

    void DeleteNoErase(AssetID id) {
        auto it = assets.find(id);
        if (it != assets.end()) {
            void *asset = it->second.ptr;
            defs[it->second.type].freeFunc(asset);
            it->second.ptr = NULL;
        }
    }

    void DeleteAll() {
        for (int i = 0; i < nextAssetID; ++i) {
            DeleteNoErase(i);
        }
        assets.clear();
        nextAssetID = 1;
    }

    ~AssetManagerImpl() { DeleteAll(); }

   private:
    std::map<AssetID, AssetWrap> assets;
    AssetID nextAssetID = 1;

   private:
    AssetManagerImpl() {
        defs[AssetTypeTexture] = {TextureNew, TextureFree};
        defs[AssetTypeShader] = {NULL, ShaderFree};
        defs[AssetTypeMaterial] = {NULL, MaterialFree};
        defs[AssetTypeMesh] = {MeshNew, MeshFree};
        defs[AssetTypeSkin] = {NULL, SkinFree};
        defs[AssetTypeAnimationClip] = {NULL, AnimationClipFree};
    }
    AssetManagerImpl(AssetManagerImpl &) = delete;
};

#define g AssetManagerImpl::GetInstance()

AssetID AssetAdd(AssetType type, void *asset) { return g.Add(type, asset); }
void *AssetGet(AssetID aid) { return g.Get(aid); }
// void *AssetNew(AssetType type, AssetID *aid) {
//    auto ret = g.New(type);
//    if (aid) *aid = ret.second;
//    return ret.first;
//}
// void AssetTypeCount(AssetType type) { return g.ge}
// void *AssetGet2(AssetType type, uint32_t idx);
void AssetDelete(AssetID aid) { g.Delete(aid); }
void AssetDeleteAll() { g.DeleteAll(); }
