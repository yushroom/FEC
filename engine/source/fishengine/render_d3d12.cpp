#include "render_d3d12.hpp"

#include <DescriptorHeap.h>
#include <DirectXHelpers.h>
#include <EffectPipelineStateDescription.h>
#include <GraphicsMemory.h>
#include <ResourceUploadBatch.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <dxgi1_4.h>
#include <winerror.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <fmt/format.h>

#include <filesystem>
#include <system_error>
namespace fs = std::filesystem;

#include "rhi.h"
#include "statistics.h"
#include "shader.h"

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
static HANDLE g_hSwapChainWaitableObject = NULL;
static ID3D12Resource* g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE
    g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {};
static DirectX::ResourceUploadBatch* g_GPUResourceUploader = nullptr;
static std::vector<ID3D12Resource*> g_PendingResources[NUM_FRAMES_IN_FLIGHT];
static UINT g_CurrentBackBufferIndex = 0;
static ComPtr<ID3D12RootSignature> g_TestRootSignature;
static DirectX::GraphicsMemory* g_CBVMemory = nullptr;

struct StatedD3D12Resource {
    ID3D12Resource* resource = nullptr;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

    StatedD3D12Resource() = default;
    // StatedD3D12Resource(const StatedD3D12Resource&) = delete;

    //~StatedD3D12Resource() { SAFE_RELEASE(resource); }

    void Transition(ID3D12GraphicsCommandList* cmdList,
                    D3D12_RESOURCE_STATES targetState) {
        if (state != targetState) {
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

static ID3D12Resource* InternalCreateBuffer(uint32_t byteSize, int bufferUsage,
                                            D3D12_RESOURCE_STATES initState) {
    assert(bufferUsage == 0 ||
           bufferUsage == D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
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
    return 1;
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

#include "renderable.h"
struct Renderable;
// struct Transform;
int SimpleDraw(Transform* t, struct Renderable* r) {
    if (!t || !r || !r->mesh) return 0;
    g_pCommandList->SetGraphicsRootSignature(g_TestRootSignature.Get());
    {
        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        auto& b = g_Buffers[r->mesh->vb];
        vbv.BufferLocation = b.resource->GetGPUVirtualAddress();
        vbv.SizeInBytes = b.byteLength;
        vbv.StrideInBytes = sizeof(Vertex);
        g_pCommandList->IASetVertexBuffers(0, 1, &vbv);
    }
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
        // g_pCommandList->DrawIndexedInstanced(MeshGetIndexCount(r->mesh), 1,
        // 0,
        //                                     0, 0);
    } else {
        // g_pCommandList->DrawInstanced(MeshGetVertexCount(r->mesh), 1, 0, 0);
    }
    return 0;
}
int GPUSkinning(struct Renderable* r) { return 1; }
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

    FrameContext* frameCtxt = WaitForNextFrameResources();
    for (ID3D12Resource* res : g_PendingResources[g_CurrentBackBufferIndex]) {
        if (res) {
            res->Release();
        }
    }
    g_PendingResources[g_CurrentBackBufferIndex].clear();

    UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
    frameCtxt->CommandAllocator->Reset();

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        g_mainRenderTargetResource[backBufferIdx], D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    g_pCommandList->Reset(frameCtxt->CommandAllocator, NULL);
    g_pCommandList->ResourceBarrier(1, &barrier);
    g_pCommandList->ClearRenderTargetView(
        g_mainRenderTargetDescriptor[backBufferIdx], (float*)&clear_color, 0,
        NULL);
    g_pCommandList->OMSetRenderTargets(
        1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, NULL);
    ID3D12DescriptorHeap* heaps[] = {g_pSrvDescHeap->Heap()};
    g_pCommandList->SetDescriptorHeaps(1, heaps);
}

void FrameEnd() {
    {
        std::future<void> result = g_GPUResourceUploader->End(g_pCommandQueue);
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
    // g_pSwapChain->Present(0, 0); // Present without vsync

    g_CurrentBackBufferIndex = g_pSwapChain->GetCurrentBackBufferIndex();

    UINT64 fenceValue = g_fenceLastSignaledValue + 1;
    g_pCommandQueue->Signal(g_fence, fenceValue);
    g_fenceLastSignaledValue = fenceValue;

    auto frameCtxt = &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];
    frameCtxt->FenceValue = fenceValue;
}

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

    g_CBVMemory = new DirectX::GraphicsMemory(g_Device);
    g_GPUResourceUploader = new DirectX::ResourceUploadBatch(g_Device);
    g_GPUResourceUploader->Begin();

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

    {
        CD3DX12_ROOT_SIGNATURE_DESC desc;
        CD3DX12_ROOT_PARAMETER1 rootParameters[5];
        rootParameters[0].InitAsConstantBufferView(0);
        g_TestRootSignature =
            CreateRootSignatureFromRootParameters(rootParameters, 1);
    }
    {
        ShaderHandle vs = CreateShaderFromCompiledFile(
            R"(E:\workspace\cengine\engine\shaders\runtime\pbrMetallicRoughness_vs.cso)");
        ReflectShader(g_Shaders[vs], ShaderTypeVertex);
        ShaderHandle ps = CreateShaderFromCompiledFile(
            R"(E:\workspace\cengine\engine\shaders\runtime\pbrMetallicRoughness_ps.cso)");
        ReflectShader(g_Shaders[ps], ShaderTypePixel);
    }
    //{
    //    D3D12_INPUT_LAYOUT_DESC* inputLayout = NULL;
    //    CD3DX12_BLEND_DESC blend(D3D12_DEFAULT);
    //    CD3DX12_DEPTH_STENCIL_DESC depthStencil(D3D12_DEFAULT);
    //    CD3DX12_RASTERIZER_DESC rasterizer(D3D12_DEFAULT);
    //    DirectX::RenderTargetState renderTarget;
    //    DirectX::EffectPipelineStateDescription desc(
    //        inputLayout, blend, depthStencil, rasterizer, renderTarget);
    //    ID3D12PipelineState* pso = NULL;
    //    desc.CreatePipelineState(g_Device, g_TestRootSignature.Get(), , ,
    //    &pso);
    //}
    return true;
}

void CleanupDeviceD3D() {
    { g_GPUResourceUploader->End(g_pCommandQueue); }
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
    SAFE_RELEASE(g_pCommandQueue);
    SAFE_RELEASE(g_pCommandList);
    g_TestRootSignature.Reset();
    SAFE_DELETE(g_CBVMemory);
    SAFE_DELETE(g_GPUResourceUploader);
    SAFE_DELETE(g_pRtvDescHeap);
    SAFE_DELETE(g_pSrvDescHeap);
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

void CreateRenderTarget() {
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
        ID3D12Resource* pBackBuffer = NULL;
        g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        g_Device->CreateRenderTargetView(pBackBuffer, NULL,
                                         g_mainRenderTargetDescriptor[i]);
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
    sd.Width = width;
    sd.Height = height;

    IDXGIFactory4* dxgiFactory = NULL;
    g_pSwapChain->GetParent(IID_PPV_ARGS(&dxgiFactory));

    g_pSwapChain->Release();
    CloseHandle(g_hSwapChainWaitableObject);

    IDXGISwapChain1* swapChain1 = NULL;
    dxgiFactory->CreateSwapChainForHwnd(g_pCommandQueue, hWnd, &sd, NULL, NULL,
                                        &swapChain1);
    swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain));
    swapChain1->Release();
    dxgiFactory->Release();

    g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);

    g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
    assert(g_hSwapChainWaitableObject != NULL);
}
