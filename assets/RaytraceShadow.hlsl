#include "Utils.hlsli"
#include "BasePassCommons.hlsli"

// ---[ Resources ]---
cbuffer g_Const : register(b0)
{
    float4x4 InvViewProjMatrixWithJitter;
    float3 LightDirection;
    uint Scroll;
    float RenderWidth;
    float RenderHeight;
    float SunSize;
};

RWTexture2D<float4> u_Output : register(u0);
RaytracingAccelerationStructure SceneBVH : register(t0);
Texture2D t_GBufferDepth : register(t1);
StructuredBuffer<FInstanceData> InstanceBuffer : register(t2);
StructuredBuffer<FMaterialData> MaterialBuffer : register(t3);
StructuredBuffer<FMeshData> MeshDataBuffer : register(t4);
StructuredBuffer<FVertex> VertexBuffer : register(t5);
StructuredBuffer<uint> IndexBuffer : register(t6);
Texture2D<float4> Noise : register(t7);

Texture2D Textures[] : register(t0, space1);
sampler Samplers[9] : register(s0);

#define PI 3.14159265359
#define SAMPLE_COUNT 16

#ifndef INLINE
// ---[ Structures ]---

struct HitInfo
{
    float ShadowValue;
};

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
    uint2 globalIdx = DispatchRaysIndex().xy;
    float2 pixelPosition = float2(globalIdx) + 0.5;
    float2 inTexcoord = pixelPosition / float2(RenderWidth, RenderHeight);
    float depth = t_GBufferDepth.Load(int3(pixelPosition, 0)).x;
    //ignore farplane
    if (depth == 0.f)
        return;
    float3 fragPos = DepthToWorld(depth, inTexcoord, InvViewProjMatrixWithJitter);
    
    float3 lightTangent = normalize(cross(LightDirection, float3(0.0f, 1.0f, 0.0f)));
    float3 lightBitangent = normalize(cross(lightTangent, LightDirection));
    
    // Setup the ray
    RayDesc ray;
    ray.Origin = fragPos += LightDirection * 0.001f;
    ray.TMin = 0.01f;
    ray.TMax = 100.f;
    ray.Direction = LightDirection;
    
#if 1
    int2 TexelPos = int2(globalIdx.x % 128, globalIdx.y % 128);
    float RadiusRng = Noise[TexelPos].x * SunSize;
    float AngleRng = Noise[TexelPos].y * 2.0f * PI;
    float2 DiscPoint = float2(RadiusRng * cos(AngleRng), RadiusRng * sin(AngleRng));
    ray.Direction = normalize(LightDirection + lightTangent * DiscPoint.x + lightBitangent * DiscPoint.y);
    
    HitInfo payload;
    payload.ShadowValue = 0.f;
    TraceRay(
        SceneBVH,
        0,
        0xFF,
        0,
        0,
        0,
        ray,
        payload);
    u_Output[globalIdx] = float4(payload.ShadowValue, 0.xx, 1.f);
#else
    HitInfo payload;
    payload.ShadowValue = 0.f;
    TraceRay(
        SceneBVH,
        0,
        0xFF,
        0,
        0,
        0,
        ray,
        payload);
    u_Output[globalIdx] = float4(1.xxx, payload.ShadowValue);
#endif
    
}

// ---[ Miss Shader ]---

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.ShadowValue = 0.f;
}

[shader("anyhit")]
void AnyHit(inout HitInfo payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    FInstanceData Instance = InstanceBuffer[InstanceID()];
    FMaterialData Material = MaterialBuffer[Instance.MaterialIndex];
    //if it is opaque just exit
    if (Material.Flags & ALPHA_MODE_OPAQUE)
    {
        payload.ShadowValue = 1.f;
        AcceptHitAndEndSearch();
    }
    bool cutoffEnabled = Material.Flags & ALPHA_MODE_MASK;
    if (Material.AlbedoIndex == -1)//there is no texture, check if it is transparent
    {
        if (Material.AlbedoFactor.a - (cutoffEnabled ? Material.AlphaCutoff : 0.f) <= 0.0f)
        {
        //do nothing if is transparent
            IgnoreHit();
        }
        else
        {
            //means there is something opaque in the way, will cast shadow no matter what
            payload.ShadowValue = 1.f;
            AcceptHitAndEndSearch();
        }
    }
    FMeshData Mesh = MeshDataBuffer[Instance.MeshIndex];
    float3 Barycentric = float3(1.f - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    float2 v0 = VertexBuffer[IndexBuffer[Mesh.IndexOffset + PrimitiveIndex() * 3 + 0] + Mesh.VertexOffset].texcoord;
    float2 v1 = VertexBuffer[IndexBuffer[Mesh.IndexOffset + PrimitiveIndex() * 3 + 1] + Mesh.VertexOffset].texcoord;
    float2 v2 = VertexBuffer[IndexBuffer[Mesh.IndexOffset + PrimitiveIndex() * 3 + 2] + Mesh.VertexOffset].texcoord;
    float2 uv = v0 * Barycentric.x + v1 * Barycentric.y + v2 * Barycentric.z;
    float4 color = Textures[Material.AlbedoIndex].SampleLevel(Samplers[Material.AlbedoSamplerIndex], uv, 0);
    color.a *= Material.AlbedoFactor.a;
    if (color.a - (cutoffEnabled ? Material.AlphaCutoff : 0.f) <= 0.f)
    {
        //do nothing if transparent
        IgnoreHit();
    }
    else
    {
        //means there is something opaque in the way, will cast shadow no matter what
        payload.ShadowValue = 1.f;
        AcceptHitAndEndSearch();
    }
}
#else
//return true if hit, false if not
bool TestAlpha(uint InstanceId)
{
    return true;
}

//inline raytracing compute
[numthreads(8, 8, 1)]
void RaytraceShadowCS(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
    //each pixel one ray
    if (DTid.x >= RenderWidth || DTid.y >= RenderHeight)
        return;
    float2 pixelPosition = float2(DTid.xy) + 0.5;
    float2 inTexcoord = pixelPosition / float2(RenderWidth, RenderHeight);
    float depth = t_GBufferDepth.Load(int3(pixelPosition, 0)).x;
    //ignore farplane
    if (depth == 0.f)
        return;
    float3 fragPos = DepthToWorld(depth, inTexcoord, InvViewProjMatrixWithJitter);
    
    float3 lightTangent = normalize(cross(LightDirection, float3(0.0f, 1.0f, 0.0f)));
    float3 lightBitangent = normalize(cross(lightTangent, LightDirection));
    
    // Setup the ray
    RayDesc ray;
    ray.Origin = fragPos += LightDirection * 0.001f;
    ray.TMin = 0.01f;
    ray.TMax = 100.f;
    ray.Direction = LightDirection;
    RayQuery<RAY_FLAG_NONE> q;
    q.TraceRayInline(SceneBVH, 0, 0xFF, ray);
    
    while(q.Proceed())
    {
        if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            //check for alpha
            u_Output[DTid.xy] = float4(0.f, 0.xx, 1.f);
            break;
        }
    }
}
#endif