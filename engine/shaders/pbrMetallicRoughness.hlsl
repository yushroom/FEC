// adapted from https://github.com/KhronosGroup/glTF-WebGL-PBR/blob/master/shaders/pbr-frag.glsl
//keyword HAS_OCCLUSIONMAP 0
#include <AppData.hlsl>
#include <ShaderVariables.hlsl>
#include <Common.hlsl>
#include <BRDF.hlsl>
#include <ShadingModels.hlsl>

#define vec2 float2
#define vec3 float3
#define vec4 float4
#define mat3 float3x3
#define dFdx ddx
#define dFdy ddy
#define mix lerp
#define texture2D(t, uv) FESample(t, uv)
#define textureLod(t, uv, lod) FESampleLevel(t, uv, lod)


struct VSToPS
{
	float4 Position : SV_POSITION;
	float3 WorldPosition : POSITION;
	float3 WorldNormal : NORMAL;
    float4 WorldTangent : TANGENT;
	float2 TexCoord : TEXCOORD0;
};

VSToPS VS(AppData vin)
{
	VSToPS vout;
    float4 posH = float4(vin.Position.xyz, 1.0);
	vout.Position = mul(MATRIX_MVP, posH);
	vout.WorldPosition = mul(MATRIX_M, posH).xyz;
	vout.TexCoord = vin.TexCoord;
	vout.WorldNormal = normalize(mul(MATRIX_IT_M, float4(vin.Normal.xyz, 0.0)).xyz);
    vout.WorldTangent.xyz = normalize(mul(MATRIX_M, float4(vin.Tangent.xyz, 0.0)).xyz);
    vout.WorldTangent.w = vin.Tangent.w;
	return vout;
}


float4 baseColorFactor;
float metallicFactor;
float roughnessFactor;

#if USE_IBL
	FETextureCube(DiffuseEnv)
	FETextureCube(SpecularEnv)
	FETexture2D(brdfLUT)
#endif

#if HAS_BASECOLORMAP
	FETexture2D(baseColorTexture)
#endif

#if HAS_NORMALMAP
	FETexture2D(normalTexture)
    float normalScale;
#endif

#if HAS_EMISSIVEMAP
	FETexture2D(emissiveTexture)
#endif
float3 emissiveFactor;

#if HAS_METALROUGHNESSMAP
	FETexture2D(metallicRoughnessTexture)
#endif

#if HAS_OCCLUSIONMAP
	FETexture2D(occlusionTexture)
	float occlusionStrength;
#endif

#if ALPHA_TEST
float alphaCutoff;
#endif


// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.
struct PBRInfo
{
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector
    float VdotH;                  // cos angle between view direction and half vector
    float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    float metalness;              // metallic value at the surface
    vec3 reflectance0;            // full reflectance color (normal incidence angle)
    vec3 reflectance90;           // reflectance color at grazing angle
    float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 diffuseColor;            // color contribution from diffuse lighting
    vec3 specularColor;           // color contribution from specular lighting
};

static const float M_PI = 3.141592653589793;
static const float c_MinRoughness = 0.04;

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
#if MANUAL_SRGB
    #if SRGB_FAST_APPROXIMATION
    vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
    #else //SRGB_FAST_APPROXIMATION
    vec3 bLess = step(vec3(0.04045),srgbIn.xyz);
    vec3 linOut = mix( srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), bLess );
	#endif //SRGB_FAST_APPROXIMATION
    return vec4(linOut,srgbIn.w);;
#else //MANUAL_SRGB
    return srgbIn;
#endif //MANUAL_SRGB
}


#define HAS_NORMALS 1
#define HAS_TANGENTS 0


// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
float3 getNormal(VSToPS pin)
{
//#if HAS_NORMALMAP
//    float2 v_UV = pin.TexCoord;
//    float3 v_Normal = normalize(pin.WorldNormal);
//    float3 v_Tangent = normalize(pin.WorldTangent.xyz);
//    float3 v_Bitangent = cross(v_Normal, v_Tangent) * pin.WorldTangent.w;
//    float3x3 tbn = {v_Tangent, v_Bitangent, v_Normal};
//
//    vec3 n = texture2D(normalTexture, v_UV).rgb;
//    n = normalize(mul((2.0 * n - 1.0) * vec3(normalScale, normalScale, 1.0), tbn));
//    return n;
//#else
//    return normalize(pin.WorldNormal);
//#endif

	float2 v_UV = pin.TexCoord;
	float3 v_Normal = normalize(pin.WorldNormal);
	float3 v_Position = pin.WorldPosition;

	// Retrieve the tangent space matrix
#if !HAS_TANGENTS
    vec3 pos_dx = dFdx(v_Position);
    vec3 pos_dy = dFdy(v_Position);
    vec3 tex_dx = dFdx(vec3(v_UV, 0.0));
    vec3 tex_dy = dFdy(vec3(v_UV, 0.0));
    // vec3 t = (tex_dy.t * pos_dx - tex_dx.t * pos_dy) / (tex_dx.s * tex_dy.t - tex_dy.s * tex_dx.t);
    vec3 t = (tex_dy.y * pos_dx - tex_dx.y * pos_dy) / (tex_dx.x * tex_dy.y - tex_dy.x * tex_dx.y);

#if HAS_NORMALS
    vec3 ng = normalize(v_Normal);
#else
    vec3 ng = cross(pos_dx, pos_dy);
#endif

    t = normalize(t - ng * dot(ng, t));
    vec3 b = normalize(cross(ng, t));
    mat3 tbn = mat3(t, b, ng);
#else // HAS_TANGENTS
    mat3 tbn = v_TBN;
#endif

#if HAS_NORMALMAP
    vec3 n = textureLod(normalTexture, v_UV, 0).rgb;
    // n = normalize(tbn * ((2.0 * n - 1.0) * vec3(normalScale, normalScale, 1.0)));
    n = normalize(mul((2.0 * n - 1.0) * vec3(normalScale, normalScale, 1.0), tbn));
#else
    // The tbn matrix is linearly interpolated, so we need to re-normalize
    vec3 n = normalize(tbn[2].xyz);
#endif

return n;
}


