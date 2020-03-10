#include "render_d3d12.hpp"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <dxgi1_4.h>
#include <winerror.h>
#include <wrl/client.h>

#include <cassert>
using Microsoft::WRL::ComPtr;
#include <DDSTextureLoader.h>
#include <DescriptorHeap.h>
#include <DirectXHelpers.h>
#include <DirectXTex.h>
#include <EffectPipelineStateDescription.h>
#include <GraphicsMemory.h>
#include <ResourceUploadBatch.h>
#include <fmt/format.h>
#include <pix.h>

#include <filesystem>
#include <system_error>
namespace fs = std::filesystem;

#include "camera.h"
#include "ecs.h"
#include "light.h"
#include "material_internal.hpp"
#include "renderable.h"
#include "rhi.h"
#include "shader.h"
#include "shader_internal.hpp"
#include "statistics.h"
#include "texture.h"
#include "transform.h"

#if _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) \
    delete p;          \
    p = nullptr
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)             \
    if (p != nullptr) p->Release(); \
    p = nullptr
#endif

// Data
static FrameContext g_frameContext[NUM_FRAMES_IN_FLIGHT] = {};
static UINT g_frameIndex = 0;

static constexpr int NUM_BACK_BUFFERS = NUM_FRAMES_IN_FLIGHT;
static ID3D12Device* g_Device = NULL;
static DirectX::DescriptorPile* g_pRtvDescHeap = NULL;
static DirectX::DescriptorPile* g_pSrvDescHeap = NULL;
static ID3D12CommandQueue* g_pCommandQueue = NULL;
static ID3D12GraphicsCommandList* g_pCommandList = NULL;
static ID3D12Fence* g_fence = NULL;
static HANDLE g_fenceEvent = NULL;
static UINT64 g_fenceLastSignaledValue = 0;
static IDXGISwapChain3* g_pSwapChain = NULL;
static int g_SwapChainWidth = 1280;
static int g_SwapChainHeight = 800;
static HANDLE g_hSwapChainWaitableObject = NULL;
static ID3D12Resource* g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE
    g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {};
static DirectX::ResourceUploadBatch* g_GPUResourceUploader = nullptr;
static std::vector<ID3D12Resource*> g_PendingResources[NUM_FRAMES_IN_FLIGHT];
static UINT g_CurrentBackBufferIndex = 0;
static ComPtr<ID3D12RootSignature> g_TestRootSignature;
// static ID3D12PipelineState* g_TestPSO = NULL;
static DirectX::GraphicsMemory* g_CBVMemory = nullptr;
DirectX::DescriptorPile* g_RTVDescriptorHeap;
DirectX::DescriptorPile* g_DSVDescriptorHeap;
DirectX::DescriptorPile* g_StaticSamplerDescriptorHeap;
DirectX::DescriptorPile* g_StaticSrvDescriptorHeap;
DirectX::DescriptorPile* g_StaticUavDescriptorHeap;
DirectX::DescriptorPile* g_DynamicSrvUavDescriptorHeaps[NUM_FRAMES_IN_FLIGHT];
DirectX::DescriptorPile* g_DynamicSamplerDescriptorHeaps[NUM_FRAMES_IN_FLIGHT];
TextureHandle g_MainDepthRTs[3] = {0, 0, 0};
D3D12_CPU_DESCRIPTOR_HANDLE emptySRV2D;
D3D12_CPU_DESCRIPTOR_HANDLE emptySRV3D;

std::vector<DirectX::GraphicsResource> g_CBVInFlight[NUM_FRAMES_IN_FLIGHT];

inline DirectX::DescriptorPile* GetCurrentSrvDescriptorHeap() {
    return g_DynamicSrvUavDescriptorHeaps[g_CurrentBackBufferIndex];
}

inline DirectX::DescriptorPile* GetCurrentSamplerDescriptorHeap() {
    return g_DynamicSamplerDescriptorHeaps[g_CurrentBackBufferIndex];
}

struct StatedD3D12Resource {
    ID3D12Resource* resource = nullptr;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

    StatedD3D12Resource() = default;
    // StatedD3D12Resource(const StatedD3D12Resource&) = delete;

    //~StatedD3D12Resource() { SAFE_RELEASE(resource); }

    void Transition(ID3D12GraphicsCommandList* cmdList,
                    D3D12_RESOURCE_STATES targetState) {
        if ((state | targetState) != state) {
            DirectX::TransitionResource(cmdList, resource, state, targetState);
            state = targetState;
        }
    }
};

struct BufferWrap : public StatedD3D12Resource {
    uint32_t byteLength = 0;
    GPUResourceUsageFlags usage = GPUResourceUsageNone;
};

struct TextureWrap : public StatedD3D12Resource {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t width = 1;
    uint32_t height = 1;
    size_t srvIndex = (size_t)-1;
    size_t uavIndex = (size_t)-1;
    union {
        size_t rtvIndex = (size_t)-1;
        size_t dsvIndex;
    };
};

constexpr int FE_MAX_TEXTURE_COUNT = 1024;

static std::vector<BufferWrap> g_Buffers;
static std::vector<ID3DBlob*> g_Shaders;
static std::vector<TextureWrap> g_Textures;
static std::vector<D3D12_INPUT_LAYOUT_DESC> g_vertexDescriptors;

static void ThrowIfFailed(HRESULT hr) {
    if (FAILED(hr)) {
        std::string message = std::system_category().message(hr);
        printf("Error: %s", message.c_str());
        throw std::exception();
    }
}

inline void SetDebugObjectName(ID3D12DeviceChild* resource, const char* name) {
#if _DEBUG
    wchar_t wname[MAX_PATH];
    int len = (int)strlen(name);
    int result = MultiByteToWideChar(CP_UTF8, 0, name, len, wname, MAX_PATH);
    if (result > 0) {
        resource->SetName(wname);
    }
#else
    (void)resource;
    (void)name;
#endif
}

inline void SetDebugObjectName(ID3D12DeviceChild* resource,
                               const wchar_t* name) {
#if _DEBUG
    resource->SetName(name);
#else
    (void)resource;
    (void)name;
#endif
}

void BeginRenderEvent(const char* label) {
    PIXBeginEvent(g_pCommandList, 0, label);
}

void EndRenderEvent() { PIXEndEvent(g_pCommandList); }

static ID3D12Resource* InternalCreateBuffer(uint32_t byteSize, int bufferUsage,
                                            D3D12_RESOURCE_STATES initState) {
    constexpr int _usage =
        GPUResourceUsageUnorderedAccess | GPUResourceUsageShaderResource;
    assert((bufferUsage | _usage) == _usage);
    ID3D12Resource* ret = nullptr;
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (bufferUsage & GPUResourceUsageUnorderedAccess)
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    if (!(bufferUsage & GPUResourceUsageShaderResource))
        flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(byteSize, flags);
    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(g_Device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
                                                    &desc, initState, nullptr,
                                                    IID_PPV_ARGS(&ret)));
    g_statistics.gpu.bufferSize += byteSize;
    g_statistics.gpu.bufferCount++;
    return ret;
}

static void InternalUpdateBuffer(ID3D12Resource* buffer,
                                 D3D12_RESOURCE_STATES& state,
                                 const Memory& data) {
    D3D12_SUBRESOURCE_DATA subresource;
    subresource.pData = data.buffer;
    subresource.RowPitch = data.byteLength;
    subresource.SlicePitch = data.byteLength;
#if _DEBUG
    auto desc = buffer->GetDesc();
    assert(desc.Width >= data.byteLength);
#endif
    if (state != D3D12_RESOURCE_STATE_COPY_DEST) {
        g_GPUResourceUploader->Transition(buffer, state,
                                          D3D12_RESOURCE_STATE_COPY_DEST);
        state = D3D12_RESOURCE_STATE_COPY_DEST;
    }
    g_GPUResourceUploader->Upload(buffer, 0, &subresource, 1);
}

