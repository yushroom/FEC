#ifndef AppData_hlsl
#define AppData_hlsl

// struct AppData
// {
// 	float3 Position : POSITION;
// 	float2 TexCoord : TEXCOORD0;
// 	float3 Normal : NORMAL;
// 	float4 Tangent : TANGENT;
// 	//float3 Color : COLOR0;
// };

struct AppData
{
	float4 Position : POSITION;
	float4 Normal : NORMAL;
	float4 Tangent : TANGENT;
	float2 TexCoord : TEXCOORD0;
	float2 TexCoord1 : TEXCOORD1;
};


#endif // AppData_hlsl