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
};

StructuredBuffer<FBoundingBox> BoundingBoxBufferInput : register(t0);
StructuredBuffer<PointLightProxy> PointLightBufferInput : register(t1);
RWStructuredBuffer<uint> LightListBufferOutput : register(u0);
RWTexture3D<uint2> LightGrid : register(u1);

groupshared uint LightListCount = 0;
groupshared uint LightListOffset = 0;

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
    if(DTid.x == 0)
        LightListCount = LightListOffset = 0;
    if(DTid.x >= LightCount)
        return;
    GroupMemoryBarrierWithGroupSync();
    //for each light grid cuboid
    int PassCount = LightCount / 64 + (LightCount % 64 > 0 ? 1 : 0);
    for (int i = 0; i < LightGridCount; ++i)
    {
        //for each set of 64 lights;
        for (int j = 0; j < PassCount; ++j)
        {
            uint LightIndex = DTid.x + j * 64;
            if (LightIndex >= LightCount)
                continue;
            //TODO: move this into a outer loop so transform only has to be done once per pass
            float3 LightViewSpacePosition = mul(float4(PointLightBufferInput[LightIndex].Position.xyz, 1.0), View).xyz;
            //test if the light intersects with a frustum tile
            if (IsLightInsideTile(BoundingBoxBufferInput[i], LightViewSpacePosition, PointLightBufferInput[LightIndex].Color.w))
            {
                uint LightListIndex;
                InterlockedAdd(LightListCount, 1, LightListIndex);
                LightListBufferOutput[LightListOffset + LightListIndex] = LightIndex;
            }
        }
        GroupMemoryBarrierWithGroupSync();
        //update the light grid
        if (DTid.x == 0)
        {
            //update the light grid texture
            uint X = i % TextureWidth;
            uint Y = (i / TextureWidth) % TextureHeight;
            uint Z = i / (TextureWidth * TextureHeight);

            LightGrid[uint3(X, Y, Z)].x = LightListOffset;
            LightGrid[uint3(X, Y, Z)].y = LightListCount;
            LightListOffset += LightListCount;
            LightListCount = 0;
        }
        GroupMemoryBarrierWithGroupSync();
    }
}