Shader "pbr"
{
	Properties {
		baseColorFactor("Base Color", Color) = (1, 1, 1, 1)
		metallicFactor("Metallic", Range(0, 1)) = 0
		roughnessFactor("Roughness", Range(0, 1)) = 0
		baseColorTexture("Base Color Texture", 2D) = "white" {}
	}
	SubShader {
		Pass {
			HLSLPROGRAM
			#pragma vertex VS
			#pragma fragment PS
			#pragma target 5.1
			#pragma multi_compile _ HAS_BASECOLORMAP
			#pragma multi_compile _ HAS_METALROUGHNESSMAP
			
			#include "./pbrMetallicRoughness.hlsl"

			ENDHLSL
		}
	}
}