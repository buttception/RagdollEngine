#include "Utils.hlsli"

#define vl_pi 3.14159265358979323846

cbuffer g_Const : register(b0) {
    float4x4 InvViewProjMatrix;
	float3 CameraPosition;
};

Texture2D SkyTexture : register(t0);
sampler Sampler : register(s0);

void main_ps(
	in float4 inPos : SV_Position,
	in float2 inTexcoord : TEXCOORD0,
	out float3 outColor : SV_Target0
)
{
    //get whr the pixel is in the sky
    float Depth = 0.01f;
    //this frag pos is relative the camera but in world coordinate
	float3 fragPos = DepthToWorld(Depth, inTexcoord, InvViewProjMatrix);
    //get the direction of the fragment from the camera
    float3 dir = normalize(fragPos - CameraPosition);

    // Create texture coordinate
    float2 texCoord = float2(0.5f, 0.5f) + dir.xz * 0.5f;

    // Sample the sky texture using the computed texture coordinates
    float3 skyColor = SkyTexture.Sample(Sampler, texCoord).rgb;

    // Assign the sampled color to the output
    outColor = skyColor;
}
