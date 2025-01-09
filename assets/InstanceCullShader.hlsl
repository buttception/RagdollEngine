#include "BasePassCommons.hlsli"
#include "Utils.hlsli"

#define INSTANCE_DATA_BUFFER_SRV_SLOT t1
#define MESH_BUFFER_SRV_SLOT t2

#define INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT u0
#define INSTANCE_ID_BUFFER_UAV_SLOT u1
#define INSTANCE_VISIBLE_COUNT_UAV_SLOT u2
#define INSTANCE_NOT_OCCLUDED_COUNT_UAV_SLOT u3
#define INSTANCE_OCCLUDED_ID_BUFFER_UAV_SLOT u4
#define INSTANCE_OCCLUDED_COUNT_UAV_SLOT u5

#define DEBUG_BUFFER_UAV_SLOT u9

#define INFINITE_Z_ENABLED 1
#define PREVIOUS_FRAME_ENABLED 1 << 1
#define IS_PHASE_1 1 << 2

cbuffer g_Const : register(b0)
{
    float4x4 ViewProjectionMatrix;
    float4 FrustumPlanes[6];
    uint MipBaseWidth;
    uint MipBaseHeight;
    uint MipLevels;
    uint ProxyCount;
    uint Flags;
}

StructuredBuffer<FInstanceData> InstanceDataInput : register(INSTANCE_DATA_BUFFER_SRV_SLOT);
StructuredBuffer<FMeshData> MeshDataInput : register(MESH_BUFFER_SRV_SLOT);

RWStructuredBuffer<FDrawIndexedIndirectArguments> DrawIndexedIndirectArgsOutput : register(INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT);
RWStructuredBuffer<uint> InstanceIdBufferOutput : register(INSTANCE_ID_BUFFER_UAV_SLOT);
RWStructuredBuffer<uint> InstanceVisibleCountOutput : register(INSTANCE_VISIBLE_COUNT_UAV_SLOT);
RWStructuredBuffer<uint> OccludedInstanceIdBufferOutput : register(INSTANCE_OCCLUDED_ID_BUFFER_UAV_SLOT);
RWStructuredBuffer<uint> InstanceNotOccludedCountOutput : register(INSTANCE_NOT_OCCLUDED_COUNT_UAV_SLOT);
RWStructuredBuffer<uint> OccludedCountBufferOutput : register(INSTANCE_OCCLUDED_COUNT_UAV_SLOT);
RWStructuredBuffer<float> DebugBuffer : register(DEBUG_BUFFER_UAV_SLOT);

Texture2D<float> HZBMips : register(t0);
sampler SamplerPoint : register(s0);

[numthreads(64, 1, 1)]
void FrustumCullCS(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
    if (DTid.x >= ProxyCount)
    {
        return;
    }
    FInstanceData InstanceData = InstanceDataInput[DTid.x];
    //each thread will cull 1 proxy
    float3 Center = MeshDataInput[InstanceData.MeshIndex].Center;
    float3 Extents = MeshDataInput[InstanceData.MeshIndex].Extents;
    //view planes are already in world space
    float3 Corners[8] =
    {
        Center + float3(-Extents.x, -Extents.y, -Extents.z),
        Center + float3(-Extents.x, -Extents.y, Extents.z),
        Center + float3(-Extents.x, Extents.y, -Extents.z),
        Center + float3(-Extents.x, Extents.y, Extents.z),
        Center + float3(Extents.x, -Extents.y, -Extents.z),
        Center + float3(Extents.x, -Extents.y, Extents.z),
        Center + float3(Extents.x, Extents.y, -Extents.z),
        Center + float3(Extents.x, Extents.y, Extents.z)
    };
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        Corners[i] = mul(float4(Corners[i], 1.f), InstanceData.ModelToWorld).xyz;
    }
    uint PlaneCount = Flags | INFINITE_Z_ENABLED ? 5 : 6;
    //only check the first 5 planes as the 6th plane is at infinity
    //looking for a plane that the proxy is completely outside of, if found, exit early
    [unroll]
    for (int i = 0; i < PlaneCount; ++i)
    {
        bool Visible = false;
        [unroll]
        for (int j = 0; j < 8; ++j)
        {
            //as long as one corner is visible, the proxy is visible
            if ((dot(Corners[j], FrustumPlanes[i].xyz) + FrustumPlanes[i].w) >= 0.f)
            {
                Visible = true;
                break;
            }
        }
        if (!Visible)
        {
            return;
        }
    }
    uint Index;
    InterlockedAdd(InstanceVisibleCountOutput[0], 1, Index);
    InstanceIdBufferOutput[Index] = DTid.x;
    
    //immediately populate the indirect args buffer
    FMeshData MeshData = MeshDataInput[InstanceData.MeshIndex];
    DrawIndexedIndirectArgsOutput[Index].indexCount = MeshData.IndexCount;
    DrawIndexedIndirectArgsOutput[Index].startIndexLocation = MeshData.IndexOffset;
    DrawIndexedIndirectArgsOutput[Index].baseVertexLocation = MeshData.VertexOffset;
    DrawIndexedIndirectArgsOutput[Index].instanceCount = 1;
    DrawIndexedIndirectArgsOutput[Index].startInstanceLocation = Index;
}

