cbuffer g_Const : register(b0) {
		float CompAdd;
		float CompMul;
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
    float4 finalColor = float4(1.f, 1.f, 1.f, 1.f);
    [branch]
    switch(ComponentCount){
        case 1:
            finalColor.x = Source.Sample(Sampler, inTexcoord).x * CompMul + CompAdd;
            break;
        case 2:
            finalColor.xy = Source.Sample(Sampler, inTexcoord).xy * CompMul + CompAdd.xx;
            break;
        case 3:
            finalColor.xyz = Source.Sample(Sampler, inTexcoord).xyz * CompMul + CompAdd.xxx;
            break;
        case 4:
            finalColor.xyzw = Source.Sample(Sampler, inTexcoord).xyzw * CompMul + CompAdd.xxxx;
            break;
        default:
            break;
    }
    outColor = finalColor;
}