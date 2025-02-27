#include "Utils.hlsli"

// ---[ Structures ]---

struct HitInfo
{
    bool missed;
};

// ---[ Resources ]---
cbuffer g_Const : register(b0)
{
    float4x4 InvViewProjMatrixWithJitter;
    float3 LightDirection;
    float RenderWidth;
    float RenderHeight;
};

RWTexture2D<float4> u_Output : register(u0);
RaytracingAccelerationStructure SceneBVH : register(t0);
Texture2D t_GBufferDepth : register(t1);

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void RayGen()
{
    uint2 globalIdx = DispatchRaysIndex().xy;
    float2 pixelPosition = float2(globalIdx) + 0.5;
    float2 inTexcoord = pixelPosition / float2(RenderWidth, RenderHeight);
    float depth = t_GBufferDepth.Load(int3(pixelPosition, 0)).x;
    float3 fragPos = DepthToWorld(depth, inTexcoord, InvViewProjMatrixWithJitter);
    
    // Setup the ray
    RayDesc ray;
    ray.Origin = fragPos;
    ray.Direction = LightDirection;
    ray.TMin = 0.01f;
    ray.TMax = 100.f;
    
    // Trace the ray
    HitInfo payload;
    payload.missed = false;

    TraceRay(
        SceneBVH,
        0,
        0xFF,
        0,
        0,
        0,
        ray,
        payload);
    
    float shadow = (payload.missed) ? 0.f : 1.f;
    u_Output[globalIdx] = float4(1.xxx, shadow);
}

// ---[ Miss Shader ]---

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.missed = true;
}
