//#include <metal_stdlib>
//using namespace metal;
//
//struct Vertex1
//{
//	float3 POSITION [[attribute(0)]];
//	uint4 index [[attribute(5)]];
//	float4 weight [[attribute(6)]];
//};
//
//struct Vertex2
//{
//	float3 POSITION [[attribute(0)]];
////	float2 TEXCOORD0 [[attribute(1)]];
////	float3 NORMAL [[attribute(2)]];
//};
//
//constexpr int MAXBONES = 64;
//
//struct SkinningMatrices
//{
//	float4x4 position_transforms[MAXBONES];
//};
//
//kernel void
//GPUSkinning(const device Vertex1* inVertices [[buffer(0)]],
//			device Vertex2* outVertices [[buffer(1)]],
//			constant SkinningMatrices& skinningMatrices [[buffer(3)]],
//			uint id [[thread_position_in_grid]])
//{
//	outVertices[id].POSITION = float3(0, 0, 0);
//	const auto& v = inVertices[id];
//	for (int i = 0; i < 4; ++i)
//	{
//		outVertices[id].POSITION += skinningMatrices[v.index[i]] * v.POSITION * v.weight;
//	}
//}

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct SVertInVBO
{
    packed_float3 pos;
    packed_float2 uv;
    packed_float3 norm;
    packed_float4 tang;
};

struct SVertInSkin
{
    float4 weight;
    uint4 index;
};

kernel void Internal_Skinning_CS(constant uint& g_VertCount [[buffer(0)]],
                  constant SVertInVBO* g_SourceVBO [[buffer(1)]],
                  constant SVertInSkin* g_SourceSkin [[buffer(2)]],
                  device SVertInVBO* g_MeshVertsOut [[buffer(3)]],
                  constant float4x4* g_mBones [[buffer(5)]],
                  uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    if (gl_GlobalInvocationID.x >= g_VertCount)
    {
        return;
    }
    auto skin = g_SourceSkin[gl_GlobalInvocationID.x];
    float4x4 skinMatrix = skin.weight.x * g_mBones[skin.index.x];
    skinMatrix += skin.weight.y * g_mBones[skin.index.y];
    skinMatrix += skin.weight.z * g_mBones[skin.index.z];
    skinMatrix += skin.weight.w * g_mBones[skin.index.w];
    auto vert = g_SourceVBO[gl_GlobalInvocationID.x];
    float4 pos = float4(vert.pos, 1) * skinMatrix;
    g_MeshVertsOut[gl_GlobalInvocationID.x].pos = pos.xyz/pos.w;
    float4 normal = float4(vert.norm, 0) * skinMatrix;
    g_MeshVertsOut[gl_GlobalInvocationID.x].norm = normal.xyz;
}
