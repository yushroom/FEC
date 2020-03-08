#include "material.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <set>

#include "asset.h"
#include "material_internal.hpp"
#include "shader.h"
#include "shader_internal.hpp"

struct MaterialPropertyBlock {
    bool IsEmpty() const { return m_FloatPos.empty() && m_Textures.empty(); }
    void Clear();

    // void SetFloat(std::string_view name, float value);
    void SetFloat(int nameID, float value) { SetFloats(nameID, &value, 1); }
    // void SetInt(std::string_view name, int value);
    void SetInt(int nameID, int value) {
        SetFloats(nameID, (float *)&value, 1);
    }
    // void SetVector(std::string_view name, const Vector4& value);
    void SetVector(int nameID, float4 value) {
        SetFloats(nameID, (float *)&value, 4);
    }
    // void SetColor(std::string_view name, const Color& value);
    void SetColor(int nameID, float4 value) {
        SetFloats(nameID, (float *)&value, 4);
    }
    // void SetMatrix(std::string_view name, const Matrix4x4& value);
    void SetMatrix(int nameID, float4x4 value) {
        SetFloats(nameID, (float *)&value, 16);
    }
    // void SetBuffer(std::string_view name, const ComputeBuffer& value);
    // void SetBuffer(int nameID, const ComputeBuffer& value);
    // void SetTexture(std::string_view name, const )
    void SetTexture(int nameID, Texture *value) { m_Textures[nameID] = value; }
    //	void SetFloatArray(int nameID, const std::vector<float>& values) {
    //		SetFloats(nameID, values.data(), (int)values.size());
    //	}
    //	void SetVectorArray(int nameID, const std::vector<Vector4>& values) {
    //		SetFloats(nameID, (float*)values.data(), (int)values.size() *
    // 4);
    //	}
    //	void SetMatrixArray(int nameID, const std::vector<Matrix4x4>& values) {
    //		SetFloats(nameID, (float*)values.data(), (int)values.size() *
    // 16);
    //	}

    float GetFloat(int nameID) {
        float v = 0;
        GetFloats(nameID, &v, 1);
        return v;
    }
    int GetInt(int nameID) {
        int v = 0;
        GetFloats(nameID, (float *)&v, 1);
        return v;
    }
    float4 GetVector(int nameID) {
        float4 v = float4_zero;
        GetFloats(nameID, (float *)&v, 4);
        return v;
    }
    //	Color GetColor(int nameID) {
    //		Color v = Color::black;
    //		GetFloats(nameID, v.data(), 4);
    //		return v;
    //	}
    float4x4 GetMatrix(int nameID) {
        float4x4 mat = float4x4_identity();
        GetFloats(nameID, mat.a, 16);
        return mat;
    }
    Texture *GetTexture(int nameID) {
        auto search = m_Textures.find(nameID);
        if (search == m_Textures.end()) {
            return nullptr;
        } else {
            return search->second;
        }
    }
    std::vector<float> GetFloatArray(int nameID);

    void SetFloats(int nameID, const float *values, int count);
    bool GetFloats(int nameID, float *values, int count);

   private:
   private:
    struct Pos {
        int offset = 0;
        int count = 0;
    };
    std::vector<float> m_Floats;
    std::map<int, Pos> m_FloatPos;
    std::map<int, Texture *> m_Textures;
};

void MaterialPropertyBlock::Clear() {
    m_FloatPos.clear();
    m_Floats.clear();
    m_Textures.clear();
}

void MaterialPropertyBlock::SetFloats(int nameID, const float *values,
                                      int count) {
    assert(values != nullptr && count > 0);
    auto search = m_FloatPos.find(nameID);
    if (search == m_FloatPos.end()) {
        int offset = (int)m_Floats.size();
        m_Floats.resize(m_Floats.size() + count);
        auto &pos = m_FloatPos[nameID];
        pos.offset = offset;
        pos.count = count;
        memcpy(&m_Floats[pos.offset], values, pos.count * sizeof(float));
    } else {
        auto &pos = search->second;
        assert(pos.count == count);
        memcpy(&m_Floats[pos.offset], values, pos.count * sizeof(float));
    }
}

