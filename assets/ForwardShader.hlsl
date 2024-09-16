cbuffer g_Const : register(b0) {
	float4x4 viewProjMatrix;
	float4 lightDiffuseColor;
	float4 sceneAmbientColor;
	float3 lightDirection;
	float3 cameraPosition;
};

struct InstanceData{
	float4x4 worldMatrix;
	float4x4 invWorldMatrix;

	float4 albedoFactor;
	float roughness;
	float metallic;

	int albedoIndex;
	int normalIndex;
	int roughnessMetallicIndex;
	int isLit;
};

StructuredBuffer<InstanceData> InstanceDatas : register(t0);
Texture2D Textures[] : register(t0, space1);

void main_vs(
	in float3 inPos : POSITION,
	in float4 inColor : COLOR,
	in float3 inNormal : NORMAL,
	in float3 inTangent : TANGENT,
	in float3 inBinormal : BINORMAL,
	in float2 inTexcoord : TEXCOORD,
	in uint inInstanceId : SV_INSTANCEID,
	out float4 outPos : SV_Position,
	out float4 outColor : TEXCOORD1,
	out float4 outFragPos : TEXCOORD2,
	out float3 outNormal : TEXCOORD3,
	out float3 outTangent : TEXCOORD4,
	out float3 outBinormal : TEXCOORD5,
	out float2 outTexcoord : TEXCOORD6,
	out uint outInstanceId : TEXCOORD7
)
{
	outFragPos = mul(float4(inPos, 1), InstanceDatas[inInstanceId].worldMatrix); 
	outPos = mul(outFragPos, viewProjMatrix);

	outNormal = normalize(mul(inNormal, transpose((float3x3)InstanceDatas[inInstanceId].invWorldMatrix)));
	outTangent = normalize(mul(inTangent, transpose((float3x3)InstanceDatas[inInstanceId].invWorldMatrix)));
	outBinormal = normalize(mul(inBinormal, transpose((float3x3)InstanceDatas[inInstanceId].invWorldMatrix)));
	outTexcoord = inTexcoord;
	outColor = inColor;
	outInstanceId = inInstanceId;
}

sampler albedoSampler : register(s0);
sampler normalSampler : register(s1);
sampler RMSampler : register(s2);

void main_ps(
	in float4 inPos : SV_Position,
	in float4 inColor : TEXCOORD1,
	in float4 inFragPos : TEXCOORD2,
	in float3 inNormal : TEXCOORD3,
	in float3 inTangent : TEXCOORD4,
	in float3 inBinormal : TEXCOORD5,
	in float2 inTexcoord : TEXCOORD6,
	in uint inInstanceId : TEXCOORD7,
	out float4 outColor : SV_Target0
)
{
	if(InstanceDatas[inInstanceId].isLit)
	{
		// Sample textures
		float4 albedo = InstanceDatas[inInstanceId].albedoFactor * inColor;
		if(InstanceDatas[inInstanceId].albedoIndex != -1){
			albedo = Textures[InstanceDatas[inInstanceId].albedoIndex].Sample(albedoSampler, inTexcoord) * inColor;
		}
		float4 RM = float4(InstanceDatas[inInstanceId].roughness, InstanceDatas[inInstanceId].metallic, 0, 0);
		if(InstanceDatas[inInstanceId].roughnessMetallicIndex != -1){
			RM = Textures[InstanceDatas[inInstanceId].roughnessMetallicIndex].Sample(RMSampler, inTexcoord);
		}
		float roughness = RM.r;
		float metallic = RM.g;

		// Sample normal map and transform to world space
		float3 N = inNormal;
		if(InstanceDatas[inInstanceId].normalIndex != -1){
			float3 normalMapValue = normalize(Textures[InstanceDatas[inInstanceId].normalIndex].Sample(normalSampler, inTexcoord).xyz * 2.0f - 1.0f);
			float3x3 TBN = float3x3(inTangent, inBinormal, inNormal);
			N = normalize(mul(normalMapValue, TBN));
		}

		float3 diffuse = max(dot(N, lightDirection), 0) * albedo.rgb;

		// Combine lighting contributions
		float3 ambient = sceneAmbientColor.rgb * albedo.rgb;
		float3 lighting = ambient + diffuse;

		// Final color output
		outColor = float4(lighting, albedo.a);
	}
	else
	{
		outColor = InstanceDatas[inInstanceId].albedoFactor * inColor;
	}
}