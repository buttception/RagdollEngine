cbuffer g_Const : register(b0) {
	float4x4 viewProjMatrix;
	float4 lightDiffuseColor;
	float4 sceneAmbientColor;
	float3 lightDirection;
	int instanceOffset;
	float3 cameraPosition;
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
Texture2D Textures[] : register(t0, space1);

void main_vs(
	in float3 inPos : POSITION,
	in float3 inNormal : NORMAL,
	in float3 inTangent : TANGENT,
	in float2 inTexcoord : TEXCOORD,
	in uint inInstanceId : SV_INSTANCEID,
	out float4 outPos : SV_Position,
	out float4 outFragPos : TEXCOORD1,
	out float3 outNormal : TEXCOORD2,
	out float3 outTangent : TEXCOORD3,
	out float3 outBinormal : TEXCOORD4,
	out float2 outTexcoord : TEXCOORD5,
	out nointerpolation uint outInstanceId : TEXCOORD6
)
{
	InstanceData data = InstanceDatas[inInstanceId + instanceOffset];
	outFragPos = mul(float4(inPos, 1), data.worldMatrix); 
	outPos = mul(outFragPos, viewProjMatrix);

	float binormalSign = inNormal.x > 0.0f ? -1.0f : 1.0f;
	outNormal = normalize(mul(inNormal, transpose((float3x3)data.invWorldMatrix)));
	outTangent = normalize(mul(inTangent, transpose((float3x3)data.invWorldMatrix)));
	outBinormal = normalize(cross(outTangent, outNormal)) * binormalSign;
	outTexcoord = inTexcoord;
	outInstanceId = inInstanceId;
}

sampler Samplers[9] : register(s0);

void main_ps(
	in float4 inPos : SV_Position,
	in float4 inFragPos : TEXCOORD1,
	in float3 inNormal : TEXCOORD2,
	in float3 inTangent : TEXCOORD3,
	in float3 inBinormal : TEXCOORD4,
	in float2 inTexcoord : TEXCOORD5,
	in uint inInstanceId : TEXCOORD6,
	out float4 outColor : SV_Target0
)
{
	InstanceData data = InstanceDatas[inInstanceId + instanceOffset];
	if(data.isLit)
	{
		// Sample textures
		float4 albedo = data.albedoFactor;
		if(data.albedoIndex != -1){
			albedo *= Textures[data.albedoIndex].Sample(Samplers[data.albedoSamplerIndex], inTexcoord);
		}
		float4 RM = float4(data.roughness, data.metallic, 0, 0);
		if(data.roughnessMetallicIndex != -1){
			RM = Textures[data.roughnessMetallicIndex].Sample(Samplers[data.roughnessMetallicSamplerIndex], inTexcoord);
		}
		float roughness = RM.r;
		float metallic = RM.g;

		// Sample normal map and transform to world space
		float3 N = inNormal;
		if(data.normalIndex != -1){
			float3 normalMapValue = normalize(Textures[data.normalIndex].Sample(Samplers[data.normalSamplerIndex], inTexcoord).xyz * 2.0f - 1.0f);
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
		outColor = data.albedoFactor;
	}
}