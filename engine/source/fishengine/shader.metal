#include <simd/simd.h>
#include <metal_stdlib>
using namespace metal;

using float4x4 = simd::float4x4;

typedef struct
{
	float4x4 modelViewProjection;
	float4x4 modelViewMatrix;
	float4x4 modelMatrix;
	float4x4 viewMatrix;
	float4x4 projMatrix;
} Uniforms;

struct Vertex
{
	float4 position [[attribute(0)]];
	float4 normal [[attribute(1)]];
	float4 tangent [[attribute(2)]];
	float2 uv0 [[attribute(3)]];
	float2 uv1 [[attribute(4)]];
//	uint4 index[[attribute(5)]];
//	float4 weight[[attribute(6)]];
};

struct V2F
{
	float4 position [[position]];
	float3 normal;
	float2 uv0;
};

#define MAXBONES 64

struct SkinningMatrices
{
	float4x4 T[MAXBONES];
};

vertex V2F VS(Vertex i [[stage_in]],
			  constant Uniforms &uniforms [[buffer(0)]])
{
	V2F o;
	o.position = uniforms.modelViewProjection * float4(i.position.xyz*float3(1,1,1), 1);
	o.normal = i.normal.xyz*float3(1, 1, 1);
	o.uv0 = i.uv0;
	return o;
}

//vertex V2F VS_Skinned(Vertex i [[stage_in]],
//			  constant Uniforms &uniforms [[buffer(0)]],
//			  constant SkinningMatrices &bones [[buffer(1)]])
//{
//	V2F o;
//	uint4 index = i.index;
//	float4 weight = i.weight;
//	float4x4 skinMat = bones.T[index.x] * weight.x;
//	skinMat += bones.T[index.y] * weight.y;
//	skinMat += bones.T[index.z] * weight.z;
//	skinMat += bones.T[index.w] * weight.w;
////	skinMat *= 1.f / (weight.x + weight.y + weight.z + weight.w);
//	o.position = uniforms.modelViewProjection * skinMat * float4(i.position.xyz*float3(1,1,1), 1);
//	o.normal = i.normal.xyz*float3(1, 1, 1);
//	o.uv0 = i.uv0;
//	return o;
//}


fragment float4 PS(V2F i [[stage_in]], texture2d<float> tex0 [[texture(0)]])
{
//	constexpr sampler s(coord::normalized, address::clamp_to_edge, filter::linear, mip_filter::linear);
	return float4(i.normal*0.5+0.5, 1);
//	return tex0.sample(s, i.uv0);
}

fragment float4 PS_texture(V2F i [[stage_in]],
						   texture2d<float> tex0 [[texture(0)]],
						   sampler s [[sampler(0)]])
{
//	constexpr sampler s(coord::normalized, address::clamp_to_edge, filter::linear, mip_filter::linear);
//	return float4(i.color*0.5+0.5, 1);
	return tex0.sample(s, i.uv0);
}
