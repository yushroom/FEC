#ifndef SHADER_REFLECT_HPP
#define SHADER_REFLECT_HPP

#include <string>
#include <vector>
#include <map>

#include "shader.h"

struct ShaderReflectCBType {
    struct Member {
        std::string name;
        int nameID = 0;
        std::string type;
        uint32_t offset = 0;

        uint32_t bytes = 0;
        bool used = true;
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

    ShaderProperty() = default;
    ShaderProperty(const std::string& name, ShaderPropertyType type)
        : name(name), type(type) {}
};

struct ShaderReflect {
    ShaderReflectItem vs;
    ShaderReflectItem ps;
};

struct ShaderVariant {
    ShaderHandle vertexShader = 0;
    ShaderHandle pixelShader = 0;
    ShaderReflect reflect;
};

struct ShaderPass {
    std::vector<std::vector<std::string>> multiCompiles;
    std::vector<std::string> shaderFeatures;
    std::vector<ShaderVariant> variants;
};

struct ShaderImpl {
    std::vector<ShaderProperty> properties;
    std::vector<ShaderPass> passes;
    std::vector<std::string> keywords;
};

inline ShaderImpl* ShaderGetImpl(Shader* s) {
    return (ShaderImpl*)s->impl;
}

inline const std::vector<std::string>& ShaderGetKeywords(Shader *s) {
    return ShaderGetImpl(s)->keywords;
}

#endif /* SHADER_REFLECT_HPP */
