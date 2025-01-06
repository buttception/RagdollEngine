struct FInstanceData
{
    float4x4 worldMatrix;
    float4x4 prevWorldMatrix;

    float4 albedoFactor;
    uint meshIndex;
    float roughness;
    float metallic;

    int albedoIndex;
    int albedoSamplerIndex;
    int normalIndex;
    int normalSamplerIndex;
    int roughnessMetallicIndex;
    int roughnessMetallicSamplerIndex;
    int isLit;
};

struct DrawIndexedIndirectArguments
{
    uint indexCount;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

struct BoundingBox
{
    float3 Center;
    float pad0;
    float3 Extent;
    float pad1;
};

struct MeshData
{
    uint IndexCount;
    uint VertexCount;
    uint IndexOffset;
    uint VertexOffset;
};

#define INSTANCE_DATA_BUFFER_SRV_SLOT t0
#define INSTANCE_BOUNDING_BOX_BUFFER_SRV_SLOT t1
#define MESH_BUFFER_SRV_SLOT t2

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
StructuredBuffer<BoundingBox> BoundingBoxInput : register(INSTANCE_BOUNDING_BOX_BUFFER_SRV_SLOT);
StructuredBuffer<MeshData> MeshDataInput : register(MESH_BUFFER_SRV_SLOT);

RWStructuredBuffer<DrawIndexedIndirectArguments> DrawIndexedIndirectArgsOutput : register(INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT);
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
    BoundingBox BBox = BoundingBoxInput[DTid.x];
    //view planes are already in world space
    float3 Corners[8] =
    {
        BBox.Center + float3(-BBox.Extent.x, -BBox.Extent.y, -BBox.Extent.z),
        BBox.Center + float3(-BBox.Extent.x, -BBox.Extent.y, BBox.Extent.z),
        BBox.Center + float3(-BBox.Extent.x, BBox.Extent.y, -BBox.Extent.z),
        BBox.Center + float3(-BBox.Extent.x, BBox.Extent.y, BBox.Extent.z),
        BBox.Center + float3(BBox.Extent.x, -BBox.Extent.y, -BBox.Extent.z),
        BBox.Center + float3(BBox.Extent.x, -BBox.Extent.y, BBox.Extent.z),
        BBox.Center + float3(BBox.Extent.x, BBox.Extent.y, -BBox.Extent.z),
        BBox.Center + float3(BBox.Extent.x, BBox.Extent.y, BBox.Extent.z)
    };
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
    DrawIndexedIndirectArgsOutput[Index].indexCount = MeshDataInput[InstanceData.meshIndex].IndexCount;
    DrawIndexedIndirectArgsOutput[Index].startIndexLocation = MeshDataInput[InstanceData.meshIndex].IndexOffset;
    DrawIndexedIndirectArgsOutput[Index].baseVertexLocation = MeshDataInput[InstanceData.meshIndex].VertexOffset;
    DrawIndexedIndirectArgsOutput[Index].instanceCount = 1;
    DrawIndexedIndirectArgsOutput[Index].startInstanceLocation = Index;
}