[numthreads(64, 1, 1)]
void OcclusionCullCS(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
    //go through all the instances in the instance id buffer and test occlusion
    uint InstanceId;
    if (Flags & IS_PHASE_1)
    {
        if (DTid.x >= InstanceVisibleCountOutput[0])
        {
            return;
        }
        InstanceId = InstanceIdBufferOutput[DTid.x];
    }
    else
    {
        if (DTid.x >= OccludedCountBufferOutput[0])
        {
            return;
        }
        InstanceId = OccludedInstanceIdBufferOutput[DTid.x];
    }
    FInstanceData InstanceData = InstanceDataInput[InstanceId];
    //each thread will cull 1 proxy
    float3 Center = MeshDataInput[InstanceData.MeshIndex].Center;
    float3 Extents = MeshDataInput[InstanceData.MeshIndex].Extents;
    float3 Corners[8] =
    {
        Center + float3(-Extents.x, -Extents.y, -Extents.z),
        Center + float3(-Extents.x, -Extents.y, Extents.z),
        Center + float3(-Extents.x, Extents.y, -Extents.z),
        Center + float3(-Extents.x, Extents.y, Extents.z),
        Center + float3(Extents.x, -Extents.y, -Extents.z),
        Center + float3(Extents.x, -Extents.y, Extents.z),
        Center + float3(Extents.x, Extents.y, -Extents.z),
        Center + float3(Extents.x, Extents.y, Extents.z)
    };
    //transform the corners into clipspace and to viewport space [0,1]
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        if (Flags & PREVIOUS_FRAME_ENABLED)
            Corners[i] = mul(float4(Corners[i], 1.f), InstanceData.PrevModelToWorld).xyz;
        else
            Corners[i] = mul(float4(Corners[i], 1.f), InstanceData.ModelToWorld).xyz;
        float4 ClipPosition = mul(float4(Corners[i], 1.f), ViewProjectionMatrix);
        Corners[i] = ClipPosition.xyz / ClipPosition.w;
        Corners[i].xy = clamp(Corners[i].xy, float2(-1.f, -1.f), float2(1.f, 1.f));
        Corners[i].xy = ScreenPosToViewportUV(Corners[i].xy);
        
    }
    float2 maxXY = float2(0.f, 0.f);
    float2 minXY = float2(1.f, 1.f);
    //get max z in reverse z as larger z value is closer to camera
    float maxZ = 0.f;
    //get the min max of the aabb box
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        minXY = min(minXY, Corners[i].xy);
        maxXY = max(maxXY, Corners[i].xy);
        maxZ = saturate(max(maxZ, Corners[i].z));
    }
    float4 boxUVs = float4(minXY, maxXY);
    //calculate hi-Z buffer mip
    int2 size = (maxXY - minXY) * float2(MipBaseWidth, MipBaseHeight);
    float mip = floor(log2(max(size.x, size.y)));
 
    //mip = clamp(mip, 0, MipLevels);
 
    //// Texel footprint for the lower (finer-grained) level
    //float level_lower = max(mip - 1, 0);
    //float2 scale = exp2(-level_lower);
    //float2 a = floor(boxUVs.xy * scale);
    //float2 b = ceil(boxUVs.zw * scale);
    //float2 dims = b - a;
 
    //// Use the lower level if we only touch <= 2 texels in both dimensions
    //if (dims.x <= 2 && dims.y <= 2)
    //    mip = level_lower;
 
    //load depths from high z buffer
    float4 depth =
    {
        HZBMips.SampleLevel(SamplerPoint, boxUVs.xy, mip),
        HZBMips.SampleLevel(SamplerPoint, boxUVs.zy, mip),
        HZBMips.SampleLevel(SamplerPoint, boxUVs.xw, mip),
        HZBMips.SampleLevel(SamplerPoint, boxUVs.zw, mip)
    };
    //find the min depth, smaller means further away
    float minDepth = min(min(min(depth.x, depth.y), depth.z), depth.w);
    //reverse z, smaller means closer to far plane, meaning likely behind
    bool Occluded = maxZ < minDepth;
 
    if (Occluded)
    {
        //if fail, place into failed buffer
        uint Index;
        InterlockedAdd(OccludedCountBufferOutput[0], 1, Index);
        //TODO: potential racing issue where threads have not read the id yet
        OccludedInstanceIdBufferOutput[Index] = InstanceId;
        DebugBuffer[Index] = mip + maxZ;
    }
    else
    {
        //if pass, place into a to draw buffer and populate the indirect draw args buffer
        uint Index;
        InterlockedAdd(InstanceNotOccludedCountOutput[0], 1, Index);
        //TODO: potential racing issue where threads have not read the id yet
        InstanceIdBufferOutput[Index] = InstanceId;
    
        //immediately populate the indirect args buffer
        FMeshData MeshData = MeshDataInput[InstanceData.MeshIndex];
        DrawIndexedIndirectArgsOutput[Index].indexCount = MeshData.IndexCount;
        DrawIndexedIndirectArgsOutput[Index].startIndexLocation = MeshData.IndexOffset;
        DrawIndexedIndirectArgsOutput[Index].baseVertexLocation = MeshData.VertexOffset;
        DrawIndexedIndirectArgsOutput[Index].instanceCount = 1;
        DrawIndexedIndirectArgsOutput[Index].startInstanceLocation = Index;
    }
    
}