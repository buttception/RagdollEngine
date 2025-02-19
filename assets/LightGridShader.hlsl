#include "BasePassCommons.hlsli"

#define MAX_LIGHT_COUNT 128
#define TILE_SIZE 64
#define DEPTH_SLICE_COUNT 17
#define LIGHT_LIST_MAX_SIZE 2024
#define FLT_MAX 3.402823466e+38 

cbuffer g_Const : register(b0)
{
    float4x4 InvProjection;
    uint Width;
    float InvWidth;
    uint Height;
    float InvHeight;
};

StructuredBuffer<float> DepthBounds : register(t0);
RWStructuredBuffer<FBoundingBox> BoundingBoxBufferOutput : register(u0);

[numthreads(8, 8, 1)]
void UpdateBoundingBoxCS(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
    if (DTid.x > Width || DTid.y > Height)
        return;
    for (int i = 1; i < DEPTH_SLICE_COUNT; i++)
    {
        uint Index = (i - 1) * Width * Height + DTid.y * Width + DTid.x;
        float2 Corners[4] =
        {
            float2(DTid.x, DTid.y) * float2(InvWidth, InvHeight),
            float2(DTid.x + 1, DTid.y) * float2(InvWidth, InvHeight),
            float2(DTid.x, DTid.y + 1) * float2(InvWidth, InvHeight),
            float2(DTid.x + 1, DTid.y + 1) * float2(InvWidth, InvHeight)
            
        };
        float3 Min = FLT_MAX.xxx;
        float3 Max = -FLT_MAX.xxx;
        for (int j = 0; j < 4; ++j)
        {
            float4 ProjPos = float4(Corners[j] * 2.f - 1.f.xx, DepthBounds[i - 1], 1.0);
            float4 ViewPos = mul(ProjPos, InvProjection);
            ViewPos /= ViewPos.w;
            Min.x = min(Min.x, ViewPos.x);
            Min.y = min(Min.y, ViewPos.y);
            Min.z = min(Min.z, ViewPos.z);
            Max.x = max(Max.x, ViewPos.x);
            Max.y = max(Max.y, ViewPos.y);
            Max.z = max(Max.z, ViewPos.z);
        }
        for (int k = 0; k < 4; ++k)
        {
            float4 ProjPos = float4(Corners[k] * 2.f - 1.f.xx, DepthBounds[i], 1.0);
            float4 ViewPos = mul(ProjPos, InvProjection);
            ViewPos /= ViewPos.w;
            Min.x = min(Min.x, ViewPos.x);
            Min.y = min(Min.y, ViewPos.y);
            Min.z = min(Min.z, ViewPos.z);
            Max.x = max(Max.x, ViewPos.x);
            Max.y = max(Max.y, ViewPos.y);
            Max.z = max(Max.z, ViewPos.z);
        }
        float3 Center = (Min + Max) * 0.5;
        float3 Extents = (Max - Min) * 0.5;
        BoundingBoxBufferOutput[Index].Center = Center;
        BoundingBoxBufferOutput[Index].Extents = Extents;
    }
}

struct PointLightProxy
{
	//xyz is position, w is intensity
    float4 Position;
	//float Intensity;
	//rgb is color, w is maximum range
    float4 Color;
	//float Range;
};

cbuffer g_Const : register(b0)
{
    float4x4 View;
    float4x4 InverseProjectionWithJitter;
    uint LightGridCount;
    uint LightCount;
    uint TextureWidth;
    uint TextureHeight;
    uint FieldsNeeded;
    uint DepthWidth;
    uint DepthHeight;
    uint MipBaseWidth;
    uint MipBaseHeight;
};

Texture2D<float> DepthBuffer : register(t0);
Texture2D<float2> HZBMips : register(t1);
StructuredBuffer<FBoundingBox> BoundingBoxBufferInput : register(t2);
StructuredBuffer<PointLightProxy> PointLightBufferInput : register(t3);
StructuredBuffer<float> DepthBoundsViewspace : register(t4);
RWStructuredBuffer<uint> LightBitFieldsBufferOutput : register(u0);