// Calculation of the lighting contribution from an optional Image Based Light source.
// Precomputed Environment Maps are required uniform inputs and are computed as outlined in [1].
// See our README.md on Environment Maps [3] for additional discussion.
#if USE_IBL
vec3 getIBLContribution(PBRInfo pbrInputs, vec3 n, vec3 reflection)
{
    float mipCount = 9.0; // resolution of 512x512
    float lod = (pbrInputs.perceptualRoughness * mipCount);
    // retrieve a scale and bias to F0. See [1], Figure 3
    vec3 brdf = SRGBtoLINEAR(texture2D(u_brdfLUT, vec2(pbrInputs.NdotV, 1.0 - pbrInputs.perceptualRoughness))).rgb;
    vec3 diffuseLight = SRGBtoLINEAR(textureCube(u_DiffuseEnvSampler, n)).rgb;

#if USE_TEX_LOD
    vec3 specularLight = SRGBtoLINEAR(textureCubeLodEXT(u_SpecularEnvSampler, reflection, lod)).rgb;
#else
    vec3 specularLight = SRGBtoLINEAR(textureCube(u_SpecularEnvSampler, reflection)).rgb;
#endif

    vec3 diffuse = diffuseLight * pbrInputs.diffuseColor;
    vec3 specular = specularLight * (pbrInputs.specularColor * brdf.x + brdf.y);

    // For presentation, this allows us to disable IBL terms
    diffuse *= u_ScaleIBLAmbient.x;
    specular *= u_ScaleIBLAmbient.y;

    return diffuse + specular;
}
#endif

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
vec3 diffuse(PBRInfo pbrInputs)
{
    return pbrInputs.diffuseColor / M_PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(PBRInfo pbrInputs)
{
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometricOcclusion(PBRInfo pbrInputs)
{
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(PBRInfo pbrInputs)
{
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (M_PI * f * f);
}


float4 PS(VSToPS pin, bool gl_FrontFacing : SV_IsFrontFace) : SV_Target
{
	float4 gl_FragColor;

	float3 v_Position = pin.WorldPosition;
	float2 v_UV = pin.TexCoord;
	float3 u_LightDirection = -LightDir.xyz;
	float3 u_Camera = WorldSpaceCameraPos.xyz;
	float3 u_LightColor = {1, 1, 1};

    // Metallic and Roughness material properties are packed together
    // In glTF, these factors can be specified by fixed scalar values
    // or from a metallic-roughness map
    float perceptualRoughness = roughnessFactor;
    float metallic = metallicFactor;
#if HAS_METALROUGHNESSMAP
    // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
    // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
    vec4 mrSample = texture2D(metallicRoughnessTexture, v_UV);
    perceptualRoughness = mrSample.g * perceptualRoughness;
    metallic = mrSample.b * metallic;
#endif
    perceptualRoughness = clamp(perceptualRoughness, c_MinRoughness, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);
    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    float alphaRoughness = perceptualRoughness * perceptualRoughness;

    // The albedo may be defined from a base texture or a flat color
#if HAS_BASECOLORMAP
    vec4 baseColor = SRGBtoLINEAR(texture2D(baseColorTexture, v_UV)) * baseColorFactor;
#else
    vec4 baseColor = baseColorFactor;
#endif

#if ALPHA_TEST
    if (baseColor.a < alphaCutoff)
    {
        discard;
    }
#endif

    vec3 f0 = vec3(0.04, 0.04, 0.04);
    vec3 diffuseColor = baseColor.rgb * (vec3(1.0, 1.0, 1.0) - f0);
    diffuseColor *= 1.0 - metallic;
    vec3 specularColor = mix(f0, baseColor.rgb, metallic);

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    vec3 n = getNormal(pin);                             // normal at surface point
	n *= (2.0 * float(gl_FrontFacing) - 1.0);			// fix for dobleSided
    vec3 v = normalize(u_Camera - v_Position);        // Vector from surface point to camera
    vec3 l = normalize(u_LightDirection);             // Vector from surface point to light
    vec3 h = normalize(l+v);                          // Half vector between both l and v
    vec3 reflection = -normalize(reflect(v, n));

    float NdotL = clamp(dot(n, l), 0.001, 1.0);
    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    PBRInfo pbrInputs = {
    	NdotL,
        NdotV,
        NdotH,
        LdotH,
        VdotH,
        perceptualRoughness,
        metallic,
        specularEnvironmentR0,
        specularEnvironmentR90,
        alphaRoughness,
        diffuseColor,
        specularColor
    };

    // Calculate the shading terms for the microfacet specular shading model
    vec3 F = specularReflection(pbrInputs);
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution
    vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);
    vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    vec3 color = NdotL * u_LightColor * (diffuseContrib + specContrib);

    // Calculate lighting contribution from image based lighting source (IBL)
#if USE_IBL
    color += getIBLContribution(pbrInputs, n, reflection);
#endif

    // Apply optional PBR terms for additional (optional) shading
#if HAS_OCCLUSIONMAP
    float ao = texture2D(occlusionTexture, v_UV).r;
    color = mix(color, color * ao, occlusionStrength);
#endif

#if HAS_EMISSIVEMAP
    vec3 emissive = SRGBtoLINEAR(texture2D(emissiveTexture, v_UV)).rgb * emissiveFactor;
    color += emissive;
#else
    color += emissiveFactor;
#endif

#if 0
    // This section uses mix to override final color for reference app visualization
    // of various parameters in the lighting equation.
    color = mix(color, F, u_ScaleFGDSpec.x);
    color = mix(color, vec3(G), u_ScaleFGDSpec.y);
    color = mix(color, vec3(D), u_ScaleFGDSpec.z);
    color = mix(color, specContrib, u_ScaleFGDSpec.w);

    color = mix(color, diffuseContrib, u_ScaleDiffBaseMR.x);
    color = mix(color, baseColor.rgb, u_ScaleDiffBaseMR.y);
    color = mix(color, vec3(metallic), u_ScaleDiffBaseMR.z);
    color = mix(color, vec3(perceptualRoughness), u_ScaleDiffBaseMR.w);
#endif

    gl_FragColor = vec4(ClampedPow(color,(1.0/2.2).xxx), baseColor.a);

    return gl_FragColor;
}
