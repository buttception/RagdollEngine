cbuffer g_Const : register(b0) {
    float4x4 LightViewProj;
	uint InstanceOffset;
	uint CascadeIndex;
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

void directional_vs(
	in float3 inPos : POSITION,
	in float3 inNormal : NORMAL,
	in float3 inTangent : TANGENT,
	in float2 inTexcoord : TEXCOORD,
	in uint inInstanceId : SV_INSTANCEID,
	out float4 outPos : SV_Position
)
{
	InstanceData data = InstanceDatas[inInstanceId + InstanceOffset];
	float4 worldPos = mul(float4(inPos, 1), data.worldMatrix);
	outPos = mul(worldPos, LightViewProj) * 0.5f;
	if(CascadeIndex == 0){
		outPos.xy += float2(-0.5f, -0.5f);
	}
	else if(CascadeIndex == 1){
		outPos.xy += float2(0.5f, -0.5f);
	}
	else if(CascadeIndex == 2){
		outPos.xy += float2(-0.5f, 0.5f);
	}
	else if(CascadeIndex == 3){
		outPos.xy += float2(0.5f, 0.5f);
	}
}

