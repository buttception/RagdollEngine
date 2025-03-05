#include "Utils.hlsli"

cbuffer g_Const : register(b0){
    float4x4 LightViewProj[4];
    float4x4 InvViewProjMatrix;
    float4x4 View;
	int EnableCascadeDebug;
}

Texture2D ShadowMaps[4] : register(t0);
Texture2D DepthBuffer : register(t4);
sampler Sampler : register(s0);
SamplerComparisonState ShadowSample : register(s1);

void main_ps(
    in float4 inPos : SV_Position,
    in float2 inTexcoord : TEXCOORD0,
    out float4 outColor : SV_Target0
)
{
    const float SubfrustaFarPlanes[5] = { 0.001f, 10.f, 25.f, 50.f, 100.f };
    //get the frag pos
	float3 fragPos = DepthToWorld(DepthBuffer.Sample(Sampler, inTexcoord).r, inTexcoord, InvViewProjMatrix);
    float distanceFromCam = length(fragPos - View[3].xyz);
    //float distanceFromCam = mul(float4(fragPos, 1.f), View).z;
    //decide which matrix and texture to use
    Texture2D shadowMap;
    float4x4 lightMatrix;
    float4 color = float4(1.f, 1.f, 1.f, 1.f);
    float bias;
    if (distanceFromCam < SubfrustaFarPlanes[1])
    {
        if(EnableCascadeDebug)
            color = float4(1.f, 0.5f, 0.5f, 1.f);
        shadowMap = ShadowMaps[0];
        lightMatrix = LightViewProj[0];
        bias = 0.002f;
    }
    else if (distanceFromCam < SubfrustaFarPlanes[2])
    {
        if(EnableCascadeDebug)
            color = float4(1.f, 1.f, 0.5f, 1.f);
        shadowMap = ShadowMaps[1];
        lightMatrix = LightViewProj[1];
        bias = 0.003f;
    }
    else if (distanceFromCam < SubfrustaFarPlanes[3])
    {
        if(EnableCascadeDebug)
            color = float4(0.5f, 1.f, 0.5f, 1.f);
        shadowMap = ShadowMaps[2];
        lightMatrix = LightViewProj[2];
        bias = 0.004f;
    }
    else
    {
        if(EnableCascadeDebug)
            color = float4(1.f, 0.5f, 1.f, 1.f);
        shadowMap = ShadowMaps[3];
        lightMatrix = LightViewProj[3];
        bias = 0.007f;
    }

    //get frag pos in light space
    float3 projCoord = WorldToLight(fragPos, lightMatrix, bias);
    float shadow = 0.f;

    // Percentage-Closer Filtering (PCF) for smoother shadow edges.
    float2 texelSize = float2(1.f, 1.f) / 2000.f;
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            shadow += shadowMap.SampleCmpLevelZero(ShadowSample, projCoord.xy + offset, projCoord.z);
        }
    }
    // Divide by number of samples (9)
    shadow /= 9.f;
    //write shadow factor into the shadow mask
    outColor.r = shadow;
    outColor.gba = color.rgb;
}

#define TILE_X 8
#define TILE_Y 4

cbuffer g_Const : register(b0)
{
    float2 ShadowMaskSize;
}

Texture2D<float4> ShadowMaskInput : register(t0);
RWTexture2D<uint> ShadowMaskOutput : register(u0);

//only used with the raytracing shadow mask result
[numthreads(8, 8, 1)]
void CompressShadowMaskCS(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
    ShadowMaskOutput[DTid.xy] = 0;
    for (int x = 0; x < TILE_X; ++x)
    {
        for (int y = 0; y < TILE_Y; ++y)
        {
            uint2 coord = uint2(DTid.x * TILE_X + x, DTid.y * TILE_Y + y);
            if (coord.x < ShadowMaskSize.x && coord.y < ShadowMaskSize.y)
            {
                //shadow result is binary, so each bit in the uint32 is 1 or 0
                if ((1.f - ShadowMaskInput[coord].r) > 0.f)
                {
                    ShadowMaskOutput[DTid.xy] |= 1 << (y * TILE_X + x);
                }
            }
        }
    }

}