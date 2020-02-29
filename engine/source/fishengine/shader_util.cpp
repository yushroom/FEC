#include "shader_util.h"

#include "shader_reflect.hpp"

int ShaderUtilGetPropertyCount(Shader *s) {
    //	ShaderReflect& r = ShaderGetReflect(s);
    //	size_t count = r.vs.globals.members.size() +
    // r.ps.globals.members.size(); 	return (int)count;
    return (int)ShaderGetReflect(s).properties.size();
}

const char *ShaderUtilGetPropertyName(Shader *s, int propertyIdx) {
    ShaderReflect &r = ShaderGetReflect(s);
    assert(propertyIdx >= 0 && propertyIdx < ShaderUtilGetPropertyCount(s));
    return r.properties[propertyIdx].name.c_str();
}

enum ShaderPropertyType ShaderUtilGetPropertyType(Shader *s, int propertyIdx) {
    ShaderReflect &r = ShaderGetReflect(s);
    assert(propertyIdx >= 0 && propertyIdx < ShaderUtilGetPropertyCount(s));
    return r.properties[propertyIdx].type;
}
