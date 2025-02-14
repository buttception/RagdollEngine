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
    uint LightGridCount;
    uint LightCount;
    uint TextureWidth;
    uint TextureHeight;
    uint FieldsNeeded;
    uint DepthWidth;
    uint DepthHeight;
};

StructuredBuffer<FBoundingBox> BoundingBoxBufferInput : register(t0);
StructuredBuffer<PointLightProxy> PointLightBufferInput : register(t1);
Texture2D<float> DepthBuffer : register(t2);
RWStructuredBuffer<uint> LightBitFieldsBufferOutput : register(u0);
RWTexture3D<uint2> LightGridOutput : register(u1);

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
    const uint Y = TextureHeight - (DTid.x / TextureWidth) % TextureHeight - 1;
    const uint Z = DTid.x / (TextureWidth * TextureHeight);
    //reset the field
    for (int i = 0; i < FieldsNeeded; ++i)
    {
        LightBitFieldsBufferOutput[DTid.x * FieldsNeeded + i] = 0;
    }
    uint IntersectingLightCount = 0;
    //TODO: 2.5d culling, skip if depth range is not in the tile
    for (int j = 0; j < LightCount; ++j)
    {
        float3 LightViewSpacePosition = mul(float4(PointLightBufferInput[j].Position.xyz, 1.f), View).xyz;
        if (IsLightInsideTile(BoundingBoxBufferInput[DTid.x], LightViewSpacePosition, PointLightBufferInput[j].Color.w))
        {
            const uint BucketIndex = j / 32;
            const uint BucketPlace = j % 32;
            LightBitFieldsBufferOutput[DTid.x * FieldsNeeded + BucketIndex] |= 1 << BucketPlace;
            IntersectingLightCount++;
        }
    }
    LightGridOutput[int3(X, Y, Z)].x = IntersectingLightCount;
}