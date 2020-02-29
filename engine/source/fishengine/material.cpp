#include "material.h"

#include <map>
#include <string>
#include <vector>
#include <algorithm>

#include "asset.h"
#include "shader.h"
#include "shader_reflect.hpp"

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
    // int count;
    uint32_t hash = 0;
    std::vector<std::pair<std::string, int>> bit;

    ShaderKeywordCombination() = default;

    ShaderKeywordCombination(const ShaderKeywordCombination &b) {
        hash = b.hash;
        bit = b.bit;
    }

    ShaderKeywordCombination &operator=(const ShaderKeywordCombination &b) {
        hash = b.hash;
        bit = b.bit;
        return *this;
    }

    void SetKeyword(const std::string &keyword, bool enabled) {
        for (auto &[k, v] : bit) {
            if (keyword == k) {
                v = enabled ? 1 : 0;
                return;
            }
        }
        assert(false && "keyword not found");
    }

    void Create(const std::vector<std::string> &keywords) {
        assert(keywords.size() < 32);
        bit.clear();
        for (size_t count = 0; count < keywords.size(); ++count) {
            bit.emplace_back(keywords[count], 0);
        }
    }

    uint32_t GetHashKey() const {
        uint32_t key = 0;
        for (auto [k, v] : bit) {
            key <<= 1;
            key |= (v & 1);
        }
        return key;
    }
};

struct MaterialImpl {
    ShaderKeywordCombination m_Keywords;
    MaterialPropertyBlock m_PropertyBlock;
    std::vector<uint8_t> m_VSGlobals;
    std::vector<uint8_t> m_PSGlobals;
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

static MaterialImpl *MaterialGetImpl(Material *mat) {
    return (MaterialImpl *)mat->impl;
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
    mat->shader = shader;
    ShaderReflect &reflect = ShaderGetReflect(shader);
    MaterialImpl *impl = MaterialGetImpl(mat);
    impl->m_VSGlobals.resize(NextMultipleOf8(reflect.vs.globals.block_size));
    impl->m_PSGlobals.resize(NextMultipleOf8(reflect.ps.globals.block_size));
    impl->m_PropertyBlock.Clear();

    for (auto &m : reflect.vs.globals.members) {
        int nameID = ShaderPropertyToID(m.name.c_str());
        if (m.type == "float") {
            impl->m_PropertyBlock.SetFloat(nameID, 0);
        } else if (m.type == "vec3" || m.type == "vec4") {
            impl->m_PropertyBlock.SetVector(nameID, float4_zero);
        }
    }

    for (auto &m : reflect.ps.globals.members) {
        int nameID = ShaderPropertyToID(m.name.c_str());
        if (m.type == "float") {
            impl->m_PropertyBlock.SetFloat(nameID, 0);
        } else if (m.type == "vec3" || m.type == "vec4") {
            impl->m_PropertyBlock.SetVector(nameID, float4_zero);
        }
    }
}

Memory MaterialBuildGloblsCB(Material *mat) {
    ShaderReflect &reflect = ShaderGetReflect(mat->shader);
    MaterialImpl *impl = MaterialGetImpl(mat);
    Memory mem;
    mem.buffer = impl->m_PSGlobals.data();
    mem.byteLength = impl->m_PSGlobals.size();

    for (auto &m : reflect.ps.globals.members) {
        if (m.type == "float") {
            float v = MaterialGetFloat(mat, m.nameID);
            float *ptr = (float *)((uint8_t *)mem.buffer + m.offset);
            *ptr = v;
        } else if (m.type == "vec2" || m.type == "vec3" || m.type == "vec4") {
            float4 v = MaterialGetVector(mat, m.nameID);
            memcpy((uint8_t *)mem.buffer + m.offset, &v, m.bytes);
        }
    }

    return mem;
}
