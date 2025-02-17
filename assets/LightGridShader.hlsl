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

Texture2D<float> DepthBuffer : register(t0);
RWTexture2D<float2> LightGridOutput : register(u0);

groupshared float GroupMinDepth[64]; // One per thread
groupshared float GroupMaxDepth[64];

[numthreads(8, 8, 1)]
void GetTileMinMaxDepth(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    float MinDepth = 1.0f; // Max possible depth
    float MaxDepth = 0.0f; // Min possible depth

    // Each thread processes 64 pixels in the 64x64 tile
    for (uint i = 0; i < 64; i++)
    {
        uint2 PixelCoord = Gid.xy * 64 + GTid.xy * 8 + uint2(i % 8, i / 8);
        float Depth = DepthBuffer.Load(int3(PixelCoord, 0)).r;
        if (Depth == 0.f)
            continue;
        MinDepth = min(MinDepth, Depth);
        MaxDepth = max(MaxDepth, Depth);
    }

    // Store in shared memory
    GroupMinDepth[GIid] = MinDepth;
    GroupMaxDepth[GIid] = MaxDepth;
    GroupMemoryBarrierWithGroupSync();

    // Parallel reduction
    for (uint stride = 32; stride > 0; stride >>= 1)
    {
        if (GIid < stride)
        {
            GroupMinDepth[GIid] = min(GroupMinDepth[GIid], GroupMinDepth[GIid + stride]);
            GroupMaxDepth[GIid] = max(GroupMaxDepth[GIid], GroupMaxDepth[GIid + stride]);
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // Store result
    if (GIid == 0)
    {
        LightGridOutput[Gid.xy] = float2(GroupMinDepth[0], GroupMaxDepth[0]);
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

Texture2D<float2> LightGridInput : register(t1);
StructuredBuffer<FBoundingBox> BoundingBoxBufferInput : register(t2);
StructuredBuffer<PointLightProxy> PointLightBufferInput : register(t3);
RWStructuredBuffer<uint> LightBitFieldsBufferOutput : register(u0);

bool IsLightInsideTile(FBoundingBox Tile, float3 Position, float Range)
{
    //early exit if light is outside the min max depth of the tile
    if (Tile.Center.z - Tile.Extents.z > Position.z + Range || Tile.Center.z + Tile.Extents.z < Position.z - Range)
        return false;
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
    float MinDepth = LightGridInput[DTid.xy].x;
    float MaxDepth = LightGridInput[DTid.xy].y;
    for (int j = 0; j < LightCount; ++j)
    {
        PointLightProxy Light = PointLightBufferInput[j];
        //cull light out if it is outside the depth bounds
        if (Light.Position.z + Light.Color.w < MinDepth || Light.Position.z - Light.Color.w > MaxDepth)
            continue;
        float3 LightViewSpacePosition = mul(float4(Light.Position.xyz, 1.f), View).xyz;
        if (IsLightInsideTile(BoundingBoxBufferInput[DTid.x], LightViewSpacePosition, PointLightBufferInput[j].Color.w))
        {
            const uint BucketIndex = j / 32;
            const uint BucketPlace = j % 32;
            LightBitFieldsBufferOutput[DTid.x * FieldsNeeded + BucketIndex] |= 1 << BucketPlace;
        }
    }
}