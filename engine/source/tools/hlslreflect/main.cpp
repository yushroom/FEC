#include <vector>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <system_error>
#include <filesystem>
namespace fs = std::filesystem;

#include <fmt/format.h>
#include <rapidjson/prettywriter.h>

enum ShaderType {
    ShaderTypeVertex = 0,
    ShaderTypePixel,
    ShaderTypeCompute,
    ShaderTypeGeometry,
    ShaderTypeHull,
    ShaderTypeDomain,
};

struct ShaderReflectType {
    struct Member {
        std::string name;
        std::string type;
        uint32_t offset = 0;

        bool used = true;
    };

    std::string name;
    uint32_t block_size = 0;
    std::vector<Member> members;
};

struct ShaderReflectBindInfo {
    std::string type;
    std::string name;
    uint32_t set = 0;
    uint32_t binding = 0;
};

struct ShaderReflectCBufferBindInfo : public ShaderReflectBindInfo {
    uint32_t block_size = 0;
};

struct ShaderReflectShader {
    std::vector<ShaderReflectType> types;
    std::vector<ShaderReflectCBufferBindInfo> ubos;
    std::vector<ShaderReflectBindInfo> images;
    std::vector<ShaderReflectBindInfo> samplers;
};

struct ShaderReflect {
    ShaderReflectShader vs;
    ShaderReflectShader ps;
};



static void ThrowIfFailed(HRESULT hr) {
    if (FAILED(hr)) {
        std::string message = std::system_category().message(hr);
        printf("Error: %s", message.c_str());
        throw std::exception();
    }
}

inline std::wstring ConvertString(const std::string& s) {
    constexpr size_t WCHARBUF = MAX_PATH;
    wchar_t wszDest[WCHARBUF];
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, s.c_str(), -1, wszDest,
        WCHARBUF);
    return std::wstring(wszDest);
}

#define LogInfo fmt::print
#define LogWarnning fmt::print

static ID3D10Blob *CreateShaderFromCompiledFile(const char* path) {
    if (!fs::exists(path)) {
        return NULL;
    }
    //fs::path p(path);
    //std::string path2 = p.lexically_normal().string();
    //assert(fs::exists(path));
    //LogWarnning("Compiling Shader: {}\n", path);
    ID3DBlob* shaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(ConvertString(path).c_str(), &shaderBlob));
    return shaderBlob;
}

