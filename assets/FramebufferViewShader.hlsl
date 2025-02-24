cbuffer g_Const : register(b0) {
	float4 CompAdd;
	float4 CompMul;
    float2 TexcoordAdd;
    float2 TexcoordMul;
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
    float2 finalTexcoord = inTexcoord * TexcoordMul + TexcoordAdd;
    [branch]
    switch(ComponentCount){
        case 1:
            finalColor.x = float(Source.Sample(Sampler, finalTexcoord).x) * CompMul.x + CompAdd.x;
            break;
        case 2:
            finalColor.xy = float2(Source.Sample(Sampler, finalTexcoord).xy) * CompMul.xy + CompAdd.xy;
            break;
        case 3:
            finalColor.xyz = Source.Sample(Sampler, finalTexcoord).xyz * CompMul.xyz + CompAdd.xyz;
            break;
        case 4:
            finalColor.xyzw = Source.Sample(Sampler, finalTexcoord).xyzw * CompMul.xyzw + CompAdd.xyzw;
            break;
        default:
            break;
    }
    outColor = finalColor;
}