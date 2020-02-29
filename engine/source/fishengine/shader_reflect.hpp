#ifndef SHADER_REFLECT_HPP
#define SHADER_REFLECT_HPP

#include <string>
#include <vector>

#include "shader.h"

struct ShaderReflectCBType {
    struct Member {
        std::string name;
        int nameID = 0;
        std::string type;
        uint32_t offset = 0;

        uint32_t bytes = 0;
    };

    std::string name;
    std::vector<Member> members;
    uint32_t block_size = 0;
};

struct ShaderReflectUBO {
    std::string type;
    std::string name;
    uint32_t block_size = 0;
    uint32_t set = 0;
    uint32_t binding = 0;
};

struct ShaderReflectTexture {
    std::string type;
    std::string name;
    uint32_t set = 0;
    uint32_t binding = 0;
    uint32_t nameID = 0;
};

struct ShaderReflectItem {
    ShaderReflectCBType globals;
    std::vector<ShaderReflectUBO> ubos;
    std::vector<ShaderReflectTexture> images;
    std::vector<ShaderReflectTexture> samplers;
    uint32_t bindingMask = 0;
};

struct ShaderProperty {
    std::string name;
    ShaderPropertyType type;

    ShaderProperty(const std::string& name, ShaderPropertyType type)
        : name(name), type(type) {}
};

struct ShaderReflect {
    ShaderReflectItem vs;
    ShaderReflectItem ps;
    std::vector<ShaderProperty> properties;
};

inline ShaderReflect& ShaderGetReflect(Shader* s) {
    return *(ShaderReflect*)s->reflect;
}

#endif /* SHADER_REFLECT_HPP */
