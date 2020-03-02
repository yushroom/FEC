#ifndef ShaderVariables_hlsl
#define ShaderVariables_hlsl

// do not use register(b0)
cbuffer PerDrawUniforms : register(b1)
{
    float4x4 MATRIX_MVP;
    float4x4 MATRIX_MV;
    float4x4 MATRIX_M;
    float4x4 MATRIX_IT_M;
};

cbuffer PerCameraUniforms : register(b2)
{
    float4x4 MATRIX_P;
    float4x4 MATRIX_V;
    float4x4 MATRIX_I_V;
    float4x4 MATRIX_VP;

    float4 WorldSpaceCameraPos;     // w = 1, not used
    float4 WorldSpaceCameraDir;     // w = 0, not used
};

cbuffer LightingUniforms : register(b3)
{
    float4 LightPos;        // w=1 not used
    float4 LightDir;        // w=0 not used
}

#endif // ShaderVariables_hlsl