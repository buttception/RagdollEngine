#include "Utils.hlsli"

#define GROUP_SIZE 256
#define THREADS_X 16
#define THREADS_Y 16

#define vl_pi 3.14159265358979323846
#define vl_halfPi vl_pi / 2.0

// Uniforms:
cbuffer g_Const : register(b0)
{
    float3 sun;
    uint width;
    float3 PerezInvDen;
    uint height;
};

Texture2D<float4> ThetaGammaTable : register(t0);
RWTexture2D<float3> Target : register(u0);

float MapGamma(float g)
{
    return sqrt(0.5f * (1.0f - g));
}
float4 LerpArray(float s, int index)
{
    s = clamp(s, 0.f, 1.f - 1e-6f);
    s *= 64 - 1;

    int si0 = (int)s;
    int si1 = (si0 + 1);

    float sf = s - si0;

    return ThetaGammaTable[int2(si0, index)] * (1 - sf)
            + ThetaGammaTable[int2(si1, index)] *    sf   ;
}
float3 SkyRGB(float3 v)  // Use precalculated table to return fast sky colour on CPU
{
    float cosTheta = v.z;
    float cosGamma = dot(sun, v);

    if (cosTheta < 0.0f)
        cosTheta = 0.0f;

    float t = cosTheta;
    float g = MapGamma(cosGamma);

    float3 F = LerpArray(t, 0).xyz;
    float3 G = LerpArray(g, 1).xyz;

    float3 xyY = (float3(1.f, 1.f, 1.f) - F) * (float3(1.f, 1.f, 1.f) + G);

    xyY *= PerezInvDen;

    return convertxyYToRGB(xyY);
}
int HemiInset(float y2, int width)
{
    float maxX2 = 1.0f - y2;
    float maxX = sqrt(maxX2);

    return (int)(ceil((1.0f - maxX) * width / 2.0f + 0.5f));
}

[numthreads(THREADS_X, THREADS_Y, 1)]
void main_cs(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
    //update the hemisphere texture, each thread draws 1 pixel
    // Calculate normalized coordinates
    float x = (2.0f * (DTid.x + 0.5f) / width) - 1.0f; // Normalized x coordinate
    float y = (2.0f * (DTid.y + 0.5f) / height) - 1.0f; // Normalized y coordinate
    float y2 = y * y; // y^2 for further calculations

    // Compute the width of the surround area
    int sw = HemiInset(y2, width);

    // If we are within the surround area, skip this thread
    if (DTid.x < sw || DTid.x >= width - sw)
    {
        Target[DTid.xy] = float3(0.0f, 0.0f, 0.0f); // Fill with black or transparent
        return;
    }

    // Compute the x and y coordinates in the hemisphere
    float x2 = x * x;
    float h2 = x2 + y2; // h^2 = x^2 + y^2

    // Compute the direction vector
    float3 v = float3(x, y, sqrt(1.0f - h2));

    // Get the color from the sky based on the computed vector
    float3 c = SkyRGB(v); 
    c *= 4.99999987e-05;

    // Write the final color to the target texture, is in hdr
    Target[DTid.xy] = c;

    GroupMemoryBarrierWithGroupSync();
}
