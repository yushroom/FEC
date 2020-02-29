#include "render_metal.h"
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include "camera.h"
#include "component.h"
#include "ecs.h"
#include "light.h"
#include "mesh.h"
#include "rhi.h"
#include "shader.h"
#include "simd_math.h"
#include "statistics.h"
#include "texture.h"
#include "transform.h"

static const size_t MaxBufferCount = 2048;
id<MTLBuffer> g_buffers[MaxBufferCount];
int g_nextBuffer = 1;

id<MTLTexture> g_textures[MaxBufferCount];
int g_nextTexture = 1;

struct ShaderWrap {
    id<MTLFunction> vs;
    id<MTLFunction> ps;
};
ShaderWrap g_shaders[MaxBufferCount];
int g_nextShader = 1;

uint32_t g_freeRT[MaxBufferCount];
int g_freeRTCount = 1;

static const int MaxSamplerCount =
    _FilterModeCount * _TextureWrapModeCount * _TextureWrapModeCount * _TextureWrapModeCount;
id<MTLSamplerState> g_samplers[MaxSamplerCount];

CAMetalLayer *g_layer = NULL;
id<MTLDevice> g_device;
id<MTLLibrary> g_defaultLibrary;
id<MTLComputePipelineState> g_skin_cps;
// id<MTLRenderPipelineState> g_rps;
// id<MTLRenderPipelineState> g_rps_texture;
MTLRenderPassDescriptor *renderPassDescriptor;

static uint32_t g_depthRT = 0;
id<MTLDepthStencilState> g_defaultDS;

static const NSUInteger VertexBufferBindingIndex = 30;
// static const NSUInteger SkinBufferBindingIndex = 29;

id<MTLTexture> g_shadowMap;

uint32_t CreateBuffer(Memory memory) {
    //	assert(g_nextBuffer < MaxBufferCount);
    assert(memory.byteLength > 0);
    uint32_t idx = g_nextBuffer;
    assert(g_buffers[idx] == nil && "ring buffer is full");
    id<MTLBuffer> buffer;
    if (memory.buffer) {
        buffer = [g_device newBufferWithBytes:memory.buffer
                                       length:memory.byteLength
                                      options:MTLResourceStorageModeShared];
    } else {
        buffer = [g_device newBufferWithLength:memory.byteLength
                                       options:MTLResourceStorageModePrivate];
    }
    g_buffers[idx] = buffer;
    g_nextBuffer = (g_nextBuffer + 1) % MaxBufferCount;
    if (g_nextBuffer == 0) g_nextBuffer = 1;

    g_statistics.gpu.bufferCount++;
    g_statistics.gpu.bufferSize += buffer.allocatedSize;

    return idx;
}

void DeleteBuffer(uint32_t bufferID) {
    if (bufferID == 0) return;
    assert(bufferID < MaxBufferCount);
    id<MTLBuffer> buffer = g_buffers[bufferID];
    g_buffers[bufferID] = nil;

    if (buffer) {
        g_statistics.gpu.bufferCount--;
        g_statistics.gpu.bufferSize -= buffer.allocatedSize;
    }
}

void DeleteTexture(uint32_t textureid) {
    if (textureid == 0) return;
    assert(textureid < MaxBufferCount);
    id<MTLTexture> t = g_textures[textureid];
    g_textures[textureid] = nil;

    if (t) {
        g_statistics.gpu.textureCount--;
        g_statistics.gpu.textureSize -= t.allocatedSize;
    }
}

uint32_t CreateShader(const char *path, const char *vs_name, const char *ps_name) {
    NSError *error = NULL;
    id<MTLLibrary> lib = [g_device newLibraryWithFile:[NSString stringWithUTF8String:path]
                                                error:&error];
    if (!lib) {
        NSLog(@"Library error: %@", error.localizedDescription);
        return 0;
    }
    ShaderWrap s;
    s.vs = [lib newFunctionWithName:[NSString stringWithUTF8String:vs_name]];
    s.ps = [lib newFunctionWithName:[NSString stringWithUTF8String:ps_name]];
    int idx = g_nextShader++;
    g_shaders[idx] = s;
    return idx;
}

