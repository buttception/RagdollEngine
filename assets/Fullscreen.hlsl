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

struct MeshOutput
{
    float4 Position : SV_Position;
    float2 Texcoord : TEXCOORD0;
};

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main_ms(out indices uint3 triangles[1], out vertices MeshOutput vertices[3])
{
    SetMeshOutputCounts(3, 1);
    
    triangles[0] = uint3(0, 1, 2);
	
    vertices[0].Position = float4(g_positions[0], 1.f);
    vertices[0].Texcoord = g_texcoord[0];
	
    vertices[1].Position = float4(g_positions[1], 1.f);
    vertices[1].Texcoord = g_texcoord[1];
    
    vertices[2].Position = float4(g_positions[2], 1.f);
    vertices[2].Texcoord = g_texcoord[2];
}

cbuffer g_Const : register(b0)
{
    float2 TexcoordAdd;
    float2 TexcoordMul;
};

Texture2D Source : register(t0);
sampler Sampler : register(s0); 

void main_ps(
    in float4 inPos: SV_Position,
    in float2 inTexcoord : TEXCOORD0,
    out float4 outColor : SV_Target0
)
{
    outColor = Source.Sample(Sampler, inTexcoord * TexcoordMul + TexcoordAdd).rgba;
}