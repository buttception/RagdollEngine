#include "BasePassCommons.hlsli"
#include "ShadingModel.hlsli"
#include "Utils.hlsli"

#define MAX_VERTICES 64
#define MAX_TRIANGLES 124
#define AS_GROUP_SIZE 64

cbuffer g_Const : register(b0)
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
    uint MipBaseWidth;
    uint MipBaseHeight;
    uint MipLevels;
    uint ProxyCount;
    uint Flags;
}

struct FAmplificationGroupInfo
{
    uint InstanceId;
    uint MeshletGroupOffset;
};

struct PayloadStruct
{
    uint MeshletIndices[AS_GROUP_SIZE];
    uint InstanceId;
};

#define INVALID 0
#define VISIBLE 1
#define CS_CULLED 2
#define AS_CULLED 3

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
    //each AS group has 64 threads, each thread processes 1 meshlet
    uint ASCount = (MeshData.MeshletCount + AS_GROUP_SIZE - 1) / AS_GROUP_SIZE;
    uint Index;
    InterlockedAdd(DispatchMeshleIndirectArgumentsBuffer[0].ThreadGroupCountX, ASCount, Index);
    for (uint i = 0; i < ASCount; i++)
    {
        AmplificationGroupInfoOutput[Index + i].InstanceId = dtid.x;
        AmplificationGroupInfoOutput[Index + i].MeshletGroupOffset = i;
    }
}

StructuredBuffer<FAmplificationGroupInfo> AmplificationGroupInfoInput : register(t7);
groupshared PayloadStruct s_Payload;
groupshared uint Test;

[numthreads(AS_GROUP_SIZE, 1, 1)]
void MeshletAS(in uint3 gtid: SV_GroupThreadID, in uint3 gid : SV_GroupID)
{
    //each AS thread cull one meshlet
    FAmplificationGroupInfo GroupInfo = AmplificationGroupInfoInput[gid.x];
    FInstanceData InstanceData = InstanceDatas[GroupInfo.InstanceId];
    if (gtid.x == 0)    //first thread set the payload instance id
    {
        s_Payload.InstanceId = GroupInfo.InstanceId;
        Test = 0;
    }
    uint VisiblityFlag = INVALID;
    uint MeshletIndex = GroupInfo.MeshletGroupOffset * AS_GROUP_SIZE + gtid.x;
    //hardcode all visible first
    if (MeshletIndex < Meshes[InstanceData.MeshIndex].MeshletCount)
    {
        VisiblityFlag = VISIBLE;
    }
    else
    {
        //set to count so i know it is invalid
        MeshletIndex = Meshes[InstanceData.MeshIndex].MeshletCount;
    }
    uint Index = WavePrefixCountBits(VisiblityFlag == VISIBLE);
    s_Payload.MeshletIndices[gtid.x] = MeshletIndex;
    if (VisiblityFlag == VISIBLE)
        InterlockedAdd(Test, 1);
    //uint VisibleCount = WaveActiveCountBits(VisiblityFlag == VISIBLE);
    //why is this 10 if i use wave active count bits
    DispatchMesh(Test, 1, 1, s_Payload);
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

VertexOutput GetVertexOutput(uint outInstanceId, FMeshData MeshData, uint vertexIndex)
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
    FMeshlet m = Meshlets[MeshData.MeshletGroupOffset + payload.MeshletIndices[gid.x]];
    SetMeshOutputCounts(m.VertexCount, m.TriangleCount);
    if (gtid < m.TriangleCount)
    {
        triangles[gtid] = GetPrimitive(m, MeshData, gtid);
    } 
    if (gtid < m.VertexCount)
    {
        vertices[gtid] = GetVertexOutput(payload.InstanceId, MeshData, GetVertexIndex(m, MeshData, gtid));
    }
}

Texture2D Textures[] : register(t0, space1);
sampler Samplers[9] : register(s0);