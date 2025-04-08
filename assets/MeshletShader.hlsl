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
#define CONE_CULLED 2
#define FRUSTUM_CULLED 3
#define OCCLUSION_CULLED 4

StructuredBuffer<FInstanceData> InstanceDatas : register(t0);
StructuredBuffer<FMaterialData> MaterialDatas : register(t1);
StructuredBuffer<FVertex> Vertices : register(t2);
StructuredBuffer<FMeshlet> Meshlets : register(t3);
StructuredBuffer<uint> VertexIndices : register(t4);
StructuredBuffer<uint> TriangleIndices : register(t5);
StructuredBuffer<FMeshData> Meshes : register(t6);
StructuredBuffer<FAmplificationGroupInfo> AmplificationGroupInfoInput : register(t7);
StructuredBuffer<FMeshletBounds> MeshletBounds : register(t8);
StructuredBuffer<uint> InstanceCountInput : register(t9);
StructuredBuffer<uint> InstanceIdBufferInput : register(t10);
Texture2D<float> HZBMips : register(t11);

Texture2D Textures[] : register(t0, space1);
sampler Samplers[9] : register(s0);

RWStructuredBuffer<FAmplificationGroupInfo> AmplificationGroupInfoOutput : register(u0);
RWStructuredBuffer<DispatchMeshleIndirectArguments> DispatchMeshleIndirectArgumentsBuffer : register(u1);
RWStructuredBuffer<uint> MeshletOcclusionCulledPhase1CountOutput : register(u6);
RWStructuredBuffer<uint> MeshletOcclusionCulledPhase2CountOutput : register(u7);
RWStructuredBuffer<uint> MeshletFrustumCulledCountOutput : register(u8);
RWStructuredBuffer<uint> MeshletDegenerateConeCountOutput : register(u9);

[numthreads(1,1,1)]
void ResetMeshletIndirectCS()
{
    DispatchMeshleIndirectArgumentsBuffer[0].ThreadGroupCountX = 0;
    DispatchMeshleIndirectArgumentsBuffer[0].ThreadGroupCountY = 1;
    DispatchMeshleIndirectArgumentsBuffer[0].ThreadGroupCountZ = 1;
}

[numthreads(64, 1, 1)]
void MeshletBuildCommandParameters(in uint3 dtid : SV_DispatchThreadID)
{
    //after the instance culling pass, we have a list of visible instances
    //update the amplification group info buffer and indirect meshlet draw buffer
    if (dtid.x >= InstanceCountInput[0])
    {
        return;
    }
    uint InstanceId = InstanceIdBufferInput[dtid.x];
    FInstanceData InstanceData = InstanceDatas[InstanceId];
    FMeshData MeshData = Meshes[InstanceData.MeshIndex];
    //add the instance meshlets to the indirect draw buffer and amplification group info buffer
    //1 thread 1 meshlet for the AS
    uint ASCount = (MeshData.MeshletCount + AS_GROUP_SIZE - 1) / AS_GROUP_SIZE;
    uint Index;
    InterlockedAdd(DispatchMeshleIndirectArgumentsBuffer[0].ThreadGroupCountX, ASCount, Index);
    for (uint asi = 0; asi < ASCount; asi++)
    {
        AmplificationGroupInfoOutput[Index + asi].InstanceId = InstanceId;
        AmplificationGroupInfoOutput[Index + asi].AmplificationGroupOffset = asi;
    }
}

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

