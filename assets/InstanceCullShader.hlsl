#include "BasePassCommons.hlsli"
#include "Utils.hlsli"

#define INSTANCE_DATA_BUFFER_SRV_SLOT t1
#define MESH_BUFFER_SRV_SLOT t2
#define MATERIAL_BUFFER_SRV_SLOT t3

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
#define ALPHA_TEST_ENABLED 1 << 3
#define CULL_ALL 1 << 4

cbuffer g_Const : register(b0)
{
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4 FrustumPlanes[6];
    uint MipBaseWidth;
    uint MipBaseHeight;
    uint MipLevels;
    uint ProxyCount;
    uint Flags;
}

StructuredBuffer<FInstanceData> InstanceDataInput : register(INSTANCE_DATA_BUFFER_SRV_SLOT);
StructuredBuffer<FMeshData> MeshDataInput : register(MESH_BUFFER_SRV_SLOT);
StructuredBuffer<FMaterialData> MaterialDataInput : register(MATERIAL_BUFFER_SRV_SLOT);

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
    FMaterialData MaterialData = MaterialDataInput[InstanceData.MaterialIndex];
    if ((Flags & CULL_ALL) == 0 && Flags & ALPHA_TEST_ENABLED) //cull only mode blend or mask
    {
        if (MaterialData.Flags & ALPHA_MODE_OPAQUE)
            return;
    }
    if ((Flags & CULL_ALL) == 0 && (Flags & ALPHA_TEST_ENABLED) == 0) //cull only opaques
    {
        if ((MaterialData.Flags & ALPHA_MODE_OPAQUE) == 0)
            return;
    }
        
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
    if (DTid.x >= InstanceVisibleCountOutput[0])
    {
        return;
    }
    //go through all the instances in the instance id buffer and test occlusion
    uint InstanceId;
    float4x4 Transform;
    if (Flags & IS_PHASE_1)
    {
        InstanceId = InstanceIdBufferOutput[DTid.x];
        Transform = InstanceDataInput[InstanceId].PrevModelToWorld;
    }
    else
    {
        InstanceId = OccludedInstanceIdBufferOutput[DTid.x];
        Transform = InstanceDataInput[InstanceId].ModelToWorld;
    }
    FInstanceData InstanceData = InstanceDataInput[InstanceId];
    //each thread will cull 1 proxy
    float3 Center = MeshDataInput[InstanceData.MeshIndex].Center;
    float3 Extents = MeshDataInput[InstanceData.MeshIndex].Extents;
    //get transformed r
    float3 ScaleX = Transform[0].xyz;
    float3 ScaleY = Transform[1].xyz;
    float3 ScaleZ = Transform[2].xyz;
    float Scale = max(max(length(ScaleX), length(ScaleY)), length(ScaleZ));
    float Radius = length(Extents) * Scale;
    //calculate the closest possible position of the sphere in viewspace
    float3 PositionInWorld = mul(float4(Center, 1.f), Transform).xyz;
    float3 ViewSpacePosition = mul(float4(PositionInWorld, 1.f), ViewMatrix).xyz;
    ViewSpacePosition.z = -ViewSpacePosition.z;
    float3 ClosestViewSpacePoint = ViewSpacePosition - float3(0.f, 0.f, Radius);
    
    bool Occluded = true;
    //determine mip map level by getting projected bounds first
    float4 aabb = float4(0.f, 0.f, 0.f, 0.f);
    bool Valid = projectSphereView(ViewSpacePosition, Radius, ProjectionMatrix[3].z, ProjectionMatrix[0].x, ProjectionMatrix[1].y, aabb);
    float mip = -1.f;
    if (!Valid)
    {
        //if invalid, point is behind the near plane, just accpet as not occluded
        Occluded = false;
    }
    else
    {
        //get the position in clip space
        float4 ClipPosition = mul(float4(ClosestViewSpacePoint, 1.f), ProjectionMatrix);
        ClipPosition.xyz /= ClipPosition.w;
        ClipPosition.z = -ClipPosition.z;
        //clamp texcoord
        aabb = clamp(aabb, float4(0.f, 0.f, 0.f, 0.f), float4(1.f, 1.f, 1.f, 1.f));
        //calculate hi-Z buffer mip
        int2 size = (aabb.xy - aabb.zw) * float2(MipBaseWidth, MipBaseHeight);
        mip = ceil(log2(max(size.x, size.y)));
 
        //load depths from high z buffer
        float4 depth =
        {
            HZBMips.SampleLevel(SamplerPoint, aabb.xy, mip),
            HZBMips.SampleLevel(SamplerPoint, aabb.zy, mip),
            HZBMips.SampleLevel(SamplerPoint, aabb.xw, mip),
            HZBMips.SampleLevel(SamplerPoint, aabb.zw, mip)
        };
        //find the min depth, smaller means further away
        float minDepth = min(min(min(depth.x, depth.y), depth.z), depth.w);
        //reverse z, smaller means closer to far plane, meaning likely behind
        Occluded = ClipPosition.z < minDepth;
        DebugBuffer[DTid.x] = mip + ClipPosition.z;
    }
 
    if (Occluded)
    {
        //if fail, place into failed buffer
        uint Index;
        InterlockedAdd(OccludedCountBufferOutput[0], 1, Index);
        OccludedInstanceIdBufferOutput[Index] = InstanceId;
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