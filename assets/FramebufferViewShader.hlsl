cbuffer g_Const : register(b0) {
		float4 CompAdd;
		float4 CompMul;
		uint ComponentCount;
};

Texture2D Source : register(t0);
sampler Sampler : register(s0); 

void main_ps(
    in float4 inPos: SV_Position,
    in float2 inTexcoord : TEXCOORD0,
    out float4 outColor : SV_Target0
)
{
    float4 finalColor = float4(0.f, 0.f, 0.f, 1.f);
    [branch]
    switch(ComponentCount){
        case 1:
            finalColor.x = float(Source.Sample(Sampler, inTexcoord).x) * CompMul.x + CompAdd.x;
            break;
        case 2:
            finalColor.xy = float2(Source.Sample(Sampler, inTexcoord).xy) * CompMul.xy + CompAdd.xy;
            break;
        case 3:
            finalColor.xyz = Source.Sample(Sampler, inTexcoord).xyz * CompMul.xyz + CompAdd.xyz;
            break;
        case 4:
            finalColor.xyzw = Source.Sample(Sampler, inTexcoord).xyzw * CompMul.xyzw + CompAdd.xyzw;
            break;
        default:
            break;
    }
    outColor = finalColor;
}