sampler SamplerPoint : register(s0);

bool IsLightInsideTile(FBoundingBox Tile, float3 Position, float Range)
{
    float3 CenterToLight = Tile.Center - Position;
    float DistanceSq = dot(CenterToLight, CenterToLight);
    float RadiusSq = (length(Tile.Extents) + Range) * (length(Tile.Extents) + Range);
    return DistanceSq < RadiusSq;
}

[numthreads(64, 1, 1)]
void CullLightsCS(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
    //1 thread 1 tile
    if (DTid.x >= TextureWidth * TextureHeight * (DEPTH_SLICE_COUNT - 1))
        return;
    const uint X = DTid.x % TextureWidth;
    const uint Y = (DTid.x / TextureWidth) % TextureHeight;
    const uint Z = DTid.x / (TextureWidth * TextureHeight);
    //reset the field
    for (int i = 0; i < FieldsNeeded; ++i)
    {
        LightBitFieldsBufferOutput[DTid.x * FieldsNeeded + i] = 0;
    }
    float2 InvRes = float2(1.f / TextureWidth, 1.f / TextureHeight);
    float2 UV = InvRes * float2(X, Y) + InvRes * 0.5f.xx;
    UV.y = 1.f - UV.y;
    //get the correct mip to read from the min max hzb, reduce mip by 1 as hzb is half render size
    float Size = max(InvRes.x * MipBaseWidth, InvRes.y * MipBaseHeight);
    float Mip = max(0, ceil(log2(Size)));
    float2 Depths[4] =
    {
        { HZBMips.SampleLevel(SamplerPoint, UV + float2(-InvRes.x * 0.5f, -InvRes.y * 0.5f), Mip) },
        { HZBMips.SampleLevel(SamplerPoint, UV + float2(InvRes.x * 0.5f, -InvRes.y), Mip) },
        { HZBMips.SampleLevel(SamplerPoint, UV + float2(-InvRes.x * 0.5f, InvRes.y * 0.5f), Mip) },
        { HZBMips.SampleLevel(SamplerPoint, UV + float2(InvRes.x * 0.5f, InvRes.y * 0.5f), Mip) },
    };
    float2 DepthRange = float2(min(min(Depths[0].x, Depths[1].x), min(Depths[2].x, Depths[3].x)), max(max(Depths[0].y, Depths[1].y), max(Depths[2].y, Depths[3].y)));
    //get min max of the tile in view space, the smaller value is max due to reverse Z
    float4 ProjPos = mul(float4(float3(UV, DepthRange.x), 1.0), InverseProjectionWithJitter);
    float MaxZ = ProjPos.z / ProjPos.w;
    ProjPos = mul(float4(float3(UV, DepthRange.y), 1.0), InverseProjectionWithJitter);
    float MinZ = ProjPos.z / ProjPos.w;
    FBoundingBox Tile = BoundingBoxBufferInput[DTid.x];
    //test if the tile is inside the depth range
    if (MaxZ < Tile.Center.z - Tile.Extents.z - 0.001f || MinZ > Tile.Center.z + Tile.Extents.z + 0.001f)
        return;
    for (int j = 0; j < LightCount; ++j)
    {
        PointLightProxy Light = PointLightBufferInput[j];
        float3 LightViewSpacePosition = mul(float4(Light.Position.xyz, 1.f), View).xyz;
        if (LightViewSpacePosition.z + PointLightBufferInput[j].Color.w < MinZ || LightViewSpacePosition.z - PointLightBufferInput[j].Color.w > MaxZ)
            continue;
        if (IsLightInsideTile(Tile, LightViewSpacePosition, PointLightBufferInput[j].Color.w))
        {
            const uint BucketIndex = j / 32;
            const uint BucketPlace = j % 32;
            LightBitFieldsBufferOutput[DTid.x * FieldsNeeded + BucketIndex] |= 1 << BucketPlace;
        }
    }
}