bool OcclusionCull(float3 SphereWorldPos, float SphereRadius)
{
    float3 ViewSpacePosition = mul(float4(SphereWorldPos, 1.f), ViewMatrix).xyz;
    ViewSpacePosition.z = -ViewSpacePosition.z;
    float3 ClosestViewSpacePoint = ViewSpacePosition - float3(0.f, 0.f, SphereRadius);
    //determine mip map level by getting projected bounds first
    float4 aabb = float4(0.f, 0.f, 0.f, 0.f);
    bool Valid = projectSphereView(ViewSpacePosition, SphereRadius, ProjectionMatrix[3].z, ProjectionMatrix[0].x, ProjectionMatrix[1].y, aabb);
    float mip = -1.f;
    if (!Valid)
    {
        //if invalid, point is behind the near plane, just accept as not occluded
        return true;
    }
    else
    {
        //get the position in clip space
        float4 ClipPosition = mul(float4(ClosestViewSpacePoint, 1.f), ProjectionMatrix);
        ClipPosition.xyz /= ClipPosition.w;
        ClipPosition.z = -ClipPosition.z;
        //clamp texcoord
        aabb = clamp(aabb, float4(0.f, 0.f, 0.f, 0.f), float4(1.f, 1.f, 1.f, 1.f));
        //calculate hi-Z buffer mip
        int2 size = (aabb.xy - aabb.zw) * float2(MipBaseWidth, MipBaseHeight);
        mip = ceil(log2(max(size.x, size.y)));
 
        //load depths from high z buffer
        float4 depth =
        {
            HZBMips.SampleLevel(Samplers[0], aabb.xy, mip),
            HZBMips.SampleLevel(Samplers[0], aabb.zy, mip),
            HZBMips.SampleLevel(Samplers[0], aabb.xw, mip),
            HZBMips.SampleLevel(Samplers[0], aabb.zw, mip)
        };
        //find the min depth, smaller means further away
        float minDepth = min(min(min(depth.x, depth.y), depth.z), depth.w);
        //reverse z, smaller means closer to far plane, meaning likely behind
        return ClipPosition.z >= minDepth;
    }
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
        FMeshletBounds MeshletBound = MeshletBounds[GlobalMeshletIndex];
        //cone culling fastest i tinkA
        if (Flags & ENABLE_AS_CONE_CULL)
        {
            if (!ConeCull(s_Payload.InstanceId, MeshletBound, CameraPosition))
                VisiblityFlag = CONE_CULLED;
        }
        //get the meshlet bounding sphere with the new largest scale
        float3 s0 = length(InstanceData.ModelToWorld[0].xyz);
        float3 s1 = length(InstanceData.ModelToWorld[1].xyz);
        float3 s2 = length(InstanceData.ModelToWorld[2].xyz);
        float s = max(max(s0, s1), s2);
        //offset by meshlet group offset to access the global bounding sphere data
        float3 SphereWorldPos = mul(float4(MeshletBound.Center, 1.f), InstanceData.ModelToWorld).xyz;
        float SphereRadius = MeshletBound.Radius * s;
        //frustum culling second fastest
        if (VisiblityFlag == INVALID && Flags & ENABLE_AS_FRUSTUM_CULL)
        {
            if (!FrustumCull(SphereWorldPos, SphereRadius))
                VisiblityFlag = FRUSTUM_CULLED;
        }
        //occlusion culling last
        if (VisiblityFlag == INVALID && Flags & ENABLE_AS_OCCLUSION_CULL)
        {
            if (!OcclusionCull(SphereWorldPos, SphereRadius))
                VisiblityFlag = OCCLUSION_CULLED;
        }
        //if still invalid, it passed all tests, and hence is visible
        if (VisiblityFlag == INVALID)
        {
            VisiblityFlag = VISIBLE;
            uint Index;
            InterlockedAdd(MeshletCount, 1, Index);
            s_Payload.MeshletIndices[Index] = LocalMeshletIndex;
        }
    }
    if (VisiblityFlag == CONE_CULLED)
        InterlockedAdd(MeshletDegenerateConeCountOutput[0], 1);
    if (VisiblityFlag == FRUSTUM_CULLED)
        InterlockedAdd(MeshletFrustumCulledCountOutput[0], 1);
    if (VisiblityFlag == OCCLUSION_CULLED)
        if (Flags & IS_PHASE_1)
            InterlockedAdd(MeshletOcclusionCulledPhase1CountOutput[0], 1);
        else
            InterlockedAdd(MeshletOcclusionCulledPhase2CountOutput[0], 1);
    //interlocked add all the debug buffers to readback
    DispatchMesh(MeshletCount, 1, 1, s_Payload);
}

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
    else if (Flags & ENABLE_INSTANCE_COLOR)
        outColor *= float4(ColorPalette[inInstanceId % 32], 1.f);
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