struct FBoundingBox
{
    float3 Center;
    float pad0;
    float3 Extents;
    float pad1;
};

struct FVertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 texcoord;
};

//used in compute shader to populate indirect calls
struct FMeshData
{
    uint IndexCount;
    uint VertexCount;
    uint IndexOffset;
    uint VertexOffset;
    
    uint MeshletCount;
    uint MeshletGroupOffset;
    uint MeshletGroupPrimitivesOffset;
    uint MeshletVerticesOffset;
    
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

#define ALPHA_MODE_OPAQUE 1
#define ALPHA_MODE_MASK 1 << 1
#define ALPHA_MODE_BLEND 1 << 2
#define DOUBLE_SIDED 1 << 3

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
    
    float AlphaCutoff;
    uint Flags;
};

struct FDrawIndexedIndirectArguments
{
    uint indexCount;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

struct FMeshlet
{
    uint VertexOffset;
    uint TriangleOffset;
    uint VertexCount;
    uint TriangleCount;
};

struct DispatchMeshleIndirectArguments
{
    uint ThreadGroupCountX;
    uint ThreadGroupCountY;
    uint ThreadGroupCountZ;
};