void DeleteShader(uint32_t shaderID) {
    assert(shaderID < g_nextShader);
    g_shaders[shaderID].vs = nil;
    g_shaders[shaderID].ps = nil;
}

id<MTLTexture> CreateRenderTexture(uint16_t width, uint16_t height, MTLPixelFormat pixelFormat) {
    MTLTextureDescriptor *texDesc = [MTLTextureDescriptor new];
    texDesc.width = width;
    texDesc.height = height;
    texDesc.pixelFormat = pixelFormat;
    texDesc.storageMode = MTLStorageModePrivate;
    texDesc.usage = MTLTextureUsageRenderTarget;
    id<MTLTexture> t = [g_device newTextureWithDescriptor:texDesc];

    g_statistics.gpu.renderTextureCount++;
    g_statistics.gpu.renderTextureSize += t.allocatedSize;

    return t;
}

void DeleteRenderTexture(uint32_t textureid) {
    if (textureid == 0) return;
    assert(textureid < MaxBufferCount);
    id<MTLTexture> t = g_textures[textureid];
    g_textures[textureid] = nil;

    if (t) {
        g_statistics.gpu.renderTextureCount--;
        g_statistics.gpu.renderTextureSize -= t.allocatedSize;
    }
}

static bool IsPowerOfTwo(uint32_t v) { return !(v & (v - 1)); }

uint32_t CreateTexture(uint32_t width, uint32_t height, uint32_t mipmaps, Memory memory) {
    if (!IsPowerOfTwo(width) || !IsPowerOfTwo(height)) return 0;
    MTLTextureDescriptor *texDesc = [MTLTextureDescriptor new];
    texDesc.width = width;
    texDesc.height = height;
    texDesc.mipmapLevelCount = mipmaps;
    texDesc.pixelFormat = MTLPixelFormatBC1_RGBA;
    texDesc.storageMode = MTLStorageModeManaged;
    texDesc.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> t = [g_device newTextureWithDescriptor:texDesc];
    uint32_t w = width, h = height;
    uint8_t *p = (uint8_t *)memory.buffer;
    uint32_t size = 0;
    for (int i = 0; i < mipmaps; ++i) {
        [t replaceRegion:MTLRegionMake2D(0, 0, w, h)
             mipmapLevel:i
               withBytes:p
             bytesPerRow:w * 2];  // bc1 uses 4x4 block
        size += w * h * 2 / 4;
        p += w * h * 2 / 4;
        if (w > 4) w /= 2;
        if (h > 4) h /= 2;
    }
    assert(size == memory.byteLength);
    uint32_t idx = g_nextTexture;
    g_textures[g_nextTexture++] = t;

    g_statistics.gpu.textureCount++;
    g_statistics.gpu.textureSize += t.allocatedSize;

    return idx;
}

// void CreatePipelineState(id<MTLDevice> device, NSString *name, id<MTLFunction> vs,
// id<MTLFunction> fs, MTLVertexDescriptor *vertexDecl)
//{
//	MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
//    desc.label = name;
//    desc.vertexFunction = vs;
//    desc.fragmentFunction = fs;
//    desc.vertexDescriptor = vertexDecl;
//    desc.colorAttachments[0].pixelFormat = g_layer.pixelFormat;
//    desc.depthAttachmentPixelFormat = depthFormat;
//}

MTLSamplerAddressMode ConvertTextureWrapMode(TextureWrapMode m) {
    MTLSamplerAddressMode ret = MTLSamplerAddressModeRepeat;
    switch (m) {
        case TextureWrapModeRepeat:
            ret = MTLSamplerAddressModeRepeat;
            break;
        case TextureWrapModeClamp:
            ret = MTLSamplerAddressModeClampToEdge;
            break;
        case TextureWrapModeMirror:
            ret = MTLSamplerAddressModeMirrorRepeat;
            break;
        case TextureWrapModeMirrorOnce:
            ret = MTLSamplerAddressModeMirrorClampToEdge;
            break;
        default:
            break;
    }
    return ret;
}

