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
    float3 Extent;
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
    //view planes are in view space, move all corners to view space
    float3 Corners[8] =
    {
        mul(float4(BBox.Center + float3(-BBox.Extent.x, -BBox.Extent.y, -BBox.Extent.z), 1.f), ViewMatrix).xyz,
        mul(float4(BBox.Center + float3(-BBox.Extent.x, -BBox.Extent.y, BBox.Extent.z), 1.f), ViewMatrix).xyz,
        mul(float4(BBox.Center + float3(-BBox.Extent.x, BBox.Extent.y, -BBox.Extent.z), 1.f), ViewMatrix).xyz,
        mul(float4(BBox.Center + float3(-BBox.Extent.x, BBox.Extent.y, BBox.Extent.z), 1.f), ViewMatrix).xyz,
        mul(float4(BBox.Center + float3(BBox.Extent.x, -BBox.Extent.y, -BBox.Extent.z), 1.f), ViewMatrix).xyz,
        mul(float4(BBox.Center + float3(BBox.Extent.x, -BBox.Extent.y, BBox.Extent.z), 1.f), ViewMatrix).xyz,
        mul(float4(BBox.Center + float3(BBox.Extent.x, BBox.Extent.y, -BBox.Extent.z), 1.f), ViewMatrix).xyz,
        mul(float4(BBox.Center + float3(BBox.Extent.x, BBox.Extent.y, BBox.Extent.z), 1.f), ViewMatrix).xyz
    };
    uint VisibilityMask = 0;
    //only check the first 5 planes as the 6th plane is at infinity
    [unroll]
    for (int i = 0; i < 6; ++i)
    {
        [unroll]
        for (int j = 0; j < 8; ++j)
        {
            //as long as one corner is visible, the proxy is visible
            VisibilityMask |= (dot(FrustumPlanes[i].xyz, Corners[j]) + FrustumPlanes[i].w) >= 0.f ? 1 : 0;
        }
    }
    //if the mask is 0, all points are outside the frustum
    InstanceIdBufferOutput[DTid.x] = VisibilityMask != 0 ? DTid.x : -1;
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

}