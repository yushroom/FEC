#include "shader.h"

#include <map>
#include <string>
#include <vector>

#include "asset.h"
#include "shader_internal.hpp"
#include "shader_util.h"
#include <fmt/format.h>

class NonCopyable {
   public:
    NonCopyable() = default;
    NonCopyable(NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = delete;
    NonCopyable& operator=(NonCopyable&) = delete;
};

class Singleton : public NonCopyable {
   protected:
    Singleton() = default;
};

class Static {
   public:
    Static() = delete;
};

struct PropertyHash : public Singleton {
    std::map<std::string, int> name2ID;
    std::vector<std::string> ID2Name;

    PropertyHash() {
        name2ID[""] = 0;
        ID2Name.reserve(32);
        ID2Name.push_back("");
    }

    static PropertyHash& GetInstance() {
        static PropertyHash inst;
        return inst;
    }

    int operator[](const std::string& name) {
        auto search = name2ID.find(name);
        if (search != name2ID.end()) return search->second;
        int nextID = (int)ID2Name.size();
        int ret = nextID;
        name2ID[name] = ret;
        ID2Name.push_back(name);
        return ret;
    }

    std::string GetName(int nameID) {
        if (nameID < 0 || nameID >= (int)ID2Name.size()) return "";
        return ID2Name[nameID];
    }
};

int ShaderPropertyToID(const char* name) {
    return PropertyHash::GetInstance()[name];
}

Shader* ShaderNew() {
    Shader* s = (Shader*)malloc(sizeof(Shader));
    memset(s, 0, sizeof(Shader));
    AssetAdd(AssetTypeShader, s);
    s->impl = new ShaderImpl();
    return s;
}

void ShaderFree(void* s) {
    if (!s) return;
    Shader* shader = (Shader*)s;
    auto impl = (ShaderImpl*)shader->impl;
    delete impl;
    free(s);
}

#include <rapidjson/document.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
namespace fs = std::filesystem;

std::string ReadFileAsString(const std::string& path) {
    assert(fs::exists(path));
    std::ifstream f(path);
    if (f) {
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }
    return "";
}

bool ReadBinaryFile(const std::string& path, std::vector<char>& bin) {
    assert(fs::exists(path));
    std::ifstream input(path.c_str(), std::ios::binary);
    assert(input.is_open());
    input.seekg(0, std::ios::end);
    size_t size = input.tellg();
    input.seekg(0, std::ios::beg);
    bin.resize(size);
    input.read(bin.data(), size);
    input.close();
    return true;
}

static void LoadShaderReflectItemFromMemory(Memory json,
                                            ShaderReflectItem& item) {
    rapidjson::Document d;
    const char* str = (const char*)json.buffer;
    auto& root = d.Parse(str, json.byteLength);
    std::string globals_type;
    uint32_t mask = 0;
    if (root.HasMember("ubos")) {
        auto ubos = root["ubos"].GetArray();
        item.ubos.reserve(ubos.Size());
        for (auto& ubo : ubos) {
            ShaderReflectUBO u;
            u.type = ubo["type"].GetString();
            u.name = ubo["name"].GetString();
            u.block_size = ubo["block_size"].GetUint();
            u.set = ubo["set"].GetUint();
            u.binding = ubo["binding"].GetUint();
            mask |= (1 << u.binding);
            item.ubos.push_back(u);
            if (u.name == "type__Globals" || u.name == "$Globals") {
                globals_type = u.type;
                item.globals.block_size = u.block_size;
            }
        }
    }
    if (root.HasMember("separate_images")) {
        for (auto& image : root["separate_images"].GetArray()) {
            ShaderReflectTexture t;
            t.type = image["type"].GetString();
            t.name = image["name"].GetString();
            t.set = image["set"].GetUint();
            t.binding = image["binding"].GetUint();
            t.nameID = ShaderPropertyToID(t.name.c_str());
            item.images.push_back(t);
        }
    }
    if (root.HasMember("separate_samplers")) {
        for (auto& image : root["separate_samplers"].GetArray()) {
            ShaderReflectTexture t;
            t.type = image["type"].GetString();
            t.name = image["name"].GetString();
            t.set = image["set"].GetUint();
            t.binding = image["binding"].GetUint();
            t.nameID = ShaderPropertyToID(t.name.c_str());
            item.samplers.push_back(t);
        }
    }
    item.bindingMask = mask;
    if (globals_type != "") {
        auto members =
            root["types"][globals_type.c_str()]["members"].GetArray();
        item.globals.members.reserve(members.Size());
        for (auto& m : members) {
            ShaderReflectCBType::Member mem;
            mem.name = m["name"].GetString();
            mem.type = m["type"].GetString();
            if (mem.type == "float") {
                mem.bytes = 4;
            } else if (mem.type == "vec2") {
                mem.bytes = 8;
            } else if (mem.type == "vec3") {
                mem.bytes = 12;
            } else if (mem.type == "vec4") {
                mem.bytes = 16;
            }
            mem.nameID = ShaderPropertyToID(mem.name.c_str());
            mem.offset = m["offset"].GetUint();
            item.globals.members.push_back(mem);
        }
    }
}

static void LoadShaderReflectItemFromFile(const std::string& json_path,
                                          ShaderReflectItem& item) {
    auto json = ReadFileAsString(json_path);
    LoadShaderReflectItemFromMemory(
        MemoryMake((void*)json.c_str(), json.size()), item);
}

ShaderHandle CreateShaderFromCompiledFile(const char* path);

Shader* ShaderFromFile(const char* path) {
    Shader* s = ShaderNew();
    ShaderImpl* impl = (ShaderImpl*)s->impl;

    {
        rapidjson::Document d;
        std::string json_path = path;
        json_path += ".json";
        std::string str = ReadFileAsString(json_path);
        auto &root = d.Parse(str.c_str(), str.length());
        for (auto& kw : root["keywords"].GetArray()) {
            impl->keywords.push_back(kw.GetString());
        }

        impl->properties.reserve(root["properties"].GetArray().Size());
        for (auto& p : root["properties"].GetArray()) {
            auto& _p = impl->properties.emplace_back();
            _p.name = p["name"].GetString();
            std::string type = p["type"].GetString();
            if (type == "Color") {
                _p.type = ShaderPropertyTypeVector;
            } else if (type == "Range") {
                _p.type = ShaderPropertyTypeFloat;
            } else if (type == "2D") {
                _p.type = ShaderPropertyTypeTexture;
            }
        }
        
        int passIdx = 0;
        s->passCount = root["passes"].GetArray().Size();
        for (auto& p : root["passes"].GetArray()) {
            auto& sp = impl->passes.emplace_back();
            uint32_t variantCount = 1;
            for (auto& mc : p["multi_compiles"].GetArray()) {
                auto& _mc = sp.multiCompiles.emplace_back();
                for (auto& x : mc.GetArray()) {
                    _mc.push_back(x.GetString());
                }
                variantCount *= mc.GetArray().Size();
            }

            sp.variants.resize(variantCount);
            for (int i = 0; i < variantCount; ++i) {
                auto& var = sp.variants[i];
                std::string prefix = fmt::format("{}_{}_{}", path, passIdx, i);
                std::string path = prefix + "_vs.cso";
                var.vertexShader = CreateShaderFromCompiledFile(path.c_str());
                path = prefix + "_ps.cso";
                var.pixelShader = CreateShaderFromCompiledFile(path.c_str());
                auto& reflect = var.reflect;
                LoadShaderReflectItemFromFile(prefix + "_vs.reflect.json",
                                              reflect.vs);
                LoadShaderReflectItemFromFile(prefix + "_ps.reflect.json",
                                              reflect.ps);
            }

            passIdx++;
        }
    }

    return s;
}

int ShaderUtilGetPropertyCount(Shader* s) {
    return (int)ShaderGetImpl(s)->properties.size();
}

const char* ShaderUtilGetPropertyName(Shader* s, int propertyIdx) {
    assert(propertyIdx >= 0 && propertyIdx < ShaderUtilGetPropertyCount(s));
    return ShaderGetImpl(s)->properties[propertyIdx].name.c_str();
}

enum ShaderPropertyType ShaderUtilGetPropertyType(Shader* s, int propertyIdx) {
    assert(propertyIdx >= 0 && propertyIdx < ShaderUtilGetPropertyCount(s));
    return ShaderGetImpl(s)->properties[propertyIdx].type;
}
