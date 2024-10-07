#include "Utils.hlsli"

cbuffer g_Const : register(b0) {
    float Gamma;
    float Exposure;
    int UseFixedExposure;
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
    if(UseFixedExposure == 0)
    {
        float3 yxyColor = convertRGB2Yxy(hdrColor);
        yxyColor.x /= (9.6 * Target[0] + 0.0001);
        hdrColor = convertYxy2RGB(yxyColor);
    }
    else
        hdrColor *= Exposure;
    float3 finalColor = GammaCorrect(ACESFilm(hdrColor * Exposure), Gamma);
    outColor = float4(finalColor, 1.f);
}