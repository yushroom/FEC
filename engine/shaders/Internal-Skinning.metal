#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct SVertInVBO
{
    float4 pos;
    float4 norm;
    float4 tang;
	float4 uv0uv1;
//	packed_float2 uv0;
//	packed_float2 uv1;
};

struct SVertInSkin
{
	uint4 index;
    float4 weight;
};

//uint g_VertCount;

kernel void Internal_Skinning_CS(constant uint& g_VertCount [[buffer(0)]],
								 constant SVertInVBO* g_SourceVBO [[buffer(1)]],
								 constant SVertInSkin* g_SourceSkin [[buffer(2)]],
								 constant float4x4* g_mBones [[buffer(3)]],
								 device SVertInVBO* g_MeshVertsOut [[buffer(4)]],
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
	auto vid = gl_GlobalInvocationID.x;
    auto vert = g_SourceVBO[vid];
    float4 pos = skinMatrix * float4(vert.pos.xyz, 1);
    g_MeshVertsOut[vid].pos = pos.xyzw/pos.w;
    float4 normal = skinMatrix * float4(vert.norm.xyz, 0);
    g_MeshVertsOut[vid].norm = normal;
	// TODO: Tangent
	g_MeshVertsOut[vid].tang = vert.tang;
	g_MeshVertsOut[vid].uv0uv1 = vert.uv0uv1;
}
