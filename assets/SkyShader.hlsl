#include "Utils.hlsli"

#define vl_pi 3.14159265358979323846

cbuffer g_Const : register(b0) {
    float4x4 InvViewProjMatrix;
	float3 CameraPosition;
};

Texture2D SkyTexture : register(t0);
Texture2D DepthBuffer : register(t1);
sampler Sampler : register(s0);

void main_ps(
	in float4 inPos : SV_Position,
	in float2 inTexcoord : TEXCOORD0,
	out float3 outColor : SV_Target0
)
{
    float Depth = DepthBuffer.Sample(Sampler, inTexcoord).r;
    if(Depth > 0.0001f)
        discard;
    //otherwise presume depth is at some funny value
    Depth = 0.001f;
	float3 fragPos = DepthToWorld(Depth, inTexcoord, InvViewProjMatrix);
    float3 dir = fragPos - CameraPosition;
    dir = normalize(dir);
    // Convert direction vector to spherical coordinates
    float theta = abs(asin(dir.y));
    float phi = atan2(dir.x, dir.z);

    // Map spherical coordinates to texture coordinates
    float mag = 1.f - theta / (vl_pi / 2.f);
    // Create texture coordinate
    float2 texCoord = float2(0.5f, 0.5f) * normalize(dir.xz) * mag * 0.5f + 0.5f;

    // Sample the sky texture using the computed texture coordinates
    float3 skyColor = SkyTexture.Sample(Sampler, texCoord).rgb;

    // Assign the sampled color to the output
    outColor = skyColor;
}

