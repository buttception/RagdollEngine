static const float3 g_positions[] = {
	{-1.f, -1.f, 1.f},
	{3.f, -1.f, 1.f},
	{-1.f, 3.f, 1.f}
};
static const float2 g_texcoord[] = {
	{0.f, 1.f},
	{2.f, 1.f},
	{0.f, -1.f}
};

void main_vs(
	in uint inVertexId : SV_VERTEXID,
	out float4 outPos : SV_Position,
	out float2 outTexcoord : TEXCOORD1
)
{
	outPos = float4(g_positions[inVertexId], 1.f);
	outTexcoord = g_texcoord[inVertexId];
}

Texture2D sceneColor : register(t0);
sampler sceneColorSampler : register(t1); 

void main_ps(
    in float4 inPos: SV_Position,
    in float2 inTexcoord : TEXCOORD1,
    out float4 outColor : SV_Target0
)
{
    outColor = sceneColor.Sample(sceneColorSampler, inTexcoord);
}