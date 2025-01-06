cbuffer g_Const : register(b0) {
    float4x4 LightViewProj;
	uint InstanceOffset;
	uint CascadeIndex;
    uint MeshIndex;
};

struct InstanceData{
	float4x4 worldMatrix;
	float4x4 prevWorldMatrix;

    float4 albedoFactor;
    uint meshIndex;
	float roughness;
	float metallic;

	int albedoIndex;
	int albedoSamplerIndex;
	int normalIndex;
	int normalSamplerIndex;
	int roughnessMetallicIndex;
	int roughnessMetallicSamplerIndex;
	int isLit;
};

StructuredBuffer<InstanceData> InstanceDatas : register(t0);
void directional_vs(
	in float3 inPos : POSITION,
	in float3 inNormal : NORMAL,
	in float3 inTangent : TANGENT,
	in float2 inTexcoord : TEXCOORD,
	in int inInstanceId : INSTANCEID,
	out float4 outPos : SV_Position
)
{
    InstanceData data = InstanceDatas[inInstanceId];
	float4 worldPos = mul(float4(inPos, 1), data.worldMatrix);
	outPos = mul(worldPos, LightViewProj);
}

