struct FBoundingBox
{
    float3 Center;
    float pad0;
    float3 Extents;
    float pad1;
};

//used in compute shader to populate indirect calls
struct FMeshData
{
    uint IndexCount;
    uint VertexCount;
    uint IndexOffset;
    uint VertexOffset;
    float3 Center;
    float3 Extents;
};

struct FInstanceData
{
    float4x4 ModelToWorld;
    float4x4 PrevModelToWorld;
    uint MeshIndex;
    uint MaterialIndex;
};

struct FMaterialData
{
    float4 AlbedoFactor;
    float RoughnessFactor;
    float MetallicFactor;

    int AlbedoIndex;
    int AlbedoSamplerIndex;
    int NormalIndex;
    int NormalSamplerIndex;
    int ORMIndex;
    int ORMSamplerIndex;
    int bIsLit;
};

struct FDrawIndexedIndirectArguments
{
    uint indexCount;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};