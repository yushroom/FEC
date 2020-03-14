#include <ShaderVariables.hlsl>

TextureCube _Tex;
SamplerState _TexSampler;
float4 _Tex_HDR;
float4 _Tint;
float _SkyDistance;
float _Exposure;
float _Rotation;

#define UNITY_PI 3.1415926536

float4 UnityObjectToClipPos(float3 pos) {
    return mul(MATRIX_MVP, float4(pos, 1));
}

inline float4 ComputeNonStereoScreenPos(float4 pos) {
    float4 o = pos * 0.5f;
    o.xy = float2(o.x, o.y*_ProjectionParams.x) + o.w;
    o.zw = pos.zw;
    return o;
}

float4 ComputeScreenPos(float4 pos) {
    float4 o = ComputeNonStereoScreenPos(pos);
    return o;
}

float3 RotateAroundYInDegrees (float3 vertex, float degrees)
{
    float alpha = degrees * UNITY_PI / 180.0;
    float sina, cosa;
    sincos(alpha, sina, cosa);
    float2x2 m = float2x2(cosa, -sina, sina, cosa);
    return float3(mul(m, vertex.xz), vertex.y).xzy;
}

struct appdata_t {
    float4 vertex : POSITION;
};

struct v2f {
    float4 vertex : SV_POSITION;
    float3 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float3 screenUV : TEXCOORD2;
};

v2f vert (appdata_t v)
{
    v2f o;
    float3 rotated = RotateAroundYInDegrees(v.vertex.xyz, _Rotation);
    o.vertex = UnityObjectToClipPos(rotated);
    o.texcoord = v.vertex.xyz;
    o.worldPos = rotated;
    o.screenUV = ComputeScreenPos(o.vertex).xyw;
    return o;
}

// Decodes HDR textures
// handles dLDR, RGBM formats
inline half3 DecodeHDR (half4 data, half4 decodeInstructions)
{
    // Take into account texture alpha if decodeInstructions.w is true(the alpha value affects the RGB channels)
    half alpha = decodeInstructions.w * (data.a - 1.0) + 1.0;

    // If Linear mode is not supported we can skip exponent part
    #if defined(UNITY_COLORSPACE_GAMMA)
        return (decodeInstructions.x * alpha) * data.rgb;
    #else
    #   if defined(UNITY_USE_NATIVE_HDR)
            return decodeInstructions.x * data.rgb; // Multiplier for future HDRI relative to absolute conversion.
    #   else
            return (decodeInstructions.x * pow(alpha, decodeInstructions.y)) * data.rgb;
    #   endif
    #endif
}

#ifdef UNITY_COLORSPACE_GAMMA
static const float4 unity_ColorSpaceDouble = 2;
#else
static const float4 unity_ColorSpaceDouble = 4.59;
#endif

float4x4 _LastSkyVP;
float2 _Jitter;
void frag (v2f i, out float3 color : SV_TARGET0, out float2 mv : SV_TARGET1)
{
    float2 uv = i.screenUV.xy / i.screenUV.z;
    float4 lastProj = mul(_LastSkyVP, float4(i.worldPos, 1));
    lastProj /= lastProj.w;
    mv = uv - (lastProj.xy * 0.5 + 0.5) + _Jitter;
    float4 tex = _Tex.SampleLevel(_TexSampler, i.texcoord, 0);
    //color = DecodeHDR(tex, _Tex_HDR);
    color = tex.xyz;
    color = color * _Tint.rgb * unity_ColorSpaceDouble.rgb * _Exposure;
}