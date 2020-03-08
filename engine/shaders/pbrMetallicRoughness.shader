Shader "pbr"
{
	Properties {
		baseColorFactor("Base Color", Color) = (1, 1, 1, 1)
		metallicFactor("Metallic", Range(0, 1)) = 0
		roughnessFactor("Roughness", Range(0, 1)) = 0
		baseColorTexture("Base Color Texture", 2D) = "white" {}
		emissiveFactor("Emissive", Color) = (0, 0, 0, 1)
		alphaCutoff("Alpha cutoff", Range(0, 1)) = 0
	}
	SubShader {
		Pass {
			HLSLPROGRAM
			#pragma vertex VS
			#pragma fragment PS
			#pragma target 5.1
			#pragma multi_compile _ HAS_BASECOLORMAP
			#pragma multi_compile _ HAS_NORMALMAP
			#pragma multi_compile _ HAS_METALROUGHNESSMAP
			#pragma multi_compile _ HAS_EMISSIVEMAP
			#pragma multi_compile _ HAS_OCCLUSIONMAP
			#pragma multi_compile _ ALPHA_TEST
			
			#include "./pbrMetallicRoughness.hlsl"

			ENDHLSL
		}
	}
}