#include "BasePassCommons.hlsli"
#include "ShadingModel.hlsli"
#include "Utils.hlsli"

#define MAX_VERTICES 64
#define MAX_TRIANGLES 124
#define AS_GROUP_SIZE 64

cbuffer g_Const : register(b1)
{
    float4x4 viewProjMatrix;
    float4x4 viewProjMatrixWithAA;
    float4x4 prevViewProjMatrix;
    float2 RenderResolution;
};

cbuffer g_Const : register(b0)
{
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4 FrustumPlanes[6];
    float3 CameraPosition;
    uint MipBaseWidth;
    uint MipBaseHeight;
    uint MipLevels;
    uint ProxyCount;
    uint Flags;
}

struct FAmplificationGroupInfo
{
    uint InstanceId;
    uint AmplificationGroupOffset;
};

struct PayloadStruct
{
    uint MeshletIndices[AS_GROUP_SIZE];
    uint InstanceId;
};

#define INVALID 0
#define VISIBLE 1

RWStructuredBuffer<DispatchMeshleIndirectArguments> DispatchMeshleIndirectArgumentsBuffer : register(u1);

[numthreads(1,1,1)]
void ResetMeshletIndirectCS()
{
    DispatchMeshleIndirectArgumentsBuffer[0].ThreadGroupCountX = 0;
    DispatchMeshleIndirectArgumentsBuffer[0].ThreadGroupCountY = 1;
    DispatchMeshleIndirectArgumentsBuffer[0].ThreadGroupCountZ = 1;
}

StructuredBuffer<FInstanceData> InstanceDatas : register(t0);
StructuredBuffer<FMeshData> Meshes : register(t6);

RWStructuredBuffer<FAmplificationGroupInfo> AmplificationGroupInfoOutput : register(u0);

bool FrustumCull(float3 Center, float3 Extents, float4x4 ModelToWorld)
{
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
        Corners[i] = mul(float4(Corners[i], 1.f), ModelToWorld).xyz;
    }
    uint PlaneCount = Flags | INFINITE_Z_ENABLED ? 5 : 6;
    for (int j = 0; j < PlaneCount; ++j)
    {
        bool Visible = false;
        for (int k = 0; k < 8; ++k)
        {
            if ((dot(Corners[k], FrustumPlanes[j].xyz) + FrustumPlanes[j].w) >= 0.f)
            {
                Visible = true;
                break;
            }
        }
        if (!Visible)
        {
            return false;
        }
    }
    return true;
}

[numthreads(64, 1, 1)]
void MeshletCullCS(in uint3 dtid : SV_DispatchThreadID)
{
    //frustum cull instance, phase 1 occlusion cull instance
    //temp for now, just give all instances to the meshlet shader
    if (dtid.x >= ProxyCount)
    {
        return;
    }
    FInstanceData InstanceData = InstanceDatas[dtid.x];
    FMeshData MeshData = Meshes[InstanceData.MeshIndex];
    //frustum cull this instance
    //each thread will cull 1 proxy
    float3 Center = MeshData.Center;
    float3 Extents = MeshData.Extents;
    //view planes are already in world space
    if ((Flags & ENABLE_INSTANCE_FRUSTUM_CULL) && !FrustumCull(Center, Extents, InstanceData.ModelToWorld))
    {
        return;
    }
    //each AS group has 64 threads, each thread processes 1 meshlet
    uint ASCount = (MeshData.MeshletCount + AS_GROUP_SIZE - 1) / AS_GROUP_SIZE;
    uint Index;
    InterlockedAdd(DispatchMeshleIndirectArgumentsBuffer[0].ThreadGroupCountX, ASCount, Index);
    for (uint asi = 0; asi < ASCount; asi++)
    {
        AmplificationGroupInfoOutput[Index + asi].InstanceId = dtid.x;
        AmplificationGroupInfoOutput[Index + asi].AmplificationGroupOffset = asi;
    }
}

StructuredBuffer<FAmplificationGroupInfo> AmplificationGroupInfoInput : register(t7);
StructuredBuffer<FMeshletBounds> MeshletBounds : register(t8);
groupshared PayloadStruct s_Payload;
groupshared uint MeshletCount;

bool FrustumCull(float3 Center, float Radius)
{
    uint PlaneCount = (Flags | INFINITE_Z_ENABLED) ? 5 : 6;
    
    for (int j = 0; j < PlaneCount; ++j)
    {
        float Distance = dot(Center, FrustumPlanes[j].xyz) + FrustumPlanes[j].w;

        // If the sphere is completely outside one plane, cull it
        if (Distance < -Radius)
        {
            return false;
        }
    }
    return true; // Sphere is at least partially visible
}

bool ConeCull(uint InstanceId, FMeshletBounds Bounds, float3 CameraPosition)
{
    float3 BoundsWorldApex = mul(float4(Bounds.ConeApex, 1.f), InstanceDatas[InstanceId].ModelToWorld).xyz;
    float3 BoundsWorldAxis = normalize(mul(float4(Bounds.ConeAxis, 0.f), InstanceDatas[InstanceId].ModelToWorld).xyz);
    
    float3 v = normalize(BoundsWorldApex - CameraPosition);
    if (dot(v, BoundsWorldAxis) >= Bounds.ConeCutoff)
    {
        return false;
    }
    return true;
}