MTLVertexDescriptor *vertexDecl;

void RenderInit(CAMetalLayer *layer) {
    g_layer = layer;
    g_device = layer.device;
    g_defaultLibrary = [g_device newDefaultLibrary];

    NSError *error = NULL;
    //    id<MTLLibrary> lib =
    //        [g_device newLibraryWithFile:
    //                      @"/Users/yushroom/program/cengine/engine/shaders/runtime/shaders.metallib"
    //                               error:&error];
    //    if (!lib) {
    //        NSLog(@"Library error: %@", error.localizedDescription);
    //    }

    MTLPixelFormat depthFormat = MTLPixelFormatDepth32Float;
    id<MTLTexture> depthRT = CreateRenderTexture(800, 600, depthFormat);
    g_depthRT = g_nextTexture;
    g_textures[g_nextTexture++] = depthRT;

    MTLDepthStencilDescriptor *dsDesc = [MTLDepthStencilDescriptor new];
    dsDesc.depthCompareFunction = MTLCompareFunctionLess;
    dsDesc.depthWriteEnabled = YES;
    g_defaultDS = [g_device newDepthStencilStateWithDescriptor:dsDesc];

    //    id<MTLFunction> vs = [g_defaultLibrary newFunctionWithName:@"VS"];
    //    id<MTLFunction> fs = [g_defaultLibrary newFunctionWithName:@"PS"];
    //    id<MTLFunction> fs_texture = [g_defaultLibrary newFunctionWithName:@"PS_texture"];

    id<MTLFunction> skin_cs = [g_defaultLibrary newFunctionWithName:@"Internal_Skinning_CS"];

    //    id<MTLFunction> vs2 = [lib newFunctionWithName:@"pbrMetallicRoughness_VS"];
    //    id<MTLFunction> fs2 = [lib newFunctionWithName:@"pbrMetallicRoughness_PS"];

    {
        vertexDecl = [[MTLVertexDescriptor alloc] init];
        NSUInteger offset = 0;
        vertexDecl.attributes[0].format = MTLVertexFormatFloat4;
        vertexDecl.attributes[0].offset = 0;
        vertexDecl.attributes[0].bufferIndex = VertexBufferBindingIndex;
        offset += 4 * 4;
        vertexDecl.attributes[1].format = MTLVertexFormatFloat4;
        vertexDecl.attributes[1].offset = offset;
        vertexDecl.attributes[1].bufferIndex = VertexBufferBindingIndex;
        offset += 4 * 4;
        vertexDecl.attributes[2].format = MTLVertexFormatFloat4;
        vertexDecl.attributes[2].offset = offset;
        vertexDecl.attributes[2].bufferIndex = VertexBufferBindingIndex;
        offset += 4 * 4;
        vertexDecl.attributes[3].format = MTLVertexFormatFloat2;
        vertexDecl.attributes[3].offset = offset;
        vertexDecl.attributes[3].bufferIndex = VertexBufferBindingIndex;
        offset += 2 * 4;
        vertexDecl.attributes[4].format = MTLVertexFormatFloat2;
        vertexDecl.attributes[4].offset = offset;
        vertexDecl.attributes[4].bufferIndex = VertexBufferBindingIndex;
        offset += 2 * 4;
        assert(offset == sizeof(Vertex));
        vertexDecl.layouts[VertexBufferBindingIndex].stride = sizeof(Vertex);
    }

    //    MTLVertexDescriptor *vertexDecl_skinned;
    //    {
    //        MTLVertexDescriptor *vertexDecl = [[MTLVertexDescriptor alloc] init];
    //        NSUInteger offset = 0;
    //        vertexDecl.attributes[0].format = MTLVertexFormatFloat4;
    //        vertexDecl.attributes[0].offset = 0;
    //        vertexDecl.attributes[0].bufferIndex = VertexBufferBindingIndex;
    //        offset += 4 * 4;
    //        vertexDecl.attributes[1].format = MTLVertexFormatFloat4;
    //        vertexDecl.attributes[1].offset = offset;
    //        vertexDecl.attributes[1].bufferIndex = VertexBufferBindingIndex;
    //        offset += 4 * 4;
    //        vertexDecl.attributes[2].format = MTLVertexFormatFloat4;
    //        vertexDecl.attributes[2].offset = offset;
    //        vertexDecl.attributes[2].bufferIndex = VertexBufferBindingIndex;
    //        offset += 4 * 4;
    //        vertexDecl.attributes[3].format = MTLVertexFormatFloat2;
    //        vertexDecl.attributes[3].offset = offset;
    //        vertexDecl.attributes[3].bufferIndex = VertexBufferBindingIndex;
    //        offset += 2 * 4;
    //        vertexDecl.attributes[4].format = MTLVertexFormatFloat2;
    //        vertexDecl.attributes[4].offset = offset;
    //        vertexDecl.attributes[4].bufferIndex = VertexBufferBindingIndex;
    //        offset += 2 * 4;
    //        assert(offset == sizeof(Vertex));
    //        offset = 0;
    //        vertexDecl.attributes[5].format = MTLVertexFormatUInt4;
    //        vertexDecl.attributes[5].offset = offset;
    //        vertexDecl.attributes[5].bufferIndex = SkinBufferBindingIndex;
    //        offset += 4 * 4;
    //        vertexDecl.attributes[6].format = MTLVertexFormatFloat4;
    //        vertexDecl.attributes[6].offset = offset;
    //        vertexDecl.attributes[6].bufferIndex = SkinBufferBindingIndex;
    //        offset += 4 * 4;
    //        assert(offset == sizeof(BoneWeight));
    //        vertexDecl.layouts[VertexBufferBindingIndex].stride = sizeof(Vertex);
    //        vertexDecl.layouts[SkinBufferBindingIndex].stride = sizeof(BoneWeight);
    //        vertexDecl_skinned = vertexDecl;
    //    }

    {
        //		MTLComputePipelineDescriptor * desc = [[MTLComputePipelineDescriptor alloc]
        // init]; 		desc.computeFunction = skin_cs;
        g_skin_cps = [g_device newComputePipelineStateWithFunction:skin_cs error:&error];
    }

    //    MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
    //    desc.label = @"Simple Pipeline";
    //    desc.vertexFunction = vs2;
    //    desc.fragmentFunction = fs2;
    //    desc.vertexDescriptor = vertexDecl;
    //    desc.colorAttachments[0].pixelFormat = g_layer.pixelFormat;
    //    desc.depthAttachmentPixelFormat = depthFormat;
    //
    //    // NSError *error = NULL;
    //    g_rps = [g_device newRenderPipelineStateWithDescriptor:desc error:&error];
    //    if (!g_rps) {
    //        NSLog(@"Failed to create pipeline state: %@", error);
    //        abort();
    //    }

    //    desc.label = @"texture RPS";
    //    desc.vertexFunction = vs;
    //    desc.fragmentFunction = fs_texture;
    //    desc.vertexDescriptor = vertexDecl;
    //    g_rps_texture = [g_device newRenderPipelineStateWithDescriptor:desc error:&error];
    //    if (!g_rps_texture) {
    //        NSLog(@"Failed to create pipeline state: %@", error);
    //        abort();
    //    }

    {
        MTLSamplerDescriptor *desc = [MTLSamplerDescriptor new];
        for (int i = 0; i < _FilterModeCount; ++i) {
            if (i == FilterModePoint) {
                desc.minFilter = MTLSamplerMinMagFilterNearest;
                desc.magFilter = MTLSamplerMinMagFilterNearest;
                desc.mipFilter = MTLSamplerMipFilterNearest;
            } else if (i == FilterModeBilinear) {
                desc.minFilter = MTLSamplerMinMagFilterLinear;
                desc.magFilter = MTLSamplerMinMagFilterLinear;
                desc.mipFilter = MTLSamplerMipFilterNearest;
            } else {  // FilterModeTrilinear
                desc.minFilter = MTLSamplerMinMagFilterLinear;
                desc.magFilter = MTLSamplerMinMagFilterLinear;
                desc.mipFilter = MTLSamplerMipFilterLinear;
            }
            for (int j = 0; j < _TextureWrapModeCount; ++j) {
                desc.sAddressMode = ConvertTextureWrapMode((TextureWrapMode)j);
                for (int k = 0; k < _TextureWrapModeCount; ++k) {
                    desc.tAddressMode = ConvertTextureWrapMode((TextureWrapMode)k);
                    for (int l = 0; l < _TextureWrapModeCount; ++l) {
                        desc.rAddressMode = ConvertTextureWrapMode((TextureWrapMode)l);
                        uint32_t hash = (((((i << 2) | j) << 2) | k) << 2) | l;
                        id<MTLSamplerState> sampler = [g_device newSamplerStateWithDescriptor:desc];
                        assert(hash < MaxSamplerCount && g_samplers[hash] == nil);
                        g_samplers[hash] = sampler;
                    }
                }
            }
        }
    }

    renderPassDescriptor = [MTLRenderPassDescriptor new];

    g_shadowMap = CreateRenderTexture(1024, 1024, MTLPixelFormatDepth32Float);
}

