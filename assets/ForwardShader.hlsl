#include "ShadingModel.hlsli"
#include "Utils.hlsli"

cbuffer g_Const : register(b0) {
	float4x4 viewProjMatrix;
	float4x4 viewMatrix;
    float4x4 LightViewProj[4];
	float4 lightDiffuseColor;
	float4 sceneAmbientColor;
	float3 lightDirection;
	int instanceOffset;
	float3 cameraPosition;
	float lightIntensity;
	int enableCascadeDebug;
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
	outBinormal = cross(outTangent, outNormal) * binormalSign;
	outTexcoord = inTexcoord;
	outInstanceId = inInstanceId;
}

Texture2D ShadowMaps[4] : register(t1);
sampler Samplers[8] : register(s0);
SamplerComparisonState ShadowSample : register(s9);

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
		Texture2D shadowMap;
		float4x4 lightMatrix;
		float4 viewPos = mul(inFragPos, viewMatrix);
		float bias;
		// Sample textures
		if(abs(viewPos.z) < 5.f)
		{
			if(enableCascadeDebug)
				data.albedoFactor = float4(1.f, 0.5f, 0.5f, 1.f);
			shadowMap = ShadowMaps[0];
			lightMatrix = LightViewProj[0];
        bias = 0.002f;
		}
		else if(abs(viewPos.z) < 10.f)
		{
			if(enableCascadeDebug)
				data.albedoFactor = float4(1.f, 1.f, 0.5f, 1.f);
			shadowMap = ShadowMaps[1];
			lightMatrix = LightViewProj[1];
        bias = 0.003f;
		}
		else if(abs(viewPos.z) < 15.f)
		{
			if(enableCascadeDebug)
				data.albedoFactor = float4(0.5f, 1.f, 0.5f, 1.f);
			shadowMap = ShadowMaps[2];
			lightMatrix = LightViewProj[2];
        	bias = 0.005f;
		}
		else
		{
			if(enableCascadeDebug)
				data.albedoFactor = float4(1.f, 0.5f, 1.f, 1.f);
			shadowMap = ShadowMaps[3];
			lightMatrix = LightViewProj[3];
        	bias = 0.007f;
		}

		float4 albedo = data.albedoFactor;
		if(data.albedoIndex != -1){
			albedo *= Textures[data.albedoIndex].Sample(Samplers[data.albedoSamplerIndex], inTexcoord);
		}
		clip(albedo.a - 0.01f);
		float4 RM = float4(1.f, data.roughness, data.metallic, 0);
		if(data.roughnessMetallicIndex != -1){
			RM = Textures[data.roughnessMetallicIndex].Sample(Samplers[data.roughnessMetallicSamplerIndex], inTexcoord);
		}
		float ao = 1.f;
		float roughness = RM.g;
		float metallic = RM.b;

		// Sample normal map and transform to world space
		float3 N = inNormal;
		if(data.normalIndex != -1){
			float3 normalMapValue = normalize(Textures[data.normalIndex].Sample(Samplers[data.normalSamplerIndex], inTexcoord).xyz * 2.0f - 1.0f);
			float3x3 TBN = float3x3(inTangent, inBinormal, inNormal);
			N = normalize(mul(normalMapValue, TBN));
		}

		float3 diffuse = PBRLighting(albedo.rgb, N, cameraPosition - inFragPos.xyz, lightDirection, lightDiffuseColor.rgb * lightIntensity, metallic, roughness, ao);

    	float3 projCoord = WorldToLight(inFragPos.xyz, lightMatrix, bias);
		float shadow = 0.f;

		// Percentage-Closer Filtering (PCF) for smoother shadow edges.
		float2 texelSize = float2(1.f, 1.f) / 2000.f;
		for(int x = -1; x <= 1; ++x)
		{
			for(int y = -1; y <= 1; ++y)
			{
				float2 offset = float2(x, y) * texelSize;
				shadow += shadowMap.SampleCmpLevelZero(ShadowSample, projCoord.xy + offset, projCoord.z);
			}
		}
		// Divide by number of samples (9)
		shadow /= 9.f;

		// Combine lighting contributions
		float3 ambient = sceneAmbientColor.rgb * albedo.rgb;
		float3 lighting = ambient + diffuse * (1.f - shadow);

		// Final color output
		outColor = float4(lighting, albedo.a);
	}
	else
	{
		outColor = data.albedoFactor;
	}
}