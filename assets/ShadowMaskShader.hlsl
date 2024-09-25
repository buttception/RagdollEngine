cbuffer g_Const : register(b0){
    float4x4 LightViewProj[4];
    float4x4 InvViewProjMatrix;
    float4x4 View;
	int EnableCascadeDebug;
}

Texture2D ShadowMaps[4] : register(t0);
Texture2D DepthBuffer : register(t4);
sampler Samplers[8] : register(s0);
SamplerComparisonState ShadowSample : register(s9);

void main_ps(
    in float4 inPos : SV_Position,
    in float2 inTexcoord : TEXCOORD1,
    out float4 outColor : SV_Target0
)
{
    //get the frag pos
	float depth = DepthBuffer.Sample(Samplers[5], inTexcoord).r;
    inTexcoord.y = 1.f - inTexcoord.y;
	float4 clipspacePos = float4(inTexcoord * 2.0 - 1.0, depth, 1.0);
	float4 homogenousPos = mul(clipspacePos, InvViewProjMatrix);
	float3 fragPos = homogenousPos.xyz / homogenousPos.w;
    
    float distanceFromCam = mul(float4(fragPos, 1.f), View).z;
    //decide which matrix and texture to use
    Texture2D shadowMap;
    float4x4 lightMatrix;
    float4 color = float4(1.f, 1.f, 1.f, 1.f);
    if(distanceFromCam < 5.f)
    {
        if(EnableCascadeDebug)
            color = float4(1.f, 0.5f, 0.5f, 1.f);
        shadowMap = ShadowMaps[0];
        lightMatrix = LightViewProj[0];
    }
    else if(distanceFromCam < 10.f)
    {
        if(EnableCascadeDebug)
            color = float4(1.f, 1.f, 0.5f, 1.f);
        shadowMap = ShadowMaps[1];
        lightMatrix = LightViewProj[1];
    }
    else if(distanceFromCam < 15.f)
    {
        if(EnableCascadeDebug)
            color = float4(0.5f, 1.f, 0.5f, 1.f);
        shadowMap = ShadowMaps[2];
        lightMatrix = LightViewProj[2];
    }
    else
    {
        if(EnableCascadeDebug)
            color = float4(1.f, 0.5f, 1.f, 1.f);
        shadowMap = ShadowMaps[3];
        lightMatrix = LightViewProj[3];
    }

    //get frag pos in light space
    float4 fragPosLightSpace = mul(float4(fragPos, 1.f), lightMatrix);
    float3 projCoord = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoord.xy = projCoord.xy * 0.5f + float2(0.5f, 0.5f);
    projCoord.y = projCoord.y * -1.0f + 1.0f;
    projCoord.z += 0.005f;
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