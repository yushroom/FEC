#include "shader.h"

#include <map>
#include <string>
#include <vector>

#include "asset.h"
#include "shader_reflect.hpp"

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
    s->reflect = new ShaderReflect();
    return s;
}

void ShaderFree(void* s) {
    if (!s) return;
    Shader* shader = (Shader*)s;
    ShaderReflect* r = (ShaderReflect*)shader->reflect;
    delete r;
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

static void LoadShaderReflectItemFromJSON(const std::string& json_path,
                                          ShaderReflectItem& item) {
    rapidjson::Document d;
    auto json = ReadFileAsString(json_path);
    auto& root = d.Parse(json.c_str());
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

Shader* ShaderFromFile(const char* path) {
    Shader* s = ShaderNew();

    fs::path p = path;
    std::string vs_name = p.stem().string() + "_vs";
    std::string ps_name = p.stem().string() + "_ps";
    const std::string vs_path = vs_name + ".reflect.json";
    const std::string ps_path = ps_name + ".reflect.json";

    //    const char* vs_path =
    //        "/Users/yushroom/program/cengine/engine/shaders/runtime/"
    //        "pbrMetallicRoughness_vs.reflect.json";
    //    const char* ps_path =
    //        "/Users/yushroom/program/cengine/engine/shaders/runtime/"
    //        "pbrMetallicRoughness_ps.reflect.json";

    ShaderReflect& reflect = ShaderGetReflect(s);
    LoadShaderReflectItemFromJSON(vs_path, reflect.vs);
    LoadShaderReflectItemFromJSON(ps_path, reflect.ps);

    //	const std::string type_str = "float vec3 vec4";

    for (auto& m : reflect.vs.globals.members) {
        if (m.type == "float") {
            reflect.properties.emplace_back(m.name, ShaderPropertyTypeFloat);
        } else if (m.type == "vec4") {
            reflect.properties.emplace_back(m.name, ShaderPropertyTypeVector);
        }
    }
    for (auto& m : reflect.ps.globals.members) {
        if (m.type == "float") {
            reflect.properties.emplace_back(m.name, ShaderPropertyTypeFloat);
        } else if (m.type == "vec4") {
            reflect.properties.emplace_back(m.name, ShaderPropertyTypeVector);
        }
    }
    for (auto& t : reflect.ps.images) {
        reflect.properties.emplace_back(t.name, ShaderPropertyTypeTexture);
    }

    s->handle = CreateShader(path, vs_name.c_str(), ps_name.c_str());

    return s;
}