BufferHandle CreateBuffer(Memory data, GPUResourceUsageFlags usage) {
    assert(data.byteLength > 0);
    BufferHandle handle = g_Buffers.size();
    BufferWrap& b = g_Buffers.emplace_back();
    b.usage = usage;
    b.byteLength = data.byteLength;
    b.state = D3D12_RESOURCE_STATE_COPY_DEST;
    b.resource = InternalCreateBuffer(b.byteLength, b.usage, b.state);
    if (data.buffer != nullptr) {
        InternalUpdateBuffer(b.resource, b.state, data);
        // fmt::print("{}: {}, on frame {} {}\n", __FUNCTION__, handle,
        //           g_frameIndex, g_CurrentBackBufferIndex);
    }
#if _DEBUG
    std::wstring name = fmt::format(L"Buffer{}", handle);
    SetDebugObjectName(b.resource, name.c_str());
#endif
    return handle;
}

void UpdateBuffer(BufferHandle handle, Memory data) {
    assert(handle < g_Buffers.size());
    BufferWrap& b = g_Buffers[handle];
    if (data.byteLength > b.byteLength) {  // need resize
        DeleteBuffer(handle);
        b.byteLength = data.byteLength;
        b.state = D3D12_RESOURCE_STATE_COPY_DEST;
        b.resource = InternalCreateBuffer(data.byteLength, b.usage, b.state);
    }
    InternalUpdateBuffer(b.resource, b.state, data);
}

void DeleteBuffer(BufferHandle handle) {
    assert(handle < g_Buffers.size());
    auto& b = g_Buffers[handle];
    auto res = b.resource;
    if (res != nullptr) {
        g_PendingResources[g_CurrentBackBufferIndex].push_back(res);
        // fmt::print("{}: {}, on frame {} {}\n", __FUNCTION__, handle,
        // g_frameIndex, g_CurrentBackBufferIndex);
        b.resource = nullptr;
        g_statistics.gpu.bufferCount--;
        g_statistics.gpu.bufferSize -= b.byteLength;
    }
    b.byteLength = 0;
}

uint32_t CreateTexture(uint32_t width, uint32_t height, uint32_t mipmaps,
                       Memory memory) {
    ID3D12Resource* texture = nullptr;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    HRESULT hr = DirectX::CreateDDSTextureFromMemoryEx(
        g_Device, *g_GPUResourceUploader, (uint8_t*)memory.buffer,
        memory.byteLength, 0, D3D12_RESOURCE_FLAG_NONE,
        DirectX::DDS_LOADER_DEFAULT | DirectX::DDS_LOADER_MIP_RESERVE,
        &texture);
    assert(SUCCEEDED(hr));
    size_t idx = g_StaticSrvDescriptorHeap->Allocate();
    DirectX::CreateShaderResourceView(
        g_Device, texture, g_StaticSrvDescriptorHeap->GetCpuHandle(idx));
    uint32_t handle = g_Textures.size();
    auto& t = g_Textures.emplace_back();
    auto size = DirectX::GetTextureSize(texture);
    t.srvIndex = idx;
    t.resource = texture;
    t.width = size.x;
    t.height = size.y;
    t.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    return handle;
}

void DeleteTexture(uint32_t textureID) {}

uint32_t CreateShader(const char* path, const char* vs_name,
                      const char* ps_name) {
    return 0;
}

inline std::wstring ConvertString(const std::string& s) {
    constexpr size_t WCHARBUF = MAX_PATH;
    wchar_t wszDest[WCHARBUF];
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, s.c_str(), -1, wszDest,
                        WCHARBUF);
    return std::wstring(wszDest);
}

ShaderHandle CreateShaderFromBlob(ID3DBlob* shaderBlob) {
    assert(shaderBlob != nullptr);
    ShaderHandle handle = g_Shaders.size();
    g_Shaders.push_back(shaderBlob);
    return handle;
}

ShaderHandle CreateShaderFromCompiledFile(const char* path) {
    fs::path p(path);
    std::string path2 = p.lexically_normal().string();
    // if (g_CachedShaderHandles.find(path2) != g_CachedShaderHandles.end())
    //    return g_CachedShaderHandles[path2];
    assert(fs::exists(path));
    // LogWarnning("Compiling Shader: {}\n", path.c_str());
    ID3DBlob* shaderBlob;
    ThrowIfFailed(D3DReadFileToBlob(ConvertString(path).c_str(), &shaderBlob));
    auto ret = CreateShaderFromBlob(shaderBlob);
    // g_CachedShaderHandles[path2] = ret;
    return ret;
}

#define LogInfo fmt::print

void ReflectShader(ID3D10Blob* shader, ShaderType shaderType) {
    constexpr bool printMsg = true;
    ComPtr<ID3D12ShaderReflection> pReflector;
    HRESULT hr = D3DReflect(shader->GetBufferPointer(), shader->GetBufferSize(),
                            IID_PPV_ARGS(&pReflector));
    ThrowIfFailed(hr);

    if (shaderType == ShaderTypeCompute) {
        UINT x, y, z;
        pReflector->GetThreadGroupSize(&x, &y, &z);
    }

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
    for (UINT i = 0; i < desc.InputParameters; ++i) {
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
        }
    }
}

void DeleteShader(uint32_t shaderID) {}

enum class RenderTextureCreationFlags : uint32_t {
    MipMap = 1 << 0,
    AutoGenerateMips = 1 << 1,
    SRGB = 1 << 2,
    EyeTexture = 1 << 3,
    EnableRandomWrite = 1 << 4,
    CreatedFromScript = 1 << 5,
    AllowVerticalFlip = 1 << 6,
    NoResolvedColorSurface = 1 << 7,
    DynamicallyScalable = 1 << 8,
    BindMS = 1 << 9
};

inline RenderTextureCreationFlags& operator|=(RenderTextureCreationFlags& lhs,
                                              RenderTextureCreationFlags rhs) {
    uint32_t l = (uint32_t)lhs;
    l |= (uint32_t)rhs;
    lhs = (RenderTextureCreationFlags)l;
    return lhs;
};

template <typename T>
struct HashBuilder {
    T h = 0;
    int usedBits = 0;
    void with(T v, int bits) {
        assert(v < (((T)1) << bits));
        h <<= bits;
        h |= v;
        usedBits += bits;
        assert(usedBits <= sizeof(h) * 8);
    }
};

struct RenderTextureDescriptor {
    RenderTextureDescriptor() = default;
    RenderTextureDescriptor(int width, int height)
        : width(width), height(height) {}
    RenderTextureDescriptor(int width, int height,
                            RenderTextureFormat colorFormat)
        : width(width), height(height), colorFormat(colorFormat){};
    RenderTextureDescriptor(int width, int height,
                            RenderTextureFormat colorFormat,
                            int depthBufferBits)
        : width(width),
          height(height),
          colorFormat(colorFormat),
          depthBufferBits(depthBufferBits) {}

    int width = 1;
    int height = 1;
    // RenderTextureCreationFlags flags = (RenderTextureCreationFlags)0;
    bool autoGenerateMips = false;
    bool useMipmap = false;
    bool sRGB = false;
    bool enableRandomWrite = false;
    int msaaSamples = 1;
    TextureDimension dimension = TextureDimensionTex2D;
    RenderTextureFormat colorFormat = RenderTextureFormatARGB32;
    int volumeDepth = 1;       // 1~2000
    int depthBufferBits = 24;  // 0, 16, 24

    RenderTextureCreationFlags flags() const {
        RenderTextureCreationFlags f = RenderTextureCreationFlags(0);
        if (useMipmap) f |= RenderTextureCreationFlags::MipMap;
        if (autoGenerateMips) f |= RenderTextureCreationFlags::AutoGenerateMips;
        if (sRGB) f |= RenderTextureCreationFlags::SRGB;
        if (enableRandomWrite)
            f |= RenderTextureCreationFlags::EnableRandomWrite;
        return f;
    }

