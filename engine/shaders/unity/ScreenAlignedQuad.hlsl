// #include "CoreRP/ShaderLibrary/API/D3D11.hlsl"
#define UNITY_UV_STARTS_AT_TOP 1
#define UNITY_NEAR_CLIP_VALUE (1.0)

// #include "CoreRP/ShaderLibrary/Common.hlsl"
// Generates a triangle in homogeneous clip space, s.t.
// v0 = (-1, -1, 1), v1 = (3, -1, 1), v2 = (-1, 3, 1).
float2 GetFullScreenTriangleTexCoord(uint vertexID)
{
#if UNITY_UV_STARTS_AT_TOP
    return float2((vertexID << 1) & 2, 1.0 - (vertexID & 2));
#else
    return float2((vertexID << 1) & 2, vertexID & 2);
#endif
}

float4 GetFullScreenTriangleVertexPosition(uint vertexID, float z = UNITY_NEAR_CLIP_VALUE)
{
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    return float4(uv * 2.0 - 1.0, z, 1.0);
}

struct Attributes
{
    uint vertexID : SV_VertexID;
};

struct Varyings
{
    float4 positionCS : SV_POSITION;
    float2 texCoord   : TEXCOORD0;
};

Varyings Vert(Attributes input)
{
    Varyings output;

    output.positionCS = GetFullScreenTriangleVertexPosition(input.vertexID);
    output.texCoord   = GetFullScreenTriangleTexCoord(input.vertexID);

    return output;
}
