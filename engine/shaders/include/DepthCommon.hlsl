#include "Common.hlsl"

FETexture2D(DepthBuffer);

// float4x4 MATRIX_MVP;
// Values used to linearize the Z buffer (http://www.humus.name/temp/Linearize%20depth.txt)
// x = 1-far/near=(n-f)/n
// y = far/near
// z = x/far=(n-f)/(nf)
// w = y/far=1/n
float4 ZBufferParams;

float4 CascadesSplitPlaneNear;
float4 CascadesSplitPlaneFar;
float4x4 MATRIX_I_VP;
float4 WorldSpaceCameraPos;

// Z buffer to linear 0..1 depth (0 at eye, 1 at far plane)
float Linear01Depth( float z )
{
    return 1.0 / (ZBufferParams.x * z + ZBufferParams.y);
}
// Z buffer to linear depth
float LinearEyeDepth( float z )
{
    //(n * f) / (f - z * (f - n))
    return 1.0 / (ZBufferParams.z * z + ZBufferParams.w);
}

float4 GetWorldPosition(float2 ScreenPos)
{
    // float2 ScreenUV = ScreenPos * 0.5 + 0.5;
    // ScreenUV.y = 1.0 - ScreenUV.y;
    float2 ScreenUV = ScreenPos * float2(0.5, -0.5) + float2(0.5, 0.5);
    float Depth = FESample(DepthBuffer, ScreenUV).r;
    float w = LinearEyeDepth( Depth );
    float4 PosInNDC = float4(ScreenPos.x, ScreenPos.y, Depth, 1.0f);
    PosInNDC *= w;
    float4 PosWorld = mul(PosInNDC, MATRIX_I_VP);
    return PosWorld;
}

float4 GetCascadeWeights( float Depth )
{
    float4 near = step( CascadesSplitPlaneNear, Depth.xxxx );
    float4 far  = step( Depth.xxxx, CascadesSplitPlaneFar);
    return near * far;
}