
cbuffer g_Const : register(b0) 
{
    uint Width;
    uint Height;
};

Texture2D Source : register(t0);
sampler Sampler : register(s0);

void downsample_ps(
	in float4 inPos : SV_Position,
	in float2 inTexcoord : TEXCOORD0,
	out float3 outColor : SV_Target0
)
{
    float2 srcTexelSize = float2(1.f / Width, 1.f / Height);
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = Source.Sample(Sampler, float2(inTexcoord.x - 2*x, inTexcoord.y + 2*y)).rgb;
    float3 b = Source.Sample(Sampler, float2(inTexcoord.x,       inTexcoord.y + 2*y)).rgb;
    float3 c = Source.Sample(Sampler, float2(inTexcoord.x + 2*x, inTexcoord.y + 2*y)).rgb;

    float3 d = Source.Sample(Sampler, float2(inTexcoord.x - 2*x, inTexcoord.y)).rgb;
    float3 e = Source.Sample(Sampler, float2(inTexcoord.x,       inTexcoord.y)).rgb;
    float3 f = Source.Sample(Sampler, float2(inTexcoord.x + 2*x, inTexcoord.y)).rgb;

    float3 g = Source.Sample(Sampler, float2(inTexcoord.x - 2*x, inTexcoord.y - 2*y)).rgb;
    float3 h = Source.Sample(Sampler, float2(inTexcoord.x,       inTexcoord.y - 2*y)).rgb;
    float3 i = Source.Sample(Sampler, float2(inTexcoord.x + 2*x, inTexcoord.y - 2*y)).rgb;

    float3 j = Source.Sample(Sampler, float2(inTexcoord.x - x, inTexcoord.y + y)).rgb;
    float3 k = Source.Sample(Sampler, float2(inTexcoord.x + x, inTexcoord.y + y)).rgb;
    float3 l = Source.Sample(Sampler, float2(inTexcoord.x - x, inTexcoord.y - y)).rgb;
    float3 m = Source.Sample(Sampler, float2(inTexcoord.x + x, inTexcoord.y - y)).rgb;

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    // a,b,d,e * 0.125
    // b,c,e,f * 0.125
    // d,e,g,h * 0.125
    // e,f,h,i * 0.125
    // j,k,l,m * 0.5
    // This shows 5 square areas that are being sampled. But some of them overlap,
    // so to have an energy preserving downsample we need to make some adjustments.
    // The weights are the distributed, so that the sum of j,k,l,m (e.g.)
    // contribute 0.5 to the final color output. The code below is written
    // to effectively yield this sum. We get:
    // 0.125*5 + 0.03125*4 + 0.0625*4 = 1
    outColor = e*0.125;
    outColor += (a+c+g+i)*0.03125;
    outColor += (b+d+f+h)*0.0625;
    outColor += (j+k+l+m)*0.125;
    outColor = max(outColor, 0.0001f);
}


cbuffer g_Const : register(b0)
{
    uint OutputWidth;
    uint OutputHeight;
    uint MipLevel;
};

#define MAX_HZB_MIP_COUNT 16
RWTexture2D<float> Mips[MAX_HZB_MIP_COUNT] : register(u0);

[numthreads(8, 8, 1)]
void DownSamplePoTCS(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
    if (all(DTid.xy < float2(OutputWidth, OutputHeight)))
    {
        float4 depths;
        if (MipLevel == 0)
        {
            depths = Source.Gather(Sampler, (DTid.xy + 0.5) / float2(OutputWidth, OutputHeight));
        }
        else
        {
            depths.x = Mips[MipLevel - 1].Load(int3(DTid.xy * 2 + int2(0, 0), 0));
            depths.y = Mips[MipLevel - 1].Load(int3(DTid.xy * 2 + int2(1, 0), 0));
            depths.z = Mips[MipLevel - 1].Load(int3(DTid.xy * 2 + int2(0, 1), 0));
            depths.w = Mips[MipLevel - 1].Load(int3(DTid.xy * 2 + int2(1, 1), 0));
        }
        Mips[MipLevel][DTid.xy] = min(min(depths.x, depths.y), min(depths.z, depths.w));
    }
}