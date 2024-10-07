#include "Utils.hlsli"

static const float3 g_positions[] = {
	{-1.f, -1.f, 0.f},
	{3.f, -1.f, 0.f},
	{-1.f, 3.f, 0.f}
};
static const float2 g_texcoord[] = {
	{0.f, 1.f},
	{2.f, 1.f},
	{0.f, -1.f}
};

void main_vs(
	in uint inVertexId : SV_VERTEXID,
	out float4 outPos : SV_Position,
	out float2 outTexcoord : TEXCOORD0
)
{
	outPos = float4(g_positions[inVertexId], 1.f);
	outTexcoord = g_texcoord[inVertexId];
}

Texture2D Source : register(t0);
sampler Sampler : register(s0); 

void main_ps(
    in float4 inPos: SV_Position,
    in float2 inTexcoord : TEXCOORD0,
    out float3 outColor : SV_Target0
)
{
    outColor = Source.Sample(Sampler, inTexcoord).rgb;
}