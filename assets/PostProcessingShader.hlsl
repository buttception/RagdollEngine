#include "Utils.hlsli"

cbuffer g_Const : register(b0) {
    float Gamma;
    float Exposure;
};

Texture2D SceneColor : register(t0);
sampler Sampler : register(s0);
RWBuffer<float> Target : register(u0);

void tone_map_ps(
	in float4 inPos : SV_Position,
	in float2 inTexcoord : TEXCOORD0,
	out float4 outColor : SV_Target0
)
{
	float3 hdrColor = SceneColor.Sample(Sampler, inTexcoord).rgb;
    float3 finalColor = GammaCorrect(ACESFilm(hdrColor * Exposure), Gamma);
    outColor = float4(finalColor, 1.f);
}