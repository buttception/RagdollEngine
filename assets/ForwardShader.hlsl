cbuffer g_Const : register(b0) {
	float4x4 worldMatrix;
	float4x4 invWorldMatrix;
	float4x4 viewProjMatrix;
	float4 lightDiffuseColor;
	float4 sceneAmbientColor;
	float4 albedoFactor;
	float3 lightDirection;
	float roughness;
	float3 cameraPosition;
	float metallic;

	int useAlbedo;
	int useNormalMap;
	int useRoughnessMetallicMap;
	int isLit;
};

void main_vs(
	in float3 inPos : POSITION,
	in float4 inColor : COLOR,
	in float3 inNormal : NORMAL,
	in float3 inTangent : TANGENT,
	in float3 inBinormal : BINORMAL,
	in float2 inTexcoord : TEXCOORD,
	out float4 outPos : SV_Position,
	out float4 outColor : COLOR1,
	out float4 outFragPos : POSITION1,
	out float3 outNormal : NORMAL1,
	out float3 outTangent : TANGENT1,
	out float3 outBinormal : BINORMAL1,
	out float2 outTexcoord : TEXCOORD
)
{
	outFragPos = mul(float4(inPos, 1), worldMatrix); 
	outPos = mul(outFragPos, viewProjMatrix);

	outNormal = normalize(mul(inNormal, transpose((float3x3)invWorldMatrix)));
	outTangent = normalize(mul(inTangent, transpose((float3x3)invWorldMatrix)));
	outBinormal = normalize(mul(inBinormal, transpose((float3x3)invWorldMatrix)));
	outTexcoord = inTexcoord;
	outColor = inColor;
}

sampler albedoSampler : register(s0);
Texture2D albedoTexture : register(t0);
sampler normalSampler : register(s1);
Texture2D normalTexture : register(t1);
sampler RMSampler : register(s2);
Texture2D RMTexture : register(t2);

void main_ps(
	in float4 inPos : SV_Position,
	in float4 inColor : COLOR1,
	in float4 inFragPos : POSITION1,
	in float3 inNormal : NORMAL1,
	in float3 inTangent : TANGENT1,
	in float3 inBinormal : BINORMAL1,
	in float2 inTexcoord : TEXCOORD,
	out float4 outColor : SV_Target0
)
{
	if(isLit)
	{
		// Sample textures
		float4 albedo = albedoFactor * inColor;
		if(useAlbedo){
			albedo = albedoTexture.Sample(albedoSampler, inTexcoord) * inColor;
		}
		float4 RM = float4(roughness, metallic, 0, 0);
		if(useRoughnessMetallicMap){
			RM = RMTexture.Sample(RMSampler, inTexcoord);
		}
		float roughness = RM.r;
		float metallic = RM.g;

		// Sample normal map and transform to world space
		float3 N = inNormal;
		if(useNormalMap){
			float3 normalMapValue = normalize(normalTexture.Sample(normalSampler, inTexcoord).xyz * 2.0f - 1.0f);
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
		outColor = inColor;
	}
}