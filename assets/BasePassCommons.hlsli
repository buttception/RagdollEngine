struct FBoundingBox
{
    float3 Center;
    float3 Extents;
};

//used in compute shader to populate indirect calls
struct FMeshData
{
    uint IndexCount;
    uint VertexCount;
    uint IndexOffset;
    uint VertexOffset;
	//TODO: bounding box if needed in the future
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