    typedef uint64_t H;

    H Hash() const {
        HashBuilder<H> h;
        h.with((H)flags(), 10);
        h.with((H)dimension, 3);
        h.with((H)colorFormat, 5);
        int dbb = depthBufferBits == 0 ? 0 : (depthBufferBits == 16 ? 1 : 2);
        h.with((H)dbb, 2);
        h.with((H)width, 14);
        h.with((H)height, 14);
        h.with((H)volumeDepth, 11);
        h.with((H)msaaSamples, 5);
        return h.h;
    }
};

class Static {
   public:
    Static() = delete;
};

class SystemInfo : public Static {
   public:
    static constexpr bool usesReversedZBuffer = false;
};

inline DXGI_FORMAT Translate(RenderTextureFormat format) {
    DXGI_FORMAT f = DXGI_FORMAT_UNKNOWN;
    switch (format) {
        case RenderTextureFormatR8:
            f = DXGI_FORMAT_R8_UNORM;
            break;
        case RenderTextureFormatRHalf:
            f = DXGI_FORMAT_R16_FLOAT;
            break;
        case RenderTextureFormatRFloat:
            f = DXGI_FORMAT_R32_FLOAT;
            break;
        case RenderTextureFormatARGB32:
            f = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        case RenderTextureFormatDepth:
            f = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
            break;
        case RenderTextureFormatARGB2101010:
            f = DXGI_FORMAT_R10G10B10A2_UNORM;
            break;
        case RenderTextureFormatRGB111110Float:
            f = DXGI_FORMAT_R11G11B10_FLOAT;
            break;
        case RenderTextureFormatRGHalf:
            f = DXGI_FORMAT_R16G16_FLOAT;
            break;
        default:
            abort();
    }
    return f;
}