static ShaderReflectShader ReflectShader(ID3D10Blob* shader, ShaderType shaderType) {
#ifdef _DEBUG
    constexpr bool printMsg = true;
#else
    constexpr bool printMsg = false;
#endif
    ComPtr<ID3D12ShaderReflection> pReflector;
    HRESULT hr = D3DReflect(shader->GetBufferPointer(), shader->GetBufferSize(),
        IID_PPV_ARGS(&pReflector));
    ThrowIfFailed(hr);

    if (shaderType == ShaderTypeCompute) {
        UINT x, y, z;
        pReflector->GetThreadGroupSize(&x, &y, &z);
    }

    ShaderReflectShader _shader;

    D3D12_SHADER_DESC desc;
    ThrowIfFailed(pReflector->GetDesc(&desc));

    if constexpr (printMsg)
        LogInfo(">>>Creator: {} {}\n", desc.Creator, desc.Version);

    if constexpr (printMsg) LogInfo(">>>Input:\n");
    for (UINT i = 0; i < desc.InputParameters; ++i) {
        D3D12_SIGNATURE_PARAMETER_DESC spd;
        pReflector->GetInputParameterDesc(i, &spd);
        if constexpr (printMsg)
            LogInfo("\t{}: {}{}, vt {}, ct {}, mask {}, reg {}\n", i,
                spd.SemanticName, spd.SemanticIndex, spd.SystemValueType,
                spd.ComponentType, spd.Mask, spd.Register);
    }

    if constexpr (printMsg) LogInfo(">>>Output:\n");
    for (UINT i = 0; i < desc.OutputParameters; ++i) {
        D3D12_SIGNATURE_PARAMETER_DESC spd;
        pReflector->GetOutputParameterDesc(i, &spd);
        if constexpr (printMsg)
            LogInfo("\t{}: {}{}, {}, {}\n", i, spd.SemanticName,
                spd.SemanticIndex, spd.SystemValueType, spd.ComponentType);
    }

    if constexpr (printMsg)
        LogInfo(">>>Constant buffers: count={}\n", desc.ConstantBuffers);
    for (UINT i = 0; i < desc.ConstantBuffers; ++i) {
        ID3D12ShaderReflectionConstantBuffer* cbuffer =
            pReflector->GetConstantBufferByIndex(i);
        D3D12_SHADER_BUFFER_DESC bufferDesc;
        ThrowIfFailed(cbuffer->GetDesc(&bufferDesc));
        if constexpr (printMsg) {
            LogInfo("  Constant buffer {} {}, {}, vars {}, size {}\n", i,
                bufferDesc.Name, bufferDesc.Type, bufferDesc.Variables,
                bufferDesc.Size);
        }

        D3D12_SHADER_INPUT_BIND_DESC bindDesc;
        hr = pReflector->GetResourceBindingDescByName(bufferDesc.Name,
            &bindDesc);
        ThrowIfFailed(hr);

        ShaderReflectType _type;
        _type.name = bindDesc.Name;
        _type.block_size = bufferDesc.Size;

        for (uint32_t j = 0; j < bufferDesc.Variables; ++j) {
            ID3D12ShaderReflectionVariable* var =
                cbuffer->GetVariableByIndex(j);
            ID3D12ShaderReflectionType* type = var->GetType();
            D3D12_SHADER_VARIABLE_DESC varDesc;
            hr = var->GetDesc(&varDesc);
            ThrowIfFailed(hr);

            D3D12_SHADER_TYPE_DESC typeDesc;
            hr = type->GetDesc(&typeDesc);
            ThrowIfFailed(hr);

            bool used = 0 != (varDesc.uFlags & D3D_SVF_USED);
            if constexpr (printMsg) {
                LogInfo("\t{}({}), {}, size {}, Elements {}, flags {}, {}\n",
                    varDesc.Name, typeDesc.Name, varDesc.StartOffset,
                    varDesc.Size, typeDesc.Elements, varDesc.uFlags,
                    used ? "used" : "unused");
            }

            ShaderReflectType::Member _member;
            _member.name = varDesc.Name;
            _member.type = typeDesc.Name;
            _member.offset = varDesc.StartOffset;
            _member.used = used;
            _type.members.push_back(_member);
        }

        _shader.types.push_back(_type);
    }

    for (uint32_t i = 0; i < desc.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC bindDesc;
        hr = pReflector->GetResourceBindingDesc(i, &bindDesc);
        ThrowIfFailed(hr);

        if (D3D_SIT_SAMPLER == bindDesc.Type)
        {
            ShaderReflectBindInfo t;
            t.type = "sampler";
            t.name = bindDesc.Name;
            t.set = bindDesc.Space;
            t.binding = bindDesc.BindPoint;
            _shader.samplers.push_back(t);
        }
        else if (D3D_SIT_TEXTURE == bindDesc.Type)
        {
            ShaderReflectBindInfo t;
            t.name = bindDesc.Name;
            t.set = bindDesc.Space;
            t.binding = bindDesc.BindPoint;
            switch (bindDesc.Dimension)
            {
            case D3D_SRV_DIMENSION_TEXTURE2D:
                t.type = "texture2D";
                break;
            case D3D_SRV_DIMENSION_TEXTURE3D:
                t.type = "texture3D";
                break;
            //case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
            //    t.dimension = TextureDimension::Tex2DArray;
            //    break;
            //case D3D_SRV_DIMENSION_TEXTURECUBE:
            //    t.dimension = TextureDimension::Cube;
            //    break;
            //case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:
            //    t.dimension = TextureDimension::CubeArray;
            //    break;
            default:
                abort();
            }
            _shader.images.push_back(t);
        }
        else if (D3D_SIT_CBUFFER == bindDesc.Type) {
            ShaderReflectCBufferBindInfo ub;
            ub.name = bindDesc.Name;
            ub.type = bindDesc.Name;
            ub.block_size = 0;
            ub.set = bindDesc.Space;
            ub.binding = bindDesc.BindPoint;
            bool found = false;
            for (auto& type : _shader.types)
            {
                if (type.name == ub.name)
                {
                    found = true;
                    ub.block_size = type.block_size;
                    break;
                }
            }
            assert(found);
            _shader.ubos.push_back(ub);
        }
    }

    return _shader;
}

