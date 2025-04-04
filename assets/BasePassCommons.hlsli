#define INFINITE_Z_ENABLED 1
#define PREVIOUS_FRAME_ENABLED 1 << 1
#define IS_PHASE_1 1 << 2
#define ALPHA_TEST_ENABLED 1 << 3
#define CULL_ALL 1 << 4

#define ENABLE_INSTANCE_FRUSTUM_CULL 1 << 5
#define ENABLE_INSTANCE_OCCLUSION_CULL 1 << 6
#define ENABLE_AS_FRUSTUM_CULL 1 << 6
#define ENABLE_AS_CONE_CULL 1 << 7
#define ENABLE_AS_OCCLUSION_CULL 1 << 8
#define ENABLE_MESHLET_COLOR 1 << 9

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

struct FMeshletBounds
{
	/* bounding sphere, useful for frustum and occlusion culling */
    float3 Center;
    float Radius;

	/* normal cone, useful for backface culling */
    float3 ConeApex;
    float ConeCutoff; /* = cos(angle/2) */
    float3 ConeAxis;
};

struct DispatchMeshleIndirectArguments
{
    uint ThreadGroupCountX;
    uint ThreadGroupCountY;
    uint ThreadGroupCountZ;
};