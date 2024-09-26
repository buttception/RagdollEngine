float2 OctWrap(float2 v)
{
    return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}
 
float2 Encode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

float3 Decode(float2 f)
{
    f = f * 2.0 - 1.0;
 
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += n.xy >= 0.0 ? -t : t;
    return normalize(n);
}

// Maps standard viewport UV to screen position.
float2 ViewportUVToScreenPos(float2 ViewportUV)
{
	return float2(2 * ViewportUV.x - 1, 1 - 2 * ViewportUV.y);
}

float2 ScreenPosToViewportUV(float2 ScreenPos)
{
	return float2(0.5 + 0.5 * ScreenPos.x, 0.5 - 0.5 * ScreenPos.y);
}

float3 DepthToWorld(float depth, float2 inTexcoord, float4x4 InvViewProj)
{
	float4 clipspacePos = float4(ViewportUVToScreenPos(inTexcoord), depth, 1.0);
	float4 homogenousPos = mul(clipspacePos, InvViewProj);
    return homogenousPos.xyz / homogenousPos.w;;
}

float3 WorldToLight(float3 fragPos, float4x4 lightMatrix, float bias)
{
    float4 fragPosLightSpace = mul(float4(fragPos, 1.f), lightMatrix);
    float3 projCoord = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoord.xy = projCoord.xy * 0.5f + float2(0.5f, 0.5f);
    projCoord.y = projCoord.y * -1.0f + 1.0f;
    projCoord.z += bias;
    return projCoord;
}