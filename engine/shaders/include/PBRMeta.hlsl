#ifndef _PBRMeta_hlsl_
#define _PBRMeta_hlsl_

#include <AppData.hlsl>
#include <ShaderVariables.hlsl>

struct VSToPS
{
	float4 Position : SV_POSITION;
	float3 WorldPosition : POSITION;
	float3 WorldNormal : NORMAL;
	float2 TexCoord : TEXCOORD0;
};

VSToPS VS(AppData vin)
{
	VSToPS vout;
	vout.Position = mul(float4(vin.Position, 1.0), MATRIX_MVP);
	vout.WorldPosition = mul(float4(vin.Position, 1.0), MATRIX_M).xyz;
	vout.TexCoord = vin.TexCoord;
	vout.WorldNormal = normalize(mul(float4(vin.Normal, 0.0), MATRIX_IT_M).xyz);

	return vout;
}

float4 baseColorFactor;
float Metallic;
float Roughness;
float Specular;

#if 0
Texture2D baseColorTexture;
SamplerState baseColorTextureSampler;

Texture2D metallicRoughnessTexture;
SamplerState metallicRoughnessSampler;

Texture2D emissiveTexture;
SamplerState emissiveSampler;

Texture2D normalTexture;
SamplerState normalSampler;

Texture2D occlusionTexture;
SamplerState occlusionSampler;
#else
#define FETexture2D(name) Texture2D name##Texture; SamplerState name##Sampler;
FETexture2D(baseColor)
FETexture2D(metallicRoughness)
FETexture2D(emissive)
FETexture2D(normal)
FETexture2D(occlusion)
#endif

struct SurfaceData
{
	float3 L;
	float3 V;
	float3 N;
	float2 UV;
	// float Depth;

	float3 BaseColor;
	float Metallic;
	float Roughness;
	float Specular;
};


const static float ambient = 0.2f;

float4 SurfacePS(SurfaceData surfaceData);

float4 PS(VSToPS pin) : SV_Target
{
	float4 gl_FragColor;
	// gl_FragColor = texture2D(baseColorTexture, v_texcoord0);
	// gl_FragColor = baseColorTexture.Sample(baseColorTextureSampler, pin.v_texcoord0);
	// gl_FragColor *= baseColorFactor;

	SurfaceData data;
	data.UV = pin.TexCoord;
	data.L = -normalize(LightDir.xyz);
	data.V = normalize(WorldSpaceCameraPos.xyz - pin.WorldPosition);
	data.N = normalize(pin.WorldNormal);
	// data.UV = pin.v_texcoord0;
#if 1
	data.BaseColor = baseColorTexture.Sample(baseColorSampler, pin.TexCoord).rgb;
	// data.BaseColor *= baseColorFactor.rgb;
#else
	data.BaseColor = baseColorFactor.rgb;
#endif
#if 1
	float4 c = metallicRoughnessTexture.Sample(metallicRoughnessSampler, pin.TexCoord);
	data.Metallic = c.b;
	data.Roughness = c.g;
#else
	data.Metallic = Metallic;
	data.Roughness = Roughness;
#endif
	data.Specular = Specular;
	// data.Specular = 1.0;
	return SurfacePS(data);
}

#include <Common.hlsl>
#include <BRDF.hlsl>
#include <ShadingModels.hlsl>

#if 0
// example
float4 SurfacePS(SurfaceData s)
{
	float4 outColor = {0, 0, 0, 1};
	float3 L = s.L;
	float3 V = s.V;
	float3 N = s.N;
	float3 R0 = 2 * dot(V, N) * N - V;
	float3 LightColor = float3(1, 1, 1);

	float3 DiffuseColor = s.BaseColor - s.BaseColor * s.Metallic;
	float3 SpecularColor = lerp( float3(0.08, 0.08, 0.08)*s.Specular, s.BaseColor, s.Metallic);
	float NoL = saturate( dot(N, L) );
	float NoV = saturate( dot(N, V) );
	outColor.rgb = PI * LightColor * NoL * StandardShading(DiffuseColor, SpecularColor, s.Roughness, L, V, N);

#if 0
	outColor.rgb *= occlusionTexture.Sample(occlusionSampler, s.UV).r;  // ao
	outColor.rgb += emissiveTexture.Sample(emissiveSampler, s.UV).rgb;
#endif
	return outColor;
}
#endif

#endif _PBRMeta_hlsl_