struct InstanceData
{
    float4x4 worldMatrix;
    float4x4 invWorldMatrix;
    float4x4 prevWorldMatrix;

    float4 albedoFactor;
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

#define INSTANCE_DATA_BUFFER_SRV_SLOT t0
#define INSTANCE_BOUNDING_BOX_BUFFER_SRV_SLOT t1
#define INSTANCE_OFFSET_BUFFER_SRV_SLOT t2

#define INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT u0
#define INSTANCE_ID_BUFFER_UAV_SLOT u1

cbuffer g_Const : register(b0)
{
    float4x4 ViewMatrix;
    float4 FrustumPlanes[6];
    uint ProxyCount;
    uint MeshCount;
    uint InfiniteZEnabled;
}

RWStructuredBuffer<DrawIndexedIndirectArguments> DrawIndexedIndirectArgsOutput : register(INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT);

[numthreads(64, 1, 1)]
void ResetIndirectCS(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
    if (DTid.x < MeshCount)
    {
        DrawIndexedIndirectArgsOutput[DTid.x].instanceCount = 0;
    }
}

StructuredBuffer<InstanceData> InstanceDatas : register(INSTANCE_DATA_BUFFER_SRV_SLOT);
StructuredBuffer<BoundingBox> BoundingBoxInput : register(INSTANCE_BOUNDING_BOX_BUFFER_SRV_SLOT);
RWStructuredBuffer<uint> InstanceIdBufferOutput : register(INSTANCE_ID_BUFFER_UAV_SLOT);

[numthreads(64, 1, 1)]
void FrustumCullCS(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
    if (DTid.x >= ProxyCount)
    {
        return;
    }
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
    bool CompletelyOutside = true; // Assume completely outside initially
    [unroll]
    for (int i = 0; i < PlaneCount; ++i)
    {
        int VisibleCorners = 0;
        [unroll]
        for (int j = 0; j < 8; ++j)
        {
            float Distance = dot(Corners[j], FrustumPlanes[i].xyz) + FrustumPlanes[i].w;
            VisibleCorners |= (Distance >= 0.0f) ? 1 : 0; // Mark as visible if any corner is inside
        }
        CompletelyOutside &= (VisibleCorners == 0);
    }
    InstanceIdBufferOutput[DTid.x] = CompletelyOutside ? -1 : DTid.x;

}

StructuredBuffer<uint> InstanceOffsetBufferInput : register(INSTANCE_OFFSET_BUFFER_SRV_SLOT);

[numthreads(64, 1, 1)]
void PackInstanceIdCS(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
    if(DTid.x >= MeshCount)
        return;
    //1 thread 1 mesh, iterate through the id buffer and pack all the indices
    uint StartIndex = InstanceOffsetBufferInput[DTid.x];
    uint EndIndex = DTid.x == MeshCount - 1 ? ProxyCount : InstanceOffsetBufferInput[DTid.x + 1];
    uint InstanceCount = 0;
    for (int i = StartIndex; i < EndIndex; ++i)
    {
        int InstanceId = InstanceIdBufferOutput[i];
        if (InstanceId == -1)
        {
            continue;
        }
        InstanceIdBufferOutput[StartIndex + InstanceCount] = InstanceId;
        InstanceCount++;
    }
    DrawIndexedIndirectArgsOutput[DTid.x].instanceCount = InstanceCount;
    DrawIndexedIndirectArgsOutput[DTid.x].startInstanceLocation = DrawIndexedIndirectArgsOutput[DTid.x].startIndexLocation == 0 ? 0 : StartIndex;
}