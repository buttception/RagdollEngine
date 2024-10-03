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
    //get the frag pos
	float3 fragPos = DepthToWorld(DepthBuffer.Sample(Sampler, inTexcoord).r, inTexcoord, InvViewProjMatrix);
    
    float distanceFromCam = mul(float4(fragPos, 1.f), View).z;
    //decide which matrix and texture to use
    Texture2D shadowMap;
    float4x4 lightMatrix;
    float4 color = float4(1.f, 1.f, 1.f, 1.f);
    float bias;
    if(distanceFromCam < 5.f)
    {
        if(EnableCascadeDebug)
            color = float4(1.f, 0.5f, 0.5f, 1.f);
        shadowMap = ShadowMaps[0];
        lightMatrix = LightViewProj[0];
        bias = 0.002f;
    }
    else if(distanceFromCam < 10.f)
    {
        if(EnableCascadeDebug)
            color = float4(1.f, 1.f, 0.5f, 1.f);
        shadowMap = ShadowMaps[1];
        lightMatrix = LightViewProj[1];
        bias = 0.003f;
    }
    else if(distanceFromCam < 15.f)
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
    outColor.a = shadow;
    outColor.rgb = color.rgb;
}