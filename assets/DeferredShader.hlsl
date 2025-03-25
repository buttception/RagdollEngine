#include "BasePassCommons.hlsli"
#include "ShadingModel.hlsli"
#include "Utils.hlsli"

#define TILE_SIZE 64
#define DEPTH_SLICE_COUNT 17
#define MAX_VERTICES 64
#define MAX_TRIANGLES 124

//thanks chatgpt
static const float3 ColorPalette[32] =
{
    float3(1.0, 0.0, 0.0), // Red
    float3(0.0, 1.0, 0.0), // Green
    float3(0.0, 0.0, 1.0), // Blue
    float3(1.0, 1.0, 0.0), // Yellow
    float3(1.0, 0.0, 1.0), // Magenta
    float3(0.0, 1.0, 1.0), // Cyan
    float3(0.5, 0.0, 0.0), // Dark Red
    float3(0.0, 0.5, 0.0), // Dark Green
    float3(0.0, 0.0, 0.5), // Dark Blue
    float3(0.5, 0.5, 0.0), // Olive
    float3(0.5, 0.0, 0.5), // Purple
    float3(0.0, 0.5, 0.5), // Teal
    float3(1.0, 0.5, 0.0), // Orange
    float3(1.0, 0.0, 0.5), // Pink
    float3(0.5, 1.0, 0.0), // Chartreuse
    float3(0.5, 0.0, 1.0), // Violet
    float3(0.0, 1.0, 0.5), // Spring Green
    float3(0.0, 0.5, 1.0), // Azure
    float3(1.0, 0.75, 0.0), // Goldenrod
    float3(1.0, 0.25, 0.25), // Light Red
    float3(0.25, 1.0, 0.25), // Light Green
    float3(0.25, 0.25, 1.0), // Light Blue
    float3(0.75, 0.75, 0.75), // Light Gray
    float3(0.5, 0.5, 0.5), // Gray
    float3(0.25, 0.25, 0.25), // Dark Gray
    float3(1.0, 0.5, 0.5), // Soft Red
    float3(0.5, 1.0, 0.5), // Soft Green
    float3(0.5, 0.5, 1.0), // Soft Blue
    float3(1.0, 0.75, 0.75), // Pale Red
    float3(0.75, 1.0, 0.75), // Pale Green
    float3(0.75, 0.75, 1.0), // Pale Blue
    float3(0.2, 0.8, 0.6)   // Extra unique color
};

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

struct VertexOutput
{
    float4 outPos : SV_Position;
    float4 outPrevFragPos : TEXCOORD1;
    float4 outFragPos : TEXCOORD2;
    float3 outNormal : TEXCOORD3;
    float3 outTangent : TEXCOORD4;
    float3 outBinormal : TEXCOORD5;
    float2 outTexcoord : TEXCOORD6;
    nointerpolation uint outInstanceId : TEXCOORD7;
};

StructuredBuffer<FVertex> Vertices : register(t2);
StructuredBuffer<FMeshlet> Meshlets : register(t3);
StructuredBuffer<uint> VertexIndices : register(t4);
StructuredBuffer<uint> TriangleIndices : register(t5);

//helpers
uint3 GetPrimitive(FMeshlet m, uint index)
{
    uint packed = TriangleIndices[m.TriangleOffset + index];
    return uint3(
        packed & 0xFF,
        (packed >> 8) & 0xFF,
        (packed >> 16) & 0xFF
    );
}

uint GetVertexIndex(FMeshlet m, uint index)
{
    return VertexIndices[m.VertexOffset + index];
}

VertexOutput GetVertexOutput(uint meshletId, uint vertexIndex)
{
    FVertex v = Vertices[vertexIndex];
	//hardcoded to be first
    FInstanceData data = InstanceDatas[0];
    VertexOutput vout;
    vout.outFragPos = mul(float4(v.position, 1), data.ModelToWorld);
    vout.outPrevFragPos = mul(float4(v.position, 1), data.PrevModelToWorld);
    vout.outPos = mul(vout.outFragPos, viewProjMatrixWithAA);

    float binormalSign = v.normal.x > 0.0f ? -1.0f : 1.0f;
    float3x3 AdjugateMatrix = Adjugate(data.ModelToWorld);
    vout.outNormal = normalize(mul(v.normal, AdjugateMatrix));
    vout.outTangent = normalize(mul(v.tangent, AdjugateMatrix));
    vout.outBinormal = normalize(cross(vout.outTangent, vout.outNormal)) * binormalSign;
    vout.outTexcoord = float2(v.texcoord.x, v.texcoord.y);
    vout.outInstanceId = meshletId;
    return vout;
}

[OutputTopology("triangle")]
[numthreads(MAX_TRIANGLES, 1, 1)]
void gbuffer_ms(uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out indices uint3 triangles[MAX_TRIANGLES], out vertices VertexOutput vertices[MAX_VERTICES])
{
    FMeshlet m = Meshlets[gid];
    SetMeshOutputCounts(m.VertexCount, m.TriangleCount);
    if (gtid < m.TriangleCount)
    {
        triangles[gtid] = GetPrimitive(m, gtid);
    }
    if (gtid < m.VertexCount)
    {
        vertices[gtid] = GetVertexOutput(gid, GetVertexIndex(m, gtid));
    }
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
    FInstanceData data = InstanceDatas[0];
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
    outColor = float4(ColorPalette[inInstanceId % 32], 1.f);
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
		//this is technically wrong as the boxes are right to left and top to bottom, but it is ok as i only need the z value
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