TextureHandle InternalCreateRenderTexture(const RenderTextureDescriptor& desc) {
    ID3D12Resource* texture = nullptr;

    DXGI_FORMAT dxgiFormat = Translate(desc.colorFormat);
    if (desc.sRGB) {
        dxgiFormat = DirectX::MakeSRGB(dxgiFormat);
    }
    DXGI_FORMAT dxgiFormat_typeless = DirectX::MakeTypeless(dxgiFormat);
    if (dxgiFormat == DXGI_FORMAT_D32_FLOAT_S8X24_UINT) {
        dxgiFormat_typeless = DXGI_FORMAT_R32G8X24_TYPELESS;
    }
    CD3DX12_RESOURCE_DESC colorDesc;
    if (desc.dimension == TextureDimensionTex2D ||
        desc.dimension == TextureDimensionTex2DArray)
        colorDesc =
            CD3DX12_RESOURCE_DESC::Tex2D(dxgiFormat_typeless, desc.width,
                                         desc.height, (UINT16)desc.volumeDepth);
    else
        abort();
    assert(!desc.useMipmap);
    colorDesc.MipLevels = 1;
    D3D12_CLEAR_VALUE colorClearValue;
    colorClearValue.Format = dxgiFormat;
    bool isDepth = desc.colorFormat == RenderTextureFormatDepth;
    if (isDepth) {
        colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        colorClearValue.DepthStencil.Depth = 1.0f;
        if constexpr (SystemInfo::usesReversedZBuffer) {
            colorClearValue.DepthStencil.Depth = 0.0f;
        }
        colorClearValue.DepthStencil.Stencil = 0;
    } else {
        colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        colorClearValue.Color[0] = 1.f;
        colorClearValue.Color[1] = 0.f;
        colorClearValue.Color[2] = 0.f;
        colorClearValue.Color[3] = 1.f;
    }
    if (desc.enableRandomWrite) {
        colorDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    // bool newCreated = !handle.IsValid();
    // if (newCreated) handle.id = g_Textures.Next();
    TextureHandle handle = g_Textures.size();
    bool newCreated = true;

    auto& t = g_Textures.emplace_back();
    assert(t.resource == nullptr);

    D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON;
    if (!newCreated) initState = t.state;
    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(g_Device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &colorDesc, initState, &colorClearValue,
        IID_PPV_ARGS(&texture)));

    t.resource = texture;
    t.state = initState;
    t.format = dxgiFormat;
    t.width = desc.width;
    t.height = desc.height;

    if (isDepth) {
        if (newCreated) {
            size_t start = g_DSVDescriptorHeap->Allocate();
            t.dsvIndex = start;
        }
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
        memset(&dsv, 0, sizeof(dsv));
        dsv.Format = dxgiFormat;
        dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv.Texture2D.MipSlice = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
            g_DSVDescriptorHeap->GetCpuHandle(t.dsvIndex);
        g_Device->CreateDepthStencilView(texture, &dsv, dsvHandle);
    } else {
        if (newCreated) {
            size_t start = g_RTVDescriptorHeap->Allocate();
            t.rtvIndex = start;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
            g_RTVDescriptorHeap->GetCpuHandle(t.rtvIndex);
        D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
        memset(&rtv, 0, sizeof(rtv));
        rtv.Format = dxgiFormat;
        rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtv.Texture2D.MipSlice = 0;
        rtv.Texture2D.PlaneSlice = 0;
        g_Device->CreateRenderTargetView(texture, &rtv, rtvHandle);
    }
    {
        // if (newCreated)
        {
            size_t start = g_StaticSrvDescriptorHeap->Allocate();
            // assert(start == handle.id);
            t.srvIndex = start;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
            g_StaticSrvDescriptorHeap->GetCpuHandle(t.srvIndex);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format = dxgiFormat;
        if (srvDesc.Format == DXGI_FORMAT_D32_FLOAT) {
            srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        } else if (srvDesc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT) {
            srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        }
        srvDesc.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        if (desc.dimension == TextureDimensionTex2D) {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = colorDesc.MipLevels;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.PlaneSlice = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.f;
        } else if (desc.dimension == TextureDimensionTex2DArray) {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MipLevels = colorDesc.MipLevels;
            srvDesc.Texture2DArray.ArraySize = desc.volumeDepth;
        } else {
            abort();
        }
        g_Device->CreateShaderResourceView(texture, &srvDesc, srvHandle);
    }
    return handle;
}

void DeleteRenderTexture(TextureHandle handle) {
    assert(handle < g_Textures.size());
    auto& tex = g_Textures[handle];
    auto res = tex.resource;
    g_PendingResources[g_CurrentBackBufferIndex].push_back(res);
    tex.resource = nullptr;
}

struct PerDrawUniforms {
    float4x4 MATRIX_MVP;
    float4x4 MATRIX_MV;
    float4x4 MATRIX_M;
    float4x4 MATRIX_IT_M;
};

struct PerCameraUniforms {
    float4x4 MATRIX_P;
    float4x4 MATRIX_V;
    float4x4 MATRIX_I_V;
    float4x4 MATRIX_VP;

    float4 WorldSpaceCameraPos;  // w = 1, not used
    float4 WorldSpaceCameraDir;  // w = 0, not used
};

struct LightingUniforms {
    float4 LightPos;  // w=1 not used
    float4 LightDir;  // w=0 not used
};

struct BuiltinConstantBuffers {
    struct PerDrawUniforms cb1;
    struct PerCameraUniforms cb2;
    struct LightingUniforms cb3;
};

extern "C" {
extern World* defaultWorld;
}

void MaterialBuildGloblsCB(Material* mat, ShaderReflect& reflect, Memory& mem) {
    assert(mem.byteLength >= reflect.ps.globals.block_size &&
           mem.buffer != NULL);

    for (auto& m : reflect.ps.globals.members) {
        if (!m.used) continue;
        if (m.type == "float") {
            float v = MaterialGetFloat(mat, m.nameID);
            float* ptr = (float*)((uint8_t*)mem.buffer + m.offset);
            *ptr = v;
        } else if (m.type == "vec2" || m.type == "vec3" || m.type == "vec4") {
            float4 v = MaterialGetVector(mat, m.nameID);
            memcpy((uint8_t*)mem.buffer + m.offset, &v, m.bytes);
        }
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE GetSampler(FilterMode filterMode,
                                       TextureWrapMode wrapModeU,
                                       TextureWrapMode wrapModeV,
                                       TextureWrapMode wrapModeW,
                                       UINT anisoLevel = 1) {
    assert(anisoLevel == 1);
    uint32_t i = filterMode, j = wrapModeU, k = wrapModeV, l = wrapModeW;
    uint32_t hash = (((((i << 2) | j) << 2) | k) << 2) | l;
    return g_StaticSamplerDescriptorHeap->GetCpuHandle(hash);
}

D3D12_CPU_DESCRIPTOR_HANDLE GetSampler(FilterMode filterMode,
                                       TextureWrapMode wrapMode) {
    return GetSampler(filterMode, wrapMode, wrapMode, wrapMode);
}

D3D12_CPU_DESCRIPTOR_HANDLE GetSampler(Texture* tex) {
    return GetSampler(tex->filterMode, tex->wrapModeU, tex->wrapModeV,
                      tex->wrapModeW, tex->anisoLevel);
}

ID3D12PipelineState* GetPipelineState(ShaderHandle vs, ShaderHandle ps);

constexpr int srvRootRange = 5;

struct Renderable;
// struct Transform;
int SimpleDraw(Transform* t, struct Renderable* r) {
    if (!t || !r || !r->mesh || !r->material || !r->material->shader) return 0;

    BeginRenderEvent("SimpleDraw");
    g_pCommandList->SetGraphicsRootSignature(g_TestRootSignature.Get());

    World* w = defaultWorld;
    Camera* camera = CameraGetMainCamera(w);
    Entity cameraEntity = ComponentGetEntity(w, camera, CameraID);
    SingletonTransformManager* tm =
        (SingletonTransformManager*)WorldGetSingletonComponent(
            w, SingletonTransformManagerID);

    uint32_t tindex = WorldGetComponentIndex(w, t, TransformID);

    float4x4 l2w = TransformGetLocalToWorldMatrix(tm, w, tindex);

    float4x4 view;
    float3 cameraPos, cameraDir;
    {
        cameraDir = TransformGetForward(tm, w, cameraEntity);
        cameraPos = TransformGetPosition(tm, w, cameraEntity);
        float3 up = TransformGetUp(tm, w, cameraEntity);
        view = float4x4_look_to(cameraPos, cameraDir, up);
        //		float4x4 camera2world =
        // TransformGetLocalToWorldMatrix(tm, w, cameraEntity);
        //        view = float4x4_inverse(camera2world); // this view mat
        //        contains local scale
        // reverse-z
        //		view = float4x4_mul(view, float4x4_diagonal(1, 1, -1,
        // 1));
    }
    const float aspect = (float)g_SwapChainWidth / g_SwapChainHeight;
    float4x4 p =
        float4x4_perspective(camera->fieldOfView, aspect, camera->nearClipPlane,
                             camera->farClipPlane);
    float4x4 mvp = float4x4_mul(p, float4x4_mul(view, l2w));

    struct BuiltinConstantBuffers g_cbuffers = {};
    g_cbuffers.cb1.MATRIX_MVP = mvp;
    g_cbuffers.cb1.MATRIX_MV = float4x4_mul(view, l2w);
    g_cbuffers.cb1.MATRIX_M = l2w;
    g_cbuffers.cb1.MATRIX_IT_M = float4x4_transpose(float4x4_inverse(l2w));

    g_cbuffers.cb2.MATRIX_P = p;
    g_cbuffers.cb2.MATRIX_V = view;
    g_cbuffers.cb2.MATRIX_I_V = float4x4_inverse(view);
    g_cbuffers.cb2.MATRIX_VP = float4x4_mul(p, view);
    g_cbuffers.cb2.WorldSpaceCameraPos =
        float4_make(cameraPos.x, cameraPos.y, cameraPos.z, 1);
    g_cbuffers.cb2.WorldSpaceCameraDir =
        float4_make(cameraDir.x, cameraDir.y, cameraDir.z, 0);

    {
        Light* light = (Light*)WorldGetComponentAt(w, LightID, 0);
        Entity lightEntity = ComponentGetEntity(w, light, LightID);
        float3 p = TransformGetPosition(tm, w, lightEntity);
        float3 f = TransformGetForward(tm, w, lightEntity);
        g_cbuffers.cb3.LightPos = float4_make(p.x, p.y, p.z, 1);
        g_cbuffers.cb3.LightDir = float4_make(-f.x, -f.y, -f.z, 0);
    }

    {
        BufferHandle vbHandle = r->mesh->vb;
        if (r->skin && (r->mesh->sb != 0) && (r->mesh->skinnedvb != 0)) {
            vbHandle = r->mesh->skinnedvb;
        }
        {
            D3D12_VERTEX_BUFFER_VIEW vbv = {};
            auto& b = g_Buffers[vbHandle];
            b.Transition(g_pCommandList,
                         D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            vbv.BufferLocation = b.resource->GetGPUVirtualAddress();
            vbv.SizeInBytes = b.byteLength;
            vbv.StrideInBytes = sizeof(Vertex);
            g_pCommandList->IASetVertexBuffers(0, 1, &vbv);
        }
        g_pCommandList->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    auto cb1 = g_CBVMemory->AllocateConstant<PerDrawUniforms>();
    memcpy(cb1.Memory(), &g_cbuffers.cb1, sizeof(g_cbuffers.cb1));

    auto cb2 = g_CBVMemory->AllocateConstant<PerCameraUniforms>();
    memcpy(cb2.Memory(), &g_cbuffers.cb2, sizeof(g_cbuffers.cb2));

    auto cb3 = g_CBVMemory->AllocateConstant<LightingUniforms>();
    memcpy(cb3.Memory(), &g_cbuffers.cb3, sizeof(g_cbuffers.cb3));

    ShaderImpl* shaderImpl = ShaderGetImpl(r->material->shader);
    for (int passIdx = 0; passIdx < r->material->shader->passCount; ++passIdx) {
        ShaderPass& pass = shaderImpl->passes[passIdx];
        ShaderVariant& var =
            pass.variants[MaterialGetVariantIndex(r->material, passIdx)];

        ID3D12PipelineState* pso =
            GetPipelineState(var.vertexShader, var.pixelShader);

        g_pCommandList->SetPipelineState(pso);
        g_pCommandList->SetGraphicsRootSignature(g_TestRootSignature.Get());
        g_pCommandList->SetGraphicsRootConstantBufferView(0, cb1.GpuAddress());
        g_pCommandList->SetGraphicsRootConstantBufferView(2, cb2.GpuAddress());
        g_pCommandList->SetGraphicsRootConstantBufferView(3, cb3.GpuAddress());

        uint32_t srvCount = var.reflect.ps.images.size();
        if (srvCount > 0) {
            auto heap = GetCurrentSrvDescriptorHeap();
            size_t start, end;
            heap->AllocateRange(srvRootRange, start, end);
            for (int i = 0; i < srvCount; ++i) {
                auto& img = var.reflect.ps.images[i];
                Texture* tex = MaterialGetTexture(r->material, img.nameID);
                D3D12_CPU_DESCRIPTOR_HANDLE src = emptySRV2D;
                if (tex != nullptr) {
                    TextureWrap& t = g_Textures[tex->handle];
                    t.Transition(g_pCommandList,
                                 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    src = g_StaticSrvDescriptorHeap->GetCpuHandle(t.srvIndex);
                } else {
                    printf("texture not set\n");
                }
                D3D12_CPU_DESCRIPTOR_HANDLE dst = heap->GetCpuHandle(start + i);
                g_Device->CopyDescriptorsSimple(1, dst, src, heap->Type());
            }
            for (int i = srvCount; i < srvRootRange; ++i) {
                D3D12_CPU_DESCRIPTOR_HANDLE src = emptySRV2D;
                D3D12_CPU_DESCRIPTOR_HANDLE dst = heap->GetCpuHandle(start + i);
                g_Device->CopyDescriptorsSimple(1, dst, src, heap->Type());
            }
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = heap->GetGpuHandle(start);
            g_pCommandList->SetGraphicsRootDescriptorTable(4, gpuHandle);
        }
        if (srvCount > 0) {
            auto heap = GetCurrentSamplerDescriptorHeap();
            size_t start, end;
            heap->AllocateRange(srvRootRange, start, end);
            D3D12_CPU_DESCRIPTOR_HANDLE defaultSampler =
                GetSampler(FilterModeBilinear, TextureWrapModeClamp);
            for (int i = 0; i < srvCount; ++i) {
                auto& img = var.reflect.ps.images[i];
                Texture* tex = MaterialGetTexture(r->material, img.nameID);
                D3D12_CPU_DESCRIPTOR_HANDLE src;
                if (tex != nullptr) {
                    src = GetSampler(tex);
                } else {
                    printf("texture not set\n");
                    src = defaultSampler;
                }
                D3D12_CPU_DESCRIPTOR_HANDLE dst = heap->GetCpuHandle(start + i);
                g_Device->CopyDescriptorsSimple(1, dst, src, heap->Type());
            }
            for (int i = srvCount; i < srvRootRange; ++i) {
                D3D12_CPU_DESCRIPTOR_HANDLE src = defaultSampler;
                D3D12_CPU_DESCRIPTOR_HANDLE dst = heap->GetCpuHandle(start + i);
                g_Device->CopyDescriptorsSimple(1, dst, src, heap->Type());
            }
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = heap->GetGpuHandle(start);
            g_pCommandList->SetGraphicsRootDescriptorTable(5, gpuHandle);
        }

        std::vector<uint8_t> cb0(var.reflect.ps.globals.block_size, 0);
        Memory mem;
        mem.buffer = cb0.data();
        mem.byteLength = cb0.size();
        MaterialBuildGloblsCB(r->material, var.reflect, mem);
        auto _cb0 = g_CBVMemory->Allocate(
            mem.byteLength, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        memcpy(_cb0.Memory(), mem.buffer, mem.byteLength);
        g_pCommandList->SetGraphicsRootConstantBufferView(1, _cb0.GpuAddress());
        g_CBVInFlight[g_CurrentBackBufferIndex].emplace_back(std::move(_cb0));

        if (r->mesh->ib != 0) {
            D3D12_INDEX_BUFFER_VIEW ibv = {};
            auto& b = g_Buffers[r->mesh->ib];
            ibv.BufferLocation = b.resource->GetGPUVirtualAddress();
            if (r->mesh->triangles.stride == 4)
                ibv.Format = DXGI_FORMAT_R32_UINT;
            else
                ibv.Format = DXGI_FORMAT_R16_UINT;
            ibv.SizeInBytes = b.byteLength;
            g_pCommandList->IASetIndexBuffer(&ibv);
            g_pCommandList->DrawIndexedInstanced(MeshGetIndexCount(r->mesh), 1,
                                                 0, 0, 0);
        } else {
            g_pCommandList->DrawInstanced(MeshGetVertexCount(r->mesh), 1, 0, 0);
        }
    }

    g_CBVInFlight[g_CurrentBackBufferIndex].emplace_back(std::move(cb1));
    g_CBVInFlight[g_CurrentBackBufferIndex].emplace_back(std::move(cb2));
    g_CBVInFlight[g_CurrentBackBufferIndex].emplace_back(std::move(cb3));

    EndRenderEvent();
    return 0;
}

void BeginPass() {}

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void WaitForLastSubmittedFrame();
FrameContext* WaitForNextFrameResources();
void ResizeSwapChain(HWND hWnd, int width, int height);

ID3D12Device* GetDevice() { return g_Device; }
ID3D12GraphicsCommandList* GetCurrentGraphicsCommandList() {
    return g_pCommandList;
}
ID3D12DescriptorHeap* GetRTVDescriptorHeap() { return g_pRtvDescHeap->Heap(); }
ID3D12DescriptorHeap* GetSRVDescriptorHeap() { return g_pSrvDescHeap->Heap(); }

void FrameBegin() {
    const float clear_color[4] = {0.45f, 0.55f, 0.60f, 1.00f};

    UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
    g_CurrentBackBufferIndex = backBufferIdx;
    for (ID3D12Resource* res : g_PendingResources[g_CurrentBackBufferIndex]) {
        if (res) {
            res->Release();
        }
    }
    g_PendingResources[g_CurrentBackBufferIndex].clear();
    g_CBVInFlight[g_CurrentBackBufferIndex].clear();

    FrameContext* frameCtxt = WaitForNextFrameResources();
    frameCtxt->CommandAllocator->Reset();

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        g_mainRenderTargetResource[backBufferIdx], D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    g_pCommandList->Reset(frameCtxt->CommandAllocator, NULL);
    g_pCommandList->ResourceBarrier(1, &barrier);
    g_pCommandList->ClearRenderTargetView(
        g_mainRenderTargetDescriptor[backBufferIdx], (float*)&clear_color, 0,
        NULL);

    auto& depth = g_Textures[g_MainDepthRTs[g_CurrentBackBufferIndex]];
    if (depth.width != g_SwapChainWidth || depth.height != g_SwapChainHeight) {
        RenderTextureDescriptor depthRTDesc(g_SwapChainWidth, g_SwapChainHeight,
                                            RenderTextureFormatDepth);
        g_MainDepthRTs[g_CurrentBackBufferIndex] =
            InternalCreateRenderTexture(depthRTDesc);
    }
    auto depthHandle = g_DSVDescriptorHeap->GetCpuHandle(
        g_Textures[g_MainDepthRTs[g_CurrentBackBufferIndex]].dsvIndex);
    g_Textures[g_MainDepthRTs[g_CurrentBackBufferIndex]].Transition(
        g_pCommandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    g_pCommandList->ClearDepthStencilView(depthHandle, D3D12_CLEAR_FLAG_DEPTH,
                                          1, 0, 0, NULL);
    g_pCommandList->OMSetRenderTargets(
        1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, &depthHandle);

    GetCurrentSrvDescriptorHeap()->Reset();
    GetCurrentSamplerDescriptorHeap()->Reset();
    ID3D12DescriptorHeap* heaps[] = {GetCurrentSrvDescriptorHeap()->Heap(),
                                     GetCurrentSamplerDescriptorHeap()->Heap()};
    g_pCommandList->SetDescriptorHeaps(2, heaps);

    UINT width = g_SwapChainWidth;
    UINT height = g_SwapChainHeight;
    CD3DX12_VIEWPORT vp(0.f, 0.f, (float)width, (float)height);
    g_pCommandList->RSSetViewports(1, &vp);
    CD3DX12_RECT rect(0, 0, LONG_MAX, LONG_MAX);
    g_pCommandList->RSSetScissorRects(1, &rect);
}

void FrameEnd() {
    {
        std::future<void> result = g_GPUResourceUploader->End(g_pCommandQueue);
        g_CBVMemory->Commit(g_pCommandQueue);
        result.get();
        g_GPUResourceUploader->Begin();
    }

    UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        g_mainRenderTargetResource[backBufferIdx],
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    g_pCommandList->ResourceBarrier(1, &barrier);
    g_pCommandList->Close();

    g_pCommandQueue->ExecuteCommandLists(
        1, (ID3D12CommandList* const*)&g_pCommandList);

    g_pSwapChain->Present(1, 0);  // Present with vsync
    // g_pSwapChain->Present(0, 0);  // Present without vsync

    g_CurrentBackBufferIndex = g_pSwapChain->GetCurrentBackBufferIndex();

    UINT64 fenceValue = g_fenceLastSignaledValue + 1;
    g_pCommandQueue->Signal(g_fence, fenceValue);
    g_fenceLastSignaledValue = fenceValue;

    auto frameCtxt = &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];
    frameCtxt->FenceValue = fenceValue;
}

D3D12_INPUT_LAYOUT_DESC GetVerextDecl(VertexDeclHandle handle);

ComPtr<ID3D12RootSignature> CreateRootSignatureFromRootParameters(
    D3D12_ROOT_PARAMETER1* rootParameters, int rootParameterCount) {
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_NONE |
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
        //| D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
        ;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(rootParameterCount, rootParameters, 0,
                                      nullptr, rootSignatureFlags);
    // TODO: make it global
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(g_Device->CheckFeatureSupport(
            D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    // Serialize the root signature.
    ComPtr<ID3DBlob> rootSignatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(
        &rootSignatureDescription, featureData.HighestVersion,
        &rootSignatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        DebugBreak();
    }
    // Create the root signature.
    ComPtr<ID3D12RootSignature> rootSignature;
    ThrowIfFailed(g_Device->CreateRootSignature(
        0, rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
    return rootSignature;
}

inline D3D12_FILTER Translate(FilterMode f) {
    D3D12_FILTER ret = D3D12_FILTER_MIN_MAG_MIP_POINT;
    switch (f) {
        case FilterModePoint:
            ret = D3D12_FILTER_MIN_MAG_MIP_POINT;
            break;
        case FilterModeBilinear:
            ret = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            break;
        case FilterModeTrilinear:
            ret = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            break;
    }
    return ret;
}

inline D3D12_TEXTURE_ADDRESS_MODE Translate(TextureWrapMode w) {
    D3D12_TEXTURE_ADDRESS_MODE ret = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    switch (w) {
        case TextureWrapModeClamp:
            ret = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            break;
        case TextureWrapModeMirror:
            ret = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            break;
        case TextureWrapModeMirrorOnce:
            ret = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
            break;
        case TextureWrapModeRepeat:
            ret = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            break;
    }
    return ret;
}

bool CreateDeviceD3D(HWND hWnd) {
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC1 sd;
    {
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = NUM_BACK_BUFFERS;
        sd.Width = 0;
        sd.Height = 0;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.Stereo = FALSE;
    }

#ifdef DX12_ENABLE_DEBUG_LAYER
    ID3D12Debug* pdx12Debug = NULL;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug)))) {
        pdx12Debug->EnableDebugLayer();
        pdx12Debug->Release();
    }
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (D3D12CreateDevice(NULL, featureLevel, IID_PPV_ARGS(&g_Device)) != S_OK)
        return false;

#ifdef DX12_ENABLE_DEBUG_LAYER
    ID3D12InfoQueue* pInfoQueue = nullptr;
    if (SUCCEEDED(g_Device->QueryInterface(IID_PPV_ARGS(&pInfoQueue)))) {
        // Suppress whole categories of messages.
        // D3D12_MESSAGE_CATEGORY Categories[] = {};

        // Suppress messages based on their severity level.
        D3D12_MESSAGE_SEVERITY Severities[] = {D3D12_MESSAGE_SEVERITY_INFO};

        // Suppress individual messages by their ID.
        D3D12_MESSAGE_ID DenyIds[] = {
            // The 11On12 implementation does not use optimized clearing yet.
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE};

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        // NewFilter.DenyList.NumCategories = _countof(Categories);
        // NewFilter.DenyList.pCategoryList = Categories;
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;

        pInfoQueue->PushStorageFilter(&NewFilter);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
        pInfoQueue->Release();
    }
#endif

    g_Buffers.reserve(1024);
    g_Textures.reserve(1024);
    g_vertexDescriptors.reserve(1024);
    g_Shaders.reserve(1024);
    // 0 is reserved
    g_Buffers.resize(1);
    g_Textures.resize(1);
    g_vertexDescriptors.resize(1);
    g_Shaders.resize(1);

    g_CBVMemory = new DirectX::GraphicsMemory(g_Device);
    g_GPUResourceUploader = new DirectX::ResourceUploadBatch(g_Device);
    g_GPUResourceUploader->Begin();

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = 128;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        g_RTVDescriptorHeap =
            new DirectX::DescriptorPile(g_Device, &desc, NUM_FRAMES_IN_FLIGHT);
    }
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = 16;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        g_DSVDescriptorHeap = new DirectX::DescriptorPile(g_Device, &desc);
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = 1024;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;
        g_pRtvDescHeap = new DirectX::DescriptorPile(g_Device, &desc);
        size_t start, end;
        g_pRtvDescHeap->AllocateRange(3, start, end);
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
            g_mainRenderTargetDescriptor[i] =
                g_pRtvDescHeap->GetCpuHandle(start + i);
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1024;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        g_pSrvDescHeap = new DirectX::DescriptorPile(g_Device, &desc, 1);
        // the first one is reserved for imgui
    }

    {
        constexpr UINT COUNT = 2048;
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = COUNT;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;  // GPU visible
        for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i) {
            g_DynamicSrvUavDescriptorHeaps[i] =
                new DirectX::DescriptorPile(g_Device, &desc);
        }
        desc.NumDescriptors = FE_MAX_TEXTURE_COUNT;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;  // CPU only
        g_StaticSrvDescriptorHeap =
            new DirectX::DescriptorPile(g_Device, &desc, NUM_FRAMES_IN_FLIGHT);
        // reserved for back buffers

        emptySRV2D =
            g_StaticSrvDescriptorHeap->GetCpuHandle(FE_MAX_TEXTURE_COUNT - 1);
        emptySRV3D =
            g_StaticSrvDescriptorHeap->GetCpuHandle(FE_MAX_TEXTURE_COUNT - 2);
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
            ZeroMemory(&srvDesc, sizeof(srvDesc));
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping =
                D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = 1;
            g_Device->CreateShaderResourceView(nullptr, &srvDesc, emptySRV2D);
        }
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
            ZeroMemory(&srvDesc, sizeof(srvDesc));
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            srvDesc.Shader4ComponentMapping =
                D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture3D.MipLevels = 1;
            g_Device->CreateShaderResourceView(nullptr, &srvDesc, emptySRV3D);
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        desc.NumDescriptors = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;  // GPU visible
        for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i) {
            g_DynamicSamplerDescriptorHeaps[i] =
                new DirectX::DescriptorPile(g_Device, &desc);
        }
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;  // CPU only
        g_StaticSamplerDescriptorHeap =
            new DirectX::DescriptorPile(g_Device, &desc);
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        if (g_Device->CreateCommandQueue(
                &desc, IID_PPV_ARGS(&g_pCommandQueue)) != S_OK)
            return false;
    }

    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_Device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&g_frameContext[i].CommandAllocator)) != S_OK)
            return false;

    if (g_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                    g_frameContext[0].CommandAllocator, NULL,
                                    IID_PPV_ARGS(&g_pCommandList)) != S_OK ||
        g_pCommandList->Close() != S_OK)
        return false;

    if (g_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                              IID_PPV_ARGS(&g_fence)) != S_OK)
        return false;

    g_fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (g_fenceEvent == NULL) return false;

    {
        IDXGIFactory4* dxgiFactory = NULL;
        IDXGISwapChain1* swapChain1 = NULL;
        if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK ||
            dxgiFactory->CreateSwapChainForHwnd(
                g_pCommandQueue, hWnd, &sd, NULL, NULL, &swapChain1) != S_OK ||
            swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain)) != S_OK)
            return false;
        swapChain1->Release();
        dxgiFactory->Release();
        g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
        g_hSwapChainWaitableObject =
            g_pSwapChain->GetFrameLatencyWaitableObject();
    }

    CreateRenderTarget();
    g_Textures.resize(3);

    RenderTextureDescriptor depthRTDesc(1280, 800, RenderTextureFormatDepth);
    for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i) {
        g_MainDepthRTs[i] = InternalCreateRenderTexture(depthRTDesc);
    }
    {
        CD3DX12_ROOT_PARAMETER1 rootParameters[6];
        D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
        D3D12_SHADER_VISIBILITY v = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[0].InitAsConstantBufferView(1, 0, flags, v);
        v = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[1].InitAsConstantBufferView(0, 0, flags, v);
        rootParameters[2].InitAsConstantBufferView(2, 0, flags, v);
        rootParameters[3].InitAsConstantBufferView(3, 0, flags, v);
        CD3DX12_DESCRIPTOR_RANGE1 range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                        srvRootRange, 0, 0);
        rootParameters[4].InitAsDescriptorTable(1, &range, v);
        CD3DX12_DESCRIPTOR_RANGE1 range2(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                         srvRootRange, 0, 0);
        rootParameters[5].InitAsDescriptorTable(1, &range2, v);
        g_TestRootSignature =
            CreateRootSignatureFromRootParameters(rootParameters, 6);
    }

    {
        D3D12_SAMPLER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));

        for (int i = 0; i < _FilterModeCount; ++i) {
            if (i == FilterModePoint) {
                desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
            } else if (i == FilterModeBilinear) {
                desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            } else {
                desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            }
            desc.Filter = Translate(FilterMode(i));
            for (int j = 0; j < _TextureWrapModeCount; ++j) {
                desc.AddressU = Translate((TextureWrapMode)j);
                for (int k = 0; k < _TextureWrapModeCount; ++k) {
                    desc.AddressV = Translate((TextureWrapMode)k);
                    for (int l = 0; l < _TextureWrapModeCount; ++l) {
                        desc.AddressW = Translate((TextureWrapMode)l);
                        uint32_t hash = (((((i << 2) | j) << 2) | k) << 2) | l;
                        size_t offset =
                            g_StaticSamplerDescriptorHeap->Allocate();
                        assert(hash == offset);
                        D3D12_CPU_DESCRIPTOR_HANDLE handle =
                            g_StaticSamplerDescriptorHeap->GetCpuHandle(offset);
                        g_Device->CreateSampler(&desc, handle);
                    }
                }
            }
        }
    }

    return true;
}