using Writer = rapidjson::PrettyWriter<rapidjson::StringBuffer>;

void Serialize(Writer& writer, ShaderReflectType::Member& member) {
    writer.StartObject();
    writer.Key("name");
    writer.String(member.name.c_str());
    writer.Key("type");
    auto type = member.type;
    if (type == "float2")
        type = "vec2";
    else if (type == "float3")
        type = "vec3";
    else if (type == "float4")
        type = "vec4";
    else if (type == "float4x4")
        type = "mat4";
    writer.String(type.c_str());
    writer.Key("offset");
    writer.Uint(member.offset);
    writer.Key("used");
    writer.Bool(member.used);
    writer.EndObject();
}

void Serialize(Writer& writer, ShaderReflectType& type) {
    writer.Key(type.name.c_str());
    writer.StartObject();
    writer.Key("name");
    writer.String(type.name.c_str());
    writer.Key("members");
    writer.StartArray();
    for (auto& m : type.members) {
        Serialize(writer, m);
    }
    writer.EndArray();
    writer.EndObject();
}

void Serialize(Writer& writer, ShaderReflectBindInfo& u) {
    writer.StartObject();
    writer.Key("type");
    writer.String(u.type.c_str());
    writer.Key("name");
    writer.String(u.name.c_str());
    writer.Key("set");
    writer.Uint64(u.set);
    writer.Key("binding");
    writer.Uint64(u.binding);
    writer.EndObject();
}

void Serialize(Writer& writer, ShaderReflectCBufferBindInfo& u) {
    writer.StartObject();
    writer.Key("type");
    writer.String(u.type.c_str());
    writer.Key("name");
    writer.String(u.name.c_str());
    writer.Key("block_size");
    writer.Uint64(u.block_size);
    writer.Key("set");
    writer.Uint64(u.set);
    writer.Key("binding");
    writer.Uint64(u.binding);
    writer.EndObject();
}

void Serialize(Writer& writer, ShaderReflectShader& shader) {
    writer.StartObject();
    writer.Key("types");
    writer.StartObject();
    for (auto& t : shader.types)
        Serialize(writer, t);
    writer.EndObject();
    if (!shader.images.empty()) {
        writer.Key("separate_images");
        writer.StartArray();
        for (auto& ub : shader.images) {
            Serialize(writer, ub);
        }
        writer.EndArray();
    }
    if (!shader.samplers.empty()) {
        writer.Key("separate_samplers");
        writer.StartArray();
        for (auto& ub : shader.samplers) {
            Serialize(writer, ub);
        }
        writer.EndArray();
    }
    if (!shader.ubos.empty()) {
        writer.Key("ubos");
        writer.StartArray();
        for (auto& ub : shader.ubos) {
            Serialize(writer, ub);
        }
        writer.EndArray();
    }
    writer.EndObject();
}

//void Serialize(Writer& writer, ShaderReflect &reflect)
//{
//    writer.StartObject();
//    writer.Key("vs");
//    Serialize(writer, reflect.vs);
//    writer.Key("ps");
//    Serialize(writer, reflect.ps);
//    writer.EndObject();
//}

void PrintHelp()
{
    puts("usage:\nhlslreflect <compiled_hlsl_shader_code>\n");
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        PrintHelp();
        return 1;
    }
    //ShaderReflect reflect;
    auto blob = CreateShaderFromCompiledFile(argv[1]);
    if (blob == NULL) {
        printf("file not found\n");
        return 1;
    }
    ShaderReflectShader shader = ReflectShader(blob, ShaderTypeVertex);

    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);

    Serialize(writer, shader);

    puts(sb.GetString());

    return 0;
}