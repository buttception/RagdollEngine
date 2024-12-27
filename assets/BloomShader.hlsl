
Texture2D Source : register(t0);
Texture2D SceneColor : register(t1);
sampler Sampler : register(s0);

cbuffer g_Const : register(b0)
{
    uint Width;
    uint Height;
    float FilterRadius;
    float BloomIntensity;
};

void main_ps(
	in float4 inPos : SV_Position,
	in float2 inTexcoord : TEXCOORD0,
	out float3 outColor : SV_Target0
)
{
    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    float x = FilterRadius;
    float y = FilterRadius;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = Source.Sample(Sampler, float2(inTexcoord.x - x, inTexcoord.y + y)).rgb;
    float3 b = Source.Sample(Sampler, float2(inTexcoord.x,     inTexcoord.y + y)).rgb;
    float3 c = Source.Sample(Sampler, float2(inTexcoord.x + x, inTexcoord.y + y)).rgb;

    float3 d = Source.Sample(Sampler, float2(inTexcoord.x - x, inTexcoord.y)).rgb;
    float3 e = Source.Sample(Sampler, float2(inTexcoord.x,     inTexcoord.y)).rgb;
    float3 f = Source.Sample(Sampler, float2(inTexcoord.x + x, inTexcoord.y)).rgb;

    float3 g = Source.Sample(Sampler, float2(inTexcoord.x - x, inTexcoord.y - y)).rgb;
    float3 h = Source.Sample(Sampler, float2(inTexcoord.x,     inTexcoord.y - y)).rgb;
    float3 i = Source.Sample(Sampler, float2(inTexcoord.x + x, inTexcoord.y - y)).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    float3 color = e*4.0;
    color = e*4.0;
    color += (b+d+f+h)*2.0;
    color += (a+c+g+i);
    color *= 1.0 / 16.0;
    if(BloomIntensity > 0.001f)
        outColor = lerp(SceneColor.Sample(Sampler, inTexcoord).rgb, color, BloomIntensity);
    else
        outColor = color;
}