struct PSOCache {
    std::map<uint32_t, ID3D12PipelineState*> psos;
};

PSOCache g_PSOCache;

ID3D12PipelineState* GetPipelineState(ShaderHandle vs, ShaderHandle ps) {
    assert(vs < (1 << 16) && ps < (1 << 16));
    uint32_t hash = (vs << 16) + ps;
    auto it = g_PSOCache.psos.find(hash);
    if (it != g_PSOCache.psos.end()) return it->second;

    VertexDeclElement elements[] = {
        {VertexAttributePosition, VertexAttributeTypeFloat, 4},
        {VertexAttributeNormal, VertexAttributeTypeFloat, 4},
        {VertexAttributeTangent, VertexAttributeTypeFloat, 4},
        {VertexAttributeTexCoord0, VertexAttributeTypeFloat, 2},
        {VertexAttributeTexCoord1, VertexAttributeTypeFloat, 2},
    };
    VertexDecl decl = BuildVertexDeclFromElements(elements, countof(elements));

    D3D12_INPUT_LAYOUT_DESC inputLayout = GetVerextDecl(decl.handle);
    CD3DX12_BLEND_DESC blend(D3D12_DEFAULT);
    CD3DX12_DEPTH_STENCIL_DESC depthStencil(D3D12_DEFAULT);
    CD3DX12_RASTERIZER_DESC rasterizer(D3D12_DEFAULT);
    rasterizer.CullMode = D3D12_CULL_MODE_FRONT;
    auto& depth = g_Textures[g_MainDepthRTs[g_CurrentBackBufferIndex]];
    DirectX::RenderTargetState renderTarget(DXGI_FORMAT_R8G8B8A8_UNORM,
                                            depth.format);
    DirectX::EffectPipelineStateDescription desc(
        &inputLayout, blend, depthStencil, rasterizer, renderTarget);
    ID3D10Blob* vsblob = g_Shaders[vs];
    D3D12_SHADER_BYTECODE _vs = {vsblob->GetBufferPointer(),
                                 vsblob->GetBufferSize()};
    ID3D10Blob* psblob = g_Shaders[ps];
    D3D12_SHADER_BYTECODE _ps = {psblob->GetBufferPointer(),
                                 psblob->GetBufferSize()};

    ID3D12PipelineState* pso = nullptr;
    desc.CreatePipelineState(g_Device, g_TestRootSignature.Get(), _vs, _ps,
                             &pso);
    g_PSOCache.psos[hash] = pso;
    return pso;
}

