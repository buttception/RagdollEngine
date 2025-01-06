#include "BasePassCommons.hlsli"
#include "ShadingModel.hlsli"
#include "Utils.hlsli"

cbuffer g_Const : register(b0) {
	float4x4 viewProjMatrix;
	float4x4 viewProjMatrixWithAA;
	float4x4 prevViewProjMatrix;
	float2 RenderResolution;
};

StructuredBuffer<FInstanceData> InstanceDatas : register(t0);
Texture2D Textures[] : register(t0, space1);

void gbuffer_vs(
	in float3 inPos : POSITION,
	in float3 inNormal : NORMAL,
	in float3 inTangent : TANGENT,
	in float2 inTexcoord : TEXCOORD,
	in int inInstanceId : INSTANCEID,
	out float4 outPos : SV_Position,
	out float4 outPrevFragPos : TEXCOORD1,
	out float4 outFragPos : TEXCOORD2,
	out float3 outNormal : TEXCOORD3,
	out float3 outTangent : TEXCOORD4,
	out float3 outBinormal : TEXCOORD5,
	out float2 outTexcoord : TEXCOORD6,
	out nointerpolation uint outInstanceId : TEXCOORD7
)
{
    FInstanceData data = InstanceDatas[inInstanceId];
	outFragPos = mul(float4(inPos, 1), data.ModelToWorld); 
	outPrevFragPos = mul(float4(inPos, 1), data.PrevModelToWorld);
	outPos = mul(outFragPos, viewProjMatrixWithAA);

	float binormalSign = inNormal.x > 0.0f ? -1.0f : 1.0f;
    float3x3 AdjugateMatrix = Adjugate(data.ModelToWorld);
    outNormal = normalize(mul(inNormal, AdjugateMatrix));
    outTangent = normalize(mul(inTangent, AdjugateMatrix));
	outBinormal = normalize(cross(outTangent, outNormal)) * binormalSign;
	outTexcoord = inTexcoord;
    outInstanceId = inInstanceId;
}

StructuredBuffer<FMaterialData> MaterialDatas : register(t1);
sampler Samplers[9] : register(s0);

void gbuffer_ps(
	in float4 inPos : SV_Position,
	in float4 inPrevFragPos : TEXCOORD1,
	in float4 inFragPos : TEXCOORD2,
	in float3 inNormal : TEXCOORD3,
	in float3 inTangent : TEXCOORD4,
	in float3 inBinormal : TEXCOORD5,
	in float2 inTexcoord : TEXCOORD6,
	in uint inInstanceId : TEXCOORD7,
	out float4 outColor : SV_Target0,
	out float2 outNormals: SV_Target1,
	out float2 outRoughnessMetallic: SV_Target2,
	out float2 outVelocity: SV_Target3
)
{
    FInstanceData data = InstanceDatas[inInstanceId];
    FMaterialData materialData = MaterialDatas[data.MaterialIndex];
	// Sample textures
    float4 albedo = materialData.AlbedoFactor;
    if (materialData.AlbedoIndex != -1){
		albedo *= Textures[materialData.AlbedoIndex].Sample(Samplers[materialData.AlbedoSamplerIndex], inTexcoord);
	}
	clip(albedo.a - 0.01f);
	float4 RM = float4(0.f, materialData.RoughnessFactor, materialData.MetallicFactor, 0);
	if(materialData.ORMIndex != -1){
        RM = Textures[materialData.ORMIndex].Sample(Samplers[materialData.ORMSamplerIndex], inTexcoord);
    }
	float ao = 1.f;
	float roughness = RM.g;
	float metallic = RM.b;

	// Sample normal map and leave it in model space, the deferred lighting will calculate this instead
	float3 N = inNormal;
	if(materialData.NormalIndex != -1){
        float3 normalMapValue = normalize(Textures[materialData.NormalIndex].Sample(Samplers[materialData.NormalSamplerIndex], inTexcoord).xyz * 2.0f - 1.0f);
		float3x3 TBN = float3x3(inTangent, inBinormal, inNormal);
		N = normalize(mul(normalMapValue, TBN));
	}

	//draw to the targets
	outColor = albedo;
	outNormals.xy = Encode(N);
	outRoughnessMetallic = float2(roughness, metallic);
	float4 clipPos = mul(inFragPos, viewProjMatrix);
	float4 ndcPos = clipPos / clipPos.w;
	ndcPos.xy = ScreenPosToViewportUV(ndcPos.xy);
	float4 prevClipPos = mul(inPrevFragPos, prevViewProjMatrix);
	float4 prevNdcPos = prevClipPos / prevClipPos.w;
	prevNdcPos.xy = ScreenPosToViewportUV(prevNdcPos.xy);
	//no need div by 2 because i moved it back to viewport so its [0,1]
	outVelocity = (prevNdcPos.xy - ndcPos.xy) * float2(RenderResolution.x, RenderResolution.y);
}

cbuffer g_LightConst : register(b1) {
	float4x4 InvViewProjMatrix;
	float4 LightDiffuseColor;
	float4 SceneAmbientColor;
	float3 LightDirection;
	float LightIntensity;
	float3 CameraPosition;
};

Texture2D albedoTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D RMTexture : register(t2);
Texture2D AOTexture : register(t3);
Texture2D DepthBuffer : register(t4);
Texture2D ShadowMask : register(t5);

void deferred_light_ps(
	in float4 inPos : SV_Position,
	in float2 inTexcoord : TEXCOORD0,
	out float3 outColor : SV_Target0
)
{
	//getting texture values
	float4 albedo = albedoTexture.Sample(Samplers[6], inTexcoord);
	float3 N = Decode(normalTexture.Sample(Samplers[6], inTexcoord).xy);
	float2 RM = RMTexture.Sample(Samplers[6], inTexcoord).xy;
	float AO = AOTexture.Sample(Samplers[6], inTexcoord).x;
	float4 shadowFactor = ShadowMask.Sample(Samplers[6], inTexcoord);

	//getting fragpos
	float3 fragPos = DepthToWorld(DepthBuffer.Sample(Samplers[6], inTexcoord).r, inTexcoord, InvViewProjMatrix);

	//apply pbr lighting, AO is 1.f for now so it does nth
	//float3 diffuse = max(dot(N, LightDirection), 0) * albedo.rgb;
	float3 diffuse = PBRLighting(albedo.rgb, N, CameraPosition - fragPos, LightDirection, LightDiffuseColor.rgb * LightIntensity, RM.y, RM.x, AO);

	float3 ambient = SceneAmbientColor.rgb * albedo.rgb * AO;
	float3 lighting = ambient + diffuse * (1.f - shadowFactor.a);
	outColor = lighting * shadowFactor.rgb;
}