bool MaterialPropertyBlock::GetFloats(int nameID, float *values, int count) {
    assert(values != nullptr && count > 0);
    auto search = m_FloatPos.find(nameID);
    if (search != m_FloatPos.end()) {
        auto &pos = search->second;
        int c = std::min(count, pos.count);
        memcpy(values, &m_Floats[pos.offset], c * sizeof(float));
        return true;
    }
    return false;
}

struct ShaderKeywordCombination {
    uint32_t hash = 0;
    // std::vector<std::pair<std::string, int>> bit;
    std::vector<std::vector<std::string>> multiCompiles;
    std::vector<std::string> keywords;
    //std::vector<std::string> enabledKeywords;
    std::vector<uint32_t> indices;

    ShaderKeywordCombination() = default;

    void SetKeyword(const std::string &keyword, bool enabled) {
        //{
        //    auto it = std::find(enabledKeywords.begin(), enabledKeywords.end(),
        //                        keyword);
        //    if (it != enabledKeywords.end()) return;
        //}
        //enabledKeywords.push_back(keyword);
        {
            int mcIdx = 0;
            for (auto &mcs : multiCompiles) {
                auto it = std::find(mcs.begin(), mcs.end(), keyword);
                if (it != mcs.end()) {
                    if (enabled)
                        indices[mcIdx] = std::distance(mcs.begin(), it);
                    else
                        indices[mcIdx] = 0;
                    break;
                }
                mcIdx++;
            }
        }

        hash = 0;
        uint32_t mcCount = multiCompiles.size();
        for (int mcIdx = mcCount - 1; mcIdx >= 0; --mcIdx) {
            int val = indices[mcIdx];
            hash = hash * multiCompiles[mcIdx].size() + val;
        }
    }

    void Create(const std::vector<std::vector<std::string>> &multiCompiles,
                const std::vector<std::string> &keywords) {
        this->multiCompiles = multiCompiles;
        this->keywords = keywords;
        this->indices.resize(multiCompiles.size(), 0);
    }

    uint32_t GetHashKey() const { return hash; }
};

struct MaterialImpl {
    MaterialPropertyBlock m_PropertyBlock;
    std::vector<uint32_t> shaderPassCache;
    std::vector<ShaderKeywordCombination> keywordForPass;
    std::map<std::string, bool> keywords;
};

void *MaterialNew() {
    Material *mat = (Material *)malloc(sizeof(Material));
    memset(mat, 0, sizeof(Material));
    AssetAdd(AssetTypeMaterial, mat);
    mat->impl = new MaterialImpl();
    return mat;
}

void MaterialFree(void *m) {
    if (m == NULL) return;
    Material *material = (Material *)m;
    MaterialImpl *impl = (MaterialImpl *)material->impl;
    delete impl;
    free(material);
}

void MaterialSetFloat(Material *mat, int nameID, float value) {
    MaterialGetImpl(mat)->m_PropertyBlock.SetFloat(nameID, value);
}

float MaterialGetFloat(Material *mat, int nameID) {
    return MaterialGetImpl(mat)->m_PropertyBlock.GetFloat(nameID);
}

void MaterialSetVector(Material *mat, int nameID, float4 value) {
    MaterialGetImpl(mat)->m_PropertyBlock.SetVector(nameID, value);
}
float4 MaterialGetVector(Material *mat, int nameID) {
    return MaterialGetImpl(mat)->m_PropertyBlock.GetVector(nameID);
}
void MaterialSetTexture(Material *mat, int nameID, Texture *texture) {
    MaterialGetImpl(mat)->m_PropertyBlock.SetTexture(nameID, texture);
}
Texture *MaterialGetTexture(Material *mat, int nameID) {
    return MaterialGetImpl(mat)->m_PropertyBlock.GetTexture(nameID);
}

