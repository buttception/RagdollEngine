#include "BasePassCommons.hlsli"
#include "ShadingModel.hlsli"
#include "Utils.hlsli"

#define TILE_SIZE 64
#define DEPTH_SLICE_COUNT 17

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
    outTexcoord = float2(inTexcoord.x, inTexcoord.y);
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
#ifdef NON_OPAQUE
	if(materialData.Flags & ALPHA_MODE_MASK)
	{
		clip(albedo.a - materialData.AlphaCutoff);
		albedo.a = 1.f;
	}
	else
		clip(albedo.a <= 0.f ? -1.f : 1.f);	//min clipping
#endif
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
    float4x4 View;
	float4 LightDiffuseColor;
	float4 SceneAmbientColor;
	float3 LightDirection;
	float LightIntensity;
	float3 CameraPosition;
    uint PointLightCount;
    float2 ScreenSize;
    float2 GridSize;
    uint FieldsNeeded;
};

struct PointLightProxy
{
	//xyz is position, w is intensity
    float4 Position;
	//float Intensity;
	//rgb is color, w is maximum range
    float4 Color;
	//float Range;
};

Texture2D albedoTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D RMTexture : register(t2);
Texture2D AOTexture : register(t3);
Texture2D DepthBuffer : register(t4);
Texture2D ShadowMask : register(t5);
StructuredBuffer<PointLightProxy> PointLights : register(t6);

void deferred_light_ps(
	in float4 inPos : SV_Position,
	in float2 inTexcoord : TEXCOORD0,
	out float3 outColor : SV_Target0
)
{
	//getting texture values
    int2 PixelLocation = inTexcoord * ScreenSize;
	float4 albedo = albedoTexture.Load(int3(PixelLocation, 0));
	float3 N = Decode(normalTexture.Load(int3(PixelLocation, 0)).xy);
	float2 RM = RMTexture.Load(int3(PixelLocation, 0)).xy;
	float AO = AOTexture.Load(int3(PixelLocation, 0)).x;
	float4 shadowFactor = ShadowMask.Load(int3(PixelLocation, 0));

	//getting fragpos
    float3 fragPos = DepthToWorld(DepthBuffer.Load(int3(PixelLocation, 0)).r, inTexcoord, InvViewProjMatrix);

	//apply pbr lighting, AO is 1.f for now so it does nth
	//float3 diffuse = max(dot(N, LightDirection), 0) * albedo.rgb;
	float3 diffuse = PBRLighting(albedo.rgb, N, CameraPosition - fragPos, LightDirection, LightDiffuseColor.rgb * LightIntensity, RM.y, RM.x, AO);
	
    float3 ambient = SceneAmbientColor.rgb * albedo.rgb * AO;
    float3 lighting = ambient + diffuse * (1.f - shadowFactor.r);
	
    for (int i = 0; i < PointLightCount; i++)
    {
        float3 LightDir = PointLights[i].Position.xyz - fragPos;
        float Dist = length(LightDir);
        if (PointLights[i].Color.w != 0.f && Dist > PointLights[i].Color.w)
            continue;
        LightDir /= Dist;
        float k1 = 0.7; // Linear term
        float k2 = 1.8; // Quadratic term
        float Attenuation = saturate(1.0 / (1.0 + k1 * Dist + k2 * (Dist * Dist)));
        lighting += PBRLighting(albedo.rgb, N, CameraPosition - fragPos, LightDir, PointLights[i].Color.rgb * PointLights[i].Position.w, RM.y, RM.x, AO) * Attenuation;
    }
	outColor = lighting;
}

//StructuredBuffer<uint> LightBitFieldsInput : register(t7);
StructuredBuffer<float> DepthSliceBoundsViewspaceInput : register(t8);
StructuredBuffer<FBoundingBox> BoundingBoxBufferInput : register(t9);
RWStructuredBuffer<uint> LightBitFieldsInput : register(u0);

void deferred_light_grid_ps(
	in float4 inPos : SV_Position,
	in float2 inTexcoord : TEXCOORD0,
	out float3 outColor : SV_Target0
)
{
	//getting texture values
    int2 PixelLocation = inTexcoord * ScreenSize;
	float4 albedo = albedoTexture.Load(int3(PixelLocation, 0));
	float3 N = Decode(normalTexture.Load(int3(PixelLocation, 0)).xy);
	float2 RM = RMTexture.Load(int3(PixelLocation, 0)).xy;
	float AO = AOTexture.Load(int3(PixelLocation, 0)).x;
	float4 shadowFactor = ShadowMask.Load(int3(PixelLocation, 0));

	//getting the positions
    float depth = DepthBuffer.Load(int3(PixelLocation, 0)).r;
    float3 fragPos = DepthToWorld(depth, inTexcoord, InvViewProjMatrix);
    float3 viewPos = mul(float4(fragPos, 1), View).xyz;

	//apply pbr lighting, AO is 1.f for now so it does nth
	//float3 diffuse = max(dot(N, LightDirection), 0) * albedo.rgb;
    float3 diffuse = PBRLighting(albedo.rgb, N, CameraPosition - fragPos, LightDirection, LightDiffuseColor.rgb * LightIntensity, RM.y, RM.x, AO);
	
    float3 ambient = SceneAmbientColor.rgb * albedo.rgb * AO;
    float3 lighting = ambient + diffuse * (1.f - shadowFactor.r);
	
	//get which slice this pixel is in
    uint X = floor(inTexcoord.x * GridSize.x);
    uint Y = floor((1.f - inTexcoord.y) * GridSize.y);
    uint Z = 0;
    for (int i = 0; i < DEPTH_SLICE_COUNT - 1; ++i)
    {
        FBoundingBox Box = BoundingBoxBufferInput[X + Y * GridSize.x + i * GridSize.x * GridSize.y];
        if (viewPos.z < (Box.Center.z + Box.Extents.z) && viewPos.z > (Box.Center.z - Box.Extents.z))
        {
            Z = i;
            break;
        }
    }
	//get the index to the light list and count
    uint TileIndex = X + Y * GridSize.x + Z * GridSize.x * GridSize.y;
    uint BitFieldIndex = TileIndex * FieldsNeeded;
    for (uint Bucket = 0; Bucket < FieldsNeeded; ++Bucket)
    {
        uint BucketBits = LightBitFieldsInput[BitFieldIndex + Bucket];
        while (BucketBits != 0)
        {
            const uint BucketBitIndex = firstbitlow(BucketBits);
            const uint LightIndex = Bucket * 32 + BucketBitIndex;
            BucketBits ^= 1 << BucketBitIndex;
			
            float3 LightDir = PointLights[LightIndex].Position.xyz - fragPos;
            float Dist = length(LightDir);
            if (PointLights[LightIndex].Color.w != 0.f && Dist > PointLights[LightIndex].Color.w)
                continue;
            LightDir /= Dist;
            float k1 = 0.7; // Linear term
            float k2 = 1.8; // Quadratic term
            float Attenuation = saturate(1.0 / (1.0 + k1 * Dist + k2 * (Dist * Dist)));
            lighting += PBRLighting(albedo.rgb, N, CameraPosition - fragPos, LightDir, PointLights[LightIndex].Color.rgb * PointLights[LightIndex].Position.w, RM.y, RM.x, AO) * Attenuation;
        }
    }
    outColor = lighting;
    //outColor = lighting * float3(X / GridSize.x, Y / GridSize.y, Z / 16.f);
}