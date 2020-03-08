#include "asset.h"

#include <map>

#include "animation.h"
#include "material.h"
#include "mesh.h"
#include "texture.h"

typedef void *AssetNewFunc();
typedef void AssetFreeFunc(void *);

struct AssetClassDef {
    AssetFreeFunc *freeFunc;
};

static struct AssetClassDef defs[_AssetTypeCount];

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
        defs[AssetTypeTexture] = {TextureFree};
        defs[AssetTypeShader] = {ShaderFree};
        defs[AssetTypeMaterial] = {MaterialFree};
        defs[AssetTypeMesh] = {MeshFree};
        defs[AssetTypeSkin] = {SkinFree};
        defs[AssetTypeAnimationClip] = {AnimationClipFree};
    }
    AssetManagerImpl(AssetManagerImpl &) = delete;
};

#define g AssetManagerImpl::GetInstance()

AssetID AssetAdd(AssetType type, void *asset) { return g.Add(type, asset); }
void *AssetGet(AssetID aid) { return g.Get(aid); }
void AssetDelete(AssetID aid) { g.Delete(aid); }
void AssetDeleteAll() { g.DeleteAll(); }


#include <microtar.h>

bool string_end_with(const char *str, const char *end) {
    if (!str || !end) return false;
    size_t len1 = strlen(str);
    size_t len2 = strlen(end);
    if (len1 < len2) return false;
    return (strcmp(str + len1 - len2, end) == 0);
}

void string_combine(char *dst, const char *src1, const char *src2) {
    dst[0] = '\0';
    strcat(dst, src1);
    strcat(dst, src2);
}

void string_combine1(char *dst, const char *src1, size_t src1_len,
                     const char *src2) {
    dst[0] = '\0';
    src1_len = fmin(src1_len, strlen(src1));
    strncat(dst, src1, src1_len);
    strcat(dst, src2);
}

#include <cstdio>
#include <string>
#include <vector>

#include "shader.h"

bool ShaderDescFromTAR(const char *name_prefix, mtar_t *tar,
                       ShaderDesc *out_desc) {
    char name[512];
    mtar_header_t h1, h2;
    void *p1 = NULL;
    char *p2 = NULL;

    string_combine(name, name_prefix, ".cso");
    int ret = mtar_find(tar, name, &h1);
    if (ret) return false;
    p1 = malloc(h1.size);
    ret = mtar_read_data(tar, p1, h1.size);
    if (ret) goto FAIL;

    string_combine(name, name_prefix, ".reflect.json");
    ret = mtar_find(tar, name, &h2);
    if (ret) goto FAIL;
    p2 = (char *)malloc(h2.size + 1);
    p2[h2.size] = '\0';
    ret = mtar_read_data(tar, p2, h2.size);
    if (ret) goto FAIL;

    // puts((char *)p2);

    out_desc->bytes.byteLength = h1.size;
    out_desc->bytes.buffer = p1;
    out_desc->reflectJson.byteLength = h2.size;
    out_desc->reflectJson.buffer = p2;
    return true;
FAIL:
    free(p1);
    free(p2);
    return false;
}

void LoadTAR(const char *path) {
    char tarPath[512];
    tarPath[0] = '\0';
    strcat(tarPath, path);
    strcat(tarPath, "/shaders.tar");

    std::vector<std::string> hlsls;

    mtar_t tar;
    mtar_header_t h;
    mtar_open(&tar, tarPath, "r");
    while ((mtar_read_header(&tar, &h)) != MTAR_ENULLRECORD) {
        // printf("%s (%d bytes)\n", h.name, h.size);
        if (string_end_with(h.name, ".hlsl")) hlsls.push_back(h.name);
        mtar_next(&tar);
    }

    char name[512];
    for (auto &_h_name : hlsls) {
        const char *h_name = _h_name.c_str();
        size_t pos = strlen(h_name) - strlen(".hlsl");
        ShaderDesc vs, ps;
        string_combine1(name, h_name, pos, "_vs");
        bool ret = ShaderDescFromTAR(name, &tar, &vs);
        if (ret) {
            string_combine1(name, h_name, pos, "_ps");
            ret = ShaderDescFromTAR(name, &tar, &ps);
        }
        if (ret) {
            //ShaderFromMemory(&vs, &ps);
        }
    }
    mtar_close(&tar);
}

struct AssetBundleImpl {
    std::map<int, AssetWrap> assets;
};

AssetBundle *AssetBundleNew(const char *path) {
    AssetBundle *ab = (AssetBundle *)malloc(sizeof(AssetBundle));
    ab->impl = new AssetBundleImpl();
    return ab;
}
void AssetbundleFree(AssetBundle *ab) {
    if (ab == NULL) return;
    auto impl = (AssetBundleImpl *)ab->impl;
    delete impl;
    free(ab);
}