cbuffer g_Const : register(b0) {
	float4x4 viewProjMatrix;
    uint InstanceOffset;
};

struct InstanceData{
	float4x4 worldMatrix;
	float4x4 invWorldMatrix;

	float4 albedoFactor;
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

void main_vs(
	in float3 inPos : POSITION,
	in float3 inNormal : NORMAL,
	in float3 inTangent : TANGENT,
	in float2 inTexcoord : TEXCOORD,
	in uint inInstanceId : SV_INSTANCEID,
	out float4 outPos : SV_Position,
	out nointerpolation uint outInstanceId : TEXCOORD1
)
{
	InstanceData data = InstanceDatas[inInstanceId + InstanceOffset];
	float4 fragPos = mul(float4(inPos, 1), data.worldMatrix); 
	outPos = mul(fragPos, viewProjMatrix);
	outInstanceId = inInstanceId;
}

void main_ps(
	in float4 inPos : SV_Position,
	in uint inInstanceId : TEXCOORD1,
	out float4 outColor : SV_Target0
)
{
	InstanceData data = InstanceDatas[inInstanceId + InstanceOffset];
    outColor = data.albedoFactor;
}