void CreateRenderTarget() {
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
        ID3D12Resource* pBackBuffer = NULL;
        g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        g_Device->CreateRenderTargetView(pBackBuffer, NULL,
                                         g_mainRenderTargetDescriptor[i]);
        auto name = fmt::format(L"SwapChainBackBuffer{}", i);
        SetDebugObjectName(pBackBuffer, name.c_str());
        g_mainRenderTargetResource[i] = pBackBuffer;
    }
}

void CleanupRenderTarget() {
    WaitForLastSubmittedFrame();

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
        SAFE_RELEASE(g_mainRenderTargetResource[i]);
    }
}

void WaitForLastSubmittedFrame() {
    FrameContext* frameCtxt =
        &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtxt->FenceValue;
    if (fenceValue == 0) return;  // No fence was signaled

    frameCtxt->FenceValue = 0;
    if (g_fence->GetCompletedValue() >= fenceValue) return;

    g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
    WaitForSingleObject(g_fenceEvent, INFINITE);
}

FrameContext* WaitForNextFrameResources() {
    UINT nextFrameIndex = g_frameIndex + 1;
    g_frameIndex = nextFrameIndex;

    HANDLE waitableObjects[] = {g_hSwapChainWaitableObject, NULL};
    DWORD numWaitableObjects = 1;

    FrameContext* frameCtxt =
        &g_frameContext[nextFrameIndex % NUM_FRAMES_IN_FLIGHT];
    UINT64 fenceValue = frameCtxt->FenceValue;
    if (fenceValue != 0)  // means no fence was signaled
    {
        frameCtxt->FenceValue = 0;
        g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
        waitableObjects[1] = g_fenceEvent;
        numWaitableObjects = 2;
    }

    WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

    return frameCtxt;
}

