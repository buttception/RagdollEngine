struct Constants
{
    float2 invDisplaySize;
};

#ifdef SPIRV

[[vk::push_constant]] ConstantBuffer<Constants> g_Const;

#else

cbuffer g_Const : register(b0) { Constants g_Const; }

#endif

struct VS_INPUT
{
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

PS_INPUT main_vs(VS_INPUT input)
{
    PS_INPUT output;
    output.pos.xy = input.pos.xy * g_Const.invDisplaySize * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.pos.zw = float2(0, 1);
    output.col = input.col;
    output.uv = input.uv;
    return output;
}

sampler sampler0 : register(s0);
Texture2D texture0 : register(t0);

float4 main_ps(PS_INPUT input) : SV_Target
{
    float4 out_col = input.col * texture0.Sample(sampler0, input.uv);
    return out_col;
}