#include <map>

struct PipelineStateCache {
    using Hash = uint32_t;
    std::map<Hash, id<MTLRenderPipelineState>> cached;

    //	Hash GetHash(ShaderHandle vs, ShaderHandle fs)
    //	{
    //		Hash h = vs.id;
    //		constexpr int bits = sizeof(Hash) * 8 / 2;
    //		h <<= bits;
    //		h |= fs.id;
    //		return h;
    //	}
    Hash GetHash(ShaderID shaderID) { return shaderID; }

    id<MTLRenderPipelineState> Get(ShaderID shaderID) {
        Hash hash = GetHash(shaderID);
        auto search = cached.find(hash);
        if (search != cached.end()) {
            return search->second;
        }

        MTLRenderPipelineDescriptor *pipelineStateDescriptor =
            [[MTLRenderPipelineDescriptor alloc] init];
        pipelineStateDescriptor.label = @"Simple Pipeline";
        pipelineStateDescriptor.vertexFunction = g_shaders[shaderID].vs;
        pipelineStateDescriptor.fragmentFunction = g_shaders[shaderID].ps;
        pipelineStateDescriptor.vertexDescriptor = vertexDecl;
        pipelineStateDescriptor.colorAttachments[0].pixelFormat = g_layer.pixelFormat;
        pipelineStateDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

        NSError *error = NULL;
        id<MTLRenderPipelineState> rps =
            [g_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
        if (!rps) {
            NSLog(@"Failed to create pipeline state: %@", error);
            abort();
        }
        cached[hash] = rps;
        return rps;
    }
};

PipelineStateCache g_rpsCache;

id<MTLRenderCommandEncoder> encoder;
id<MTLComputeCommandEncoder> g_computeEncoder;

extern World *defaultWorld;

#include "statistics.h"

id<MTLCommandBuffer> g_currentCommandBuffer;
id<CAMetalDrawable> g_drawable;

void FrameBegin(id<MTLCommandBuffer> cmdBuffer, id<CAMetalDrawable> drawable) {
    g_currentCommandBuffer = cmdBuffer;
    g_drawable = drawable;

    id<MTLTexture> depthRT = g_textures[g_depthRT];
    if (drawable.texture.width != depthRT.width || drawable.texture.height != depthRT.height) {
        DeleteRenderTexture(g_depthRT);
        depthRT = CreateRenderTexture(drawable.texture.width, drawable.texture.height,
                                      depthRT.pixelFormat);
        g_textures[g_depthRT] = depthRT;
    }
    g_statistics.gpu.drawCall = 0;
}

void BeginPass() {
    id<MTLTexture> depthRT = g_textures[g_depthRT];
    const float clear_color[4] = {0.45f, 0.55f, 0.60f, 1.00f};
    renderPassDescriptor.colorAttachments[0].clearColor =
        MTLClearColorMake(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    renderPassDescriptor.colorAttachments[0].texture = g_drawable.texture;
    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    renderPassDescriptor.depthAttachment.texture = depthRT;
    renderPassDescriptor.depthAttachment.clearDepth = 1.f;
    renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
    renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;

    encoder = [g_currentCommandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];

    [encoder setDepthStencilState:g_defaultDS];
    [encoder setCullMode:MTLCullModeBack];
    [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
    MTLViewport viewport = {0, 0, g_layer.drawableSize.width, g_layer.drawableSize.height, 0, 1};
    [encoder setViewport:viewport];
}

#include "renderable.h"
#include "shader_reflect.hpp"

static uint32_t CalcSamplerHash(Texture *t) {
    uint32_t j = t->filterMode;
    j = j << 2;
    j |= t->wrapModeU;
    j = j << 2;
    j |= t->wrapModeV;
    j = j << 2;
    j |= t->wrapModeW;
    return j;
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

int GPUSkinning(Renderable *r) {
    if (!r || !r->skin || !r->mesh) return 0;
    float4x4 bonesT[64] = {};
    if (r->skin) {
        //        for (int i = 0; i < countof(bonesT); ++i) {
        //            bonesT[i] = float4x4_diagonal(1, 1, 1, 1);
        //        }
        memcpy(bonesT, r->skin->boneMats.ptr, array_get_bytelength(&r->skin->boneMats));
    }

    if (r->mesh->skinnedvb == 0) {
        Memory m = {};
        m.byteLength = array_get_bytelength(&r->mesh->vertices);
        r->mesh->skinnedvb = CreateBuffer(m);
    }

    id<MTLComputeCommandEncoder> computeEncoder = [g_currentCommandBuffer computeCommandEncoder];
    computeEncoder.label = @"GPUSkinning";
    [computeEncoder setComputePipelineState:g_skin_cps];
    uint32_t vertexCount = MeshGetVertexCount(r->mesh);
    [computeEncoder setBytes:&vertexCount length:sizeof(vertexCount) atIndex:0];
    [computeEncoder setBuffer:g_buffers[r->mesh->vb] offset:0 atIndex:1];
    [computeEncoder setBuffer:g_buffers[r->mesh->sb] offset:0 atIndex:2];
    [computeEncoder setBytes:&bonesT[0] length:sizeof(bonesT) atIndex:3];
    [computeEncoder setBuffer:g_buffers[r->mesh->skinnedvb] offset:0 atIndex:4];
    MTLSize _threadgroupSize = MTLSizeMake(64, 1, 1);
    MTLSize _threadgroupCount = MTLSizeMake(1, 1, 1);
    _threadgroupCount.width = (int)(vertexCount / 64) + 1;
    [computeEncoder dispatchThreadgroups:_threadgroupCount threadsPerThreadgroup:_threadgroupSize];
    [computeEncoder endEncoding];
    return 0;
}

int SimpleDraw(Transform *t, struct Renderable *r) {
    if (!r || !r->mesh || !r->material) return 1;
    Mesh *mesh = r->mesh;
    Material *material = r->material;

    if (mesh->vb == 0 || mesh->vb >= MaxBufferCount || mesh->ib >= MaxBufferCount) return 1;

    World *w = defaultWorld;
    Camera *camera = CameraGetMainCamera(w);
    Entity cameraEntity = ComponentGetEntity(w, camera, CameraID);
    SingletonTransformManager *tm =
        (SingletonTransformManager *)WorldGetSingletonComponent(w, SingletonTransformManagerID);

    uint32_t tindex = WorldGetComponentIndex(w, t, TransformID);

    float4x4 l2w = TransformGetLocalToWorldMatrix(tm, w, tindex);

    float4x4 view;
    float3 cameraPos, cameraDir;
    {
        cameraDir = TransformGetForward(tm, w, cameraEntity);
        cameraPos = TransformGetPosition(tm, w, cameraEntity);
        float3 up = TransformGetUp(tm, w, cameraEntity);
        view = float4x4_look_to(cameraPos, cameraDir, up);
        //		float4x4 camera2world = TransformGetLocalToWorldMatrix(tm, w, cameraEntity);
        //        view = float4x4_inverse(camera2world); // this view mat contains local scale
        // reverse-z
        //		view = float4x4_mul(view, float4x4_diagonal(1, 1, -1, 1));
    }

    const float aspect = g_layer.drawableSize.width / g_layer.drawableSize.height;
    float4x4 p = float4x4_perspective(camera->fieldOfView, aspect, camera->nearClipPlane,
                                      camera->farClipPlane);
    //    float orthoSize = 5;
    //    float4x4 p = float4x4_ortho(-orthoSize * aspect, orthoSize * aspect, -orthoSize,
    //    orthoSize,
    //                                camera->nearClipPlane, camera->farClipPlane);
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
    g_cbuffers.cb2.WorldSpaceCameraPos = float4_make(cameraPos.x, cameraPos.y, cameraPos.z, 1);
    g_cbuffers.cb2.WorldSpaceCameraDir = float4_make(cameraDir.x, cameraDir.y, cameraDir.z, 0);

    {
        Light *light = (Light *)WorldGetComponentAt(w, LightID, 0);
        Entity lightEntity = ComponentGetEntity(w, light, LightID);
        float3 p = TransformGetPosition(tm, w, lightEntity);
        float3 f = TransformGetForward(tm, w, lightEntity);
        g_cbuffers.cb3.LightPos = float4_make(p.x, p.y, p.z, 1);
        g_cbuffers.cb3.LightDir = float4_make(-f.x, -f.y, -f.z, 0);
    }

    uint32_t indexCount = MeshGetIndexCount(mesh);

    [encoder setRenderPipelineState:g_rpsCache.Get(material->shader->handle)];

    Memory ps_cb0 = MaterialBuildGloblsCB(material);

    ShaderReflect &reflect = ShaderGetReflect(material->shader);
    for (auto &img : reflect.ps.images) {
        Texture *tex = MaterialGetTexture(material, img.nameID);
        //[encoder setFragmentTexture: ]
    }

    if (material && material->mainTexture != NULL) {
        [encoder setFragmentTexture:g_textures[material->mainTexture->handle] atIndex:5];
        uint32_t samplerID = CalcSamplerHash(material->mainTexture);
        assert(samplerID < MaxSamplerCount && g_samplers[samplerID] != nil);
        [encoder setFragmentSamplerState:g_samplers[samplerID] atIndex:6];
    }

    [encoder setVertexBytes:&g_cbuffers.cb1 length:sizeof(g_cbuffers.cb1) atIndex:1];
    [encoder setFragmentBytes:&g_cbuffers.cb1 length:sizeof(g_cbuffers.cb1) atIndex:1];
    [encoder setFragmentBytes:&g_cbuffers.cb2 length:sizeof(g_cbuffers.cb2) atIndex:2];
    [encoder setFragmentBytes:&g_cbuffers.cb3 length:sizeof(g_cbuffers.cb3) atIndex:3];
    [encoder setFragmentBytes:ps_cb0.buffer length:ps_cb0.byteLength atIndex:0];
    if (r->skin && mesh->sb != 0) {
        [encoder setVertexBuffer:g_buffers[mesh->skinnedvb]
                          offset:0
                         atIndex:VertexBufferBindingIndex];
    } else {
        [encoder setVertexBuffer:g_buffers[mesh->vb] offset:0 atIndex:VertexBufferBindingIndex];
    }

    if (mesh->ib != 0) {
        MTLIndexType type = MTLIndexTypeUInt16;
        if (mesh->triangles.stride == 4) type = MTLIndexTypeUInt32;

        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:indexCount
                             indexType:type
                           indexBuffer:g_buffers[mesh->ib]
                     indexBufferOffset:0];
    } else {
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                    vertexStart:0
                    vertexCount:mesh->vertices.size];
    }
    g_statistics.gpu.drawCall++;
    return 0;
}

void FrameEnd() {
    [encoder endEncoding];
    g_drawable = nil;
    g_currentCommandBuffer = nil;
}
