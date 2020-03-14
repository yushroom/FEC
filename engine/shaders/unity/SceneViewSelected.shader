Shader "Hidden/SceneViewSelected"
{
	Properties
	{
		_MainTex ("Texture", 2D) = "white" {}
	}
	SubShader
	{
		// No culling or depth
		Cull Off ZWrite Off ZTest Always

		Pass
		{
			ZTest Always Cull Off ZWrite Off

			HLSLPROGRAM
			#pragma vertex Vert
			#pragma fragment Frag
			#pragma target 4.5

			#include "ScreenAlignedQuad.hlsl"

			// sampler2D _MainTex;
			Texture2D _MainTex;
			SamplerState _MainTexSampler;

			float2 _BlurDirection;
			float4 _MainTex_TexelSize;

			static const float2 weight[9] = {
				float2(0.0, 0.0204001982),
				float2(0.0, 0.0577929579),
				float2(0.0, 0.121591687),
				float2(0.0, 0.189985856),
				float2(1.0, 0.220458597),
				float2(0.0, 0.189985856),
				float2(0.0, 0.121591687),
				float2(0.0, 0.0577929579),
				float2(0.0, 0.0204001982),
			};

			float4 Frag(Varyings input) : SV_Target
			{
				float2 v0 = _BlurDirection * _MainTex_TexelSize.xy;
				float4 v1 = float4(0, 0, 0, 0);
				float2 v8 = v0 * -4.0 + input.texCoord;

				[unroll]
				for (int i = 0; i < 9; ++i)
				{
					float4 color = _MainTex.Sample(_MainTexSampler, v8);
					v1 = color * weight[i].xyyx + v1;
					v8 = v0 + v8;
				}
				return v1;
			}

			ENDHLSL
		}

		Pass
		{
			ZTest Always Cull Off ZWrite Off

			HLSLPROGRAM
			#pragma vertex Vert
			#pragma fragment Frag
			#pragma target 4.5

			#include "ScreenAlignedQuad.hlsl"

			// sampler2D _MainTex;
			Texture2D _MainTex;
			SamplerState _MainTexSampler;

			float4 _MainTex_TexelSize;
			float4 _OutlineColor;

			bool4 lessThan(float4 x, float4 y)
			{
				return 1 - step(y, x);
			}

			float4 Frag(Varyings input) : SV_Target
			{
				float2 u_xlat0;
				bool2 u_xlatb0;
				float4 u_xlat10_1;
				float2 u_xlat2;
				bool2 u_xlatb2;
				bool u_xlatb4;
				bool u_xlatb6;
				float2 vs_TEXCOORD0 = input.texCoord;

				u_xlat0.xy = (-_MainTex_TexelSize.xy) * float2(2.0, 2.0) + vs_TEXCOORD0.xy;
				u_xlatb0.xy = lessThan(u_xlat0.xyxx, float4(0.0, 0.0, 0.0, 0.0)).xy;
				u_xlatb0.x = u_xlatb0.y || u_xlatb0.x;
				u_xlat2.xy = _MainTex_TexelSize.xy * float2(2.0, 2.0) + vs_TEXCOORD0.xy;
				u_xlatb2.xy = lessThan(float4(1.0, 1.0, 0.0, 0.0), u_xlat2.xyxx).xy;
				u_xlatb2.x = u_xlatb2.y || u_xlatb2.x;
				u_xlatb0.x = u_xlatb2.x || u_xlatb0.x;
				u_xlat0.x = (u_xlatb0.x) ? 1.0 : _OutlineColor.w;
				u_xlat10_1 = _MainTex.Sample(_MainTexSampler, vs_TEXCOORD0.xy);
				u_xlat2.x = u_xlat10_1.z * 10.0;
				u_xlat2.x = clamp(u_xlat2.x, 0.0, 1.0);
				u_xlatb4 = 0.899999976<u_xlat10_1.w;
				u_xlatb6 = 0.0>=u_xlat10_1.y;
				u_xlat0.x = (u_xlatb4) ? u_xlat0.x : u_xlat2.x;
				u_xlat2.x = u_xlat0.x * 0.300000012;
				u_xlat2.x = (u_xlatb4) ? 0.0 : u_xlat2.x;
				float4 SV_Target0;
				SV_Target0.w = (u_xlatb6) ? u_xlat2.x : u_xlat0.x;
				SV_Target0.xyz = _OutlineColor.xyz;
				return SV_Target0;
			}

			ENDHLSL
		}
	}
	Fallback off
}