inline int NextMultipleOf8(int x) { return (x + 7) & (-8); }

void MaterialSetShader(Material *mat, Shader *shader) {
    if (shader == NULL) return;
    mat->shader = shader;
    // ShaderReflect &reflect = ShaderGetReflect(shader);
    ShaderImpl *shaderImpl = ShaderGetImpl(shader);
    MaterialImpl *impl = MaterialGetImpl(mat);
    impl->m_PropertyBlock.Clear();

    for (auto &kw : shaderImpl->keywords) {
        impl->keywords[kw] = false;
    }

    impl->keywordForPass.resize(shaderImpl->passes.size());
    for (int i = 0; i < shader->passCount; ++i) {
        impl->keywordForPass[i].Create(shaderImpl->passes[i].multiCompiles,
                                       shaderImpl->keywords);
    }

    impl->shaderPassCache.resize(shader->passCount);
    for (int i = 0; i < shader->passCount; ++i) {
        impl->shaderPassCache[i] = 0;
    }

    for (auto &pass : shaderImpl->passes) {
        for (auto &m : pass.variants[0].reflect.vs.globals.members) {
            int nameID = ShaderPropertyToID(m.name.c_str());
            if (m.type == "float") {
                impl->m_PropertyBlock.SetFloat(nameID, 0);
            } else if (m.type == "vec3" || m.type == "vec4") {
                impl->m_PropertyBlock.SetVector(nameID, float4_zero);
            }
        }

        for (auto &m : pass.variants[0].reflect.ps.globals.members) {
            int nameID = ShaderPropertyToID(m.name.c_str());
            if (m.type == "float") {
                impl->m_PropertyBlock.SetFloat(nameID, 0);
            } else if (m.type == "vec3" || m.type == "vec4") {
                impl->m_PropertyBlock.SetVector(nameID, float4_zero);
            }
        }
    }
}

bool MaterialIsKeywordEnabled(Material *mat, const char *keyword) {
    if (!mat || !mat->shader) return false;
    MaterialImpl *impl = MaterialGetImpl(mat);
    Shader *shader = mat->shader;
    ShaderImpl *shaderImpl = ShaderGetImpl(shader);

    auto &kws = shaderImpl->keywords;
    bool found = kws.end() != std::find(shaderImpl->keywords.begin(),
                                        shaderImpl->keywords.end(), keyword);
    if (!found) return false;

    return impl->keywords[keyword];
}

void MaterialSetKeyword(Material* mat, const char* keyword, bool enabled) {
    if (!mat || !mat->shader) return;
    MaterialImpl *impl = MaterialGetImpl(mat);
    Shader *shader = mat->shader;
    ShaderImpl *shaderImpl = ShaderGetImpl(shader);

    auto &kws = shaderImpl->keywords;
    bool found = kws.end() != std::find(shaderImpl->keywords.begin(),
                                        shaderImpl->keywords.end(), keyword);
    if (!found) return;

    if (impl->keywords[keyword] == enabled) return;
    impl->keywords[keyword] = enabled;

    for (auto &p : impl->keywordForPass) {
        p.SetKeyword(keyword, enabled);
    }

    for (int passIdx = 0; passIdx < shader->passCount; ++passIdx) {
        impl->shaderPassCache[passIdx] =
            impl->keywordForPass[passIdx].GetHashKey();
    }
}

void MaterialEnableKeyword(Material *mat, const char *keyword) {
    MaterialSetKeyword(mat, keyword, true);
}
void MaterialDisableKeyword(Material *mat, const char *keyword) {
    MaterialSetKeyword(mat, keyword, false);
}

uint32_t MaterialGetVariantIndex(Material *material, uint32_t passIdx) {
    MaterialImpl *impl = MaterialGetImpl(material);
    return impl->shaderPassCache[passIdx];
}
