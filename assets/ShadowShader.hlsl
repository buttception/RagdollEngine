#include "BasePassCommons.hlsli"

cbuffer g_Const : register(b0) {
    float4x4 LightViewProj;
	uint InstanceOffset;
	uint CascadeIndex;
    uint MeshIndex;
};

StructuredBuffer<FInstanceData> InstanceDatas : register(t0);
void directional_vs(
	in float3 inPos : POSITION,
	in float3 inNormal : NORMAL,
	in float3 inTangent : TANGENT,
	in float2 inTexcoord : TEXCOORD,
	in int inInstanceId : INSTANCEID,
	out float4 outPos : SV_Position
)
{
    FInstanceData data = InstanceDatas[inInstanceId];
	float4 worldPos = mul(float4(inPos, 1), data.ModelToWorld);
	outPos = mul(worldPos, LightViewProj);
}

