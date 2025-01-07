#include "BasePassCommons.hlsli"

#define INSTANCE_DATA_BUFFER_SRV_SLOT t0
#define MESH_BUFFER_SRV_SLOT t1

#define INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT u0
#define INSTANCE_ID_BUFFER_UAV_SLOT u1
#define INSTANCE_VISIBLE_COUNT_UAV_SLOT u2

cbuffer g_Const : register(b0)
{
    float4x4 ViewMatrix;
    float4 FrustumPlanes[6];
    uint ProxyCount;
    uint InfiniteZEnabled;
}

StructuredBuffer<FInstanceData> InstanceDataInput : register(INSTANCE_DATA_BUFFER_SRV_SLOT);
StructuredBuffer<FMeshData> MeshDataInput : register(MESH_BUFFER_SRV_SLOT);

RWStructuredBuffer<FDrawIndexedIndirectArguments> DrawIndexedIndirectArgsOutput : register(INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT);
RWStructuredBuffer<uint> InstanceIdBufferOutput : register(INSTANCE_ID_BUFFER_UAV_SLOT);
RWStructuredBuffer<uint> InstanceVisibleCountOutput : register(INSTANCE_VISIBLE_COUNT_UAV_SLOT);

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
    uint PlaneCount = InfiniteZEnabled == 1 ? 5 : 6;
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
    
}