void ResizeSwapChain(HWND hWnd, int width, int height) {
    DXGI_SWAP_CHAIN_DESC1 sd;
    g_pSwapChain->GetDesc1(&sd);
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;
    sd.Width = width;
    sd.Height = height;
    g_SwapChainWidth = width;
    g_SwapChainHeight = height;

    // IDXGIFactory4* dxgiFactory = NULL;
    // g_pSwapChain->GetParent(IID_PPV_ARGS(&dxgiFactory));

    // g_pSwapChain->Release();
    // CloseHandle(g_hSwapChainWaitableObject);

    // IDXGISwapChain1* swapChain1 = NULL;
    // dxgiFactory->CreateSwapChainForHwnd(g_pCommandQueue, hWnd, &sd, NULL,
    // NULL,
    //                                    &swapChain1);
    // swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain));
    // swapChain1->Release();
    // dxgiFactory->Release();

    // g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);

    // g_hSwapChainWaitableObject =
    // g_pSwapChain->GetFrameLatencyWaitableObject();
    // assert(g_hSwapChainWaitableObject != NULL);

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    ThrowIfFailed(g_pSwapChain->GetDesc(&swapChainDesc));
    ThrowIfFailed(g_pSwapChain->ResizeBuffers(
        NUM_FRAMES_IN_FLIGHT, width, height, swapChainDesc.BufferDesc.Format,
        swapChainDesc.Flags));
    g_CurrentBackBufferIndex = g_pSwapChain->GetCurrentBackBufferIndex();
    CreateRenderTarget();
}

