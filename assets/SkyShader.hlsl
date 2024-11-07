#include "Utils.hlsli"

#define vl_pi 3.14159265358979323846

cbuffer g_Const : register(b0) {
    float4x4 InvViewProjMatrix;
    float4x4 ViewProj;
    float4x4 PrevViewProj;
	float3 CameraPosition;
    float3 PrevCameraPosition;
    float2 RenderResolution;
    int DrawVelocityOnSky;
};

Texture2D SkyTexture : register(t0);
sampler Sampler : register(s0);

void main_ps(
	in float4 inPos : SV_Position,
	in float2 inTexcoord : TEXCOORD0,
	out float3 outColor : SV_Target0,
    out float2 outVelocity : SV_Target1
)
{
    //get whr the pixel is in the sky
    float Depth = 0.5f;
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

    if(DrawVelocityOnSky == 1)
    {
        float3 delta = PrevCameraPosition - CameraPosition;
        float4 clipPos = mul(float4(fragPos, 1.f), ViewProj);
        float4 ndcPos = clipPos / clipPos.w;
        ndcPos.xy = ScreenPosToViewportUV(ndcPos.xy);
        float4 prevClipPos = mul(float4(fragPos - delta, 1.f), PrevViewProj);
        float4 prevNdcPos = prevClipPos / prevClipPos.w;
        prevNdcPos.xy = ScreenPosToViewportUV(prevNdcPos.xy);
        outVelocity = (prevNdcPos.xy - ndcPos.xy) * float2(RenderResolution.x, RenderResolution.y);
    }
}