[numthreads(AS_GROUP_SIZE, 1, 1)]
void MeshletAS(in uint3 gtid: SV_GroupThreadID, in uint3 gid : SV_GroupID)
{
    //each AS thread cull one meshlet
    FAmplificationGroupInfo GroupInfo = AmplificationGroupInfoInput[gid.x];
    FInstanceData InstanceData = InstanceDatas[GroupInfo.InstanceId];
    FMeshData MeshData = Meshes[InstanceData.MeshIndex];
    if (gtid.x == 0)    //first thread set the payload instance id
    {
        s_Payload.InstanceId = GroupInfo.InstanceId;
        MeshletCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    
    uint VisiblityFlag = INVALID;
    //group index * thread count + thread.x to get which meshlet to cull, index is within a mesh group
    uint LocalMeshletIndex = GroupInfo.AmplificationGroupOffset * AS_GROUP_SIZE + gtid.x;
    uint GlobalMeshletIndex = LocalMeshletIndex + MeshData.MeshletGroupOffset;
    
    if (LocalMeshletIndex < Meshes[InstanceData.MeshIndex].MeshletCount)
    {
        bool Visible = true;
        FMeshletBounds MeshletBound = MeshletBounds[GlobalMeshletIndex];
        //cone culling fastest
        if (Visible && Flags & ENABLE_AS_CONE_CULL)
        {
            Visible = ConeCull(s_Payload.InstanceId, MeshletBound, CameraPosition);
        }
        //frustum culling second fastest
        if (Visible && Flags & ENABLE_AS_FRUSTUM_CULL)
        {
            //get the meshlet bounding sphere with the new largest scale
            float3 s0 = length(InstanceData.ModelToWorld[0].xyz);
            float3 s1 = length(InstanceData.ModelToWorld[1].xyz);
            float3 s2 = length(InstanceData.ModelToWorld[2].xyz);
            float s = max(max(s0, s1), s2);
            //offset by meshlet group offset to access the global bounding sphere data
            float4 Sphere = float4(MeshletBound.Center, MeshletBound.Radius);
            float3 SphereWorldPos = mul(float4(Sphere.xyz, 1.f), InstanceData.ModelToWorld).xyz;
            float SphereRadius = Sphere.w * s;
            Visible = FrustumCull(SphereWorldPos, SphereRadius);
        }
        //occlusion culling last
        
        if (Visible)
        {
            VisiblityFlag = VISIBLE;
            uint Index;
            InterlockedAdd(MeshletCount, 1, Index);
            s_Payload.MeshletIndices[Index] = LocalMeshletIndex;
        }
    }
    //interlocked add all the debug buffers to readback
    DispatchMesh(MeshletCount, 1, 1, s_Payload);
}

StructuredBuffer<FMaterialData> MaterialDatas : register(t1);
StructuredBuffer<FVertex> Vertices : register(t2);
StructuredBuffer<FMeshlet> Meshlets : register(t3);
StructuredBuffer<uint> VertexIndices : register(t4);
StructuredBuffer<uint> TriangleIndices : register(t5);

struct VertexOutput
{
    float4 outPos : SV_Position;
    float4 outPrevFragPos : TEXCOORD1;
    float4 outFragPos : TEXCOORD2;
    float3 outNormal : TEXCOORD3;
    float3 outTangent : TEXCOORD4;
    float3 outBinormal : TEXCOORD5;
    float2 outTexcoord : TEXCOORD6;
    nointerpolation uint outInstanceId : TEXCOORD7;
    uint outMeshletIndex : TEXCOORD8;
};
//helpers
uint3 GetPrimitive(FMeshlet m, FMeshData MeshData, uint index)
{
    uint packed = TriangleIndices[m.TriangleOffset + index + MeshData.MeshletGroupPrimitivesOffset];
    return uint3(
        packed & 0xFF,
        (packed >> 8) & 0xFF,
        (packed >> 16) & 0xFF
    );
}

uint GetVertexIndex(FMeshlet m, FMeshData MeshData, uint index)
{
    return VertexIndices[m.VertexOffset + index + MeshData.MeshletVerticesOffset];
}

VertexOutput GetVertexOutput(uint outInstanceId, FMeshData MeshData, uint vertexIndex, uint meshletIndex)
{
    FVertex v = Vertices[vertexIndex + MeshData.VertexOffset];
    FInstanceData data = InstanceDatas[outInstanceId];
    VertexOutput vout;
    vout.outFragPos = mul(float4(v.position, 1), data.ModelToWorld);
    vout.outPrevFragPos = mul(float4(v.position, 1), data.PrevModelToWorld);
    vout.outPos = mul(vout.outFragPos, viewProjMatrixWithAA);

    float binormalSign = v.normal.x > 0.0f ? -1.0f : 1.0f;
    float3x3 AdjugateMatrix = Adjugate(data.ModelToWorld);
    vout.outNormal = normalize(mul(v.normal, AdjugateMatrix));
    vout.outTangent = normalize(mul(v.tangent, AdjugateMatrix));
    vout.outBinormal = normalize(cross(vout.outTangent, vout.outNormal)) * binormalSign;
    vout.outTexcoord = float2(v.texcoord.x, v.texcoord.y);
    vout.outInstanceId = outInstanceId;
    vout.outMeshletIndex = meshletIndex;
    return vout;
}

[OutputTopology("triangle")]
[numthreads(MAX_TRIANGLES, 1, 1)]
void MeshletMS(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    in payload PayloadStruct payload,
    out indices uint3 triangles[MAX_TRIANGLES],
    out vertices VertexOutput vertices[MAX_VERTICES]
)
{
    //each ms group deals with a meshlet with 64 vertices
    FMeshData MeshData = Meshes[InstanceDatas[payload.InstanceId].MeshIndex];
    uint MeshletIndex = MeshData.MeshletGroupOffset + payload.MeshletIndices[gid.x];
    FMeshlet m = Meshlets[MeshletIndex];
    SetMeshOutputCounts(m.VertexCount, m.TriangleCount);
    if (gtid < m.TriangleCount)
    {
        triangles[gtid] = GetPrimitive(m, MeshData, gtid);
    } 
    if (gtid < m.VertexCount)
    {
        vertices[gtid] = GetVertexOutput(payload.InstanceId, MeshData, GetVertexIndex(m, MeshData, gtid), MeshletIndex);
    }
}

Texture2D Textures[] : register(t0, space1);
sampler Samplers[9] : register(s0);

void MeshletPS(
	in float4 inPos : SV_Position,
	in float4 inPrevFragPos : TEXCOORD1,
	in float4 inFragPos : TEXCOORD2,
	in float3 inNormal : TEXCOORD3,
	in float3 inTangent : TEXCOORD4,
	in float3 inBinormal : TEXCOORD5,
	in float2 inTexcoord : TEXCOORD6,
	in uint inInstanceId : TEXCOORD7,
	in uint inMeshletIndex : TEXCOORD8,
	out float4 outColor : SV_Target0,
	out float2 outNormals : SV_Target1,
	out float2 outRoughnessMetallic : SV_Target2,
	out float2 outVelocity : SV_Target3
)
{
    FInstanceData data = InstanceDatas[inInstanceId];
    FMaterialData materialData = MaterialDatas[data.MaterialIndex];
	// Sample textures
    float4 albedo = materialData.AlbedoFactor;
    if (materialData.AlbedoIndex != -1)
    {
        albedo *= Textures[materialData.AlbedoIndex].Sample(Samplers[materialData.AlbedoSamplerIndex], inTexcoord);
    }
	if(materialData.Flags & ALPHA_MODE_MASK)
	{
		clip(albedo.a - materialData.AlphaCutoff);
		albedo.a = 1.f;
	}
	else
		clip(albedo.a <= 0.f ? -1.f : 1.f);	//min clipping
    float4 RM = float4(0.f, materialData.RoughnessFactor, materialData.MetallicFactor, 0);
    if (materialData.ORMIndex != -1)
    {
        RM = Textures[materialData.ORMIndex].Sample(Samplers[materialData.ORMSamplerIndex], inTexcoord);
    }
    float ao = 1.f;
    float roughness = RM.g;
    float metallic = RM.b;

	// Sample normal map and leave it in model space, the deferred lighting will calculate this instead
    float3 N = inNormal;
    if (materialData.NormalIndex != -1)
    {
        float3 normalMapValue = normalize(Textures[materialData.NormalIndex].Sample(Samplers[materialData.NormalSamplerIndex], inTexcoord).xyz * 2.0f - 1.0f);
        float3x3 TBN = float3x3(inTangent, inBinormal, inNormal);
        N = normalize(mul(normalMapValue, TBN));
    }

	//draw to the targets
    outColor = albedo;
    if (Flags & ENABLE_MESHLET_COLOR)
        outColor *= float4(ColorPalette[inMeshletIndex % 32], 1.f);
    outNormals.xy = Encode(N);
    outRoughnessMetallic = float2(roughness, metallic);
    float4 clipPos = mul(inFragPos, viewProjMatrix);
    float4 ndcPos = clipPos / clipPos.w;
    ndcPos.xy = ScreenPosToViewportUV(ndcPos.xy);
    float4 prevClipPos = mul(inPrevFragPos, prevViewProjMatrix);
    float4 prevNdcPos = prevClipPos / prevClipPos.w;
    prevNdcPos.xy = ScreenPosToViewportUV(prevNdcPos.xy);
	//no need div by 2 because i moved it back to viewport so its [0,1]
    outVelocity = (prevNdcPos.xy - ndcPos.xy) * float2(RenderResolution.x, RenderResolution.y);
}