struct ComputeShader {
    static ComputeShader* Find(const char* name);

    struct Kernel {
        std::string name;
        ShaderHandle handle;
        // ShaderUniformSignature signature;
    };
    std::vector<Kernel> m_Kernels;
    std::string m_Name;
};

std::string ApplicationFilePath();

ComputeShader* ComputeShader::Find(const char* name) {
    ComputeShader* s = new ComputeShader;
    s->m_Name = name;
    s->m_Kernels.emplace_back();
    std::string path =
        R"(E:\workspace\cengine\engine\shaders\runtime\Internal-Skinning_cs.cso)";
    s->m_Kernels[0].handle = CreateShaderFromCompiledFile(path.c_str());
    return s;
}

inline D3D12_SHADER_BYTECODE TranslateS(ShaderHandle handle) {
    ID3D10Blob* blob = g_Shaders[handle];
    return {blob->GetBufferPointer(), blob->GetBufferSize()};
}

struct GPUSkinningPass {
    ComPtr<ID3D12RootSignature> rootSignature;
    ComputeShader* shader = nullptr;
    ComPtr<ID3D12PipelineState> pso;

    ~GPUSkinningPass() { Clean(); }

    void Clean() {
        rootSignature.Reset();
        pso.Reset();
        SAFE_DELETE(shader);
        init = false;
    }

    bool init = false;
    void Init() {
        if (init) return;
        shader = ComputeShader::Find("Internal-Skinning");

        CD3DX12_ROOT_PARAMETER1 rootParameters[5];
        rootParameters[0].InitAsConstants(1, 0);
        rootParameters[1].InitAsShaderResourceView(0);
        rootParameters[2].InitAsShaderResourceView(1);
        rootParameters[3].InitAsShaderResourceView(2);
        rootParameters[4].InitAsUnorderedAccessView(0);
        rootSignature =
            CreateRootSignatureFromRootParameters(rootParameters, 5);

        D3D12_COMPUTE_PIPELINE_STATE_DESC desc;
        memset(&desc, 0, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));
        desc.pRootSignature = rootSignature.Get();
        desc.CS = TranslateS(shader->m_Kernels[0].handle);
        desc.NodeMask = 0;
        desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        HRESULT hr =
            g_Device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso));
        assert(SUCCEEDED(hr));
        init = true;
    }
};

GPUSkinningPass g_GPUSkinningPass;

inline ID3D12Resource* Translate(BufferHandle handle) {
    return g_Buffers[handle].resource;
}

int GPUSkinning(Renderable* r) {
    BeginRenderEvent("GPUSkinning");
    g_GPUSkinningPass.Init();
    assert(r && r->skin && r->mesh);
    if (!(r && r->skin && r->mesh)) return 1;
    Mesh* mesh = r->mesh;
    auto& skinnedVB = mesh->skinnedvb;
    auto VB = mesh->vb;
    auto skin = mesh->sb;
    auto& bones = r->bonesBuffer;
    if (skinnedVB == 0) {
        Memory m = {};
        m.byteLength = array_get_bytelength(&r->mesh->vertices);
        skinnedVB = CreateBuffer(m, GPUResourceUsageUnorderedAccess);
    }
    {
        Memory m = {};
        m.buffer = r->skin->boneMats.ptr;
        m.byteLength = array_get_bytelength(&r->skin->boneMats);
        if (bones == 0)
            bones = CreateBuffer(m, GPUResourceUsageShaderResource);
        else
            InternalUpdateBuffer(g_Buffers[bones].resource,
                                 g_Buffers[bones].state, m);
    }

    uint32_t vertexCount = MeshGetVertexCount(mesh);

    g_Buffers[VB].Transition(
        g_pCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    g_Buffers[skin].Transition(g_pCommandList,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    g_Buffers[bones].Transition(g_pCommandList,
                                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    g_Buffers[skinnedVB].Transition(g_pCommandList,
                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    g_GPUSkinningPass.Init();
    g_pCommandList->SetPipelineState(g_GPUSkinningPass.pso.Get());
    g_pCommandList->SetComputeRootSignature(
        g_GPUSkinningPass.rootSignature.Get());
    g_pCommandList->SetComputeRoot32BitConstant(0, vertexCount, 0);
    g_pCommandList->SetComputeRootShaderResourceView(
        1, g_Buffers[r->mesh->vb].resource->GetGPUVirtualAddress());
    g_pCommandList->SetComputeRootShaderResourceView(
        2, Translate(skin)->GetGPUVirtualAddress());
    g_pCommandList->SetComputeRootShaderResourceView(
        3, Translate(bones)->GetGPUVirtualAddress());
    g_pCommandList->SetComputeRootUnorderedAccessView(
        4, Translate(skinnedVB)->GetGPUVirtualAddress());
    UINT x = static_cast<UINT>(std::ceil(vertexCount / 64.0f));
    g_pCommandList->Dispatch(x, 1, 1);
    EndRenderEvent();
    return 0;
}

void CleanupDeviceD3D() {
    { g_GPUResourceUploader->End(g_pCommandQueue); }
    g_GPUSkinningPass.Clean();
    for (auto& b : g_Buffers) {
        SAFE_RELEASE(b.resource);
    }
    CleanupRenderTarget();
    SAFE_RELEASE(g_pSwapChain);
    if (g_hSwapChainWaitableObject != NULL) {
        CloseHandle(g_hSwapChainWaitableObject);
    }
    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_frameContext[i].CommandAllocator) {
            g_frameContext[i].CommandAllocator->Release();
            g_frameContext[i].CommandAllocator = NULL;
        }
    for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i) {
        g_Textures[i].resource = nullptr;
    }
    for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i) {
        g_CBVInFlight[i].clear();
    }
    for (int i = NUM_FRAMES_IN_FLIGHT; i < g_Textures.size(); ++i) {
        SAFE_RELEASE(g_Textures[i].resource);
    }
    SAFE_RELEASE(g_pCommandQueue);
    SAFE_RELEASE(g_pCommandList);
    // SAFE_RELEASE(g_TestPSO);
    for (auto& [k, v] : g_PSOCache.psos) {
        SAFE_RELEASE(v);
    }
    g_TestRootSignature.Reset();
    SAFE_DELETE(g_CBVMemory);
    SAFE_DELETE(g_GPUResourceUploader);
    SAFE_DELETE(g_pRtvDescHeap);
    SAFE_DELETE(g_pSrvDescHeap);
    SAFE_DELETE(g_RTVDescriptorHeap);
    SAFE_DELETE(g_DSVDescriptorHeap);
    SAFE_DELETE(g_StaticSamplerDescriptorHeap);
    SAFE_DELETE(g_StaticSrvDescriptorHeap);
    SAFE_DELETE(g_StaticUavDescriptorHeap);
    for (auto& heap : g_DynamicSrvUavDescriptorHeaps) {
        SAFE_DELETE(heap);
    }
    for (auto& heap : g_DynamicSamplerDescriptorHeaps) {
        SAFE_DELETE(heap);
    }
    SAFE_RELEASE(g_fence);
    if (g_fenceEvent) {
        CloseHandle(g_fenceEvent);
        g_fenceEvent = NULL;
    }
    SAFE_RELEASE(g_Device);

#ifdef DX12_ENABLE_DEBUG_LAYER
    IDXGIDebug1* pDebug = NULL;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug)))) {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }
#endif
}